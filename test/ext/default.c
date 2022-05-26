
#include <stdio.h>

int val49() {
    return 49;
}

int add(int x, int y) {
    return x + y;
}

void foo(int x = val49(), int y = add(80, 4)) {
    putn(x);
    putn(y);
    putchar('\n');
}

int main() {
    foo();
    foo(19);
    foo(20, 22);
}
