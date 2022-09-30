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
    return kind == KIND_PTR || kind == KIND_ARRAY || kind == KIND_BOOL || kind == KIND_CHAR || kind == KIND_SHORT || kind == KIND_INT || kind == KIND_LONG || kind == KIND_LLONG || kind == KIND_ENUM;
}

static bool kind_is_float(int kind)
{
    return kind == KIND_FLOAT || kind == KIND_DOUBLE || kind == KIND_LDOUBLE;
}

static int emit_conv_type(Type *from, Type *to, int reg)
{
    return reg;
}

static void emit_label(char *label)
{
    emit_noindent("@%s%s", curfunc, label);
}

static void emit_branch_bool(Node *node, char *zero, char *nonzero)
{
    if (node->kind == '<' || node->kind == '>' || node->kind == OP_GE || node->kind == OP_LE || node->kind == OP_EQ || node->kind == OP_NE)
    {
        int lhs = emit_expr(node->left);
        int rhs = emit_expr(node->right);
        switch (node->kind)
        {
        case OP_EQ:
            emit("beq r%i r%i %s%s %s%s", lhs, rhs, curfunc, zero, curfunc, nonzero);
            break;
        case OP_NE:
            emit("beq r%i r%i %s%s %s%s", lhs, rhs, curfunc, nonzero, curfunc, zero);
            break;
        case '<':
            emit("blt r%i r%i %s%s %s%s", lhs, rhs, curfunc, zero, curfunc, nonzero);
            break;
        case '>':
            emit("blt r%i r%i %s%s %s%s", rhs, lhs, curfunc, zero, curfunc, nonzero);
            break;
        case OP_LE:
            emit("blt r%i r%i %s%s %s%s", rhs, lhs, curfunc, nonzero, curfunc, zero);
            break;
        case OP_GE:
            emit("blt r%i r%i %s%s %s%s", lhs, rhs, curfunc, nonzero, curfunc, zero);
            break;
        }
    }
    else
    {
        int nth = emit_expr(node);
        emit("r0 <- int 0");
        emit("beq r%i r0 %s%s %s%s", nth, curfunc, nonzero, curfunc, zero);
    }
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

static int emit_assign_to(Node *from, Node *to)
{
    Node *to0 = to;
    int offset = 0;
    while (to->kind == AST_STRUCT_REF)
    {
        Type *field = dict_get(to->struc->ty->fields, to->field);
        offset += field->offset;
        to = to->struc;
    }
    int rhs = emit_expr(from);
    if (to->kind == AST_DEREF)
    {
        int lhs = emit_expr(to->operand);
        // lhs += offset;
        for (int i = 0; i < from->ty->size; i++)
        {
            emit("r0 <- int %d", (i + offset) * sizeof(ptrdiff_t));
            emit("r0 <- add r0 r%i", lhs);
            emit("set r1 r0 r%i", rhs + i);
        }
        return rhs;
    }
    else
    {

        int out = (int)(size_t)map_get(&locals, to->varname);
        for (int i = 0; i < from->ty->size; i++)
        {
            emit("r%i <- reg r%i", out + i + offset, rhs + i);
        }
        return out;
    }
}

static int emit_assign(Node *node)
{
    return emit_assign_to(node->right, node->left);
}

static int emit_binop(Node *node)
{
    if (node->left->ty->kind == KIND_PTR || node->left->ty->kind == KIND_ARRAY) {
        if (node->kind == '+') {
            int lhs = emit_expr(node->left);
            int rhs = emit_expr(node->right);
            int rreg = nregs++;
            emit("r0 <- int %i", node->left->ty->ptr->size);
            emit("r0 <- mul r0 r%i", rhs);
            emit("r%i <- add r%i r0", rreg, lhs);
            return rreg;
        }
        if (node->kind == '-') {
            int lhs = emit_expr(node->left);
            int rhs = emit_expr(node->right);
            int rreg = nregs++;
            emit("r0 <- int %i", node->left->ty->size);
            emit("r%i <- mul r%i r%i", rreg, rhs, rhs);
            emit("r%i <- sub r%i r%i", rreg, lhs, rhs);
            return rreg;
        }
    }
    if (node->kind == OP_LOGAND) {
        int ret = nregs++;
        char *end = make_label();
        char *f0 = make_label();
        char *t1 = make_label();
        char *t2 = make_label();
        emit_branch_bool(node->left, f0, t1);
        emit_label(f0);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(t1);
        emit_branch_bool(node->right, f0, t2);
        emit_label(t2);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    if (node->kind == OP_LOGOR) {
        int ret = nregs++;
        char *end = make_label();
        char *f1 = make_label();
        char *f2 = make_label();
        char *t1 = make_label();
        emit_branch_bool(node->left, f1, t1);
        emit_label(f2);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(f1);
        emit_branch_bool(node->right, f2, t1);
        emit_label(t1);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    int lhs = emit_expr(node->left);
    int rhs = emit_expr(node->right);
    char typepre = kind_is_float(node->left->ty->kind) ? 'f' : node->left->ty->usig ? 'u' :  's';
    switch (node->kind)
    {
    case '+':
    {
        int ret = nregs++;
        emit("r%i <- add r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '-':
    {
        int ret = nregs++;
        emit("r%i <- sub r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '*':
    {
        int ret = nregs++;
        emit("r%i <- mul r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '/':
    {
        int ret = nregs++;
        emit("r%i <- div r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '%':
    {
        int ret = nregs++;
        emit("r%i <- mod r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '|':
    {
        int ret = nregs++;
        emit("r%i <- call func.__minivm_bits_or r1 r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '&':
    {
        int ret = nregs++;
        emit("r%i <- call func.__minivm_bits_and r1 r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '^':
    {
        int ret = nregs++;
        emit("r%i <- call func.__minivm_bits_xor r1 r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case OP_SHL:
    case OP_SAL:
    {
        int ret = nregs++;
        emit("r%i <- call func.__minivm_bits_shl r1 r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case OP_SHR:
    case OP_SAR:
    {
        int ret = nregs++;
        emit("r%i <- call func.__minivm_bits_shr r1 r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case OP_EQ:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("beq r%i r%i %s%s %s%s", lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_NE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("beq r%i r%i %s%s %s%s", lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    case '<':
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("blt r%i r%i %s%s %s%s", lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    case '>':
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("blt r%i r%i %s%s %s%s", rhs, lhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_LE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("blt r%i r%i %s%s %s%s", rhs, lhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_GE:
    {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("blt r%i r%i %s%s %s%s", lhs, rhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- int 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- int 1", ret);
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
    int ret = nregs++;
    if (node->ty->kind == KIND_ARRAY) {
        emit("r0 <- int %i", strlen(node->sval)+1);
        emit("r%i <- call __minivm_alloca r1", ret);
        emit("r%i <- dcall r%i r1 r0", ret, ret);
        int tmp = nregs++;
        emit("r%i <- reg r%i", tmp, ret);
        const char *s = node->sval;
        while (1) {
            emit("r0 <- int %i", (int) *s);
            emit("set r1 r%i r0", tmp);
            emit("r0 <- int %i", (int) sizeof(ptrdiff_t));
            emit("r%i <- add r%i r0", tmp, tmp);
            if (*s == '\0') {
                break;
            }
            s += 1;
        }
    }
    else
    {
        emit("r%i <- int %li", ret, node->ival);
    }
    return ret;
}

static int emit_struct_all(Type *ty, char *path)
{
    return (int)(size_t)map_get(&locals, path);
}

static int emit_arg_pack(int src, int count)
{
    if (count > 1)
    {
        int out = nregs++;
        emit("r0 <- int %i", count);
        emit("r%i <- arr r0", out);
        for (int i = 0; i < count; i++)
        {
            emit("r0 <- int %i", i*sizeof(ptrdiff_t));
            emit("r0 <- add r0 r%i", out);
            emit("r0 <- call __minivm_write r1 r0 r%i", src+i);
        }
        return out;
    }
    return src;
}

static void emit_arg_unpack(int out, int count, int src)
{
    if (count > 1)
    {
        for (int i = 0; i < count; i++)
        {
            emit("r0 <- int %i", i*sizeof(ptrdiff_t));
            emit("r0 <- add r0 r%i", src);
            emit("r%i <- call __minivm_read r1 r0", i+out);
        }
    }
    else
    {
        emit("r%i <- reg r%i", out, src);
    }
}

static const char *emit_args(Vector *vals, Vector *params)
{
    const char *args = "";
    for (int i = 0; i < vec_len(vals) && i < vec_len(params); i++)
    {
        Node *v = vec_get(vals, i);
        int regno = emit_expr(v);
        regno = emit_arg_pack(regno, v->ty->size);
        args = format("%s r%i", args, regno);
    }
    return args;
}

static int emit_func_call(Node *node)
{
    const char *args = emit_args(node->args, node->ftype->params);
    if (node->kind == AST_FUNCPTR_CALL)
    {
        error("funcptr call");
    }
    else if (!strcmp(node->fname, "putchar"))
    {
        emit("putchar%s", args);
        return 0;
    }
    else
    {
        int ret = nregs++;
        emit("r%i <- call func.%s r1 %s", ret, node->fname, args);
        return ret;
    }
}

static int emit_conv(Node *node)
{
    int reg1 = emit_expr(node->operand);
    int reg2 = emit_conv_type(node->operand->ty, node->ty, reg1);
    return reg2;
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
    emit("r0 <- int 0");
    emit("beq r%i r0 %s%s %s%s", nth, curfunc, nz, curfunc, ez);
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
        // emit("r0 <- call __minivm_leave r1 r2");
        emit("ret r%i", i);
    }
    else
    {
        // emit("r0 <- call __minivm_leave r1 r2");
        emit("r0 <- int 0");
        emit("ret r0");
    }
    return 0;
}

static void emit_local_store(int where, Node *node)
{
    int r = emit_expr(node);
    for (int i = 0; i < node->ty->size; i++)
    {
        emit("r%i <- reg r%i", where + i, r + i);
    }
}

static int emit_lvar(Node *node)
{
    if (node->lvarinit)
    {
        int n = nregs;
        nregs += node->ty->size;
        for (int i = 0; i < vec_len(node->lvarinit); i++)
        {
            Node *init = vec_get(node->lvarinit, i);
            emit_local_store(n + init->initoff, init->initval);
        }
        return n;
    }
    if (node->ty->kind == KIND_STRUCT && node->ty->is_struct == true)
    {
        return emit_struct_all(node->ty, node->varname);
    }
    else
    {
        int outreg = nregs++;
        int reg = (int)(size_t)map_get(&locals, node->varname);
        return reg;
    }
}

static int emit_struct_ref(Node *node)
{
    Type *type = dict_get(node->struc->ty->fields, node->field);
    return emit_expr(node->struc) + type->offset;
}

static int emit_deref(Node *node)
{
    int outreg = nregs;
    nregs += node->ty->size;
    int tmpreg = nregs++;
    int from = emit_expr(node->operand);
    for (int i = 0; i < node->ty->size; i++)
    {
        emit("r%i <- int %i", tmpreg, i*sizeof(ptrdiff_t));
        emit("r%i <- add r%i r%i", tmpreg, tmpreg, from);
        emit("r%i <- call __minivm_read r1 r%i", outreg + i, tmpreg);
    }
    return outreg;
}

static int emit_label_addr(Node *node) {
    int outreg = nregs++;
    emit("r%i <- addr %s%s", outreg, curfunc, node->newlabel);
    return outreg;
}

static void emit_computed_goto(Node *node) {
    int reg = emit_expr(node->operand);
    emit("djump r%i", reg);
}

static int emit_expr(Node *node)
{
    switch (node->kind)
    {
    case AST_COMPUTED_GOTO:
        emit_computed_goto(node);
        return 0;
    case OP_LABEL_ADDR:
        return emit_label_addr(node);
    case AST_ADDR: {
        int off = 0;
        Node *op = node->operand;
        while(1) {
            if(op->kind == AST_STRUCT_REF) {
                Type *ent = dict_get(op->struc->ty->fields, op->field);
                off += ent->offset;
                op = op->struc;
            } else if (op->kind == AST_CONV || op->kind == OP_CAST) {
                op = op->operand;
            } else {
                break;
            }
        }
        if (op->kind == AST_DEREF) {
            int ret = nregs++;
            int first = emit_expr(op->operand);
            emit("r%i <- int %i", ret, off*sizeof(ptrdiff_t));
            emit("r%i <- add r%i r%i", ret, ret, first);
            return ret;
        }
        error("cannot handle addr: `&` operator is bad expr: %s", node2s(node));
    }
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
        int n = nregs;
        nregs += node->declvar->ty->size + 1;
        map_put(&locals, node->declvar->varname, (void *) (size_t) n);
        if (node->declinit)
        {
            for (int i = 0; i < vec_len(node->declinit); i++)
            {
                Node *init = vec_get(node->declinit, i);
                emit_local_store(n + init->initoff, init->initval);
            }
        }
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
    case '!':
    {
        int reg = emit_expr(node->operand);
        char *zero = make_label();
        char *nonzero = make_label();
        char *out = make_label();
        emit("r0 <- int 0");
        emit("beq r%i r0 %s%s %s%s", reg, curfunc, nonzero, curfunc, zero);
        int ret = nregs++;
        emit_label(zero);
        emit("r%i <- int 1", ret);
        emit("jump %s%s", curfunc, out);
        emit_label(nonzero);
        emit("r%i <- int 0", ret);
        emit_label(out);
        return ret;
    }
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
    case '^':
    case '|':
    case '&':
    case OP_SAL:
    case OP_SAR:
    case OP_LOGAND:
    case OP_LOGOR:
        return emit_binop(node);
    default:
        error("node(%i) = %s", node->kind, node2s(node));
    }
}

static void emit_func_prologue(Node *func)
{
    if (!strcmp(func->fname, "_start"))
    {
        emit_noindent("@__entry");
        emit("r1 <- call __minivm_memory");
        emit("r0 <- call func.%s r1", func->fname);
        emit("exit");
    }
    emit_noindent("func func.%s", func->fname, nregs);
    // for (const char *c = func->fname; *c != '\0'; c++) {
    //     emit("r0 <- int %i",(int) *c);
    //     emit("putchar r0");
    // }
    // emit("r0 <- int 10");
    // emit("putchar r0");
    rettype = func->ty->rettype;
    locals = EMPTY_MAP;
    int maxarg = vec_len(func->params) + 2;
    nregs = 1;
    for (int i = 0; i < vec_len(func->params); i++)
    {
        Node *param = vec_get(func->params, i);
        int reg = maxarg;
        emit_arg_unpack(reg, param->ty->size, i+2);
        maxarg += param->ty->size;
        map_put(&locals, param->varname, (void *)(size_t)reg);
    }
    nregs = maxarg + 1;
    curfunc = func->fname;
}

void emit_toplevel(Node *v)
{
    if (v->kind == AST_FUNC)
    {
        emit_func_prologue(v);
        // emit("r2 <- call __minivm_enter r1");
        emit_expr(v->body);
        // emit("r0 <- call __minivm_leave r1 r2");
        emit("ret r0");
        emit_noindent("end\n");
    }
    else if (v->kind == AST_DECL)
    {
        error("unimplemented: global variable : %s", node2s(v));
    }
    else
    {
        error("internal error");
    }
}
