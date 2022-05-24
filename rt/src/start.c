#include "tinyalloc.h"

#if !defined(NULL)
#define NULL ({(void*)0;})
#endif

int main(int argc, char **argv);
void _start() {
    ta_init(0x1000, 16, 1);
    main(0, NULL);
}
