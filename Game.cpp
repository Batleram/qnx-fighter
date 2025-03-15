#include <raylib.h>
#include "Environment.h"

int main() {
    // Determine the Game Window Width and Height
    const int screenWidth = 300;
    const int screenHeight = 600;

    // Set Background White to re-blit sprites
    Color screenColor = Color{255, 255, 255, 255};

    Environment Game = Environment();

    
    // Initialize the Window
    InitWindow(screenWidth, screenHeight, "Street Fighter");

    // Setting the Frames Per Second
    SetTargetFPS(60);

    // The Game Loop
    while (!WindowShouldClose() /*WindowShouldClose returns true if esc is clicked and closes the window*/) {

        Game.HandleInput();
        // Setup Canvas
        BeginDrawing();
        // Clear canvas to a specific color to avoid flicker
        ClearBackground(screenColor);

        Game.Draw();

        // teardown Canvas
        EndDrawing();
    }
    CloseWindow();
    return 0;
}