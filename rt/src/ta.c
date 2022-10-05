// https://github.com/thi-ng/tinyalloc

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

uint8_t ta_mem[1 << 20];
size_t ta_head;

void ta_init(void) {
    ta_head = 0;
}

static void int_free(void *ptr) {}

static void int_memclear(uint8_t *ptr, size_t num) {
    for (size_t i = 0; i < num; i++) {
        ptr[i] = 0;
    }
}

static void *int_malloc(size_t num) {
    uint8_t *ret = &ta_mem[ta_head]; 
    int_memclear(ret, num);
    ta_head += num;
    return ret;
}

static void *int_calloc(size_t num) {
    return int_malloc(num);
}

static void free(void *ptr) {
    size_t *sptr = (size_t *)ptr;
    int_free(&sptr[-1]);
}

static void *malloc(size_t num) {
    size_t *ret = int_malloc(num + sizeof(size_t));
    ret[0] = num;
    return &ret[1];
}

static void *calloc(size_t num, size_t size) {
    num *= size;
    size_t *ret = int_calloc(num + sizeof(size_t));
    ret[0] = num;
    return &ret[1];
}

static void *realloc(void *ptr, size_t num) {
    size_t sz = ((size_t *)ptr)[-1];
    uint8_t *init = ptr;
    uint8_t *next = malloc(num);
    for (size_t i = 0; i < num && i < sz; i++) {
        next[i] = init[i];
    }
    free(ptr);
    return next;
}