
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>

int putchar(int c);

void __minivm_putui(size_t n) {
    if (n >= 10) {
        __minivm_putui(n / 10);
    }
    putchar(n % 10 + '0');
}

void __minivm_putsi(ptrdiff_t n) {
    if (n < 0) {
        putchar('-');
        __minivm_putui(-n);
    } else {
        __minivm_putui(n);
    }
}

void  __minivm_puts(const char *str) {
    while (1) {
        char chr = *str;
        if (chr == '\0') {
            break;
        }
        putchar(chr);
        str += 1;
    }
}

int printf(const char *str, ...) {
    va_list ap;
    va_start(ap, str);
    for (size_t i = 0; str[i] != '\0'; i++) {
        if (str[i] == '%') {
            i += 1;
            if (str[i] == 'd' || str[i] == 'i') {
                __minivm_putsi((ptrdiff_t) va_arg(ap, int));
            } else {
                putchar('?');
            }
        } else {
            putchar(str[i]);
        }
    }
    va_end(ap);
    return 0;
}
