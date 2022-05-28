
#include <stdio.h>
#include <stdlib.h>

typedef union {
    int x;
    int y;
} thing_t;

int main() {
    thing_t *thing = malloc(sizeof(thing_t));
    thing->y = 1234;
    return 0;
}
