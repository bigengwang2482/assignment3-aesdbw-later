/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include "aesdchar.h"
#include "aesd_ioctl.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;
loff_t total_circular_buffer_size = 0; // A global variable to store the total circular buffer size
MODULE_AUTHOR("Bigeng Wang"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

loff_t aesd_llseek(struct file *filp, loff_t off, int whence)
{
	//TODO: UPDATE THE DRAFT BELOW	
	ssize_t retval = 0;
	loff_t newpos;
    PDEBUG("llseek: filp->f_pos:%lld, offset: %lld, whence: %d",filp->f_pos, off, whence);
 	// start locking 
	struct aesd_dev *dev = filp->private_data; 	
	if (mutex_lock_interruptible(&dev->lock)) // Get the mutex for protection
		return -ERESTARTSYS;
	PDEBUG("Obtained mutex for read.");	
	PDEBUG("Update the total circular buffer size in mutex, from %lld", total_circular_buffer_size);
	uint8_t index;	
 	struct aesd_buffer_entry *free_entry;
	total_circular_buffer_size = 0; //reset the total size value
 	AESD_CIRCULAR_BUFFER_FOREACH(free_entry,dev->buf,index) {	
		if (free_entry->buffptr != NULL) { // only free when the entry's data buffer kmalloced
					total_circular_buffer_size += free_entry->size;
			}
	}
	PDEBUG("Update the total circular buffer size in mutex to be %lld.", total_circular_buffer_size);	
	newpos = fixed_size_llseek(filp, off, whence, total_circular_buffer_size); //Use the total circular buffer size as the 'fixed' size
	PDEBUG("fixed_size_llseek: found newpos at %lld.", newpos);	
	if (newpos < 0) {
		retval = -EINVAL;
		goto out;
	} else {
		filp->f_pos = newpos; //Update the f_pos from the original filepointer
		retval = newpos;
		goto out;
	}	

	out:
		mutex_unlock(&dev->lock);
		return retval;

	return retval;
	//END OF THE IMPLEMENTATION DRAFT
}

int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    /**
     * TODO: handle open
     */
	// Start of the assignment TODO code
	struct aesd_dev *dev; /* device information */
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev); /* use inode i_cdev to find the start and check for pointer to cdev in the aesd_dev */
	filp->private_data = dev; /* that's it for assign8 */
	// End of the assignment TODO code
    return 0;
}

int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    /**
     * TODO: handle release
     */
    return 0;
}

ssize_t aesd_read(struct file *filp, char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle read
     */
	// Start of the assignment TODO code
	struct aesd_dev *dev = filp->private_data; 	
	if (mutex_lock_interruptible(&dev->lock)) // Get the mutex for protection
		return -ERESTARTSYS;
	PDEBUG("Obtained mutex for read.");
	// DO ACTUAL READING HERE, WITH THE PARTIAL READ(NOT FULL COUNTS)
	size_t offset_rtn=0;
	PDEBUG("Find reading entry before reading from buffer...");
	struct aesd_buffer_entry *read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buf, *f_pos, &offset_rtn); 
	PDEBUG("Get offset_rtn %lld.", offset_rtn);
	if (read_entry == NULL) {
		PDEBUG("Unable to find the entry with offset %lld, end of file(no data read)", *f_pos);
		retval = 0;
		goto out;
	}
	else{
		retval = read_entry->size - offset_rtn;
		PDEBUG("Found the entry with offset %lld and copy to user with size %lld.", *f_pos, retval);
		if (copy_to_user(buf, read_entry->buffptr+offset_rtn, retval)) {	
			PDEBUG("Copy to user failed.");
			retval = -EFAULT;			
			goto out;
		}	
		else {
			PDEBUG("copy_to_user successfully.");		
			*f_pos += retval;// updated the f_pos to the begining of next entry
			PDEBUG("Update the next entry with offset(*f_pos +=) %lld after read in %lld in", *f_pos, retval);
			goto out;
		}
	}
	out:
		mutex_unlock(&dev->lock);
		return retval;
	// End of the assignment TODO code
    return retval;
}

ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
    /**
     * TODO: handle write
     */
	// Start of the assignment TODO code
	struct aesd_dev *dev = filp->private_data; 	

	if (mutex_lock_interruptible(&dev->lock)) // Get the mutex for protection
		return -ERESTARTSYS;
	PDEBUG("Obtained mutex for write.");
	// check or set the working entry to be the currenty entry
	bool ready_to_add_entry = false;
	if (dev->working_buf_etr == NULL) { // if having no working buf entry for appending, create new one
		struct aesd_buffer_entry *entry;
		entry = kmalloc(sizeof(struct aesd_buffer_entry), GFP_KERNEL);
		dev->working_buf_etr = entry;
		PDEBUG("Creating a new working entry for writing. Alloc size %lld.", count); 
		entry->buffptr = kmalloc((count) * sizeof(char *), GFP_KERNEL); 	
		entry->size=count;
	}
	else { // having a previous entry
		PDEBUG("Continuing a previous working entry for writing. Realloc size %lld from %lld by adding .",dev->working_done_count + count,dev->working_done_count,count);
		struct aesd_buffer_entry *entry;
		entry = dev->working_buf_etr; // get an short name for the working entry
		entry->buffptr = krealloc(entry->buffptr, (dev->working_done_count + count) * sizeof(char *), GFP_KERNEL); // glue new memeory to the buf array
		entry->size=(dev->working_done_count + count); // update the new size
	}

	if (copy_from_user(dev->working_buf_etr->buffptr+dev->working_done_count, buf, count)) {
		retval = -EFAULT;
		goto out;	
	}
	PDEBUG("added %zu bytes from user",count);
	dev->working_done_count += count;
	PDEBUG("updated working done count to %lld.", dev->working_done_count);
	if (dev->working_buf_etr->buffptr[dev->working_done_count-1] == '\n') {
		PDEBUG("Found the end of the write command. Ready to write the entry.");
		ready_to_add_entry = true;
	}
	else {
		PDEBUG("Didn't find the end of the write command. Keep waiting for new write to the entry.");
		retval = count;//dev->working_done_count; 
		*f_pos += retval;// updated the f_pos to work with the llseek
		PDEBUG("Also update the f_pos value with *f_pos +=  %lld.", retval);
		goto out;
	}
	// Now add the finished entry to the circular buffer
	if (ready_to_add_entry) {
		const char *overwritten_buf_ptr;			
		overwritten_buf_ptr = aesd_circular_buffer_add_entry(dev->buf, dev->working_buf_etr);	
		retval = count;//dev->working_done_count;
		PDEBUG("Added the entry to the buffer.");
		
		if (overwritten_buf_ptr != NULL) {
			PDEBUG("The overwritten buf ptr is not NULL. FREE it now!");
			kfree(overwritten_buf_ptr);	
		}	
		PDEBUG("Reset pointer to working entry to NULL!");
		dev->working_buf_etr = NULL;	
		PDEBUG("Reset the working done  count to 0.");
		dev->working_done_count = 0;
		PDEBUG("Done write! unlock now and return with value %lld.", retval);	
		*f_pos += retval;// updated the f_pos to work with the llseek
		PDEBUG("Also update the f_pos value with *f_pos +=  %lld.", retval);
		goto out;
	}
	out:
		mutex_unlock(&dev->lock);
		return retval;
	// End of the assignment TODO code

    return retval;
}

/**
 * Adjust the file offset (f_pos) parameter of @param filp based on the location specified by 
 * @param write_cmd (the zero referenced command to locate)
 * and @param write_cmd_offset (the zero referenced offset into the command)
 * @return 0 if succesfful, negative if error occurred:
 * 		-ERESTARTSYS if mutex could not be obtained
 *		-EINVAL if write command or write_cmd_offset was out of range
 */
static long aesd_adjust_file_offset(struct file *filp, unsigned int write_cmd, unsigned int write_cmd_offset)
{
	ssize_t retval = 0; 
    /**
     * TODO: handle write
     */
	// Start of the assignment TODO code
	struct aesd_dev *dev = filp->private_data; 	

	if (mutex_lock_interruptible(&dev->lock)) // Get the mutex for protection
		return -ERESTARTSYS;
	PDEBUG("Obtained mutex for write.");
	PDEBUG("Updating filp->f_pos according to abs write_cmd index: %d, with in-cmd write_cmd_offset %d",write_cmd, write_cmd_offset);
	PDEBUG("Loop over the whole circular buffer and check the write cmd %d and write_cmd_offset%d", write_cmd, write_cmd_offset);
	uint8_t index;		
 	struct aesd_buffer_entry *free_entry;
	uint8_t size_from_out = 0;
	uint8_t n_total_command_avail = 0; //reset the total size value	
 	for (uint8_t i_entry_since_out = 0; i_entry_since_out <  AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; i_entry_since_out++) {
		index = (i_entry_since_out + (dev->buf)->out_offs) % AESDCHAR_MAX_WRITE_OPERATIONS_SUPPORTED; // The offset is from the out offset, index is the absolute index from 0	
		free_entry=&((dev->buf)->entry[index]); // set the entry pointer for easier operations
		if (free_entry->buffptr != NULL) { // only search when the entry's data buffer(command) is kmalloced
					//Record number of available entries
					n_total_command_avail += 1;
					//accumulate the offset size from the out
					size_from_out += free_entry->size;		
					// check if the current one is the correctr command entry requested
					if (i_entry_since_out == write_cmd) {
						PDEBUG("Found the non-empty command entry that is requested with abs index %d!", index);
						if (write_cmd_offset < (free_entry->size)) {
							PDEBUG("Found that the requested in-cmd write_cmd_offset %d is valid!(<cmd_size=%d)", write_cmd_offset, free_entry->size);
							size_from_out += write_cmd_offset;
							filp->f_pos = size_from_out; //Update the fpos relative the the current out entry(starting point) 
							PDEBUG("Reset the filp->fpos to be %d from the out", (filp->f_pos));
							retval = 0; //for successful return
							goto out;
						} else {
							PDEBUG("ERROR: Found that the requested in-cmd write_cmd_offset %d is NOT valid!( >=cmd_size=%d)", write_cmd_offset, free_entry->size);	
							retval = -EINVAL; 
							goto out;
						}
					}	
			}
	}

	// Check if beyond the available commands
	if (write_cmd > n_total_command_avail) {
		PDEBUG("We have %d commands in buffer, while the requested %dth cmd is beyond the range.",n_total_command_avail, write_cmd);
		retval = -EINVAL;
		goto out;	
	}

	// the out 
	out:
		mutex_unlock(&dev->lock);
		return retval;
	// End of the assignment TODO code
    return retval;
}

long aesd_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	// Some checks following scull example
	if (_IOC_TYPE(cmd) != AESD_IOC_MAGIC) return -ENOTTY;
	if (_IOC_NR(cmd) > AESDCHAR_IOC_MAXNR) return -ENOTTY;

	switch (cmd) {

		case AESDCHAR_IOCSEEKTO:
		{
			struct aesd_seekto seekto;
			if(copy_from_user(&seekto, (const void __user *)arg, sizeof(seekto)) != 0 ) {
				retval = EFAULT;
			} else {
				retval = aesd_adjust_file_offset(filp, seekto.write_cmd, seekto.write_cmd_offset);
			}
			break;
		} 

	}
	return retval;
}


struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
	.llseek =   aesd_llseek,	
    .read =     aesd_read,
    .write =    aesd_write,
	.unlocked_ioctl = aesd_ioctl,
    .open =     aesd_open,
    .release =  aesd_release,	
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    /**
     * TODO: initialize the AESD specific portion of the device
     */

	// Start of the assignment TODO code	
	mutex_init(&aesd_device.lock); // Set the mutex lock	
	mutex_lock_interruptible(&aesd_device.lock); // Get the mutex for protection
	//Init buffer memory dynamically
	if (aesd_device.buf == NULL) {
		PDEBUG("Init buffer memory dynamically.");
		aesd_device.buf=kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
	}
	//initialize the buffer entries	
	PDEBUG("Initialize buffer entries.");		
	uint8_t index;	
 	struct aesd_buffer_entry *init_entry;
 	AESD_CIRCULAR_BUFFER_FOREACH(init_entry,aesd_device.buf,index) {
		init_entry->buffptr=NULL;
		init_entry->size=0;	
	}	
	mutex_unlock(&aesd_device.lock);	// Make sure the mutex lock is unlocked in the read/write, write this for now	
	// End of the assignment TODO code

    result = aesd_setup_cdev(&aesd_device);

    if( result ) {
        unregister_chrdev_region(dev, 1);
    }
    return result;

}

void aesd_cleanup_module(void)
{
    dev_t devno = MKDEV(aesd_major, aesd_minor);

    cdev_del(&aesd_device.cdev);

    /**
     * TODO: cleanup AESD specific poritions here as necessary
     */
	// Start of the assignment TODO code	
	//kfree(&aesd_device.buf);
	mutex_lock_interruptible(&aesd_device.lock); // Get the mutex for protection	
	PDEBUG("Clean buffer memory dynamically.");		
	uint8_t index;	
 	struct aesd_buffer_entry *free_entry;
 	AESD_CIRCULAR_BUFFER_FOREACH(free_entry,aesd_device.buf,index) {	
		if (free_entry->buffptr != NULL) { // only free when the entry's data buffer kmalloced
					kfree(free_entry->buffptr);
			}
	}
	kfree(aesd_device.buf); // Free the dynamically allocated buffer
	mutex_unlock(&aesd_device.lock);	// Make sure the mutex lock is unlocked in the read/write, write this for now
	//kfree(&aesd_device.lock); // No need to free the initialized lock since it's not a dynamically alloced
		
	// End of the assignment TODO code
    
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
