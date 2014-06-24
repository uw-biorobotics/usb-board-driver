/**
 *  File: cypress_write_ops.c
 *  Created 6-Oct-2010 
 *
 *  Breaks out the usb-write operations from the main .c file.
 */

#include "bulk_cypress.h"

extern struct usb_cypress_node USBBoards[];

/**
 *    cypress_write
 */
ssize_t cypress_write(int serial, const char *buffer, size_t count) 
{
  ssize_t bytes_written = 0;
  int retval = 0;
  struct usb_cypress *dev = NULL;

  //Make sure the device is active
  if (!USBBoards[serial].isActive)
    {
      printk(DRIVER_DESC ": Attempted to write to an invalid USB Board (%d)\n",serial);
      return -EINVAL;
    }

  dev = USBBoards[serial].data;

  /* lock this object */
  spin_lock(&dev->lock);

  /* verify that the device wasn't unplugged */
  if (!dev->present) {
    printk(DRIVER_DESC ": Device unplugged (board %d)\n", serial);
    retval = -ENODEV;
    goto exit;
  }

  /* verify that we actually have some data to write */
  if (count == 0) {
    dbg("%s - write request of 0 bytes", __FUNCTION__);
    retval = -EINVAL;
    goto exit;
  }

  /* wait for a previous write to finish up; we don't use a timeout
   * and so a nonresponsive device can delay us indefinitely.
   */
  if (atomic_read(&dev->write_busy)){
    printk(DRIVER_DESC ": Write already in progress (board %d)\n", serial);
    retval= -EBUSY;
    goto exit;
  }

  /* send the data out the bulk port */
  atomic_set (&dev->write_busy, 1);

  /* we can only write as much as our buffer will hold */
  bytes_written = min (dev->bulk_out_size, count);

  /* copy the data from buffer into our transfer buffer;
   * this is the only copy required.
   */
  memcpy(dev->write_urb->transfer_buffer, buffer, bytes_written);

  usb_cypress_debug_data(__FUNCTION__, bytes_written, dev->write_urb->transfer_buffer);

  /* this urb was already set up, except for this write size */
  dev->write_urb->transfer_buffer_length = bytes_written;
  dev->write_actual_length = 0;

  /* disable IRQs  */
  //disable_irq_nosync(0);

  /* a character device write uses GFP_KERNEL,
     unless a spinlock is held */
  retval = usb_submit_urb(dev->write_urb, GFP_ATOMIC);

  /* renable irqs */
  //enable_irq(0);


  if( retval )
    {
      printk(DRIVER_DESC ": Failed submitting write urb, error %d (board %d)\n",retval, serial);
      // re-init urb?
      atomic_set (&dev->write_busy, 0);
    }
  else
    {
      retval = bytes_written;
    }

 exit:
  spin_unlock(&dev->lock);    /* unlock the device */
  return retval;
}

/**
 *	cypress_write_bulk_callback
 */
void cypress_write_bulk_callback (struct urb *urb, struct pt_regs *regs)
{
  struct usb_cypress *dev = (struct usb_cypress *)urb->context;

  /* sync/async unlink faults aren't errors */
  if (urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET))
    {
      dbg("%s - nonzero write bulk status received: %d", __FUNCTION__, urb->status);
    }

  /* update write_actual_length with the number of bytes read */
  dev->write_actual_length = urb->actual_length;

  /* notify anyone waiting that the write has finished */
  atomic_set (&dev->write_busy, 0);
}

/**
 * cypress_get_bytes_written
 *
 * A completed write urb sets the number of bytes transferred in the callback
 * routine, but applications using the driver do not receive this information.
 * This function returns the number of bytes transferred in the most recent
 * write urb. If the requested USB board does not exist, the function returns
 * -ENODEV.
 *
 * This function can be used to check the state of the urb callback. When the
 * urb is requested, the number of bytes transferred is set to zero. When the
 * urb completes, the variable is updated with the actual length transferred.
 */
ssize_t  cypress_get_bytes_written(int serialNum)
{
  // Check if the board as active
  if( USBBoards[serialNum].isActive == TRUE )
    {
      return USBBoards[serialNum].data->write_actual_length; 
    }
  else
    {
      return -ENODEV;
    }
}



