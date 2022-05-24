
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    int x;
} thing_t;

int main() {
    thing_t foo1 = {0};
    thing_t foo2 = {1};
    thing_t *pfoo = malloc(sizeof(thing_t));
    putn(foo1.x);
    putchar('\n');
    putn(foo2.x);
    putchar('\n');
}
