# Generic Embedded CLI

A lightweight, transport-agnostic command-line interface for embedded systems.

- **Zero dynamic allocation** — all buffers are static
- **Transport-agnostic** — plug in your own ISR callbacks, no UART driver included
- **Linker-section command registration** — `CLI_CMD()` macro, commands gathered at link time (no central table to maintain)
- **Decoupled RX/TX ring buffers** — UART ISR and CLI logic never block each other
- **C99 core** with a thin C++ wrapper
- **Configurable** buffer sizes via `#define`

## Memory footprint

Measured at `-Os` with default config. Thumb2 (Cortex-M) will be ~20–30% smaller than the host figures below.

| | Size |
|---|---|
| **Flash** (code, host estimate) | ~2 KB |
| **RAM** (`cli_t` with defaults) | 320 bytes |

`cli_t` RAM breakdown:

| Field | Default | Size |
|---|---|---|
| RX ring buffer | `CLI_RX_BUF_SIZE 64` | 64 B |
| TX ring buffer | `CLI_TX_BUF_SIZE 128` | 128 B |
| Line assembly buffer | `CLI_MAX_LINE_LEN 80` | 80 B |
| Ring buffer headers + overhead | — | 48 B |

Minimum config for a very tight MCU (`-DCLI_RX_BUF_SIZE=32 -DCLI_TX_BUF_SIZE=64 -DCLI_MAX_LINE_LEN=48`): **~160 bytes RAM**.

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

> On macOS/Linux (host builds) no linker script change is needed — the section is handled automatically.

### 2. Wire up your UART ISR

```c
// RX interrupt — push each received byte in
void USART2_IRQHandler(void) {
    if (/* RXNE flag */)
        cli_rx_push(&g_cli, (uint8_t)huart2.Instance->DR);

    // TX interrupt — drain the TX buffer
    if (/* TXE flag */) {
        uint8_t byte;
        if (cli_tx_pop(&g_cli, &byte))
            huart2.Instance->DR = byte;
        else
            /* disable TXE interrupt */;
    }
}

// Called once when the TX buffer goes empty → non-empty (kick DMA / enable TXE IRQ)
static void uart_tx_start(void) {
    __HAL_UART_ENABLE_IT(&huart2, UART_IT_TXE);
}
```

### 3. Initialise and process

```c
#include "cli.h"

cli_t g_cli;   // own this in the file that has your UART ISR

int main(void) {
    cli_init(&g_cli, uart_tx_start);   // NULL = polling TX instead of IRQ-driven

    while (1) {
        cli_process(&g_cli);   // non-blocking, drains RX buffer and dispatches commands
    }
}
```

### 4. Register commands anywhere in your codebase

```c
#include "cli.h"

CLI_CMD(ping, "reply with pong") {
    CLI_PUTS("pong\r\n");
}

CLI_CMD(echo, "echo arguments back") {
    for (int i = 1; i < argc; i++) {
        CLI_PUTS(argv[i], " ");
    }
    CLI_PUTS("\r\n");
}
```

`argc` and `argv[]` are in scope inside the block. `CLI_PUTS` accepts 1–4 strings and expands to individual calls at compile time — zero runtime overhead. The built-in `help` command lists all registered commands automatically.

## Configuration

Override in your build system (`-DCLI_MAX_LINE_LEN=128`) or before including `cli.h`:

| Define | Default | Description |
|---|---|---|
| `CLI_RX_BUF_SIZE` | `64` | RX ring buffer size (bytes) |
| `CLI_TX_BUF_SIZE` | `128` | TX ring buffer size (bytes) |
| `CLI_MAX_LINE_LEN` | `80` | Input line assembly buffer (bytes) |
| `CLI_MAX_ARGS` | `8` | Maximum argument count (incl. command name) |
| `CLI_PROMPT` | `"> "` | Prompt string |
| `CLI_ECHO` | `1` | Echo input characters back to terminal |

## ISR / DMA API

| Function | Call from | Description |
|---|---|---|
| `cli_rx_push(cli, byte)` | RX ISR | Push one received byte. Sends `!` to TX if RX buffer full. |
| `cli_tx_pop(cli, &byte)` | TX ISR | Pop one byte to transmit. Returns 0 when buffer empty. |
| `cli_tx_available(cli)` | anywhere | Number of bytes waiting in the TX buffer. |

## C++ usage

```cpp
// app.cpp
#include "cli_cpp.hpp"

extern "C" cli_t g_cli;   // owned in uart.c, next to the ISR

EmbeddedCli cli(&g_cli, uart_tx_start);

// in main loop or RTOS task:
cli.process();
```

The ISR stays in C and calls `cli_rx_push` / `cli_tx_pop` directly — it never needs to see the C++ class.

## Integrating into your project

The recommended way is as a **git submodule** so you can pull updates deliberately:

```bash
git submodule add <this repo url> lib/embedded_cli
```

Then in your `CMakeLists.txt`:

```cmake
add_subdirectory(lib/embedded_cli)
target_link_libraries(my_app PRIVATE embedded_cli)
```

That single `target_link_libraries` line adds the include path and compiles both source files into your target. Override buffer sizes without touching this repo:

```cmake
target_compile_definitions(embedded_cli PUBLIC
    CLI_TX_BUF_SIZE=256
    CLI_RX_BUF_SIZE=32
)
```

Clone a project that uses it:

```bash
git clone --recurse-submodules <your project>
```

## Host simulator

A PC-side simulator stubs the MCU so you can develop and test commands without hardware:

```bash
cd sim && make
python server.py          # requires: pip install websockets
```

Then open **http://localhost:8080** — a web UI shows all registered commands in a sidebar and connects to the simulator over WebSocket. The UI is completely separate from the embedded code and does not affect the firmware binary.

## Project layout

```
include/
  cli.h             — public C API + CLI_CMD() macro
  cli_config.h      — configurable #defines
  cli_cpp.hpp       — C++ wrapper
  ring_buffer.h     — generic byte ring buffer (reusable)
src/
  cli.c             — CLI implementation
  ring_buffer.c     — ring buffer implementation
sim/
  main.c            — host simulator (add your commands here)
  uart_stub.c/h     — raw-mode stdin/stdout as a UART stand-in
  Makefile
ui/
  server.py         — spawns cli_sim, bridges stdio over WebSocket, serves UI
  index.html        — web debug interface
docs/
  linker_snippet.ld — copy-paste section stanza for your linker script
examples/
  example_usage.c
```

## Adding history / tab completion

These are the natural next additions. The hook is already in place — `cli_process` currently ignores escape sequences, so it is a clean extension.
