
#include "../vm/vm/lib.h"
#include "../rt/include/bits/raylib.h"

#define VM_MEM ((vm_value_t) {.ival = 3})

int vm_extern_strlen(ptrdiff_t *a1) {
    int i = 0;
    for (;;) {
        if (a1[i] == 0) {
            return i;
        }
        i += 1;
    }
}

char *vm_extern_to_string(ptrdiff_t *a1) {
    int max = vm_extern_strlen(a1);
    char *ret = vm_alloc0(sizeof(char) * (max + 1));
    for (int i = 0; i < max; i++) {
        ret[i] = (char) a1[i];
    }
    return ret;
}

Color vm_extern_to_color(ptrdiff_t *in) {
    Color ret = (Color) {
        .r = in[0],
        .g = in[1],
        .b = in[2],
        .a = in[3],
    };
    return ret;
}

Vector2 vm_extern_to_v2(ptrdiff_t *in) {
    return (Vector2) {
        .x = in[0],
        .y = in[1],
    };
}

void VMInitWindow(ptrdiff_t a1, ptrdiff_t a2, ptrdiff_t *a3) {
    char *name = vm_extern_to_string(a3);
    SetTraceLogLevel(LOG_WARNING);
    InitWindow(a1, a2, name);
}

void VMCloseWindow(void) {
    CloseWindow();
}

ptrdiff_t VMWindowShouldClose(void) {
    return (ptrdiff_t) WindowShouldClose();
}

void VMBeginDrawing(void) {
    BeginDrawing();
}

void VMEndDrawing(void) {
    EndDrawing();
}

void VMClearBackground(ptrdiff_t *in) {
    Color ret = vm_extern_to_color(in);
    ClearBackground(ret);
}

ptrdiff_t VMIsKeyDown(ptrdiff_t key) {
    return (ptrdiff_t) IsKeyDown(key);
}

ptrdiff_t VMIsKeyUp(ptrdiff_t key) {
    return (ptrdiff_t) IsKeyUp(key);
}

ptrdiff_t VMIsKeyPressed(ptrdiff_t key) {
    return (ptrdiff_t) IsKeyPressed(key);
}

ptrdiff_t VMIsKeyReleased(ptrdiff_t key) {
    return (ptrdiff_t) IsKeyReleased(key);
}

void VMDrawCircleV(ptrdiff_t *pos, ptrdiff_t size, ptrdiff_t *color)
{
    DrawCircleV(vm_extern_to_v2(pos), size, vm_extern_to_color(color));
}

void VMSetTargetFPS(ptrdiff_t fps) {
    SetTargetFPS(fps);
}

void VMDrawText(ptrdiff_t *a1, ptrdiff_t a2, ptrdiff_t a3, ptrdiff_t a4, ptrdiff_t *a5) {
    char *name = vm_extern_to_string(a1);
    DrawText(name, a2, a3, a4, vm_extern_to_color(a5));
}

ptrdiff_t *VMMalloc(ptrdiff_t n) {
    // void *ret = vm_malloc(sizeof(ptrdiff_t) * n);
    // fprintf(stderr, "%p\n", ret);
    // return ret;
    return vm_malloc(sizeof(ptrdiff_t) * n);
}

ptrdiff_t *VMRealloc(ptrdiff_t *ptr, ptrdiff_t n) {
    return vm_realloc(ptr, sizeof(ptrdiff_t) * n);
}

void VMFree(ptrdiff_t *ptr) {
    // vm_free(ptr);
}
