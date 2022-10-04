
#include <stdarg.h>
#include <stdio.h>

int vmprintf(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int x = va_arg(ap, int);
    __minivm_putsi(x);
    putchar('\n');
    return 0;
}

int main() {
    vmprintf("%i", 10);
    return 1;
}
