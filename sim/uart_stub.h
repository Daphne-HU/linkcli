#ifndef UART_STUB_H
#define UART_STUB_H

/*
 * UART stub for host-side testing.
 *
 * Sets the terminal to raw, non-blocking mode so the CLI behaves the same
 * way it would on a microcontroller — characters arrive one at a time,
 * no OS line buffering, no automatic echo.
 *
 * Call uart_stub_init() before cli_init() and uart_stub_deinit() on exit
 * (registered automatically via atexit).
 */

/** Configure stdin: raw mode, no echo, non-blocking. */
void uart_stub_init(void);

/**
 * Non-blocking read from stdin.
 * Returns the next byte, or -1 if no character is available.
 * Pass this as the basis for feeding cli_rx_push() in your main loop.
 */
int uart_stub_getchar(void);

/**
 * Write one character to stdout.
 * Pass this as the basis for draining cli_tx_pop() in your main loop.
 */
void uart_stub_putchar(char c);

#endif /* UART_STUB_H */
