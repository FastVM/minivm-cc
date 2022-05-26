
int putchar(int c);
void *malloc(int size);
void free(void *ptr);

int putn(int n) {
    if (n < 10) {
        putchar(n % 10 + '0');
    } else {
        putn(n / 10);
        putchar(n % 10 + '0');
    }
}

int main() {
    int y = 0;
    for (int i = 0; i < 1000; i+=1) {
        int *x = malloc(sizeof(int));
        *x = 1;
        for (int j = 0; j < 1000; j+=1) {
            y += *x;
        }
        free(x);
    }
    putn(y);
    putchar('\n');
}
