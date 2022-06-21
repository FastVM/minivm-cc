#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

#define true ((_Bool)1)
#define BLOCKSIZE 0xF

int main() {
    char mem[] = "hello world";
    int n = 0;
    for (int i = 0; i < 1000; i++) {
        if (malloc(BLOCKSIZE) == NULL) {
            break;
        }
        n+=1;
    }
    putn(n);
    putchar('\n');
}
