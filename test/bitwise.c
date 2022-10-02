#include <stdio.h>

#if !defined(__MINIVM__)
void print(int n, int l, const char *s, int r) {
    printf("%i %s %i = %i\n", l, s, r, n);
}
#else
void print(int n, int l, const char *s, int r) {
    putn(l);
    putchar(' ');
    while (*s) putchar(*s++);
    putchar(' ');
    putn(r);
    putchar(' ');    
    putchar('=');    
    putchar(' ');   
    putn(n); 
    putchar('\n');
}
#endif

#define op(op, l, r) print(l op r, l, #op, r)

int main() {
    op(+, 49, 2);
    op(-, 49, 2);
    op(*, 49, 2);
    op(/, 49, 2);
    op(%, 49, 2);
    op(^, 49, 2);
    op(&, 49, 2);
    op(|, 49, 2);
    op(<<, 49, 2);
    op(>>, 49, 2);
    return 0;
}