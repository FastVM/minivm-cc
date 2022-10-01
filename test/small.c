
int putchar(int c);

int fib(int n) {
    if (n < 2) {
        return n;
    } else {
        return fib(n-1) + fib(n-2);
    }
}

void putn(int n) {
    if (n >= 10) {
        putn(n / 10);
    }
    putchar(n % 10 + '0');
}

int main() {
    putn(fib(25));
    putchar('\n');
    return 0;
}
