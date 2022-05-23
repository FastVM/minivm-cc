#define NULL ({(void*)0;})
#define MEM_START (1<<16)

int __minivm_get(int ptr);
int __minivm_set(int ptr, int val);

void *malloc(int size) {
    int where = __minivm_get(MEM_START);
    __minivm_set(MEM_START, where + size);
    return (int*) where;
}

void free(void *alloc) {
    // TODO: implement free list
}

int main(int argc, char **argv);
void _start() {
    __minivm_set(MEM_START, MEM_START+1);
    main(0, NULL);
}
