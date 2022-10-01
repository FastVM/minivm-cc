#pragma once

#define EOF (-1)

#define stdin ((void *)0)
#define stdout ((void *)1)
#define stderr ((void *)2)

struct FILE;
typedef struct FILE FILE;

int fprintf(FILE *file, const char *fmt, ...);

int getchar(void);
int putchar(int chr);
void puts(const char *c);
void putn(int n);
FILE *fopen(const char *name, const char *flags);
void fclose(FILE *file);
int fwrite(void *buf, int s1, int s2, FILE *file);
int fread(void *buf, int s1, int s2, FILE *file);