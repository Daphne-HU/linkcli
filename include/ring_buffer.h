#ifndef RING_BUFFER_H
#define RING_BUFFER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Generic byte ring buffer — single-producer / single-consumer.
 *
 * Safe to use with one writer in an ISR and one reader in the main loop
 * (or vice versa) without a critical section, provided head and tail are
 * naturally atomic on the target (true for 16-bit aligned stores on
 * Cortex-M and most other embedded cores).
 *
 * Usage:
 *   Define a buffer with RB_DEF, then pass its address to the rb_* API.
 *
 *   RB_DEF(sensor_rx, 64);           // declares sensor_rx_buf + sensor_rx_rb
 *   rb_push(&sensor_rx_rb, byte);   // from ISR or peripheral callback
 *   rb_pop(&sensor_rx_rb, &byte);   // from main loop
 */

/* -----------------------------------------------------------------------
 * Descriptor — embed this in your struct or declare with RB_DEF.
 * buf and size are set once at init and never change.
 * ----------------------------------------------------------------------- */
typedef struct {
    uint8_t          *buf;
    uint16_t          size;   /* must be >= 2 */
    volatile uint16_t head;   /* writer advances head */
    volatile uint16_t tail;   /* reader advances tail */
} ring_buffer_t;

/* -----------------------------------------------------------------------
 * RB_DEF(name, capacity)
 *
 * Declares a backing array, a ring_buffer_t, and initialises the
 * descriptor — all at file scope, zero dynamic allocation.
 *
 * Example:
 *   RB_DEF(sensor_rx, 64);
 *   // creates: uint8_t sensor_rx_buf[64];
 *   //          ring_buffer_t sensor_rx_rb = { sensor_rx_buf, 64, 0, 0 };
 * ----------------------------------------------------------------------- */
#define RB_DEF(name, capacity)                          \
    static uint8_t     name##_buf[capacity];            \
    static ring_buffer_t name##_rb = { name##_buf, (capacity), 0, 0 }

/* -----------------------------------------------------------------------
 * API
 * ----------------------------------------------------------------------- */

/** Number of bytes currently stored. */
uint16_t rb_count(const ring_buffer_t *rb);

/** Number of bytes that can still be written. */
uint16_t rb_space(const ring_buffer_t *rb);

/**
 * Push one byte.
 * Returns 1 on success, 0 if the buffer is full (byte is dropped).
 */
int rb_push(ring_buffer_t *rb, uint8_t byte);

/**
 * Pop one byte.
 * Returns 1 and writes *byte on success, 0 if the buffer is empty.
 */
int rb_pop(ring_buffer_t *rb, uint8_t *byte);

/**
 * Peek at the next byte without consuming it.
 * Returns 1 and writes *byte on success, 0 if the buffer is empty.
 */
int rb_peek(const ring_buffer_t *rb, uint8_t *byte);

/** Discard all contents. */
void rb_clear(ring_buffer_t *rb);

#ifdef __cplusplus
}
#endif

#endif /* RING_BUFFER_H */
