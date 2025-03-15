#include <random>
#include "Environment.h"

Environment::Environment() {
}

void Environment::Draw() {
    grid.Draw();
    currentBlock.Draw();
}

void Environment::HandleInput()
{
    int keyPressed = GetKeyPressed();
    switch(keyPressed) {
        case KEY_LEFT:
            MoveBlockLeft();
            break;
        case KEY_RIGHT:
            MoveBlockRight();
            break;
        case KEY_DOWN:
            MoveBlockDown();
            break;
        case KEY_UP:
            RotateBlock();
            break;
    }
}