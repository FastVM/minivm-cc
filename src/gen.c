// Copyright 2012 Rui Ueyama. Released under the MIT license.

#include "8cc.h"

#define BUFFER_EXTRA 0

bool dumpsource = true;

static int nregs;
static Buffer *outbuf = &(Buffer){0, 0, 0};
static const char *curfunc;
static Map globals = EMPTY_MAP;
static Map locals;
static Vector globalzero = EMPTY_VECTOR;
static Vector globalinit = EMPTY_VECTOR;
static Vector globalinitval = EMPTY_VECTOR;
static int initmem = 16;
int stackn = 0;

static int emit_expr(Node *node);
static int emit_addr(Node *op);

#define newline() buf_printf(outbuf, "\n")
#define emit_noindent(...) (buf_printf(outbuf, __VA_ARGS__), newline())
#define emit(...) emit_noindent("    " __VA_ARGS__)

Buffer *emit_end(void) {
    Buffer *ret = outbuf;
    outbuf = make_buffer();
    return ret;
}

static bool kind_is_int(int kind) {
    return kind == KIND_PTR || kind == KIND_ARRAY || kind == KIND_BOOL || kind == KIND_CHAR || kind == KIND_SHORT || kind == KIND_INT || kind == KIND_LONG || kind == KIND_LLONG || kind == KIND_ENUM;
}

static bool kind_is_float(int kind) {
    return kind == KIND_FLOAT || kind == KIND_DOUBLE || kind == KIND_LDOUBLE;
}

static int emit_conv_type(Type *from, Type *to, int reg) {
    return reg;
}

static void emit_label(char *label) {
    emit_noindent("@%s%s", curfunc, label);
}

static int emit_add_ri(int reg, int num) {
    if (num == 0) {
        return reg;
    } else {
        int out = nregs++;
        emit("r%i <- int %i", out, num);
        emit("r%i <- add r%i r%i", out, out, reg);
        return out;
    }
}

static int emit_mul_ri(int reg, int num) {
    if (num == 1) {
        return reg;
    } else if (num == 2) {
        int out = nregs++;
        emit("r%i <- add r%i r%i", out, reg, reg);
        return out;
    } else {
        int out = nregs++;
        emit("r%i <- int %i", out, num);
        emit("r%i <- mul r%i r%i", out, out, reg);
        return out;
    }
}

static void emit_pre_call(void) {
    int reg = nregs++;
    emit("r0 <- int %i", stackn + BUFFER_EXTRA);
    emit("r%i <- add r2 r0", reg);
    emit("r0 <- int 1");
    emit("set r1 r0 r%i", reg);
}

static void emit_post_call(void) {
    emit("set r1 r0 r2");
}

static void emit_branch_bool(Node *node, char *zero, char *nonzero) {
    if (node->kind == '<' || node->kind == '>' || node->kind == OP_GE || node->kind == OP_LE || node->kind == OP_EQ || node->kind == OP_NE) {
        int lhs = emit_expr(node->left);
        int rhs = emit_expr(node->right);
        switch (node->kind) {
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
    } else {
        int nth = emit_expr(node);
        emit("r0 <- int 0");
        emit("beq r%i r0 %s%s %s%s", nth, curfunc, nonzero, curfunc, zero);
    }
}

static int emit_assign_to(Node *from, Node *to) {
    int offset = 0;
    while (to->kind == AST_STRUCT_REF) {
        Type *field = dict_get(to->struc->ty->fields, to->field);
        offset += field->offset;
        to = to->struc;
    }
    while (to->kind == AST_CONV) {
        to = to->operand;
    }
    int rhs = emit_expr(from);
    if (to->kind == AST_DEREF) {
        int lhs = emit_expr(to->operand);
        // lhs += offset;
        for (int i = 0; i < from->ty->size; i++) {
            int where = emit_add_ri(lhs, i + offset);
            emit("set r1 r%i r%i", where, rhs + i);
        }
    } else if (to->kind == AST_LVAR) {
        int out = (int)(size_t)map_get(&locals, to->varname);
        for (int i = 0; i < from->ty->size; i++) {
            int where = emit_add_ri(2, i + out);
            emit("set r1 r%i r%i", where, rhs + i);
        }
    } else if (to->kind == AST_GVAR) {
        int out = (int)(size_t)map_get(&globals, to->varname);
        // printf("%s: [%i]\n", to->varname, out);
        for (int i = 0; i < from->ty->size; i++) {
            emit("r0 <- int %i", out + i + offset);
            emit("set r1 r0 r%i", rhs + i);
        }
    } else {
        error("assign to bad thing: `%s`", node2s(to));
    }
    return rhs;
}

static int emit_assign(Node *node) {
    return emit_assign_to(node->right, node->left);
}

static int emit_binop(Node *node) {
    if (node->left->ty->kind == KIND_PTR || node->left->ty->kind == KIND_ARRAY) {
        if (node->kind == '+') {
            int lhs = emit_expr(node->left);
            int rhs = emit_expr(node->right);
            int rreg = nregs++;
            int reg = emit_mul_ri(rhs, node->left->ty->ptr->size);
            emit("r%i <- add r%i r%i", rreg, lhs, reg);
            return rreg;
        }
        if (node->kind == '-') {
            int lhs = emit_expr(node->left);
            int rhs = emit_expr(node->right);
            int rreg = nregs++;
            if (node->right->ty->kind == KIND_PTR) {
                if (node->left->ty->ptr->size == 1) {
                    emit("r%i <- sub r%i r%i", rreg, lhs, rhs);
                } else {
                    emit("r%i <- sub r%i r%i", rreg, lhs, rhs);
                    if (node->left->ty->ptr->size != 1) {
                        emit("r0 <- int %i", node->left->ty->ptr->size);
                        emit("r%i <- div r%i r0", rreg, rreg);
                    }
                }
            } else {
                if (node->left->ty->ptr->size == 1) {
                    emit("r%i <- sub r%i r%i", rreg, lhs, rhs);
                } else {
                    int reg = emit_mul_ri(rhs, node->left->ty->ptr->size);
                    emit("r%i <- sub r%i r%i", rreg, lhs, reg);
                }
            }
            return rreg;
        }
    }
    if (node->kind == OP_IPADD) {
        if (node->left->ty->size != 1) {
            error("postfix `++` or `--` on type too big");
        }
        int addr = emit_addr(node->left);
        int add = emit_expr(node->right);
        int ret = nregs++;
        emit("r%i <- get r1 r%i", ret, addr);
        // if (node->left->ty->kind == KIND_PTR && node->left->ty->ptr->size != 1) {
        if (node->left->ty->kind == KIND_PTR) {
            emit("r0 <- int %i", node->left->ty->ptr->size);
            emit("r%i <- mul r%i r0", add, add);
        }
        emit("r0 <- add r%i r%i", ret, add);
        emit("set r1 r%i r0", addr);
        return ret;
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
    switch (node->kind) {
        case ',': {
            return rhs;
        }
        case '+': {
            int ret = nregs++;
            emit("r%i <- add r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '-': {
            int ret = nregs++;
            emit("r%i <- sub r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '*': {
            int ret = nregs++;
            emit("r%i <- mul r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '/': {
            int ret = nregs++;
            emit("r%i <- div r%i r%i", ret, lhs, rhs);
            if (kind_is_int(node->ty->kind)) {
                emit("r0 <- int 1");
                emit("r0 <- mod r%i r0", ret);
                emit("r%i <- sub r%i r0", ret, ret);
            }
            return ret;
        }
        case '%': {
            int ret = nregs++;
            emit("r%i <- mod r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '|': {
            int ret = nregs++;
            emit("r%i <- bor r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '&': {
            int ret = nregs++;
            emit("r%i <- band r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case '^': {
            int ret = nregs++;
            emit("r%i <- bxor r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case OP_SHL:
        case OP_SAL: {
            int ret = nregs++;
            emit("r%i <- bshl r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case OP_SHR:
        case OP_SAR: {
            int ret = nregs++;
            emit("r%i <- bshr r%i r%i", ret, lhs, rhs);
            return ret;
        }
        case OP_EQ: {
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
        case OP_NE: {
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
        case '<': {
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
        case '>': {
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
        case OP_LE: {
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
        case OP_GE: {
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
            error("invalid operator '%i'", node->kind);
    }
    return 0;
}

static void emit_jmp(char *label) {
    emit("jump %s%s", curfunc, label);
}

static int emit_literal(Node *node) {
    int ret = nregs++;
    if (node->ty->kind == KIND_ARRAY) {
        int len = strlen(node->sval)+1;
        int where = initmem;
        initmem += len;
        int head = where;
        const char *s = node->sval;
        while (1) {
            int *ptr = malloc(sizeof(int) * 2);
            ptr[0] = head;
            ptr[1] = (int) *s;
            vec_push(&globalinit, ptr);
            if (*s == '\0') {
                break;
            }
            s += 1;
            head += 1;
        }
        emit("r%i <- int %i", ret, where);
    } else {
        emit("r%i <- int %li", ret, node->ival);
    }
    return ret;
}

static void emit_args(Vector *vals) {
    int buf[16] = {0};
    for (int i = 0; i < vec_len(vals); i++) {
        buf[i] = emit_expr(vec_get(vals, i));
    }
    int off = 0;
    for (int i = 0; i < vec_len(vals); i++) {
        Node *v = vec_get(vals, i);
        int regno = buf[i];
        for (int j = 0; j < v->ty->size; j++) {
            int where = emit_add_ri(2, off + stackn + BUFFER_EXTRA);
            emit("set r1 r%i r%i", where, regno+j);
            off += 1;
        }
    }
}

static int emit_funcptr_call(Node *node) {
    int func = emit_expr(node->fptr);
    int ref = nregs++;
    emit_args(node->args);
    emit_pre_call();
    emit("r%i <- dcall r%i r1", ref, func);
    emit_post_call();
    int ret = nregs++;
    if (node->fptr->ty->ptr->rettype->kind != KVOID) {
        for (int i = 0; i < node->fptr->ty->ptr->rettype->size; i++) {
            int where = emit_add_ri(ref, i);
            emit("r%i <- get r1 r%i", ret + i, where);
        }
    }
    return ret;
}

static int emit_func_call(Node *node) {
    if (!strcmp(node->fname, "getchar")) {
        int reg = nregs++;
        emit("r%i <- getchar", reg);
        return reg;
    } else if (!strcmp(node->fname, "putchar")) {
        Node *v = vec_get(node->args, 0);
        int regno = emit_expr(v);
        emit("putchar r%i", regno);
        return 0;
    } else {
        int ref = nregs;
        nregs += node->ftype->rettype->size;
        emit_args(node->args);
        emit_pre_call();
        emit("r%i <- call func.%s r1", ref, node->fname);
        emit_post_call();
        int ret = nregs++;
        if (node->ftype->rettype->kind != KVOID) {
            for (int i = 0; i < node->ftype->rettype->size; i++) {
                int where = emit_add_ri(ref, i);
                emit("r%i <- get r1 r%i", ret + i, where);
            }
        }
        return ret;
    }
}

static void emit_if(Node *node) {
    char *zero = make_label();
    char *nonzero = make_label();
    emit_branch_bool(node->cond, zero, nonzero);
    if (node->then) {
        emit_label(nonzero);
        emit_expr(node->then);
    } else {
        emit_label(nonzero);
    }
    if (node->els) {
        char *end = make_label();
        emit_jmp(end);
        emit_label(zero);
        emit_expr(node->els);
        emit_label(end);
    } else {
        emit_label(zero);
    }
}

static int emit_ternary(Node *node) {
    char *ez = make_label();
    char *nz = make_label();
    int nth = emit_expr(node->cond);
    int out = nregs++;
    emit("r0 <- int 0");
    emit("beq r%i r0 %s%s %s%s", nth, curfunc, nz, curfunc, ez);
    if (node->then) {
        emit_label(nz);
        int r = emit_expr(node->then);
        emit("r%i <- reg r%i", out, r);
    } else {
        emit_label(nz);
    }
    if (node->els) {
        char *end = make_label();
        emit_jmp(end);
        emit_label(ez);
        int r = emit_expr(node->els);
        emit("r%i <- reg r%i", out, r);
        emit_label(end);
    } else {
        emit_label(ez);
    }
    return out;
}

static int emit_return(Node *node) {
    if (node->retval) {
        int dest = nregs++;
        emit("r0 <- int %i", stackn + BUFFER_EXTRA);
        emit("r%i <- add r2 r0", dest);
        int regno = emit_expr(node->retval);
        for (int i = 0; i < node->retval->ty->size; i++) {
            emit("r0 <- int %i", i);
            emit("r0 <- add r0 r%i", dest);
            emit("set r1 r0 r%i", regno + i);
        }
        emit("ret r%i", dest);
    } else {
        emit("r0 <- nil");
        emit("ret r0");
    }
    return 0;
}

static void emit_local_store(int where, Node *node) {
    int r = emit_expr(node);
    for (int i = 0; i < node->ty->size; i++) {
        emit("r0 <- int %i", where + i);
        emit("r0 <- add r0 r2");
        emit("set r1 r0 r%i", r + i);
    }
}

static int emit_lvar(Node *node) {
    int reg = (int)(size_t)map_get(&locals, node->varname);
    if (node->lvarinit) {
        for (int i = 0; i < vec_len(node->lvarinit); i++) {
            Node *init = vec_get(node->lvarinit, i);
            emit_local_store(reg + init->initoff, init->initval);
        }
    }
    int outreg = nregs;
    nregs += node->ty->size;
    for (int i = 0; i < node->ty->size; i++) {
        int where = emit_add_ri(2, reg+i);
        emit("r%i <- get r1 r%i", outreg + i, where);
    }
    return outreg;
}

static int emit_gvar(Node *node) {
    int outreg = nregs;
    nregs += node->ty->size;
    int where = (int)(size_t)map_get(&globals, node->varname);
    for (int i = 0; i < node->ty->size; i++) {
        emit("r0 <- int %i", where+i);
        emit("r%i <- get r1 r0", outreg + i);
    }
    return outreg;
}

static int emit_addr(Node *op) {
    int off = 0;
    while (1) {
        if (op->kind == AST_STRUCT_REF) {
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
        emit("r%i <- int %i", ret, off);
        emit("r%i <- add r%i r%i", ret, ret, first);
        return ret;
    }
    if (op->kind == AST_LVAR) {
        int ret = nregs++;
        int local = (int)(size_t)map_get(&locals, op->varname);
        emit("r%i <- int %i", ret, local+off);
        emit("r%i <- add r%i r2", ret, ret);
        return ret;
    }
    if (op->kind == AST_GVAR) {
        int ret = nregs++;
        int loc = (int)(size_t)map_get(&globals, op->varname);
        emit("r%i <- int %i", ret, loc+off);
        return ret;
    }
    if (op->kind == AST_FUNCDESG) {
        int ret = nregs++;
        emit("r%i <- addr func.%s", ret, op->fname);
        return ret;
    }
    error("cannot handle addr: `&` operator is bad expr: %s", node2s(op));
    return 0;
}

static int emit_conv(Node *node) {
    if (node->operand->ty->kind == KIND_ARRAY && node->ty->kind == KIND_PTR && (node->operand->kind == AST_LVAR || node->operand->kind == AST_GVAR)) {
        return emit_addr(node->operand);
    } else {
        int reg1 = emit_expr(node->operand);
        int reg2 = emit_conv_type(node->operand->ty, node->ty, reg1);
        return reg2;
    }
}

static int emit_struct_ref(Node *node) {
    Type *type = dict_get(node->struc->ty->fields, node->field);
    return emit_expr(node->struc) + type->offset;
}

static int emit_deref(Node *node) {
    int outreg = nregs;
    nregs += node->ty->size;
    int from = emit_expr(node->operand);
    for (int i = 0; i < node->ty->size; i++) {
        emit("r0 <- int %i", i);
        emit("r0 <- add r0 r%i", from);
        emit("r%i <- get r1 r0", outreg + i);
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

static int emit_expr(Node *node) {
    switch (node->kind) {
        case AST_COMPUTED_GOTO:
            emit_computed_goto(node);
            return 0;
        case OP_LABEL_ADDR:
            return emit_label_addr(node);
        case AST_ADDR:
            return emit_addr(node->operand);
        case AST_DEREF:
            return emit_deref(node);
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
        case AST_GVAR:
            return emit_gvar(node);
        case AST_LVAR:
            return emit_lvar(node);
        case AST_FUNCPTR_CALL:
            return emit_funcptr_call(node);
        case AST_FUNCALL:
            return emit_func_call(node);
        case AST_CONV:
            return emit_conv(node);
        case AST_IF:
            emit_if(node);
            return 0;
        case AST_DECL: {
            int n = stackn;
            map_put(&locals, node->declvar->varname, (void *)(size_t)n);
            stackn += node->declvar->ty->size;
            if (node->declinit) {
                for (int i = 0; i < vec_len(node->declinit); i++) {
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
        case AST_COMPOUND_STMT: {
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
        case '!': {
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
        case '~': {
            int reg = emit_expr(node->operand);
            int ret = nregs++;
            emit("r0 <- int 1");
            emit("r%i <- add r0 r%i", ret, reg);
            emit("r0 <- int 0");
            emit("r%i <- sub r0 r%i", ret, ret);
            return ret;
        }
        case ',':
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
        case OP_IPADD:
            return emit_binop(node);
        default:
            error("node(%i) = %s", node->kind, node2s(node));
    }
}


static void emit_func_prologue(Node *func) {
    if (!strcmp(func->fname, "_start")) {
        emit_noindent("@__entry");
        emit("jump __entry_memory");
        emit_noindent("@__entry_init");
        nregs = 5;
        for (int i = 0; i < vec_len(&globalzero); i++) {
            int *pair = vec_get(&globalzero, i);
            emit("r0 <- int %i", pair[0]);
            emit("r2 <- int 0");
            emit("set r1 r0 r2");
        }
        for (int i = 0; i < vec_len(&globalinitval); i++) {
            union {int i; Node *n;} *pair = vec_get(&globalinitval, i);
            Node *initval = pair[1].n;
            int val = emit_expr(initval);
            for (int o = 0; o < initval->ty->size; o++) {
                emit("r0 <- int %i", pair[0].i + o);
                emit("set r1 r0 r%i", val + o);
            }
        }
        for (int i = 0; i < vec_len(&globalinit); i++) {
            int *pair = vec_get(&globalinit, i);
            emit("r0 <- int %i", pair[0]);
            emit("r2 <- int %i", pair[1]);
            emit("set r1 r0 r2");
        }
        emit("jump __entry_main");
        emit_noindent("@__entry_memory");
        emit("r1 <- int 12500000");
        emit("r1 <- arr r1");
        emit("r2 <- int %i", initmem + 16);
        emit("r0 <- int 1");
        emit("set r1 r0 r2");
        emit("jump __entry_init");
        emit_noindent("@__entry_main");
        // emit_pre_call();
        emit("r0 <- call func.%s r1", func->fname);
        // emit_post_call();
        emit("exit");
    }
    stackn = 0;
    emit_noindent("func func.%s", func->fname, nregs);
    locals = EMPTY_MAP;
    nregs = 3;
    emit("r0 <- int 1");
    emit("r2 <- get r1 r0");
    for (int i = 0; i < vec_len(func->params); i++) {
        Node *param = vec_get(func->params, i);
        map_put(&locals, param->varname, (void *)(size_t)stackn);
        stackn += param->ty->size;
    }
    if (func->ty->hasva) {
        stackn += 8;
    }
    curfunc = func->fname;
}

void emit_toplevel(Node *v) {
    if (v->kind == AST_FUNC) {
        stackn = 0;
        emit_func_prologue(v);
        emit_expr(v->body);
        emit("r0 <- nil");
        emit("ret r0");
        emit_noindent("end\n");
    } else if (v->kind == AST_DECL) {
        int base = initmem;
        map_put(&globals, v->declvar->varname, (void *)(size_t)base);
        initmem += v->declvar->ty->size;
        if (v->declinit) {\
            for (int i = 0; i < v->declvar->ty->size; i++) {
                int *pair = malloc(sizeof(int) * 1);
                pair[0] = base + i;
                vec_push(&globalzero, pair);
            }
            for (int i = 0; i < vec_len(v->declinit); i++) {
                Node *init = vec_get(v->declinit, i);
                union {int i; Node *n;} *pair = malloc(sizeof(union {int i; Node *n;}) * 2);
                pair[0].i = init->initoff + base;
                pair[1].n = init->initval;
                vec_push(&globalinitval, pair);
            }
            // error("unimplemented: global variable : %s", node2s(v));
            // warn("global `%s`: not nil", v->declvar->varname);
        }
    } else {
        error("internal error");
    }
}
