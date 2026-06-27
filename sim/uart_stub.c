#include "uart_stub.h"

#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

static struct termios _saved_termios;

static void restore_terminal(void)
{
    tcsetattr(STDIN_FILENO, TCSANOW, &_saved_termios);
}

void uart_stub_init(void)
{
    /* Save current terminal settings so we can restore on exit */
    tcgetattr(STDIN_FILENO, &_saved_termios);
    atexit(restore_terminal);

    /* Raw mode: no line buffering, no echo, no signal characters */
    struct termios raw = _saved_termios;
    raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
    raw.c_cc[VMIN]  = 0;   /* non-blocking read */
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);

    /* Make stdin non-blocking so uart_stub_getchar() returns -1 immediately */
    int flags = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

int uart_stub_getchar(void)
{
    unsigned char c;
    if (read(STDIN_FILENO, &c, 1) == 1) {
        return (int)c;
    }
    return -1;
}

void uart_stub_putchar(char c)
{
    putchar((unsigned char)c);
    fflush(stdout);
}
