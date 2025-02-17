#include <stdio.h>
#include <stdlib.h>

#define true 1
#define false 0
#define size (8190)

int main(int argc, char **argv) {
    char flags[size + 1];
    int i, prime, k, count, iter;
    for (iter = 1; iter <= 10; iter++) {
        count = 0;
        for (i = 0; i <= size; i++) {
            flags[i] = true;
        }
        for (i = 0; i <= size; i++) {
            if (flags[i]) {
                prime = i + i + 3;
                for (k = i + prime; k <= size; k += prime) {
                    flags[k] = false;
                }
                count++;
            }
        }
    }
#if !defined(__MINIVM__)
    printf("%i\n", count);
#else
    printf("%i", count);
    putchar('\n');
#endif
    return 0;
}