
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int x;

const char *str = "y = (1+&*x); y = ";

int main() {
    x = 2492;
    int *px = &x;
    int y = 1 + *px;
    x += y;
    y -= x / 2;
#if defined(__MINIVM__)
    putn(x - y*2 + 1);
#else
    printf("%i", x - y*2 + 1);
#endif
    putchar('\n');
    return 0;
}