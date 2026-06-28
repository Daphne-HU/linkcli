#include "cli.h"
#include "uart_stub.h"
#include <stddef.h>
#include <stdlib.h>

/* -----------------------------------------------------------------------
 * Example commands — add yours here or in separate files
 * ----------------------------------------------------------------------- */

CLI_CMD(ping, "check alive") { CLI_PUTS("pong\r\n"); }

CLI_CMD(quit, "exit the simulator") {
  CLI_PUTS("bye\r\n");
  exit(0);
}

CLI_CMD(echo, "echo arguments back") {
  for (int i = 1; i < argc; i++) {
    CLI_PUTS(argv[i], ", ");
    if (i < argc - 1)
      CLI_PUTC(' ');
  }
  CLI_PUTS("\r\n");
}

/* -----------------------------------------------------------------------
 * Sim entry point
 * ----------------------------------------------------------------------- */

static cli_t g_cli;

int main(void) {
  uart_stub_init();
  cli_init(&g_cli, NULL); /* NULL = polling TX, no ISR */

  while (1) {
    /* Feed any pending stdin bytes into the RX buffer */
    int c;
    while ((c = uart_stub_getchar()) != -1) {
      cli_rx_push(&g_cli, (uint8_t)c);
    }

    /* Run the CLI (processes RX buffer, queues responses into TX buffer) */
    cli_process(&g_cli);

    /* Drain the TX buffer to stdout */
    uint8_t byte;
    while (cli_tx_pop(&g_cli, &byte)) {
      uart_stub_putchar((char)byte);
    }
  }
}
