
int putchar(int n);

struct foo_t;
typedef struct foo_t foo_t;

struct foo_t {
    int x;
    int y;
    int z;
    foo_t *foo;
};

void printf("%i", int n) {
    if (n >= 10) {
        printf("%i", n / 10);
    }
    putchar(n % 10 + '0');
}

int main() {
    foo_t x = {
        'c',
        1,
    };
    printf("%i", sizeof(foo_t));
    putchar('\n');
    return 0;
}
