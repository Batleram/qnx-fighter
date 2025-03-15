g++ -g -O0 -I includes -lncurses -lpanel -c ./source/player.cpp -o player.o
g++ main.cpp player.o -I includes -lncurses -lpanel -g -o game