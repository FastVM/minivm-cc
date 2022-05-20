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
static FILE *outputfp;
static const char *curfunc;
static Map locals;
static Type *rettype;

static int emit_expr(Node *node);

#define emit(...) emitf(__LINE__, "\t" __VA_ARGS__)
#define emit_noindent(...) emitf(__LINE__, __VA_ARGS__)

void set_output_file(FILE *fp)
{
    outputfp = fp;
}

void close_output_file()
{
    fclose(outputfp);
}

static void emitf(int line, char *fmt, ...)
{
    // Replace "#" with "%%" so that vfprintf prints out "#" as "%".
    char buf[256];
    int i = 0;
    for (char *p = fmt; *p; p++)
    {
        assert(i < sizeof(buf) - 3);
        if (*p == '#')
        {
            buf[i++] = '%';
            buf[i++] = '%';
        }
        else
        {
            buf[i++] = *p;
        }
    }
    buf[i] = '\0';

    va_list args;
    va_start(args, fmt);
    vfprintf(outputfp, buf, args);
    va_end(args);
    fprintf(outputfp, "\n");
}

static int emit_ret()
{
    emit("r0 <- uint 0");
    emit("ret r0");
    return 0;
}

static void emit_label(char *label)
{
    fprintf(outputfp, "@%s%s\n", curfunc, label);
}

static int emit_binop(Node *node)
{
    int lhs = emit_expr(node->left);
    // if (node->left->ty->kind == KIND_INT &&  node->ty->kind == KIND_FLOAT) {
    //     int tmp = nregs++;
    //     emit("r%i <- utof r%i", tmp, lhs);
    //     lhs = tmp;
    // }
    // if (node->left->ty->kind == KIND_FLOAT &&  node->ty->kind == KIND_INT) {
    //     int tmp = nregs++;
    //     emit("r%i <- ftou r%i", tmp, lhs);
    //     lhs = tmp;
    // }
    int rhs = emit_expr(node->right);
    // if (node->right->ty->kind == KIND_INT &&  node->ty->kind == KIND_FLOAT) {
    //     int tmp = nregs++;
    //     emit("r%i <- utof r%i", tmp, rhs);
    //     lhs = tmp;
    // }
    // if (node->right->ty->kind == KIND_FLOAT &&  node->ty->kind == KIND_INT) {
    //     int tmp = nregs++;
    //     emit("r%i <- ftou r%i", tmp, rhs);
    //     lhs = tmp;
    // }
    char tpre = node->ty->kind == KIND_INT ? 'u' : 'f';
    switch (node->kind)
    {
    case '+': {
        int ret = nregs++;
        emit("r%i <- %cadd r%i r%i", ret, tpre, lhs, rhs);
        return ret;
    }
    case '-': {
        int ret = nregs++;
        emit("r%i <- %csub r%i r%i", ret, tpre, lhs, rhs);
        return ret;
    }
    case '*': {
        int ret = nregs++;
        emit("r%i <- %cmul r%i r%i", ret, tpre, lhs, rhs);
        return ret;
    }
    case '/': {
        int ret = nregs++;
        emit("r%i <- %cdiv r%i r%i", ret, tpre, lhs, rhs);
        return ret;
    }
    case '%': {
        int ret = nregs++;
        emit("r%i <- %cmod r%i r%i", ret, tpre, lhs, rhs);
        return ret;
    }
    case OP_EQ: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cbeq r%i r%i %s%s %s%s", tpre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- %cint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_NE: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cbeq r%i r%i %s%s %s%s", tpre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- %cint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret);
        emit_label(end);
        return ret;
    }
    case '<': {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", tpre, lhs, rhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- uint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret);
        emit_label(end);
        return ret;
    }
    case '>': {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", rhs, lhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- uint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_LE: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("ublt r%i r%i %s%s %s%s", rhs, lhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret);
        emit_label(end);
        return ret;
    }
    case OP_GE: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("ublt r%i r%i %s%s %s%s", tpre, lhs, rhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret);
        emit_label(end);
        return ret;
    }
    default:
        error("invalid operator '%d'", node->kind);
    }
    return 0;
}

static int emit_jmp(char *label)
{
    emit("jump %s%s", curfunc, label);
    return 0;
}

static int emit_literal(Node *node)
{
    if (node->ty->kind == KIND_FLOAT) {
        int ret = nregs++;
        emit("r%i <- fint %lu", ret, (unsigned long)node->fval);
        return ret;
    } else {
        int ret = nregs++;
        emit("r%i <- uint %lu", ret, (unsigned long)node->ival);
        return ret;
    }
}

static int emit_lvar(Node *node)
{
    return (int)(size_t)map_get(&locals, node->varname);
}

static const char *emit_args(Vector *vals, Vector *params)
{
    const char *args = "";
    for (int i = 0; i < vec_len(vals) && i < vec_len(params); i++)
    {
        Node *v = vec_get(vals, i);
        int regno = emit_expr(v);
        Node *p = vec_get(params, i);
        // printf("%i %i\n", v->ty->kind, p->kind);
        // if (v->ty->kind == KIND_INT && p->kind == KIND_FLOAT) {
        //     int next = nregs++;
        //     emit("r%i <- utof r%i", next, regno);
        //     regno = next;
        // }
        // if (v->ty->kind == KIND_FLOAT && p->kind == KIND_INT) {
        //     int next = nregs++;
        //     emit("r%i <- ftou r%i", next, regno);
        //     regno = next;
        // }
        args = format("%s r%i", args, regno);
    }
    return args;
}

static int emit_func_call(Node *node)
{
    const char *args = emit_args(node->args, node->ftype->params);
    if (node->kind == AST_FUNCPTR_CALL) {
        return 4984;
    } else if (!strcmp(node->fname, "putchar")) {
        emit("putchar%s", args);
        return 0;
    } else {
        int ret = nregs++;
        emit("r%i <- call .%s%s", ret, node->fname, args);
        return ret;
    }
}

static int emit_conv(Node *node)
{
    int reg = emit_expr(node->operand);
    if (node->operand->ty->kind == KIND_INT && node->ty->kind == KIND_FLOAT) {
        int next = nregs++;
        emit("r%i <- utof r%i", next, reg);
        reg = next;
    }
    if (node->operand->ty->kind == KIND_FLOAT && node->ty->kind == KIND_INT) {
        int next = nregs++;
        emit("r%i <- ftou r%i", next, reg);
        reg = next;
    }
    return reg;
}

static int emit_ternary(Node *node)
{
    int out = nregs++;
    char *ez = make_label();
    char *nz = make_label();
    int nth = emit_expr(node->cond);
    emit("ubb r%i %s%s %s%s", nth, curfunc, ez, curfunc, nz);
    if (node->then)
    {
        emit_label(nz);
        int r = emit_expr(node->then);
        emit("r%i <- reg r%i", out, r);
    }
    if (node->els)
    {
        char *end = make_label();
        emit_jmp(end);
        emit_label(ez);
        int r = emit_expr(node->els);
        emit_label(end);
        emit("r%i <- reg r%i", out, r);
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
        // if (node->retval->ty->kind == KIND_INT && rettype->kind == KIND_FLOAT) {
        //     emit("r%i <- utof r%i", i, i);
        // }
        // if (node->retval->ty->kind == KIND_FLOAT && rettype->kind == KIND_INT) {
        //     emit("r%i <- ftou r%i", i, i);
        // }
        emit("ret r%i", i);
    } else {
        emit("r0 <- uint 0");
        emit("ret r0");
    }
    return 0;
}

static int emit_expr(Node *node)
{
    switch (node->kind)
    {
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
    case AST_TERNARY:
        return emit_ternary(node);
    case AST_RETURN:
        emit_return(node);
        return 0;
    case AST_COMPOUND_STMT:
        for (int i = 0; i < vec_len(node->stmts); i++)
            emit_expr(vec_get(node->stmts, i));
        return 0;
    case OP_CAST:
        error("cast");
        return 0;
    default:
        return emit_binop(node);
    }
}

static void emit_func_prologue(Node *func)
{
    if (!strcmp(func->fname, "main")) {
        fprintf(outputfp, "@main\n");
        emit("r0 <- call .main");
        emit("exit");
    }
    rettype = func->ty->rettype;
    emit_noindent("func .%s 256", func->fname);
    locals = EMPTY_MAP;
    for (int i = 0; i < vec_len(func->params); i++) {
        Node *param = vec_get(func->params, i);
        map_put(&locals, param->varname, (void*)(size_t)(i+1));
    }
    curfunc = func->fname;
    nregs = 1;
}

void emit_toplevel(Node *v)
{
    if (v->kind == AST_FUNC)
    {
        emit_func_prologue(v);
        emit_expr(v->body);
        emit_ret();
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
