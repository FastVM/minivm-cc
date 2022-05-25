
#include <stdlib.h>
#include <stdio.h>

typedef struct {
    int val;
} val_t;

val_t &deref(val_t *val) {
    return *val;
}

int main() {
    val_t *val1 = calloc(sizeof(val_t), 1);
    val_t *val2 = calloc(sizeof(val_t), 1);
    val2->val = 49;
    *val1 = *val2;
    putn(val1->val);
    putchar('\n');
}
    // 
    // if (node->ty->kind == KIND_REF) {
    //     return ast_uop(AST_DEREF, node->ty->ptr, node);
    // }