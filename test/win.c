#include <raylib.h>
#include <stdio.h>

int main() {
    InitWindow(1000, 1000);
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        EndDrawing();
    }
    CloseWindow();
}
