#ifndef LOG_H
#define LOG_H

#include <stdint.h>
#include "log_config.h"
#include "ring_buffer.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -----------------------------------------------------------------------
 * Log instance — one per output channel (usually one globally).
 * Declare as a static global next to the peripheral driver that drains it.
 * ----------------------------------------------------------------------- */
typedef struct {
    /* Optional: called once when the TX buffer transitions empty → non-empty.
     * Can be the same function as cli_t's tx_start_fn if they share a peripheral. */
    void (*tx_start_fn)(void);

    ring_buffer_t tx;
    uint8_t       tx_buf[LOG_TX_BUF_SIZE];
} log_t;

/* -----------------------------------------------------------------------
 * Init & ISR API
 * ----------------------------------------------------------------------- */

/** Initialise the log. tx_start_fn may be NULL for polling mode. */
void log_init(log_t *log, void (*tx_start_fn)(void));

/**
 * Pop one byte from the TX buffer.
 * Call from your TX-complete ISR or peripheral callback.
 * Returns 1 if a byte was available, 0 if the buffer is empty.
 */
int log_tx_pop(log_t *log, uint8_t *byte);

/** Number of bytes waiting in the TX buffer. */
uint16_t log_tx_available(log_t *log);

/* -----------------------------------------------------------------------
 * Internal write — do not call directly, use the macros below.
 * ----------------------------------------------------------------------- */
extern log_t *_log_instance;
void _log_puts(log_t *log, const char *s);

/* -----------------------------------------------------------------------
 * LOG_x macros
 *
 * Accept 1–4 strings, same as CLI_PUTS.  Each macro expands to nothing
 * when LOG_LEVEL is below its threshold — zero flash cost, zero runtime.
 *
 *   LOG_INFO("temp = ", temp_str, " C\r\n");
 *   LOG_DEBUG("rx byte: ", hex_str, "\r\n");
 *
 * The \r\n is appended automatically when not provided, but including it
 * explicitly in the last argument gives you full control over line endings.
 * ----------------------------------------------------------------------- */

/* Internal selector — same trick as CLI_PUTS */
#define _LOG_1(i,t,a)         do { _log_puts(i,t); _log_puts(i,a);                                         _log_puts(i,"\r\n"); } while(0)
#define _LOG_2(i,t,a,b)       do { _log_puts(i,t); _log_puts(i,a); _log_puts(i,b);                         _log_puts(i,"\r\n"); } while(0)
#define _LOG_3(i,t,a,b,c)     do { _log_puts(i,t); _log_puts(i,a); _log_puts(i,b); _log_puts(i,c);         _log_puts(i,"\r\n"); } while(0)
#define _LOG_4(i,t,a,b,c,d)   do { _log_puts(i,t); _log_puts(i,a); _log_puts(i,b); _log_puts(i,c); _log_puts(i,d); _log_puts(i,"\r\n"); } while(0)
#define _LOG_SEL(_1,_2,_3,_4,N,...) _LOG_##N
#define _LOG(tag, ...) _LOG_SEL(__VA_ARGS__,4,3,2,1)(_log_instance, tag, __VA_ARGS__)

#if LOG_LEVEL >= 1
#  define LOG_ERROR(...) _LOG("[ERROR] ", __VA_ARGS__)
#else
#  define LOG_ERROR(...) do {} while(0)
#endif

#if LOG_LEVEL >= 2
#  define LOG_WARN(...)  _LOG("[WARN]  ", __VA_ARGS__)
#else
#  define LOG_WARN(...)  do {} while(0)
#endif

#if LOG_LEVEL >= 3
#  define LOG_INFO(...)  _LOG("[INFO]  ", __VA_ARGS__)
#else
#  define LOG_INFO(...)  do {} while(0)
#endif

#if LOG_LEVEL >= 4
#  define LOG_DEBUG(...) _LOG("[DEBUG] ", __VA_ARGS__)
#else
#  define LOG_DEBUG(...) do {} while(0)
#endif

#ifdef __cplusplus
}
#endif

#endif /* LOG_H */
