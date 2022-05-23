#define NULL ({(void*)0;})

int putchar(int chr);
void *malloc(int size);
void free(void *ptr);
int putn(int n);

typedef struct list_t list_t;

struct list_t {
    double value;
    list_t *next;
};

list_t *list_new(int value, list_t *next) {
    list_t *ret = malloc(sizeof(list_t));
    ret->value = value;
    ret->next = next;
    return ret;
}

list_t *list_range(int low, int high) {
    list_t *ret = NULL;
    int cur = high;
    while (cur > low) {
        cur -= 1;
        ret = list_new(cur, ret);
    }
    return ret;
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

int main() {
    list_t *lis = list_range(0, 10);
    list_print(lis);
}
