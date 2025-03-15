#pragma once
#include <vector>
#include <raylib.h>

class Animator {
    public:
        Animator();
        Texture2D LoadImage(std::string path);
        void LoadImages(std::string path, int numOfSprites);
        void Draw(Vector2 position, bool isFlipped);
        void Update();
        Texture2D GetFrame();
    private:
        bool Loop;
        int Img_duration;
        bool Done;
        int Frame;
        std::vector<Texture2D> Images;

};