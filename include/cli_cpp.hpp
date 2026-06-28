#pragma once

#include "cli.h"
#include <stdint.h>

/**
 * Thin C++ wrapper around the C CLI core.
 *
 * The cli_t instance lives in your UART driver file (next to the ISR) and
 * is passed in by pointer — the ISR can still call cli_rx_push() /
 * cli_tx_pop() directly in C without involving this class.
 *
 * Usage:
 *
 *   // uart.c (C)
 *   cli_t g_cli;
 *   void USART2_IRQHandler(void) {
 *       cli_rx_push(&g_cli, byte);         // RX
 *       cli_tx_pop(&g_cli, &byte);         // TX
 *   }
 *
 *   // app.cpp (C++)
 *   extern "C" cli_t g_cli;
 *   EmbeddedCli cli(&g_cli, uart_tx_start);
 *
 *   // in main loop or RTOS task:
 *   cli.process();
 */
class EmbeddedCli {
public:
    /**
     * @param cli          Pointer to an externally-owned cli_t instance.
     * @param tx_start_fn  Called when the TX buffer goes empty→non-empty.
     *                     Pass nullptr for polling mode.
     */
    EmbeddedCli(cli_t *cli, void (*tx_start_fn)(void) = nullptr)
        : _cli(cli)
    {
        cli_init(_cli, tx_start_fn);
    }

    /** Drain the RX buffer and dispatch any complete commands. Non-blocking. */
    void process() { cli_process(_cli); }

    /** Push one received byte (call from RX ISR or DMA callback). */
    void rx_push(uint8_t byte) { cli_rx_push(_cli, byte); }

    /** Pop one byte to transmit (call from TX ISR or DMA callback). */
    int tx_pop(uint8_t *byte) { return cli_tx_pop(_cli, byte); }

    /** Number of bytes waiting in the TX buffer. */
    uint16_t tx_available() { return cli_tx_available(_cli); }

    /** Write a string to the CLI output. */
    void puts(const char *s) { cli_puts(_cli, s); }

    /** Write a single character to the CLI output. */
    void putc(char c) { cli_putc(_cli, c); }

private:
    cli_t *_cli;
};
