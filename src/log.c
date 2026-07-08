#include "log.h"
#include "ring_buffer.h"
#include <string.h>

log_t *_log_instance;

static void tx_push(log_t *log, uint8_t byte)
{
    int was_empty = (rb_count(&log->tx) == 0);
    if (rb_push(&log->tx, byte) && was_empty && log->tx_start_fn) {
        log->tx_start_fn();
    }
}

void _log_puts(log_t *log, const char *s)
{
    while (*s) {
        tx_push(log, (uint8_t)*s++);
    }
}

void log_init(log_t *log, void (*tx_start_fn)(void))
{
    memset(log, 0, sizeof(*log));
    log->tx_start_fn = tx_start_fn;
    log->tx.buf      = log->tx_buf;
    log->tx.size     = LOG_TX_BUF_SIZE;
    _log_instance    = log;
}

int log_tx_pop(log_t *log, uint8_t *byte)
{
    return rb_pop(&log->tx, byte);
}

uint16_t log_tx_available(log_t *log)
{
    return rb_count(&log->tx);
}
