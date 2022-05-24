#include "tinyalloc.h"

#define NULL ({(void*)0;})

int main(int argc, char **argv);
void _start() {
    ta_init(1000, 16, 1);
    main(0, NULL);
}
