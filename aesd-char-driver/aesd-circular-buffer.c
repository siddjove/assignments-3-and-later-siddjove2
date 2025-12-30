/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer implementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.
 * @param char_offset the position to search for in the buffer list
 * @param entry_offset_byte_rtn pointer to store byte offset within entry
 * @return pointer to matching aesd_buffer_entry or NULL
 */
struct aesd_buffer_entry *
aesd_circular_buffer_find_entry_offset_for_fpos(
    struct aesd_circular_buffer *buffer,
    size_t char_offset,
    size_t *entry_offset_byte_rtn)
{
    size_t running_offset = 0;
    uint8_t index = buffer->out_offs;
    uint8_t entries;

    if (buffer->full) {
        entries = AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    } else {
        entries = buffer->in_offs;
    }

    for (uint8_t i = 0; i < entries; i++) {
        struct aesd_buffer_entry *entry = &buffer->entry[index];

        if (char_offset < (running_offset + entry->size)) {
            *entry_offset_byte_rtn = char_offset - running_offset;
            return entry;
        }

        running_offset += entry->size;
        index = (index + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    return NULL;
}

/**
 * Adds entry to circular buffer
 */
void aesd_circular_buffer_add_entry(
    struct aesd_circular_buffer *buffer,
    const struct aesd_buffer_entry *add_entry)
{
    buffer->entry[buffer->in_offs] = *add_entry;

    if (buffer->full) {
        buffer->out_offs =
            (buffer->out_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;
    }

    buffer->in_offs =
        (buffer->in_offs + 1) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED;

    buffer->full = (buffer->in_offs == buffer->out_offs);
}

/**
 * Initializes the circular buffer
 */
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer, 0, sizeof(struct aesd_circular_buffer));
}

