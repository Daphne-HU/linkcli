# Generic Embedded CLI

A lightweight, transport-agnostic command-line interface for embedded systems.

- **Zero dynamic allocation** — all buffers are static
- **Transport-agnostic** — plug in your own ISR callbacks, no UART driver included
- **Linker-section command registration** — `CLI_CMD()` macro, commands gathered at link time (no central table to maintain)
- **Decoupled RX/TX ring buffers** — peripheral callbacks and CLI logic never block each other
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

### 2. Wire up your peripheral

The CLI is peripheral-agnostic — it only needs two things from you:

- **RX**: call `cli_rx_push()` whenever a byte arrives (ISR, DMA callback, polling loop — anything)
- **TX**: call `cli_tx_pop()` to drain bytes out when the peripheral is ready to send

```c
// Called once when the TX buffer goes empty → non-empty.
// Kick whatever peripheral you use (UART, SPI, I2C, USB CDC, ...).
static void my_tx_start(void) {
    /* e.g. enable TX interrupt, start DMA, etc. */
}

// RX callback — push each received byte in
void on_byte_received(uint8_t byte) {
    cli_rx_push(&g_cli, byte);
}

// TX callback — drain one byte at a time when the peripheral is ready
void on_tx_ready(void) {
    uint8_t byte;
    if (cli_tx_pop(&g_cli, &byte)) {
        /* send byte on your peripheral */
    } else {
        /* no more data — stop TX (disable interrupt, stop DMA, etc.) */
    }
}
```

### 3. Initialise and process

```c
#include "cli.h"

cli_t g_cli;   // own this in the file that has your peripheral driver

int main(void) {
    cli_init(&g_cli, my_tx_start);   // NULL = polling TX instead of callback-driven

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
| `CLI_ECHO` | `1` | Echo input characters back to terminal |

## Peripheral API

The library has no knowledge of any specific peripheral. You wire these functions to whatever transport you use — UART, SPI, I2C, USB CDC, a socket, anything that moves bytes.

| Function | Call from | Description |
|---|---|---|
| `cli_rx_push(cli, byte)` | RX callback | Push one received byte. Sends `!` to TX if RX buffer full. |
| `cli_tx_pop(cli, &byte)` | TX callback | Pop one byte to transmit. Returns 0 when buffer empty. |
| `cli_tx_available(cli)` | anywhere | Number of bytes waiting in the TX buffer. |

CLI and logging can run on **different peripherals** — each has its own independent TX buffer and `tx_start_fn`:

```c
cli_init(&g_cli, i2c_tx_start);    // CLI over I2C
log_init(&g_log, uart_tx_start);   // logging over UART
```

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

## Logging

The logger has its own TX ring buffer, independent from the CLI — they never block each other and can run on the same or different peripherals. CLI and logging are fully independent: each can be included, excluded, or configured separately.

### Setup

```c
#include "log.h"

log_t g_log;   // next to the peripheral driver that drains it

// in main:
log_init(&g_log, my_tx_start);
```

Drain the log TX buffer in your TX callback alongside the CLI (if they share the same peripheral):

```c
void on_tx_ready(void) {
    uint8_t byte;
    // drain CLI first, then log — or reverse the order for your priority needs
    if      (cli_tx_pop(&g_cli, &byte)) { /* send byte */ }
    else if (log_tx_pop(&g_log, &byte)) { /* send byte */ }
    else    { /* both empty — stop TX */ }
}
```

Or route them to completely separate peripherals:

```c
cli_init(&g_cli, i2c_tx_start);    // CLI over I2C
log_init(&g_log, uart_tx_start);   // logging over UART
```

### Usage

```c
#include "log.h"

LOG_ERROR("init failed\r\n");
LOG_WARN("retry ", count_str, "\r\n");
LOG_INFO("system ready\r\n");
LOG_DEBUG("temp = ", temp_str, " C\r\n");
```

Each macro accepts 1–4 strings, expanded to individual writes at compile time — same as `CLI_PUTS`, zero overhead.

### Log levels

Set at build time. Everything above the threshold compiles out completely — no string in flash, no function call:

```cmake
target_compile_definitions(embedded_cli PUBLIC LOG_LEVEL=4)  # DEBUG and below
target_compile_definitions(embedded_cli PUBLIC LOG_LEVEL=0)  # nothing
```

| `LOG_LEVEL` | Compiled in |
|---|---|
| `0` | nothing |
| `1` | `LOG_ERROR` |
| `2` | + `LOG_WARN` |
| `3` | + `LOG_INFO` (default) |
| `4` | + `LOG_DEBUG` |

### Log TX buffer

```cmake
target_compile_definitions(embedded_cli PUBLIC LOG_TX_BUF_SIZE=512)
```

Default is 256 bytes. Size this based on how much log output you produce between ISR drain cycles.

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
