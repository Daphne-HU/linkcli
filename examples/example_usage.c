/*
 * Example: bare-metal usage with RX/TX ring buffers.
 *
 * The UART ISR and the CLI logic are fully decoupled:
 *   - RX ISR  → cli_rx_push()     (feed bytes in)
 *   - TX ISR  ← cli_tx_pop()      (drain bytes out)
 *   - Main loop → cli_process()   (parse lines, dispatch commands)
 */

#include "cli.h"

static cli_t g_cli;

/* -----------------------------------------------------------------------
 * Called once when the TX buffer goes from empty → non-empty.
 * Enable the TXE interrupt (or kick a DMA transfer) here so the ISR
 * starts draining the buffer.
 * ----------------------------------------------------------------------- */
static void uart_tx_start(void)
{
    /* e.g. __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE); */
}

/* -----------------------------------------------------------------------
 * UART RX interrupt — push each received byte into the CLI RX buffer.
 * ----------------------------------------------------------------------- */
void USART2_IRQHandler(void)
{
    /* RX: byte received */
    /* if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) { */
    /*     uint8_t byte = (uint8_t)huart2.Instance->DR;   */
    /*     cli_rx_push(&g_cli, byte);                     */
    /* }                                                  */

    /* TX: register empty — send next byte or disable the interrupt */
    /* if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE)) {            */
    /*     uint8_t byte;                                             */
    /*     if (cli_tx_pop(&g_cli, &byte)) {                         */
    /*         huart2.Instance->DR = byte;                          */
    /*     } else {                                                  */
    /*         __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE);         */
    /*     }                                                        */
    /* }                                                            */
}

/* -----------------------------------------------------------------------
 * Commands
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
 * Main loop
 * ----------------------------------------------------------------------- */

int main(void)
{
    /* hardware_init(); */

    cli_init(&g_cli, uart_tx_start);

    while (1) {
        cli_process(&g_cli);   /* drains RX buffer, dispatches commands */
        /* other_task(); */
    }
}
