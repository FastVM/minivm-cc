
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int x;

const char *str = "y = (1+&*x); y = ";

int main() {
    x = 4984;
    int *px = &x;
    int y = 1 + *px;
    puts(str);
    putn(y);
    putchar('\n');
    return 0;
}