
int putchar(int c);
int __minivm_get(int ptr);
int __minivm_set(int ptr, int val);

int fib(int n) {
    if (n < 2) {
        return n;
    } else {
        return fib(n-1) + fib(n-2);
    }
}

int putn(int n) {
    if (n < 10) {
        putchar(n % 10 + '0');
    } else {
        putn(n / 10);
        putchar(n % 10 + '0');
    }
}

int main() {
    putchar('\n');
    putchar('(');
    __minivm_set(100, 1234);
    putchar(':');
    putn(__minivm_get(100));
    putchar(')');
    putchar('\n');
    return 0;
}