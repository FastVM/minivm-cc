
#include <raylib.h>
#include <stdio.h>

int main(void)
{
    Vector2 winsize = {800, 450};

    InitWindow(winsize.x, winsize.y, "MiniVM Window");

    Vector2 ballPosition = {winsize.x / 2, winsize.y / 2};

    // SetTargetFPS(60);
    while (!WindowShouldClose())
    {
        // if (IsKeyDown(KEY_RIGHT))
        //     ballPosition.x += 2.0f;
        // if (IsKeyDown(KEY_LEFT))
        //     ballPosition.x -= 2.0f;
        // if (IsKeyDown(KEY_UP))
        //     ballPosition.y -= 2.0f;
        // if (IsKeyDown(KEY_DOWN))
        //     ballPosition.y += 2.0f;

        BeginDrawing();

        ClearBackground(RAYWHITE);
        // DrawText("move the ball with arrow keys", 10, 10, 20, DARKGRAY);
        DrawCircleV(ballPosition, 50, MAROON);
        
        EndDrawing();
    }

    CloseWindow();

    return 0;
}