
#include <stdio.h>
#include <stdlib.h>
#include "../vm/vm/opcode.h"

#define vm_read0() ({index += 1; ops[index - 1];})
#define vm_read() vm_read0()
#define vm_jump() ({void *ptr = ptrs[ops[index]]; index += 1; goto *ptr;})

int main() {
    int *frames = malloc(sizeof(int) * 1000);
    int *regs = malloc(sizeof(int) * 16000);

    void **ptrs = malloc(sizeof(void *) * VM_OPCODE_MAX1);
    ptrs[VM_OPCODE_EXIT] = &&exec_exit;
    ptrs[VM_OPCODE_INT] = &&exec_int;
    ptrs[VM_OPCODE_PUTCHAR] = &&exec_putchar;

    int *ops = malloc(sizeof(int) * 256);
    ops[0] = VM_OPCODE_INT;
    ops[1] = 0;
    ops[2] = 52; 
    ops[3] = VM_OPCODE_INT;
    ops[4] = 1;
    ops[5] = 10;
    ops[6] = VM_OPCODE_PUTCHAR;
    ops[7] = 0;
    ops[8] = VM_OPCODE_PUTCHAR;
    ops[9] = 1;
    ops[10] = VM_OPCODE_EXIT;
    int index = 0;
    vm_jump();
exec_exit: {
    return 0;
}
exec_int: {
    regs[vm_read()] = vm_read();
    vm_jump();
}
exec_putchar: {
    putchar(regs[vm_read()]);
    vm_jump();
}
}