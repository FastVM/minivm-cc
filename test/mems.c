#define NULL ({(void*)0;})

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

typedef struct list_t list_t;

struct list_t {
    int value;
    list_t *next;
};

list_t *list_new(int value, list_t *next) {
    list_t *ret = alloc(sizeof(list_t));
    ret->value = value;
    ret->next = next;
    return ret;
}

list_t *list_range(int low, int high) {
    if (low >= high) {
        return NULL;
    } else {
        return list_new(low, list_range(low + 1, high));
    }
}

void list_print(list_t *list) {
    putchar('(');
    int n = 0;
    while (list != NULL) {
        if (n == 0) {
            n = 1;
        } else {
            putchar(' ');
        }
        putn(list->value);
        list = list->next;
    }
    putchar(')');
    putchar('\n');
}

void entry(void) {
    list_t *lis = list_range(0, 10);
    list_print(lis);
}

int main() {
    __minivm_set(1, 2);
    entry();
    return 0;
}