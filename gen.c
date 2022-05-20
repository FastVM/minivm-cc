// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include "8cc.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool dumpsource = true;

static int nregs = 0;
static FILE *outputfp;
static const char *curfunc;

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
    int rhs = emit_expr(node->right);
    switch (node->kind)
    {
    case '+': {
        int ret = nregs++;
        emit("r%i <- uadd r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '-': {
        int ret = nregs++;
        emit("r%i <- usub r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '*': {
        int ret = nregs++;
        emit("r%i <- umul r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '/': {
        int ret = nregs++;
        emit("r%i <- udiv r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '%': {
        int ret = nregs++;
        emit("r%i <- umod r%i r%i", ret, lhs, rhs);
        return ret;
    }
    case '<': {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("ublt r%i r%i %s%s %s%s", lhs, rhs, curfunc, iff, curfunc, ift);
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
    int ret = nregs++;
    emit("r%i <- uint %lu", ret, (unsigned long)node->ival);
    return ret;
}

static int emit_lvar(Node *node)
{
    // emit("lvar");
    return 1;
}

static const char *emit_args(Vector *vals)
{
    const char *args = "";
    for (int i = 0; i < vec_len(vals); i++)
    {
        Node *v = vec_get(vals, i);
        int regno = emit_expr(v);
        args = format("%s r%i", args, regno);
    }
    return args;
}

static int emit_func_call(Node *node)
{
    const char *args = emit_args(node->args);
    if (node->kind == AST_FUNCPTR_CALL) {
        return 4984;
    } else if (!strcmp(node->fname, "putchar")) {
        emit("putchar%s", args);
    } else {
        int ret = nregs++;
        emit("r%i <- call .%s%s", ret, node->fname, args);
        return ret;
    }
}

static int emit_conv(Node *node)
{
    return emit_expr(node->operand);
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
    case AST_FUNCALL:
    case AST_FUNCPTR_CALL:
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
    curfunc = func->fname;
    nregs = 1;
    emit_noindent("func .%s 256", func->fname);
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
