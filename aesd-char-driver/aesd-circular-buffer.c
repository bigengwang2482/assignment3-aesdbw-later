/**
 * @file aesd-circular-buffer.c
 * @brief Functions and data related to a circular buffer imlementation
 *
 * @author Dan Walkes
 * @date 2020-03-01
 * @copyright Copyright (c) 2020
 *
 */

#ifdef __KERNEL__
#include <linux/string.h>
#else
#include <string.h>
#endif

#include "aesd-circular-buffer.h"

/**
 * @param buffer the buffer to search for corresponding offset.  Any necessary locking must be performed by caller.
 * @param char_offset the position to search for in the buffer list, describing the zero referenced
 *      character index if all buffer strings were concatenated end to end
 * @param entry_offset_byte_rtn is a pointer specifying a location to store the byte of the returned aesd_buffer_entry
 *      buffptr member corresponding to char_offset.  This value is only set when a matching char_offset is found
 *      in aesd_buffer.
 * @return the struct aesd_buffer_entry structure representing the position described by char_offset, or
 * NULL if this position is not available in the buffer (not enough data is written).
 */
struct aesd_buffer_entry *aesd_circular_buffer_find_entry_offset_for_fpos(struct aesd_circular_buffer *buffer,
            size_t char_offset, size_t *entry_offset_byte_rtn )
{
    /**
    * TODO: implement per description
    */
	size_t entry_floor_byte_i = 0;
	size_t entry_ceiling_byte_i = 0;
	size_t i_entry;
	for (int i_entry_since_out = 0; i_entry_since_out <  AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i_entry_since_out++) {
		i_entry = (i_entry_since_out + buffer->out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; // The offset is from the out offset.
		entry_ceiling_byte_i += buffer->entry[i_entry].size; // get the ceiling for this entry
		if (entry_ceiling_byte_i > char_offset) {
			*entry_offset_byte_rtn = char_offset - entry_floor_byte_i;
			return &buffer->entry[i_entry];
		}	
		entry_floor_byte_i = entry_ceiling_byte_i; //update the floor for the next round
	}
	// if not found above, it will proceed here and return NULL
    return NULL;
}

/**
* Adds entry @param add_entry to @param buffer in the location specified in buffer->in_offs.
* If the buffer was already full, overwrites the oldest entry and advances buffer->out_offs to the
* new start location.
* Any necessary locking must be handled by the caller
* Any memory referenced in @param add_entry must be allocated by and/or must have a lifetime managed by the caller.
*/
void aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
	
	buffer->entry[buffer->in_offs] = *add_entry; // write or overwrites the oldest entry
	// check if the current in_offs is the last one
	if (buffer->in_offs == (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1)) {
		buffer->full = true;
		buffer->in_offs = 0;
	} 
	else {
		buffer->in_offs += 1;
	}
	if (buffer->full == true) { // Buffer already full, advances buffer->out_offs to the new start location
		buffer->out_offs = buffer->in_offs;
	}	
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
