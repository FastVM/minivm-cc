
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int main() {
    int x = 4984;
    int *px = &x;
    int y = 1 + *px;
    printf("%i", y);
    putchar('\n');
    return 0;
}