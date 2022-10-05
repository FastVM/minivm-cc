#pragma once

#include "bits/def.h"

#if !defined(NULL)
#define NULL ((void *)0)
#endif

void *malloc(__size_t size);
void *calloc(__size_t n, __size_t size);
void *realloc(void *ptr, __size_t n);
void free(void *ptr);