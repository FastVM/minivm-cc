#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define BLOCKSIZE 0x100

int main() {
    char mem[] = "hello world";
    int n = 0;
    while (1) {
        if (malloc(BLOCKSIZE) == NULL) {
            break;
        }
        n+=1;
    }
    putn(n);
    putchar('\n');
}
