#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define BLOCKSIZE 0xF

int main() {
    char mem[] = "hello world";
    int n = 0;
    for (;;) {
        if (malloc(BLOCKSIZE) == NULL) {
            break;
        }
        n+=1;
    }
    putn(n);
    putchar('\n');
}
