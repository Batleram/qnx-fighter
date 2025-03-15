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
        int getMovement();
        void display();
        void update();
    private:
        int Health;
        bool isParrying; // invulnerable state
        int parryCoolDown;
        //Time cooldown;
        int xLoc, yLoc, xMax, yMax;
        WINDOW * curwin;        
};