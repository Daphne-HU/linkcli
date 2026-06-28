/*
 * Example: STM32 HAL bare-metal usage.
 *
 * Recommended file layout in your project:
 *
 *   uart.c      — owns g_cli, contains the ISR          (this file)
 *   main.c      — calls cli_init() and cli_process()
 *   commands.c  — CLI_CMD definitions
 *
 * Only uart.c and main.c need to know about g_cli.
 * Command files just include cli.h and use CLI_PUTS/CLI_PUTC.
 */

#include "cli.h"

/* -----------------------------------------------------------------------
 * g_cli lives here, next to the ISR that feeds it.
 * main.c and other files access it via:  extern cli_t g_cli;
 * ----------------------------------------------------------------------- */
cli_t g_cli;

/* -----------------------------------------------------------------------
 * uart_tx_start — called once when the TX buffer goes empty → non-empty.
 * Enable the TXE interrupt so the ISR starts draining the buffer.
 * ----------------------------------------------------------------------- */
static void uart_tx_start(void)
{
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
}

/* -----------------------------------------------------------------------
 * USART2_IRQHandler
 *
 * RX path: push each received byte into the CLI RX buffer.
 * TX path: drain the CLI TX buffer one byte at a time; disable the
 *          interrupt when the buffer empties so uart_tx_start() can
 *          re-enable it on the next write.
 * ----------------------------------------------------------------------- */
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        cli_rx_push(&g_cli, (uint8_t)huart2.Instance->DR);
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) &&
        __HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TXE)) {
        uint8_t byte;
        if (cli_tx_pop(&g_cli, &byte)) {
            huart2.Instance->DR = byte;
        } else {
            __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);
        }
    }
}

/* -----------------------------------------------------------------------
 * Commands — can also live in a separate commands.c file.
 * ----------------------------------------------------------------------- */

CLI_CMD(ping, "reply with pong")
{
    CLI_PUTS("pong\r\n");
}

CLI_CMD(echo, "echo arguments back")
{
    for (int i = 1; i < argc; i++) {
        CLI_PUTS(argv[i]);
        if (i < argc - 1) CLI_PUTC(' ');
    }
    CLI_PUTS("\r\n");
}

/* -----------------------------------------------------------------------
 * main.c (shown here for completeness, normally a separate file)
 * ----------------------------------------------------------------------- */

extern cli_t g_cli;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    cli_init(&g_cli, uart_tx_start);

    while (1) {
        cli_process(&g_cli);
        /* other application tasks */
    }
}
