#pragma once
#include <vector>

enum Movement {
  LEFT = (1,0),
  RIGHT = (-1,0),
  JUMP = (0,1),
  CROUCH = (0,-1),
};

enum Ability {
  BLOCK = 0,
  ATTACK = 1,
  IDLE = 2,
};


class Player {
    public: 
        enum Movement movement;
        enum Ability ability;
        void Update();
        void Draw();
        void HandleMovement(enum Movement movement);
        void HandleAbility(enum Ability ability);
        void TakeDamage(int damage);
        int Health;
        bool isJumping; // vulnerable state
        bool isCrouching; // invulnerable state
    private:

};