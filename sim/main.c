#include "cli.h"
#include "log.h"
#include "uart_stub.h"
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* -----------------------------------------------------------------------
 * Commands
 * ----------------------------------------------------------------------- */

CLI_CMD(ping, "check alive") { CLI_PUTS("pong\r\n"); }

CLI_CMD(quit, "exit the simulator") { CLI_PUTS("bye\r\n"); exit(0); }

CLI_CMD(echo, "echo arguments back") {
    for (int i = 1; i < argc; i++) {
        CLI_PUTS(argv[i]);
        if (i < argc - 1) CLI_PUTC(' ');
    }
    CLI_PUTS("\r\n");
}

CLI_CMD(log_test, "emit one log line at each level") {
    LOG_ERROR("this is an error\r\n");
    LOG_WARN("this is a warning\r\n");
    LOG_INFO("this is info\r\n");
    LOG_DEBUG("this is debug\r\n");
}

/* -----------------------------------------------------------------------
 * Sim entry point
 * ----------------------------------------------------------------------- */

static cli_t g_cli;
static log_t g_log;

static void drain_on_exit(void)
{
    uint8_t byte;
    while (cli_tx_pop(&g_cli, &byte)) uart_stub_putchar((char)byte);
    while (log_tx_pop(&g_log, &byte)) uart_stub_putchar((char)byte);
}

static void print_cmd_json(const char *name, const char *help, void *ctx)
{
    int *first = (int *)ctx;
    if (!*first) printf(",");
    printf("{\"name\":\"%s\",\"help\":\"%s\"}", name, help);
    *first = 0;
}

int main(int argc, char *argv[]) {
    if (argc > 1 && strcmp(argv[1], "--list-cmds") == 0) {
        int first = 1;
        printf("[");
        cli_cmd_foreach(print_cmd_json, &first);
        printf("]\n");
        return 0;
    }

    uart_stub_init();
    cli_init(&g_cli, NULL);
    log_init(&g_log, NULL);
    atexit(drain_on_exit);

    LOG_INFO("sim started — type 'help' for commands\r\n");

    while (1) {
        /* Feed stdin into the CLI RX buffer */
        int c;
        while ((c = uart_stub_getchar()) != -1) {
            cli_rx_push(&g_cli, (uint8_t)c);
        }

        /* Process commands */
        cli_process(&g_cli);

        /* Drain CLI TX then log TX to stdout */
        uint8_t byte;
        while (cli_tx_pop(&g_cli, &byte)) { uart_stub_putchar((char)byte); }
        while (log_tx_pop(&g_log, &byte)) { uart_stub_putchar((char)byte); }
    }
}
