<h1 align="center">QNX FIGHTER</h1>
<p align="center">Street Fighter, but on QNX.</p>

# About

### Inspiration:
We were mainly inspired by classic Street Fighter and retro NES-style games. There's just something about them that is so nostalgic and fun.

### What it does:
Runs a Street Fighter-inspired game we made, now on QNX. See `Building` section for setup.

### How we built it:
This project is the result of many failed attempts at getting rendering working on the QNX everywhere image.

### Challenges we ran into:
Attempts include but are not limited to:
 - Custom raylib port (decent progress, you can see it in the raylib-test branch)
 - Wayland/Weston (gitlab.freedesktop.org is down, and we couldn't build it properly)
 - Pygame We couldn't get a compiled version of the package running on QNX, we however have a really nice looking fighter game you can check out in the PYGAME folder
 - NCurses We didn't get super far, but it seemed like our only option for a while, until we found how to do native rendering
 - OpenGL Didn't put much effort into it, seemed like a massive mess
 - We considered QT for a bit, but didn't end up giving it a shot

### Accomplishments that we're proud of:
Despite the hurdles, and many many attempts, we managed to create a working version of the game. It might not be perfect, but we’re proud that we got it running on QNX!

### What we learned:
Through this project, we learned a whole lot about raylib, Pygame, NCurses, OpenGL, and especially the QNX platform.

### Things we would've wanted to do:
 - Getting audio working, it would've been cool to have audio for hitsounds and parries, it would've made the game more immersive, but we don't have a speaker Using hardware interrupts for sensitive inputs like parry and hit, since we are limited to the game clock to parse the inputs.
 - P1 has a natural advantage due to them being processed first in the game loop. Interrupts would address this.
 - RGB strip for HP display, we couldn't get the SPI drivers working and our bitbang solution was far too inconsistent to be used in the final project Getting animations and nicer graphics, currently we are only using basic shapes due to having to write our own renderer.
 - We had to focus on the gameplay loop instead of polishing the looks Slide kicking, we have a down button on the gamepads which we were planning to use on some other feature if we got the time for it, however our rendering adventures took up almost all our time Jumping, we have an up button on the gamepads which we wanted to implement jumping with.
 - Having both horizontal and vertical movement would've allowed a lot of interesting combo and gameplay, but it didn't pan out.

---

# Building

### QNX application:
Make sure you have the SDK installed as well as having it sourced in your shell.
You will also need to open the Makefile and remove the `scp` command since we were using it for rapid deployment of the application, but it will not work for your setup
All the code is in the `screen_writer.c` file, it is not pretty nor optimized, it's written to work.

### Pygame:

Just run the `game.py` file bro

### NCurses:

There is a build.sh file, it will compile for your local system. We have not setup the workflow to compile for QNX
Make sure you're on a unix-like system that supports the NCurses library.

---

# SHOWCASE
![20250316_022835](https://github.com/user-attachments/assets/4f2e7afa-7faa-47d5-99ad-bfe2e863f2da)
![20250316_022844](https://github.com/user-attachments/assets/ec5fa6d3-1d35-43b7-a717-cd2123ea788f)
![20250316_022857](https://github.com/user-attachments/assets/85b1e411-76eb-4b6d-a6a1-40184dd5f330)
![20250316_022908](https://github.com/user-attachments/assets/30c80f5f-4bce-4da9-ac52-0d178afcbbda)




