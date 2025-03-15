#include <curses.h>
using namespace std;
class Player
{
    public: 
        Player();
        void moveUp();
        void moveDown();
        void moveLeft();
        void moveRight();
        void parry();
        void getMovement();
        void display();
    private:
        int Health;
        bool isParrying; // invulnerable state
        //Time cooldown;
        int xLoc, yLoc, xMax, yMax;
        WINDOW * curwin;        
}