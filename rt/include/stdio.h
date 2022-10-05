#pragma once

#include "bits/def.h"

#define EOF (-1)

#define stdin ((void *)0)
#define stdout ((void *)1)
#define stderr ((void *)2)

struct FILE;
typedef struct FILE FILE;

int printf(const char *fmt, ...);

void __minivm_putsi(__ptrdiff_t n);
void __minivm_putui(__size_t n);
void __minivm_puts(const char *str);
int getchar(void);
int putchar(int chr);
void puts(const char *c);
FILE *fopen(const char *name, const char *flags);
void fclose(FILE *file);
int fwrite(void *buf, int s1, int s2, FILE *file);
int fread(void *buf, int s1, int s2, FILE *file);