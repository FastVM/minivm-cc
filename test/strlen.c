
#include <stdio.h>

int strlen(const char *str) {
    int ret = 0;
    while (*str++) ret++;
    return ret;
}

int main() {
    printf("%i", strlen("four"));
    putchar('\n');
    return 0;
}