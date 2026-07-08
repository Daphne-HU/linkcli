#include "uart_stub.h"

#include <termios.h>
#include <unistd.h>
#include <sys/select.h>
#include <stdio.h>
#include <stdlib.h>

static struct termios _saved_termios;
static int _is_tty;

static void restore_terminal(void)
{
    if (_is_tty) {
        tcsetattr(STDIN_FILENO, TCSANOW, &_saved_termios);
    }
}

void uart_stub_init(void)
{
    _is_tty = isatty(STDIN_FILENO);

    if (_is_tty) {
        /* Interactive terminal: raw mode so characters arrive one at a time
         * without the OS buffering a full line. */
        tcgetattr(STDIN_FILENO, &_saved_termios);
        atexit(restore_terminal);

        struct termios raw = _saved_termios;
        raw.c_lflag &= (tcflag_t)~(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN]  = 0;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    /* When stdin is a pipe (subprocess mode) we leave it untouched.
     * uart_stub_getchar() uses select() for non-blocking polling in both cases. */
}

int uart_stub_getchar(void)
{
    /* Non-blocking check via select — works on both ttys and pipes. */
    fd_set fds;
    struct timeval tv = {0, 0};
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);

    if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0) {
        unsigned char c;
        if (read(STDIN_FILENO, &c, 1) == 1) {
            return (int)c;
        }
    }
    return -1;
}

void uart_stub_putchar(char c)
{
    putchar((unsigned char)c);
    fflush(stdout);
}
