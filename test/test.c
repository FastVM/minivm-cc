
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    int x = 4984;
    int *px = &x;
    int y = 1 + *px;
    putn(y);
    putchar('\n');
    return 0;
}