#include "cli.h"
#include <string.h>

/* -----------------------------------------------------------------------
 * Internal ring buffer (see cli_ring_t in cli.h for the SPSC contract)
 * ----------------------------------------------------------------------- */

static uint16_t rb_count(const cli_ring_t* rb) {
	return (uint16_t)((rb->head - rb->tail + rb->size) % rb->size);
}

static uint16_t rb_space(const cli_ring_t* rb) {
	return (uint16_t)(rb->size - 1 - rb_count(rb));
}

/* Returns 1 on success, 0 if the buffer is full (byte is dropped). */
static int rb_push(cli_ring_t* rb, uint8_t byte) {
	if(rb_space(rb) == 0) {
		return 0;
	}
	rb->buf[rb->head] = byte;
	rb->head = (uint16_t)((rb->head + 1) % rb->size);
	return 1;
}

/* Returns 1 and writes *byte on success, 0 if the buffer is empty. */
static int rb_pop(cli_ring_t* rb, uint8_t* byte) {
	if(rb_count(rb) == 0) {
		return 0;
	}
	*byte = rb->buf[rb->tail];
	rb->tail = (uint16_t)((rb->tail + 1) % rb->size);
	return 1;
}

/* Linker-provided bounds for the cli_cmds section.
 * ELF (Linux / embedded): symbols are emitted automatically by the linker.
 * Mach-O (macOS): use the asm label trick to map to the segment/section boundary. */
#ifdef __APPLE__
extern const cli_cmd_t __start_cli_cmds __asm("section$start$__DATA$cli_cmds");
extern const cli_cmd_t __stop_cli_cmds __asm("section$end$__DATA$cli_cmds");
#else
extern const cli_cmd_t __start_cli_cmds;
extern const cli_cmd_t __stop_cli_cmds;
#endif

/* Global instance pointer — set by cli_init(), used by CLI_PUTS/CLI_PUTC */
cli_t* _cli_instance;

/* -----------------------------------------------------------------------
 * Internal TX write — queues one byte, kicks tx_start_fn if needed.
 * ----------------------------------------------------------------------- */

static void tx_push(cli_t* cli, uint8_t byte) {
	int was_empty = (rb_count(&cli->tx) == 0);
	if(rb_push(&cli->tx, byte) && was_empty && cli->tx_start_fn) {
		cli->tx_start_fn();
	}
}

/* -----------------------------------------------------------------------
 * Internal dispatch
 * ----------------------------------------------------------------------- */

static void dispatch(cli_t* cli) {
	if(cli->line_len == 0) {
		return;
	}

	char* argv[CLI_MAX_ARGS];
	int argc = 0;
	char* p = cli->line_buf;

	while(*p && argc < CLI_MAX_ARGS) {
		while(*p == ' ' || *p == '\t') {
			p++;
		}
		if(*p == '\0') { break; }

		argv[argc++] = p;

		while(*p && *p != ' ' && *p != '\t') {
			p++;
		}
		if(*p) { *p++ = '\0'; }
	}

	if(argc == 0) {
		return;
	}

	const cli_cmd_t* cmd = &__start_cli_cmds;
	while(cmd < &__stop_cli_cmds) {
		if(strcmp(cmd->name, argv[0]) == 0) {
			cmd->handler(argc, argv);
			return;
		}
		cmd++;
	}

	cli_puts(cli, "unknown command: ");
	cli_puts(cli, argv[0]);
	cli_puts(cli, "\r\n");
}

// /* -----------------------------------------------------------------------
//  * Built-in: help (names only — help strings are not stored on the device)
//  * ----------------------------------------------------------------------- */

// CLI_CMD(help, "list available commands")
// {
//     (void)argc; (void)argv;

//     const cli_cmd_t *cmd = &__start_cli_cmds;
//     while (cmd < &__stop_cli_cmds) {
//         CLI_PUTS("  ");
//         CLI_PUTS(cmd->name);
//         CLI_PUTS("\r\n");
//         cmd++;
//     }
// }

/* -----------------------------------------------------------------------
 * Public API
 * ----------------------------------------------------------------------- */

void cli_init(cli_t* cli, void (*tx_start_fn)(void)) {
	memset(cli, 0, sizeof(*cli));
	cli->tx_start_fn = tx_start_fn;
	cli->rx.buf = cli->rx_buf;
	cli->rx.size = CLI_RX_BUF_SIZE;
	cli->tx.buf = cli->tx_buf;
	cli->tx.size = CLI_TX_BUF_SIZE;
	_cli_instance = cli;
}

void cli_process(cli_t* cli) {
	uint8_t byte;
	while(rb_pop(&cli->rx, &byte)) {
		char ch = (char)byte;

		if(ch == '\r' || ch == '\n') {
			if(ch == '\n' && cli->line_len == 0) { continue; }

#if CLI_ECHO
			cli_puts(cli, "\r\n");
#endif
			cli->line_buf[cli->line_len] = '\0';
			dispatch(cli);
			cli->line_len = 0;
			memset(cli->line_buf, 0, sizeof(cli->line_buf));

		} else if(ch == '\b' || ch == 0x7F) {
			if(cli->line_len > 0) {
				cli->line_len--;
				cli->line_buf[cli->line_len] = '\0';
#if CLI_ECHO
				cli_puts(cli, "\b \b");
#endif
			}

		} else if(ch >= 0x20 && ch < 0x7F) {
			if(cli->line_len < CLI_MAX_LINE_LEN - 1) {
				cli->line_buf[cli->line_len++] = ch;
#if CLI_ECHO
				tx_push(cli, (uint8_t)ch);
#endif
			}
		}
	}
}

void cli_puts(cli_t* cli, const char* s) {
	while(*s) {
		tx_push(cli, (uint8_t)*s++);
	}
}

void cli_putc(cli_t* cli, char c) {
	tx_push(cli, (uint8_t)c);
}

/* -----------------------------------------------------------------------
 * ISR / DMA API
 * ----------------------------------------------------------------------- */

void cli_rx_push(cli_t* cli, uint8_t byte) {
	if(!rb_push(&cli->rx, byte)) {
		tx_push(cli, '!');
	}
}

int cli_tx_pop(cli_t* cli, uint8_t* byte) {
	return rb_pop(&cli->tx, byte);
}

void cli_cmd_foreach(void (*cb)(const char* name, void* ctx), void* ctx) {
	const cli_cmd_t* cmd = &__start_cli_cmds;
	while(cmd < &__stop_cli_cmds) {
		cb(cmd->name, ctx);
		cmd++;
	}
}

uint16_t cli_tx_available(cli_t* cli) {
	return rb_count(&cli->tx);
}
