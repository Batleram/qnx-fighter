#include <ncurses.h>
#include <string>
#include <cstdlib>
#include <ctime>
#include "player.h"
using namespace std;

int main(int argc, char ** argv)
{	

	/* Start Ncurses */
	initscr();
	noecho();
	cbreak();

	if (has_colors() == FALSE) {
        endwin();
        printf("Your terminal does not support color\n");
        exit(1);
    }

	// get screen demensions
	int xMax, yMax;
	getmaxyx(stdscr, yMax, xMax);

	// create a window for our input
	WINDOW * playwin = newwin(20, 50, (yMax/2)-10, 10);
	box(playwin, 0, 0);
	refresh();
	wrefresh(playwin);

	// add player to the screen
	Player * p = new Player(playwin, 1, 1);
	while(p->getMovement()!='x') // x to exit
	{
		p->update();
		p->display();
		wrefresh(playwin);
	}

	// wait before closing
	getch();
	endwin();

	return 0;
}
