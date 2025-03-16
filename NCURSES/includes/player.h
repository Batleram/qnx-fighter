#include <curses.h>
class Player
{
    public: 
        Player(WINDOW * win, int y, int x);
        void moveUp();
        void moveDown();
        void moveLeft();
        void moveRight();
        void parry();
        void attack();
        int getMovement();
        void display();
        void update();
        void collision(int box);
    private:
        int Health;
        bool isParrying; // invulnerable state
        int parryCoolDown;
        bool isAttacking;
        int attackingCoolDown;
        //Time cooldown;
        int xLoc, yLoc, xMax, yMax;
        WINDOW * curwin;        
};