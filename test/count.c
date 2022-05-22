int putchar(int c);

int main() {
    float x = 0.0;
    while (x < 10.0) {
        int y = x;
        putchar(y % 10 + '0');
        x += 1;
    }
    return 0;
}