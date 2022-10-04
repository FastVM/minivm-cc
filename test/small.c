
int putchar(int c);

int fib(int n) {
    if (n < 2) {
        return n;
    } else {
        return fib(n - 1) + fib(n - 2);
    }
}

void printf("%i", int n) {
    if (n >= 10) {
        printf("%i", n / 10);
    }
    putchar(n % 10 + '0');
}

int main() {
    printf("%i", fib(25));
    putchar('\n');
    return 0;
}
