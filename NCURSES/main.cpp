#include <ncurses.h>
#include <string>
#include <cstdlib>
#include <ctime>
using namespace std;

int main(int argc, char ** argv)
{	

	/* Start Ncurses */
	initscr();
	noecho();
	cbreak();

	// get screen demensions
	int xMax, yMax;
	getmaxyx(stdscr, yMax, xMax);

	// create a window for our input
	WINDOW * playwin = newwin(20, 50, (yMax/2)-10, 10);
	box(playwin, 0, 0);
	refresh();
	wrefresh(playwin);

	// wait before closing
	getch();
	endwin();

	return 0;
}
