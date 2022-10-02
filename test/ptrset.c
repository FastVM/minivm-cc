
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

int *x;

int main() {
    *x = 1000;
    putn(*x);
    putchar('\n');
    return 0;
}