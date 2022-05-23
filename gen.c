// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include "8cc.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool dumpsource = true;

static int nregs;
static Buffer *outbuf = &(Buffer){0, 0, 0};
static const char *curfunc;
static Map locals;
static Type *rettype;

static int emit_expr(Node *node);

#define newline() buf_printf(outbuf, "\n")
#define emit_noindent(...) (buf_printf(outbuf, __VA_ARGS__), newline())
#define emit(...) emit_noindent("    " __VA_ARGS__)

Buffer *emit_end(void)
{
    Buffer *ret = outbuf;
    outbuf = make_buffer();
    return ret;
}

static bool kind_is_int(int kind)
{
    return kind == KIND_PTR || kind == KIND_BOOL || kind == KIND_CHAR || kind == KIND_SHORT || kind == KIND_INT || kind == KIND_LONG || kind == KIND_LLONG || kind == KIND_ENUM || kind == KIND_PTR;
}

static bool kind_is_float(int kind)
{
    return kind == KIND_FLOAT || kind == KIND_DOUBLE || kind == KIND_LDOUBLE;
}

static int emit_conv_type(Type *from, Type *to, int reg)
{
    if (kind_is_int(from->kind) && kind_is_float(to->kind))
    {
        int next = nregs++;
        emit("r%i <- utof r%i", next, reg);
        return next;
    }
    if (kind_is_float(from->kind) && kind_is_int(to->kind))
    {
        int next = nregs++;
        emit("r%i <- ftou r%i", next, reg);
        return next;
    }
    return reg;
}

static void emit_label(char *label)
{
    emit_noindent("@%s%s", curfunc, label);
}

static char *pathof_node(Node *node)
{
    if (node->kind == AST_STRUCT_REF)
    {
        return format("%s.%s", pathof_node(node->struc), node->field);
    }
    else
    {
        return node->varname;
    }
}

static int emit_assign(Node *node)
{
    if (node->left->kind == AST_STRUCT_REF)
    {
        char *name = pathof_node(node->left);
        int rhs = emit_expr(node->right);
        int out = (int)(size_t)map_get(&locals, name);
        emit("r%i <- reg r%i", out, rhs);
        return out;
    }
    else if (node->left->kind == AST_DEREF)
    {
        int ret = nregs++;
        int addr = emit_expr(node->left->operand);
        int val = emit_expr(node->right);
        emit("r%i <- call lib.pool.set r1 r%i r%i", ret, addr, val);
        return val;
    }
    else
    {
        int rhs = emit_expr(node->right);
        int out = (int)(size_t)map_get(&locals, node->left->varname);
        emit("r%i <- reg r%i", out, rhs);
        return out;
    }
}

static int emit_binop(Node *node)
{
    int lhs = emit_expr(node->left);
    int rhs = emit_expr(node->right);
    char typepre = node->left->ty->kind == KIND_INT ? 'u' : 'f';
    switch (node->kind)
    {
    case '+':
    {
        int ret = nregs++;
        emit("r%i <- %cadd r%i r%i", ret, typepre, lhs, rhs);
        return ret;
    }
    case '-':
    {
        int ret = nregs++;
        emit("r%i <- %csub r%i r%i", ret, typepre, lhs, rhs);
        return ret;
    }
    case '*':
    {
        int ret = nregs++;
        emit("r%i <- %cmul r%i r%i", ret, typepre, lhs, rhs);
        return ret;
    }
    case '/':
    {
        int ret = nregs++;
        emit("r%i <- %cdiv r%i r%i", ret, typepre, lhs, rhs);
        return ret;
    }
    case '%':
    {
        int ret = nregs++;
        emit("r%i <- %cmod r%i r%i", ret, typepre, lhs, rhs);
        return ret;
    }
    case OP_EQ:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cbeq r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- %cint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    case OP_NE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cbeq r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- %cint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    case '<':
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- uint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    case '>':
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", typepre, rhs, lhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- uint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    case OP_LE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", typepre, rhs, lhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    case OP_GE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret, typepre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, typepre);
        emit_label(end);
        return ret;
    }
    default:
        error("invalid operator '%d'", node->kind);
    }
    return 0;
}

static void emit_jmp(char *label)
{
    emit("jump %s%s", curfunc, label);
}

static int emit_literal(Node *node)
{
    if (node->ty->kind != KIND_INT)
    {
        int ret = nregs++;
        emit("r%i <- fint %lu", ret, (unsigned long)node->fval);
        return ret;
    }
    else
    {
        int ret = nregs++;
        emit("r%i <- uint %lu", ret, (unsigned long)node->ival);
        return ret;
    }
}

static int emit_struct_all(Type *ty, char *path)
{
    if (ty->kind == KIND_STRUCT)
    {
        int reg = nregs++;
        emit("r%i <- uint 0", reg);
        for (int i = vec_len(ty->fields->key) - 1; i >= 0; i--)
        {
            char *name = vec_get(ty->fields->key, i);
            Type *ent = dict_get(ty->fields, name);
            char *nextpath = format("%s.%s", path, name);
            int val = emit_struct_all(ent, nextpath);
            emit("r%i <- cons r%i r%i", reg, val, reg);
        }
        return reg;
    }
    else
    {
        return (int)(size_t)map_get(&locals, path);
    }
}

static const char *emit_args(Vector *vals, Vector *params)
{
    const char *args = "";
    for (int i = 0; i < vec_len(vals) && i < vec_len(params); i++)
    {
        Node *v = vec_get(vals, i);
        int regno = emit_expr(v);
        Node *p = vec_get(params, i);
        args = format("%s r%i", args, regno);
    }
    return args;
}

static int emit_func_call(Node *node)
{
    const char *args = emit_args(node->args, node->ftype->params);
    if (node->kind == AST_FUNCPTR_CALL)
    {
        return 4984;
    }
    else if (!strcmp(node->fname, "__minivm_get"))
    {
        int ret = nregs++;
        emit("r%i <- call lib.pool.get r1%s", ret, args);
        return ret;
    }
    else if (!strcmp(node->fname, "__minivm_set"))
    {
        int ret = nregs++;
        emit("r%i <- call lib.pool.set r1%s", ret, args);
        return ret;
    }
    else if (!strcmp(node->fname, "putchar"))
    {
        emit("putchar%s", args);
        return 0;
    }
    else
    {
        int ret = nregs++;
        emit("r%i <- call func.%s r1%s", ret, node->fname, args);
        return ret;
    }
}

static int emit_conv(Node *node)
{
    int reg1 = emit_expr(node->operand);
    int reg2 = emit_conv_type(node->operand->ty, node->ty, reg1);
    return reg2;
}

static void emit_branch_bool(Node *node, char *zero, char *nonzero)
{
    if (node->kind == '<' || node->kind == '>' || node->kind == OP_GE || node->kind == OP_LE || node->kind == OP_EQ || node->kind == OP_NE)
    {
        int lhs = emit_expr(node->left);
        int rhs = emit_expr(node->right);
        char typepre = kind_is_int(node->left->ty->kind) ? 'u' : 'f';
        switch (node->kind)
        {
        case OP_EQ:
            emit("%cbeq r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, zero, curfunc, nonzero);
            break;
        case OP_NE:
            emit("%cbeq r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, nonzero, curfunc, zero);
            break;
        case '<':
            emit("%cblt r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, zero, curfunc, nonzero);
            break;
        case '>':
            emit("%cblt r%i r%i %s%s %s%s", typepre, rhs, lhs, curfunc, zero, curfunc, nonzero);
            break;
        case OP_LE:
            emit("%cblt r%i r%i %s%s %s%s", typepre, rhs, lhs, curfunc, nonzero, curfunc, zero);
            break;
        case OP_GE:
            emit("%cblt r%i r%i %s%s %s%s", typepre, lhs, rhs, curfunc, nonzero, curfunc, zero);
            break;
        }
    }
    else
    {
        int nth = emit_expr(node);
        emit("ubb r%i %s%s %s%s", nth, curfunc, zero, curfunc, nonzero);
    }
}

static void emit_if(Node *node)
{
    char *zero = make_label();
    char *nonzero = make_label();
    emit_branch_bool(node->cond, zero, nonzero);
    if (node->then)
    {
        emit_label(nonzero);
        int r = emit_expr(node->then);
    }
    else
    {
        emit_label(nonzero);
    }
    if (node->els)
    {
        char *end = make_label();
        emit_jmp(end);
        emit_label(zero);
        int r = emit_expr(node->els);
        emit_label(end);
    }
    else
    {
        emit_label(zero);
    }
}

static int emit_ternary(Node *node)
{
    char *ez = make_label();
    char *nz = make_label();
    int nth = emit_expr(node->cond);
    int out = nregs++;
    emit("ubb r%i %s%s %s%s", nth, curfunc, ez, curfunc, nz);
    if (node->then)
    {
        emit_label(nz);
        int r = emit_expr(node->then);
        emit("r%i <- reg r%i", out, r);
    }
    else
    {
        emit_label(nz);
    }
    if (node->els)
    {
        char *end = make_label();
        emit_jmp(end);
        emit_label(ez);
        int r = emit_expr(node->els);
        emit("r%i <- reg r%i", out, r);
        emit_label(end);
    }
    else
    {
        emit_label(ez);
    }
    return out;
}

static int emit_return(Node *node)
{
    if (node->retval)
    {
        int i = emit_expr(node->retval);
        emit("ret r%i", i);
    }
    else
    {
        emit("r0 <- uint 0");
        emit("ret r0");
    }
    return 0;
}

static void regs_for_pair_struct(char *name, Type *var, int reg)
{
    if (var->kind == KIND_STRUCT)
    {
        for (int i = 0; i < vec_len(var->fields->key); i++)
        {
            int r = nregs++;
            char *key = vec_get(var->fields->key, i);
            char *nextname = format("%s.%s", name, key);
            emit("r%i <- getcar r%i", r, reg);
            regs_for_pair_struct(nextname, dict_get(var->fields, key), r);
            int nextreg = nregs++;
            emit("r%i <- getcdr r%i", nextreg, reg);
            reg = nextreg;
        }
    }
    else
    {
        map_put(&locals, name, (void *)(size_t)reg);
    }
}

static void regs_for_struct(char *name, Type *var, Vector *inits, int *n)
{
    if (var->kind == KIND_STRUCT)
    {
        if (inits && *n < vec_len(inits))
        {
            Node *node = vec_get(inits, *n);
            if (node->initval->ty->kind == KIND_STRUCT)
            {
                *n += 1;
                int val = emit_expr(node->initval);
                regs_for_pair_struct(name, var, val);
                return;
            }
        }
        for (int i = 0; var->fields && i < vec_len(var->fields->key); i++)
        {
            char *src = vec_get(var->fields->key, i);
            Type *ty2 = dict_get(var->fields, src);
            regs_for_struct(format("%s.%s", name, src), ty2, inits, n);
        }
    }
    else
    {
        int reg = nregs++;
        map_put(&locals, name, (void *)(size_t)reg);
        if (inits && *n < vec_len(inits))
        {
            Node *node = vec_get(inits, *n);
            int val = emit_expr(node->initval);
            int expr = emit_conv_type(node->initval->ty, var, val);
            emit("r%i <- reg r%i", reg, expr);
            *n += 1;
        }
        else
        {
            emit("r%i <- uint 0", reg);
        }
    }
}

static int emit_lvar(Node *node)
{
    if (node->lvarinit)
    {
        int n = 0;
        regs_for_struct(node->varname, node->ty, node->lvarinit, &n);
    }
    if (node->ty->kind == KIND_STRUCT)
    {
        return emit_struct_all(node->ty, node->varname);
    }
    else
    {
        return (int)(size_t)map_get(&locals, node->varname);
    }
}

static int emit_struct_ref(Node *node)
{
    int val = emit_expr(node->struc);
    Type *type = node->struc->ty;
    for (int i = 0; i < vec_len(type->fields->key); i++)
    {
        char *name = vec_get(type->fields->key, i);
        int reg = nregs++;
        if (!strcmp(name, node->field))
        {
            emit("r%i <- getcar r%i", reg, val);
            return reg;
        }
        emit("r%i <- getcdr r%i", reg, val);
        val = reg;
    }
    return 124;
}

static int emit_deref(Node *node)
{
    int outreg = nregs++;
    int from = emit_expr(node->operand);
    emit("r%i <- call lib.pool.get r1 r%i", outreg, from);
    return outreg;
}

static int emit_expr(Node *node)
{
    switch (node->kind)
    {
    case AST_DEREF:
        return emit_deref(node);
    case AST_GOTO:
        emit("jump %s%s", curfunc, node->newlabel);
        return 0;
    case AST_LABEL:
        if (node->newlabel)
        {
            emit_label(node->newlabel);
        }
        return 0;
    case AST_LITERAL:
        return emit_literal(node);
    case AST_LVAR:
        return emit_lvar(node);
    case AST_FUNCPTR_CALL:
        error("dcall");
        return 0;
    case AST_FUNCALL:
        return emit_func_call(node);
    case AST_CONV:
        return emit_conv(node);
    case AST_IF:
        emit_if(node);
        return 0;
    case AST_DECL:
    {
        int n = 0;
        regs_for_struct(node->declvar->varname, node->declvar->ty, node->declinit, &n);
        return 0;
    }
    case AST_TERNARY:
        return emit_ternary(node);
    case AST_RETURN:
        emit_return(node);
        return 0;
    case AST_COMPOUND_STMT:
    {
        int ret = 0;
        for (int i = 0; i < vec_len(node->stmts); i++)
            ret = emit_expr(vec_get(node->stmts, i));
        return ret;
    }
    case OP_CAST:
        return emit_conv(node);
    case AST_STRUCT_REF:
        return emit_struct_ref(node);
    case '=':
        return emit_assign(node);
    case '+':
    case '-':
    case '*':
    case '/':
    case '%':
    case OP_EQ:
    case OP_NE:
    case '<':
    case '>':
    case OP_GE:
    case OP_LE:
        return emit_binop(node);
    default:
        error("node(%i) = %s", node->kind, node2s(node));
    }
}

static void emit_func_prologue(Node *func)
{
    if (!strcmp(func->fname, "main"))
    {
        emit_noindent("@main");
        emit("r0 <- call lib.pool.init");
        emit("r0 <- call func.main r0");
        emit("exit");
    }
    emit_noindent("func func.%s", func->fname, nregs);
    rettype = func->ty->rettype;
    locals = EMPTY_MAP;
    nregs = vec_len(func->params) + 2;
    for (int i = 0; i < vec_len(func->params); i++)
    {
        Node *param = vec_get(func->params, i);
        int reg = i + 2;
        if (param->ty->kind == KIND_STRUCT)
        {
            regs_for_pair_struct(param->varname, param->ty, reg);
        }
        else
        {
            map_put(&locals, param->varname, (void *)(size_t)reg);
        }
    }
    curfunc = func->fname;
}

void emit_toplevel(Node *v)
{
    if (v->kind == AST_FUNC)
    {
        emit_func_prologue(v);
        emit_expr(v->body);
        emit("r0 <- uint 0");
        emit("ret r0");
        emit_noindent("end\n");
    }
    else if (v->kind == AST_DECL)
    {
        error("internal error");
    }
    else
    {
        error("internal error");
    }
}
