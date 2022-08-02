#include <raylib.h>
#include <stdio.h>

int main() {
    InitWindow(1000, 1000, "MiniVM Window");
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
}
