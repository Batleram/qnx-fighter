#include "Animator.h"
#include <vector>


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

void Animator::Draw(Vector2 position) {
    // Draw the current frame using the position (from player)
    DrawTexture(Images[Frame], position.x, position.y, WHITE);
}

void Animator::Update() {
    // Update the frame
    if (Loop) 
    {
        Frame = (Frame + 1) % Images.size();
    }
    else 
    {
        Frame = min(Frame + 1, Img_duration * Images.size() - 1);
        if (Frame >= Img_duration * Images.size() - 1)
        {
            // This is for timed animations
            Done = True; 
        }     
    }
}

Texture2D Animator::GetFrame() {
    // Return the current frame
    return images[frame];
}