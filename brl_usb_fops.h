/**
 * File: brl_usb_fops.h
 * Created 6-Oct-2010 by Hawkeye King
 * 
 *  I declare file system operations for the brl_usb 
 * driver (bulk_cypress.ko).
 *
 */
#ifndef BRL_USB_FOPS_H
#define BRL_USB_FOPS_H
ssize_t test_read(struct file*, 
			 char*, 
			 size_t,
			 loff_t*);

ssize_t read_get_data(struct file*, 
			 char*, 
			 size_t,
		         loff_t*);

ssize_t test_write(struct file*, 
			  const char*,
			  size_t, 
			  loff_t* );

int test_open(struct inode *inode, 
		     struct file *file);

int test_release(struct inode *inode, 
			struct file *file);

int test_flush(struct file *file, 
		      fl_owner_t id);
long test_ioctl(struct file*,
		      unsigned int,
		      unsigned long);

#endif // BRL_USB_FOPS_H
