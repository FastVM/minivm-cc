
#include <stdio.h>

#if !defined(__MINIVM__)
void printf("%i", int n) {
    printf("%i", n);
}
#endif

int main() {
    int x = 4, y = 5;
    x += y++;
    y += ++x;
    x += y--;
    y += x--;
    printf("%i", x);
    putchar(':');
    printf("%i", y);
    putchar('\n');
    return 0;
}