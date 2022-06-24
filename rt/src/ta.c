// https://github.com/thi-ng/tinyalloc

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define TA_DISABLE_COMPACT
#define TA_DISABLE_SPLIT

typedef struct Block Block;

struct Block
{
    void *addr;
    Block *next;
    size_t size;
};

typedef struct
{
    Block *free;  // first free block
    Block *used;  // first used block
    Block *fresh; // first available blank block
    size_t top;   // top free addr
} Heap;

#define heap() ((Heap *)16)
#define heap_limit() (900000)
#define heap_split_thresh() 0x10
#define heap_max_blocks() 0x1000

/**
 * If compaction is enabled, inserts block
 * into free list, sorted by addr.
 * If disabled, headd block has new head of
 * the free list.
 */
static void insert_block(Block *block)
{
#ifndef TA_DISABLE_COMPACT
    Block *ptr = heap()->free;
    Block *prev = NULL;
    while (ptr != NULL)
    {
        if ((size_t)block->addr <= (size_t)ptr->addr)
        {
            break;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    if (prev != NULL)
    {
        if (ptr == NULL)
        {
        }
        prev->next = block;
    }
    else
    {
        heap()->free = block;
    }
    block->next = ptr;
#else
    block->next = heap()->free;
    heap()->free = block;
#endif
}

#ifndef TA_DISABLE_COMPACT
static void release_blocks(Block *scan, Block *to)
{
    Block *scan_next;
    while (scan != to)
    {
        scan_next = scan->next;
        scan->next = heap()->fresh;
        heap()->fresh = scan;
        scan->addr = 0;
        scan->size = 0;
        scan = scan_next;
    }
}

static void compact()
{
    Block *ptr = heap()->free;
    Block *prev;
    Block *scan;
    while (ptr != NULL)
    {
        prev = ptr;
        scan = ptr->next;
        while (scan != NULL &&
               (size_t)prev->addr + prev->size == (size_t)scan->addr)
        {
            prev = scan;
            scan = scan->next;
        }
        if (prev != ptr)
        {
            size_t new_size =
                (size_t)prev->addr - (size_t)ptr->addr + prev->size;
            ptr->size = new_size;
            Block *next = prev->next;
            // make merged blocks available
            release_blocks(ptr->next, prev->next);
            // relink
            ptr->next = next;
        }
        ptr = ptr->next;
    }
}
#endif

bool ta_init(void)
{
    heap()->free = NULL;
    heap()->used = NULL;
    heap()->fresh = (Block *)(20);
    heap()->top = (size_t)(20 + heap_max_blocks());

    Block *block = heap()->fresh;
    size_t i = heap_max_blocks() - 1;
    while (i)
    {
        i -= 1;
        block->next = block + 1;
        block += 1;
    }
    block->next = NULL;
    return true;
}

static bool int_free(void *free)
{
    Block *block = heap()->used;
    Block *prev = NULL;
    while (block != NULL)
    {
        if (free == block->addr)
        {
            if (prev)
            {
                prev->next = block->next;
            }
            else
            {
                heap()->used = block->next;
            }
            insert_block(block);
#ifndef TA_DISABLE_COMPACT
            compact();
#endif
            return true;
        }
        prev = block;
        block = block->next;
    }
    return false;
}

static Block *alloc_block(size_t num)
{
    Block *ptr = heap()->free;
    Block *prev = NULL;
    size_t top = heap()->top;
    while (ptr != NULL)
    {
        const int is_top = ((size_t)ptr->addr + ptr->size >= top) && ((size_t)ptr->addr + num <= (size_t)heap_limit());
        if (is_top || ptr->size >= num)
        {
            if (prev != NULL)
            {
                prev->next = ptr->next;
            }
            else
            {
                heap()->free = ptr->next;
            }
            ptr->next = heap()->used;
            heap()->used = ptr;
            if (is_top)
            {
                ptr->size = num;
                heap()->top = (size_t)ptr->addr + num;
#ifndef TA_DISABLE_SPLIT
            }
            else if (heap()->fresh != NULL)
            {
                size_t excess = ptr->size - num;
                if (excess >= heap_split_thresh())
                {
                    ptr->size = num;
                    Block *split = heap()->fresh;
                    heap()->fresh = split->next;
                    split->addr = (void *)((size_t)ptr->addr + num);
                    split->size = excess;
                    insert_block(split);
#ifndef TA_DISABLE_COMPACT
                    compact();
#endif
                }
#endif
            }
            return ptr;
        }
        prev = ptr;
        ptr = ptr->next;
    }
    // no matching free blocks
    // see if any other blocks available
    size_t new_top = top + num;
    if (heap()->fresh != NULL && new_top <= (size_t)heap_limit())
    {
        ptr = heap()->fresh;
        heap()->fresh = ptr->next;
        ptr->addr = (void *)top;
        ptr->next = heap()->used;
        ptr->size = num;
        heap()->used = ptr;
        heap()->top = new_top;
        return ptr;
    }
    return NULL;
}

static void *int_malloc(size_t num)
{
    Block *block = alloc_block(num);
    if (block != NULL)
    {
        return block->addr;
    }
    return NULL;
}

static void memclear(void *ptr, size_t num)
{
    uint8_t *uv = ptr;
    for (size_t i = 0; i < num; i++)
    {
        uv[i] = 0;
    }
}

static void *int_calloc(size_t num)
{
    Block *block = alloc_block(num);
    if (block != NULL)
    {
        memclear(block->addr, num);
        return block->addr;
    }
    return NULL;
}

static void free(void *ptr)
{
    size_t *sptr = (size_t *)ptr;
    int_free(&sptr[-1]);
}

static void *malloc(size_t num)
{
    size_t *ret = int_malloc(num + sizeof(size_t));
    if (ret == NULL)
    {
        return NULL;
    }
    ret[0] = num;
    return ret + 1;
}

static void *calloc(size_t num, size_t size)
{
    num *= size;
    size_t *ret = int_calloc(num + sizeof(size_t));
    if (ret == NULL)
    {
        return NULL;
    }
    ret[0] = num;
    return &ret[1];
}

static void *realloc(void *ptr, size_t num)
{
    size_t sz = ((size_t *)ptr)[-1];
    uint8_t *init = &ptr[0];
    uint8_t *next = malloc(num);
    if (next != NULL)
    {
        for (size_t i = 0; i < sz && i < num; i += 1)
        {
            next[i] = init[i];
        }
    }
    free(ptr);
    return next;
}