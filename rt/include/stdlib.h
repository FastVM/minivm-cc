#pragma once

#include <bits/def.h>

#define NULL ({(void*)0;})

void *malloc(__size_t size);
void *calloc(__size_t n, __size_t size);
void free(void *ptr);