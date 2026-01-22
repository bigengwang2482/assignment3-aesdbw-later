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
* @return NULL or, if an existing entry at out_offs was repalced,
*         the value of buffptr for the entry which was replaced (for use with dynamic memory allocation/free 
*/
const char *aesd_circular_buffer_add_entry(struct aesd_circular_buffer *buffer, const struct aesd_buffer_entry *add_entry)
{
    /**
    * TODO: implement per description
    */
	struct aesd_buffer_entry *entry_backup; 
	if (buffer->full == true) { // if the buffer is full before the actual write, take a backup of the overiten entry buffer.
		entry_backup = &buffer->entry[buffer->in_offs];	
	}
	else {
		entry_backup = NULL;
	}
	// Add the new entry
	buffer->entry[buffer->in_offs] = *add_entry; // write or overwrites the oldest entry
	// check if the current in_offs is the last one
	if (buffer->in_offs == (AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED-1)) {
		printk("Reached the full buffer state, roll the next entry to be the 0th entry.");
		buffer->full = true;
		buffer->in_offs = 0;
	} 
	else {
		printk("Move to the next entry for future write.");
		buffer->in_offs += 1;
	}
	if (buffer->full == true) { // Buffer already full, advances buffer->out_offs to the new start location
		printk("Since buffer is full, need to update the read entry to the next write entry.");
		buffer->out_offs = buffer->in_offs;	
	}	

	if (entry_backup != NULL) {
		printk("Returned the overwritten entry's buffer for dynamic removal.");
		return entry_backup->buffptr; // return this entry to be replace for dynamic removal
	}
	else {
		printk("Not yet overitten any previous entry. Return NULL.");
		return NULL;	
	}	
	return NULL; // By default
}

/**
* Initializes the circular buffer described by @param buffer to an empty struct
*/
void aesd_circular_buffer_init(struct aesd_circular_buffer *buffer)
{
    memset(buffer,0,sizeof(struct aesd_circular_buffer));
}
