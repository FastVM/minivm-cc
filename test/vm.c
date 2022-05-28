
#include "../vm/vm/asm.h"

void exit(int c);

int fprintf(void* o, void *p, char c) {
  if (o != (void*)1) {
    exit(1);
  }
  putchar(c);
}

int main()
{
  // const char *dump = "out.bc";
  const char *src = "@main\nr0 <- uint 48\nputchar r0\nr0 <- uint 10\nputchar r0\nexit\n\0";
  if (src == NULL)
  {
    // fprintf(stderr, "could not read file\n");
    return 1;
  }
  vm_asm_buf_t buf = vm_asm(src);
  vm_free((void *)src);
  if (buf.nops == 0) {
    // fprintf(stderr, "could not assemble file\n");
    return 1;
  }
  int res = vm_run_arch_int(buf.nops, buf.ops, NULL);
  if (res != 0)
  {
    // fprintf(stderr, "could not run asm\n");
    return 1;
  }
  free(buf.ops);
  return 0;
}
