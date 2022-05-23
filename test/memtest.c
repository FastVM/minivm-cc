
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

int *alloc(int size) {
    int where = __minivm_get(1);
    __minivm_set(1, where + size);
    return (int*) where;
}

void entry(void) {
    int y = 0;
    for (int i = 0; i < 1000; i+=1) {
        int *x = alloc(1);
        *x = i + 1;
        for (int j = 0; j < 1000; j+=1) {
            y += *x ? 1 : 0;
        }
    }
    putn(__minivm_get(1));
    putchar('\n');
}

int main() {
    __minivm_set(1, 2);
    entry();
    return 0;
}