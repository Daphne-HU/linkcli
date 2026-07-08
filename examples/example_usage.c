/*
 * Example: STM32 HAL bare-metal usage — CLI + logging on a single UART.
 *
 * Recommended file layout in your project:
 *
 *   uart.c      — owns g_cli and g_log, contains the ISR   (this file)
 *   main.c      — calls cli_init(), log_init(), cli_process()
 *   commands.c  — CLI_CMD definitions
 *   sensors.c   — application code that uses LOG_x()
 *
 * The ISR drains the CLI TX buffer first, then the log TX buffer.
 * Both share the same UART but each has its own ring buffer so they
 * never block each other.
 */

#include "cli.h"
#include "log.h"

/* -----------------------------------------------------------------------
 * Both instances live here, next to the ISR.
 * Other files access them via:  extern cli_t g_cli; / extern log_t g_log;
 * ----------------------------------------------------------------------- */
cli_t g_cli;
log_t g_log;

/* -----------------------------------------------------------------------
 * uart_tx_start — shared kick function.
 * Called by both cli_t and log_t when their TX buffer goes empty → non-empty.
 * ----------------------------------------------------------------------- */
static void uart_tx_start(void)
{
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
}

/* -----------------------------------------------------------------------
 * USART2_IRQHandler
 *
 * RX  → cli_rx_push()
 * TX  → drain CLI buffer first, then log buffer.
 *       Disable TXE interrupt when both are empty.
 * ----------------------------------------------------------------------- */
void USART2_IRQHandler(void)
{
    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_RXNE)) {
        cli_rx_push(&g_cli, (uint8_t)huart2.Instance->DR);
    }

    if (__HAL_UART_GET_FLAG(&huart2, UART_FLAG_TXE) &&
        __HAL_UART_GET_IT_SOURCE(&huart2, UART_IT_TXE)) {
        uint8_t byte;
        if      (cli_tx_pop(&g_cli, &byte)) { huart2.Instance->DR = byte; }
        else if (log_tx_pop(&g_log, &byte)) { huart2.Instance->DR = byte; }
        else    { __HAL_UART_DISABLE_IT(&huart2, UART_IT_TXE); }
    }
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
 * main.c (shown here for completeness, normally a separate file)
 * ----------------------------------------------------------------------- */

extern cli_t g_cli;
extern log_t g_log;

int main(void)
{
    HAL_Init();
    SystemClock_Config();
    MX_USART2_UART_Init();

    cli_init(&g_cli, uart_tx_start);
    log_init(&g_log, uart_tx_start);

    LOG_INFO("system ready\r\n");

    while (1) {
        cli_process(&g_cli);
        /* other application tasks */
    }
}

/* -----------------------------------------------------------------------
 * sensors.c — application code using the logger (normally a separate file)
 *
 * No need to know about g_log — LOG_x() uses _log_instance set at log_init().
 * No need to know about g_cli — CLI_PUTS() uses _cli_instance set at cli_init().
 * ----------------------------------------------------------------------- */

/* #include "log.h" */

/* void sensors_update(void) { */
/*     LOG_DEBUG("temp = ", temp_str, " C\r\n"); */
/*     LOG_WARN("sensor timeout\r\n");           */
/* }                                             */
