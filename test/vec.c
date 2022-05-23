typedef struct vector2_t {
    float x;
    float y;
} vector2_t;

int putchar(int c);

void putn(int n) {
    if (n >= 10) {
        putn(n / 10);
    }
    putchar(n % 10 + '0');
}

void putnln(int n) {
    putn(n);
    putchar(10);
}

void vector2_print(vector2_t val) {
    putchar('[');
    putn(val.x);
    putchar(',');
    putchar(' ');
    putn(val.y);
    putchar(']');
}

void vector2_println(vector2_t val) {
    vector2_print(val);
    putchar('\n');
}

float vector2_sum(vector2_t val) {
    return val.x + val.y;
}

#define vector2_op1(op_, x_) ({vector2_t x=x_;(vector2_t) {op_ x.x, op_ x.y};})
#define vector2_op2(op_, x_, y_) ({vector2_t x=x_;vector2_t y=y_;(vector2_t) {x.x op_ y.x, x.y op_ y.y};})

vector2_t vector2_add(vector2_t lhs, vector2_t rhs) {
    return vector2_op2(+, lhs, rhs);
}

vector2_t vector2_sub(vector2_t lhs, vector2_t rhs) {
    return vector2_op2(-, lhs, rhs);
}

int main() {
    vector2_t v1 = { 100, 200 };
    vector2_t v2 = {  10,  20 };
    vector2_t va = vector2_op2(-, v1, v2);
    vector2_println(v1);
    vector2_println(v2);
    vector2_println(va);
    vector2_println(vector2_add(v1, v2));
    return 0;
}