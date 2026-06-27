#include "ring_buffer.h"

uint16_t rb_count(const ring_buffer_t *rb)
{
    return (uint16_t)((rb->head - rb->tail + rb->size) % rb->size);
}

uint16_t rb_space(const ring_buffer_t *rb)
{
    return (uint16_t)(rb->size - 1 - rb_count(rb));
}

int rb_push(ring_buffer_t *rb, uint8_t byte)
{
    if (rb_space(rb) == 0) {
        return 0;
    }
    rb->buf[rb->head] = byte;
    rb->head = (uint16_t)((rb->head + 1) % rb->size);
    return 1;
}

int rb_pop(ring_buffer_t *rb, uint8_t *byte)
{
    if (rb_count(rb) == 0) {
        return 0;
    }
    *byte = rb->buf[rb->tail];
    rb->tail = (uint16_t)((rb->tail + 1) % rb->size);
    return 1;
}

int rb_peek(const ring_buffer_t *rb, uint8_t *byte)
{
    if (rb_count(rb) == 0) {
        return 0;
    }
    *byte = rb->buf[rb->tail];
    return 1;
}

void rb_clear(ring_buffer_t *rb)
{
    rb->head = 0;
    rb->tail = 0;
}
