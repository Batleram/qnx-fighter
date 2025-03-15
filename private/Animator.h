#pragma once
#include <vector>

class Animator {
    public:
        Animator();
        Texture2D LoadImage(std::string path);
        void LoadImages(std::string path, int numOfSprites);
        void Draw();
        void Update();
        Texture2D GetFrame();
    private:
        bool Loop;
        int img_duration;
        bool done;
        int frame;
        vector<Texture2D> images;

};