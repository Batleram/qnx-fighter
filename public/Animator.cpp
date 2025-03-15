#include "Animator.h"
#include <raylib.h>


Animator::Animator() {
    Loop = false;
    Img_duration = 0;
    Done = false;
    Frame = 0;
    vector<Texture2D> Images;

}

Texture2D Animator::LoadImage(std::string path) {
    // Load image from path
    Texture2D Images = LoadTexture("resources/explosion.png");
}

void Animator::LoadImages(std::string path, int numOfSprites) {
    // Load images from path "example0.png", "example1.png", etc.
    for (int i = 0; i < numOfSprites; i++) {
        Texture2D sprite = Animator::LoadImage(path + std::to_string(i) + ".png");
        Images.push_back(sprite);
    }
}

void Animator::Draw(Vector2D position) {
    // Draw the current frame using the position (from player)
    DrawTexture(Images[frame], position.x, position.y, WHITE);
}

void Animator::Update() {
    // Update the frame

    if (Loop) 
    {
        Frame = (Frame + 1) % Images.size();
    }
    else 
    {
        contine; // after i switch to max X.X
    }

Texture2D Animator::GetFrame() {
    return images[frame];
}