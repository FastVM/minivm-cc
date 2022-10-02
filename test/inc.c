
#include <stdio.h>

#if !defined(__MINIVM__)
void putn(int n) {
    printf("%i", n);
}
#endif

int main() {
    int x = 4, y = 5;
    x += y++;
    y += ++x;
    x += y--;
    y += x--;
    putn(x);
    putchar(':');
    putn(y);
    putchar('\n');
    return 0;
}