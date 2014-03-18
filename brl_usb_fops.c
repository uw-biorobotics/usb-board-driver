/**
 * File: brl_usb_fops.c
 * Created 6-Oct-2010 by Hawkeye King
 * Updated 3-Mar-2014 by Paul Bartell & David Caballero
 * 
 *  I implement file system operations for the brl_usb 
 * driver (bulk_cypress.ko).
 *
 */

#include "bulk_cypress.h"
#include "brl_usb_fops.h"
#include <linux/uaccess.h>
extern struct usb_cypress_node USBBoards[];
extern struct usb_driver cypress_driver;

struct usb_cypress *getDev(struct inode * inode){
  struct usb_interface *iface = usb_find_interface(&cypress_driver,iminor(inode));
  return usb_get_intfdata(iface);
}

/* Begin: Test File Operations */
int test_open(struct inode *inode, struct file *pfile)
{
  int ret = 0;
  struct usb_cypress *dev = getDev(inode);
  pfile->private_data = dev;
  dev->boardSerialNum = getSerialNum(dev);
  printk("test open (%d)\n\n", dev->boardSerialNum);

  if (ret == 0) {
    atomic_set( &dev->fs_operable, 1);
    atomic_set( &dev->fs_read_busy, 0 );
  }
  return ret;
}

ssize_t test_read(struct file *pfile, 
			 char *userBuffer, 
			 size_t count,
			 loff_t *ppos)
{
  /* TODO: put mutex on read for each board serial */
  int i,   readLen = count;
  int result[8] = {0,0,0,0,0,0,0};
  int channel=0;
  unsigned char readBuffer[count];
  size_t bytesRead=0;
  int ret;
  struct usb_cypress *dev = (struct usb_cypress*) pfile->private_data;
  int serial = dev->boardSerialNum;
  if(!atomic_read(&dev->fs_operable))
    return -1;
  atomic_set( &dev->fs_read_busy, 1 );

  dev->read_task = current;
  set_current_state(TASK_INTERRUPTIBLE);
  memset(readBuffer,0x00,count);

  // Initiate USB read
  ret = cypress_request_read( serial, readBuffer, readLen );
  if (ret < 0 ){
    printk("Error requesting read: %d\n",ret);
  }

  // Wait for USB callback to execute/finish.	
  // TODO: This timeout is too long (ms resolution).  Find high-res version
  // TODO: Wake this process from cypress_read_callback
  ret = schedule_timeout( 10 );

  // Check for usb read completion
  bytesRead = cypress_get_bytes_read(serial);
  if (bytesRead <= 0) {
    printk("Cypress read failed: No data (%zd)!\n", bytesRead);
    ret = -ENODEV;
    goto exit;
  }
  ret = bytesRead;
  
  // Copy data to userspace
  for (i=0; i<bytesRead; i++) {
    put_user( readBuffer[i], userBuffer+i);
  }


  for (channel=0; channel<8;channel++)
  {
	  result[channel] = (readBuffer[3*channel+5]<<16) | (readBuffer[3*channel+4]<<8) | (readBuffer[3*channel+3]);
  }

  printk(KERN_ERR "encVals: %d, %d, %d, %d, %d, %d, %d, %d\n",
	result[0],
	result[1],
	result[2],
	result[3],
	result[4],
	result[5],
	result[6],
	result[7]
	);


 exit:
  atomic_set( &dev->fs_read_busy, 0 );
  return ret;
}

ssize_t test_write(struct file *pfile, 
			  const char *in_buffer,
			  size_t length, 
			  loff_t *poffset)
{
  /* TODO: put mutex on write for each board serial */
  size_t cpy_len = length;
  int ret = 0;
  static char writebuff[1024];
  struct usb_cypress *dev = (struct usb_cypress*) pfile->private_data;
  int serial= dev->boardSerialNum;
  if(!atomic_read(&dev->fs_operable))
    return -1;

  // copy from user to kernel
  ret = copy_from_user(writebuff, in_buffer, cpy_len);
  if (ret != 0) {
    printk("copied partial data from userspace\n");
    return cpy_len - ret;
  }
  ret = cypress_write(serial, writebuff, cpy_len);
  if (ret<0){
    printk("Write op failed.\n");
    return -1;
  }
  return cpy_len;    // on success, return value = cpy_len
}
  
int test_release(struct inode *inode, 
		 struct file *pfile)
{
  struct usb_cypress *dev = (struct usb_cypress*) pfile->private_data;
  int serial = dev->boardSerialNum;

  printk("test release (%d)\n\n",serial);
  atomic_set( &dev->fs_operable, 0);                // stop new read/write ops
  spin_lock(&dev->lock);                            // lock device struct
  if(atomic_read(&dev->read_busy))
    {
      msleep(5);
      printk("unlink r\n");
      usb_kill_urb(dev->read_urb);                // terminate an ongoing read
    }

  if(atomic_read(&dev->write_busy))
    {
      msleep(5);
      printk("unlink w? ");
      if(atomic_read(&dev->write_busy)) 
	{
	  usb_kill_urb(dev->write_urb);               // terminate an ongoing write
	  printk("yes");
	}
      printk("\n");
      }
  spin_unlock(&dev->lock);
  
  return 0; 
}

int test_flush(struct file *pfile, fl_owner_t id)
{
  struct usb_cypress *dev = (struct usb_cypress*) pfile->private_data;
  printk("test flush (%d)\n\n",dev->boardSerialNum);
  return 0; 
}

long test_ioctl( struct file* pfile, unsigned int icommand, unsigned long d){
  char buffer[USB_MAX_OUT_LEN];
  struct usb_cypress *dev = (struct usb_cypress*) pfile->private_data;
  int serial = dev->boardSerialNum;
  printk("test ioctl cmd:%d (%d)\n", icommand, dev->boardSerialNum);
  memset(buffer, ENCDAC_RESET, USB_MAX_OUT_LEN);

  
  msleep(10);
  if(atomic_read(&dev->write_busy))
    msleep(10);

  if (icommand == 10)
    {
      cypress_write(serial, buffer, USB_MAX_OUT_LEN);
      msleep(10);
      cypress_request_read(serial, buffer, 1);
      msleep(10);
      cypress_write(serial, buffer, USB_MAX_OUT_LEN);
      msleep(10);
    }
  return 0;
}

/* End: File ops  */
