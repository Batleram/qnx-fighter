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
    isAttacking = false;
    attackingCoolDown = 0;
}

void Player::moveUp()
{
    mvwaddch(curwin, yLoc, xLoc, ' ');
    yLoc--;
    // boundary
    if(yLoc < 1)
    {
        yLoc = 1;
    }

}

void Player::moveDown()
{
    mvwaddch(curwin, yLoc, xLoc, ' ');
    yLoc++;
    // boundary
    if(yLoc > yMax - 2)
    {
        yLoc = yMax - 2;
    }

}

void Player::moveLeft()
{
    mvwaddch(curwin, yLoc, xLoc, ' ');
    xLoc--;
    // boundary
    if(xLoc < 1)
    {
        xLoc = 1;
    }

}

void Player::moveRight()
{
    mvwaddch(curwin, yLoc, xLoc, ' ');
    xLoc++;
    // boundary
    if(xLoc > xMax - 2)
    {
        xLoc = xMax - 2;
    }

}

void Player::parry()
{
    if (parryCoolDown > 0 || attackingCoolDown > 0)
    {
        return;
    }
    isParrying = true;
    parryCoolDown = 50;
    
}

void Player::attack()
{
    if (attackingCoolDown > 0 | parryCoolDown > 0)
    {
        return;
    }
    isAttacking = true;
    attackingCoolDown = 50;
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
        case 's':
            parry();
            break;
        case 'a':
            attack();
            break;
        default:
            break;
    }
    return choice;
}

void Player::display()
{
    if (parryCoolDown > 0)
    {
        wattron(curwin, COLOR_PAIR(2));
        mvwaddch(curwin, yLoc, xLoc, 'X');
        wattroff(curwin, COLOR_PAIR(2));
    }
    else if (attackingCoolDown > 0)
    {
        wattron(curwin, COLOR_PAIR(1));
        mvwaddch(curwin, yLoc, xLoc, 'A');
        wattroff(curwin, COLOR_PAIR(1));
    }
    else
    {
        wattron(curwin, COLOR_PAIR(3));
        mvwaddch(curwin, yLoc, xLoc, 'P');
        wattroff(curwin, COLOR_PAIR(3));
    }
    wrefresh(curwin);
}

void Player::update()
{
    if(parryCoolDown > 0)
    {
        parryCoolDown--;
    }
    if(attackingCoolDown > 0)
    {
        attackingCoolDown--;
    }
    else
    {
        isParrying = false;
        parryCoolDown = 0;
        isAttacking = false;
        attackingCoolDown = 0;
    }
}