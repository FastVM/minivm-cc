
int putchar(int n);

struct foo_t;
typedef struct foo_t foo_t;

struct foo_t {
    double x;
    double y;
    double z;
    foo_t *foo;
};

int putn(int n) {
    if (n >= 10) {
        putn(n / 10);
    }
    putchar(n % 10 + '0');
}

int main() {
    foo_t x = {
        'c',
        x.x,
    };
    putn(sizeof(foo_t));
    putchar('\n');
}
