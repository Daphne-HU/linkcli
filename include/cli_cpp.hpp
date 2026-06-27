#pragma once

#include "cli.h"

/**
 * Thin C++ wrapper around the C CLI core.
 *
 * Usage:
 *   EmbeddedCli cli(my_putchar, my_getchar);
 *
 *   // in your main loop or RTOS task:
 *   cli.process();
 */
class EmbeddedCli {
public:
    EmbeddedCli(void (*putchar_fn)(char), int (*getchar_fn)(void))
    {
        cli_io_t io = { putchar_fn, getchar_fn };
        cli_init(&_cli, io);
    }

    /** Call from main loop or RTOS task — non-blocking. */
    void process() { cli_process(&_cli); }

    /** Write a string to the CLI output. */
    void puts(const char *s) { cli_puts(&_cli, s); }

    /** Write a single character to the CLI output. */
    void putc(char c) { cli_putc(&_cli, c); }

private:
    cli_t _cli;
};
