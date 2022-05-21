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
    fflush(outputfp);
}

static void emit_ret()
{
    emit("r0 <- uint 0");
    emit("ret r0");
}

static void emit_label(char *label)
{
    emit_noindent("@%s%s", curfunc, label);
}

static int emit_binop(Node *node)
{
    if (node->kind == '=') {
        int rhs = emit_expr(node->right);
        int out = (int) (size_t) map_get(&locals, node->left->varname);
        emit("r%i <- reg r%i", out, rhs);
        return out;
    }
    int lhs = emit_expr(node->left);
    int rhs = emit_expr(node->right);
    char tpre = node->left->ty->kind == KIND_INT ? 'u' : 'f';
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
        emit("r%i <- %cint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret, tpre);
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
        emit("r%i <- %cint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- %cint 1", ret, tpre);
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
        emit("r%i <- uint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, tpre);
        emit_label(end);
        return ret;
    }
    case '>': {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", tpre, rhs, lhs, curfunc, iff, curfunc, ift);
        emit_label(iff);
        emit("r%i <- uint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, tpre);
        emit_label(end);
        return ret;
    }
    case OP_LE: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", tpre, rhs, lhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, tpre);
        emit_label(end);
        return ret;
    }
    case OP_GE: {
        int ret = nregs++;
        char *ift = make_label();
        char *iff = make_label();
        char *end = make_label();
        emit("%cblt r%i r%i %s%s %s%s", tpre, lhs, rhs, curfunc, ift, curfunc, iff);
        emit_label(iff);
        emit("r%i <- uint 0", ret, tpre);
        emit("jump %s%s", curfunc, end);
        emit_label(ift);
        emit("r%i <- uint 1", ret, tpre);
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
        emit("r%i <- call func.%s%s", ret, node->fname, args);
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

static void emit_if(Node *node)
{
    char *ez = make_label();
    char *nz = make_label();
    int nth = emit_expr(node->cond);
    emit("ubb r%i %s%s %s%s", nth, curfunc, ez, curfunc, nz);
    if (node->then)
    {
        emit_label(nz);
        int r = emit_expr(node->then);
    } else {
        emit_label(nz);
    }
    if (node->els)
    {
        char *end = make_label();
        emit_jmp(end);
        emit_label(ez);
        int r = emit_expr(node->els);
        emit_label(end);
    }
    else
    {
        emit_label(ez);
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
    } else {
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

static void emit_decl(Vector *inits, Node *var) {
    for (int i = 0; i < vec_len(inits); i++) {
        Node *node = vec_get(inits, i);
        int val = emit_expr(node->initval);
        map_put(&locals, var->varname, (void*)(size_t)val);
    }
}

static int emit_expr(Node *node)
{
    switch (node->kind)
    {
    case AST_GOTO:
        emit("jump %s%s", curfunc, node->newlabel);
        return 0;
    case AST_LABEL:
        if (node->newlabel) {
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
        if (node->declinit != NULL)
        {
            emit_decl(node->declinit, node->declvar);
        }
        return 0;
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
        emit_noindent("@main");
        emit("r0 <- call func.main");
        emit("exit");
    }
    emit_noindent("@body.%s", func->fname);
    rettype = func->ty->rettype;
    locals = EMPTY_MAP;
    for (int i = 0; i < vec_len(func->params); i++) {
        Node *param = vec_get(func->params, i);
        map_put(&locals, param->varname, (void*)(size_t)(i+1));
    }
    curfunc = func->fname;
    nregs = 1+vec_len(func->params);
}

static void emit_func_end(Node *func) {
    emit_noindent("func func.%s %i", func->fname, nregs);
    emit("jump body.%s\n", func->fname);
}

void emit_toplevel(Node *v)
{
    if (v->kind == AST_FUNC)
    {
        emit_func_prologue(v);
        emit_expr(v->body);
        emit_ret();
        emit_func_end(v);
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
