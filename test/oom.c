#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>

int main() {
    int n = 0;
    while (1) {
        if (malloc(2) == NULL) {
            break;
        }
        n+=1;
        putn(n);
        putchar('\n');
    }
}
