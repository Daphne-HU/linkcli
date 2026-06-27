#ifndef CLI_H
#define CLI_H

#include <stdint.h>
#include "cli_config.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Command descriptor — do not fill this manually, use CLI_CMD().
 * ----------------------------------------------------------------------- */
typedef struct {
    const char *name;
    void (*handler)(int argc, char *argv[]);
    const char *help;
} cli_cmd_t;

/* -----------------------------------------------------------------------
 * CLI instance — declare as a static global, then call cli_init().
 * ----------------------------------------------------------------------- */
typedef struct {
    /* Optional: called once when the TX buffer transitions empty → non-empty.
     * Use this to kick off a DMA transfer or enable the TXE interrupt.
     * Set to NULL if you poll cli_tx_pop() instead.                       */
    void (*tx_start_fn)(void);

    ring_buffer_t rx;
    ring_buffer_t tx;
    uint8_t       rx_buf[CLI_RX_BUF_SIZE];
    uint8_t       tx_buf[CLI_TX_BUF_SIZE];

    char     line_buf[CLI_MAX_LINE_LEN];
    uint16_t line_len;
} cli_t;

/* -----------------------------------------------------------------------
 * CLI_CMD — declare and register a command in one block.
 *
 *   CLI_CMD(ping, "reply with pong") {
 *       CLI_PUTS("pong\r\n");
 *   }
 *
 * argc and argv[] are in scope inside the block.
 * CLI_PUTS / CLI_PUTC write to the active CLI instance.
 *
 * On embedded (ELF) add the .cli_cmds stanza from docs/linker_snippet.ld.
 * On macOS/Linux the section is handled automatically — no linker script needed.
 * ----------------------------------------------------------------------- */

/* Section name differs between Mach-O (macOS) and ELF (Linux / embedded) */
#ifdef __APPLE__
#  define _CLI_CMD_SECTION  "__DATA,cli_cmds"
#else
#  define _CLI_CMD_SECTION  ".cli_cmds"
#endif

#define CLI_CMD(cmd_name, help_str)                                      \
    static void _cli_handler_##cmd_name(int argc, char *argv[]);        \
    static const cli_cmd_t _cli_cmd_##cmd_name                          \
    __attribute__((section(_CLI_CMD_SECTION), used)) =                  \
    { #cmd_name, _cli_handler_##cmd_name, help_str };                   \
    static void _cli_handler_##cmd_name(                                \
        int argc __attribute__((unused)),                               \
        char *argv[] __attribute__((unused)))

/* -----------------------------------------------------------------------
 * CLI_PUTS / CLI_PUTC — output helpers for use inside CLI_CMD blocks.
 * ----------------------------------------------------------------------- */
extern cli_t *_cli_instance;

/* CLI_PUTS accepts 1–4 strings and expands each to its own cli_puts() call.
 * Zero runtime cost — resolved entirely by the preprocessor. */
#define _CLI_PUTS_1(i, a)          cli_puts(i, a)
#define _CLI_PUTS_2(i, a, b)       cli_puts(i, a); cli_puts(i, b)
#define _CLI_PUTS_3(i, a, b, c)    cli_puts(i, a); cli_puts(i, b); cli_puts(i, c)
#define _CLI_PUTS_4(i, a, b, c, d) cli_puts(i, a); cli_puts(i, b); cli_puts(i, c); cli_puts(i, d)
#define _CLI_PUTS_SEL(_1,_2,_3,_4, N, ...) _CLI_PUTS_##N
#define CLI_PUTS(...) _CLI_PUTS_SEL(__VA_ARGS__, 4, 3, 2, 1)(_cli_instance, __VA_ARGS__)

#define CLI_PUTC(c)  cli_putc(_cli_instance, (c))

/* -----------------------------------------------------------------------
 * Init & main-loop API
 * ----------------------------------------------------------------------- */

/**
 * Initialise the CLI and queue the first prompt into the TX buffer.
 * tx_start_fn may be NULL (polling mode).
 */
void cli_init(cli_t *cli, void (*tx_start_fn)(void));

/**
 * Drain the RX buffer and process any complete lines.
 * Call from your main loop or RTOS task — non-blocking.
 */
void cli_process(cli_t *cli);

/** Queue a null-terminated string into the TX buffer. */
void cli_puts(cli_t *cli, const char *s);

/** Queue a single character into the TX buffer. */
void cli_putc(cli_t *cli, char c);

/* -----------------------------------------------------------------------
 * ISR / DMA API
 * ----------------------------------------------------------------------- */

/**
 * Push one received byte into the RX buffer.
 * Safe to call from a UART RX ISR or DMA callback.
 * Sends '!' into the TX buffer if the RX buffer is full.
 */
void cli_rx_push(cli_t *cli, uint8_t byte);

/**
 * Pop one byte from the TX buffer.
 * Safe to call from a UART TX-complete ISR or DMA callback.
 * Returns 1 and writes *byte if data is available, 0 if the buffer is empty.
 */
int cli_tx_pop(cli_t *cli, uint8_t *byte);

/**
 * Return the number of bytes currently waiting in the TX buffer.
 * Useful for polling-mode drain loops.
 */
uint16_t cli_tx_available(cli_t *cli);

#ifdef __cplusplus
}
#endif

#endif /* CLI_H */
