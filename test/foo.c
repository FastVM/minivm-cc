
int putchar(int n);

struct foo_t;
typedef struct foo_t foo_t;

struct foo_t {
    int x;
    int y;
    int z;
    foo_t *foo;
};

void putn(int n) {
    if (n >= 10) {
        putn(n / 10);
    }
    putchar(n % 10 + '0');
}

int main() {
    foo_t x = {
        'c',
        1,
    };
    putn(sizeof(foo_t));
    putchar('\n');
    return 0;
}
