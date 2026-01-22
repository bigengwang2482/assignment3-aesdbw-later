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
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

MODULE_AUTHOR("Bigeng Wang"); /** TODO: fill in your name **/
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

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
	// DO ACTUAL READING HERE, WITH THE PARTIAL READ(NOT FULL COUNTS)
	size_t offset_rtn=0;
	PDEBUG("Find reading entry before reading from buffer...");
	struct aesd_buffer_entry *read_entry = aesd_circular_buffer_find_entry_offset_for_fpos(dev->buf, *f_pos, &offset_rtn); 
	PDEBUG("Get offset_rtn %lld.", offset_rtn);
	if (read_entry == NULL) {
		PDEBUG("Unable to find the entry with offset %lld", *f_pos);
		return 0;
	}
	else{
		retval = read_entry->size - offset_rtn;
		PDEBUG("Found the entry with offset %lld and copy to user with size %lld.", *f_pos, retval);
		if (copy_to_user(buf, read_entry->buffptr+offset_rtn, retval)) {	
			retval = -EFAULT;			
		}	
		else {
			PDEBUG("copy_to_user successfully.");		
			*f_pos += retval;// updated the f_pos to the begining of next entry
			PDEBUG("Update the next entry with offset %lld after read in %lld in", *f_pos, retval);
			return retval;
		}
	}
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
		entry->buffptr = kmalloc(count * sizeof(char *), GFP_KERNEL); 	
		entry->size=count;
	}
	else { // having a previous entry
		struct aesd_buffer_entry *entry;
		entry = dev->working_buf_etr; // get an short name for the working entry
		entry->buffptr = krealloc(entry->buffptr, (dev->working_done_count + count) * sizeof(char *), GFP_KERNEL); // glue new memeory to the buf array
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
		PDEBUG("Found the end of the write command. Ready to write the entry.");
	}
	// Now add the finished entry to the circular buffer
	if (ready_to_add_entry) {
		const char *overwritten_buf_ptr;		
		if (dev->buf == NULL) {
			PDEBUG("Init buffer memory dynamically.");
			dev->buf=kmalloc(sizeof(struct aesd_circular_buffer), GFP_KERNEL);
		}
		overwritten_buf_ptr = aesd_circular_buffer_add_entry(dev->buf, dev->working_buf_etr);	
		retval = count;
		PDEBUG("Added the entry to the buffer.");
		
		if (overwritten_buf_ptr != NULL) {
			PDEBUG("The overwritten buf ptr is not NULL. FREE it now!");
			kfree(overwritten_buf_ptr);	
		}	
		PDEBUG("Reset pointer to working entry to NULL!");
		dev->working_buf_etr = NULL;	
		PDEBUG("Done write! unlock now and return with value %lld.", retval);
		goto out;
	}
	out:
		mutex_unlock(&dev->lock);
		return retval;
	// End of the assignment TODO code

    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
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
	mutex_unlock(&aesd_device.lock);	// Make sure the mutex lock is unlocked in the read/write, write this for now
	kfree(&aesd_device.buf);
	// End of the assignment TODO code
    
	unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
