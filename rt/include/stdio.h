#pragma once

#define EOF (-1)

#define stdin ((void*)0)
#define stdout ((void*)1)
#define stderr ((void*)2)

int fprintf(void *file, const char *fmt, ...);

int getchar(void);
int putchar(int chr);
void puts(const char *c);
void putn(int n);
void *fopen(const char *name, const char *flags);
void fclose(void *file);
int fwrite(void *buf, int s1, int s2, void *file);
int fread(void *buf, int s1, int s2, void *file);