#include "player.h"

Player::Player(WINDOW * win, int y, int x)
{
    curwin = win;
    xLoc = x;
    yLoc = y;
    getmaxyx(curwin, yMax, xMax);
    keypad(curwin, true);
    Health = 10;
    isParrying = false;
    //cooldown = 0;
}