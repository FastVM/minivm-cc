
#include <stdio.h>

int fib(int n) {
    if (n < 2) {
        return n;
    } else {
        return fib(n-1) + fib(n-2);
    }
}

int main() {
    putn(fib(40));
    putchar('\n');
    return 0;
}
