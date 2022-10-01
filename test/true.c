
#include <stdio.h>
#include <stdlib.h>

int main() {
    int *x = malloc(sizeof(int) * 256);
    for (int i = 0; i < 256; i++) {
        x[i] = i;
    }
    putchar(&x['t']);
    putchar(&x['r']);
    putchar(&x['u']);
    putchar(&x['e']);
    putchar(x['\n']);
    free(x);
}