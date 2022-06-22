
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    int *x = malloc(sizeof(int) * 0x230);
    putn((size_t) x);
    putchar('\n');
    return 0;
}