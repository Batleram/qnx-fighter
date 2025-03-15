#pragma once
#include <vector>

enum Movement {
  LEFT = 0,
  RIGHT = 1,
  JUMP = 2,
  CROUCH = 3,
};


class Player {
    public: 
        enum Movement movement;
        void Update();

    private:

};