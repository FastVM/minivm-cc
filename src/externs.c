
#include "../vm/vm/lib.h"
#include "../vm/vm/ir/be/gc.h"
#include "../rt/include/bits/raylib.h"

vm_value_t VMInitWindow(vm_gc_t *gc, vm_value_t a1, vm_value_t a2) {
    InitWindow(VM_VALUE_GET_INT(a1), VM_VALUE_GET_INT(a2), "MiniVM Window");
    return VM_VALUE_SET_INT(0);
}

vm_value_t VMCloseWindow(vm_gc_t *gc) {
    CloseWindow();
    return VM_VALUE_SET_INT(0);
}

vm_value_t VMWindowShouldClose(vm_gc_t *gc) {
    return VM_VALUE_SET_INT((size_t) WindowShouldClose());
}

vm_value_t VMBeginDrawing(vm_gc_t *gc) {
    BeginDrawing();
    return VM_VALUE_SET_INT(0);
}

vm_value_t VMEndDrawing(vm_gc_t *gc) {
    EndDrawing();
    return VM_VALUE_SET_INT(0);
}

vm_value_t VMClearBackground(vm_gc_t *gc, vm_value_t in) {
    Color color = (Color) {
        .r = VM_VALUE_GET_INT(vm_gc_get_i(gc, in, 0)),
        .g = VM_VALUE_GET_INT(vm_gc_get_i(gc, in, 1)),
        .b = VM_VALUE_GET_INT(vm_gc_get_i(gc, in, 2)),
        .a = VM_VALUE_GET_INT(vm_gc_get_i(gc, in, 3)),
    };
    ClearBackground(color);
    return VM_VALUE_SET_INT(0);
}
