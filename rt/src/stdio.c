
#include <stdio.h>

int putchar(int c);

int getchar(void) {
    return EOF;
}

void putn(int n) {
    if (n >= 10) {
        putn(n / 10);
    }
    putchar(n % 10 + '0');
}

void puts(const char *str) {
    while (1) {
        char chr = *str;
        if (chr == '\0') {
            break;
        }
        putchar(chr);
        str += 1;
    }
    putchar('\n');
}