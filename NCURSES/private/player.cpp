#include "player.h"

Player::Player(WINDOW * win, int y, int x)
{
    curwin = win;
    xLoc = x;
    yLoc = y;
    getmaxyx(curwin, yMax, xMax);
    keypad(curwin, true);
    Health = 3;
    isParrying = false;
    parryCoolDown = 0;

}

void Player::moveUp()
{
    yLoc--;
    // boundary
    if(yLoc < 1)
    {
        yLoc = 1;
    }

}

void Player::moveDown()
{
    yLoc++;
    // boundary
    if(yLoc > yMax - 2)
    {
        yLoc = yMax - 2;
    }

}

void Player::moveLeft()
{
    xLoc--;
    // boundary
    if(xLoc < 1)
    {
        xLoc = 1;
    }

}

void Player::moveRight()
{
    xLoc++;
    // boundary
    if(xLoc > xMax - 2)
    {
        xLoc = xMax - 2;
    }

}

void Player::parry()
{
    isParrying = true;
    parryCoolDown = 5;
    
}

int Player::getMovement()
{
    int choice = wgetch(curwin);
    switch(choice)
    {
        case KEY_UP:
            moveUp();
            break;
        case KEY_DOWN:
            moveDown();
            break;
        case KEY_LEFT:
            moveLeft();
            break;
        case KEY_RIGHT:
            moveRight();
            break;
        case ' ':
            parry();
            break;
        default:
            break;
    }
    return choice;
}

void Player::display()
{
    // display player as a colored block
    wattron(curwin, COLOR_PAIR(1));
    mvwaddch(curwin, yLoc, xLoc, 'X');
    wattroff(curwin, COLOR_PAIR(1));
    wrefresh(curwin);
}

void Player::update()
{
    if(parryCoolDown > 0)
    {
        parryCoolDown--;
    }
    else
    {
        isParrying = false;
        parryCoolDown = 0;
    }
}