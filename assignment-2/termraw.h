#include <termios.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <dirent.h>
#include <string.h>

struct termios oldterm, newterm;

#define KEY_ENTER       0x000a
#define KEY_TAB         0x0009
#define KEY_CTRL_R      0x0012
#define KEY_BACKSPACE   oldterm.c_cc[VERASE]
#define EOT             0x0004

char getch(void)
{
    int c = 0;

    tcgetattr(0, &oldterm);
    memcpy(&newterm, &oldterm, sizeof(newterm));
    newterm.c_lflag &= ~(ICANON | ECHO);
    newterm.c_cc[VMIN] = 1;
    newterm.c_cc[VTIME] = 0;
    tcsetattr(0, TCSANOW, &newterm);
    c = getchar();
    tcsetattr(0, TCSANOW, &oldterm);
    return c;
}