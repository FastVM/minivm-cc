#pragma once

#include "bits/int.h"

typedef struct {
    void *tmp;
    __uint8_t *ptr;
} va_list[1];

#define va_arg(ap, type) (*(type *)((ap)[0].tmp = (ap)[0].ptr, (ap)[0].ptr += sizeof(type), (ap)[0].tmp))
#define va_copy(dest, src) ((dest).ptr = (src).ptr)

#define va_start(ap, last) (ap[0].ptr = &(last) + 1)
#define va_end(ap) 1
