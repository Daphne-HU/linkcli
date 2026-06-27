# Generic Embedded CLI

A lightweight, transport-agnostic command-line interface for embedded systems.

- **Zero dynamic allocation** — all buffers are static
- **Transport-agnostic** — plug in your own `putchar`/`getchar` functions
- **Linker-section command registration** — `CLI_CMD()` macro, commands gathered at link time (no central table to maintain)
- **C99 core** with a thin C++ wrapper
- **Configurable** buffer sizes via `#define`

## Quick start

### 1. Add the linker section

In your `.ld` file, inside `SECTIONS { }`:

```ld
.cli_cmds :
{
    __start_cli_cmds = .;
    KEEP(*(.cli_cmds))
    __stop_cli_cmds = .;
} > FLASH
```

See [`docs/linker_snippet.ld`](docs/linker_snippet.ld) for a copy-paste snippet.

### 2. Wire up your UART (or any byte stream)

```c
static void my_putchar(char c) { /* write c to UART TX */ }
static int  my_getchar(void)   { /* return byte from RX, or -1 if empty */ }
```

### 3. Initialise and process

```c
#include "cli.h"

static cli_t g_cli;

int main(void) {
    cli_io_t io = { my_putchar, my_getchar };
    cli_init(&g_cli, io);

    while (1) {
        cli_process(&g_cli);   /* non-blocking, call every loop iteration */
    }
}
```

### 4. Register commands anywhere in your codebase

```c
#include "cli.h"

static void cmd_ping(int argc, char *argv[]) {
    extern cli_t *_cli_instance;
    cli_puts(_cli_instance, "pong\r\n");
}

CLI_CMD(ping, cmd_ping, "reply with pong");
```

The `help` command is built in and lists all registered commands.

## Configuration

Override these in your build system (`-DCLI_MAX_LINE_LEN=128`) or before including `cli.h`:

| Define | Default | Description |
|---|---|---|
| `CLI_MAX_LINE_LEN` | `80` | Input line buffer size (bytes) |
| `CLI_MAX_ARGS` | `8` | Maximum argument count (incl. command name) |
| `CLI_PROMPT` | `"> "` | Prompt string |
| `CLI_ECHO` | `1` | Echo input characters back to terminal |

## C++ usage

```cpp
#include "cli_cpp.hpp"

EmbeddedCli cli(my_putchar, my_getchar);

// in main loop or RTOS task:
cli.process();
```

## Files

```
include/
  cli.h           — public C API
  cli_config.h    — configurable #defines
  cli_cpp.hpp     — C++ wrapper
src/
  cli.c           — implementation
docs/
  linker_snippet.ld
examples/
  example_usage.c
```

## Adding history / tab completion

These are the natural next additions. Open an issue or PR when you're ready.
