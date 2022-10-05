#pragma once

#include "bits/def.h"

__size_t strlen(const char *str);

int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, __size_t n);

char *strcpy(char *dest, const char *src);
char *strncpy(char *dest, const char *src, __size_t n);

char *strcat(char *dest, const char *src);

int memcmp(const void *a, const void *b, __size_t n);

void *memcpy(void *dest, const void *src, __size_t n);

void *memset(void *dest, int b, __size_t n);
void *memmove(void *dest, int b, __size_t n);

char *strchr(const char *str, int i);
