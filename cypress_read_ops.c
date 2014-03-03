/**
 *  File: cypress_read_ops.c
 *  Created 6-Oct-2010 
 *
 *  Breaks out the usb-read operations from the main .c file.
 */

#include "bulk_cypress.h"

extern struct usb_cypress_node USBBoards[];

/**
 *    cypress_read
 */
ssize_t cypress_read(int serial, char *buffer, size_t count) 
{
  ssize_t bytes_read = 0;
  int retval = 0;
  struct usb_cypress *dev = NULL;

  //Make sure the device is active
  if( !USBBoards[serial].isActive )
    {
      printk(DRIVER_DESC ": Attempted to read from an invalid USB Board (%d)\n",serial);
      return -EINVAL;
    }
  dev = USBBoards[serial].data;

  /* lock this object */
  spin_lock(&dev->lock);
  
  /* verify that the device wasn't unplugged */
  if( !dev->present )
    {
      printk(DRIVER_DESC "Device unplugged (board %d)\n", serial);
      retval= -ENODEV;
    
      /* unlock the device */
      spin_unlock(&dev->lock);
      return retval;
    }

  /* verify that we actually have some data to read */
  if (count == 0)
    {
      dbg("%s - read request of 0 bytes", __FUNCTION__);
      retval = -EINVAL;
    
      /* unlock the device */
      spin_unlock(&dev->lock);
      return retval;    
    }

  /* wait for a previous read to finish up; we don't use a timeout
   * and so a nonresponsive device can delay us indefinitely.
   */
  if( atomic_read(&dev->read_busy) )
    {
      printk(DRIVER_DESC ": Read already in progress (board %d)\n", serial);
      retval= -EBUSY;

      /* unlock the device */
      spin_unlock(&dev->lock);
      return retval;
    }

  /* we can only read as much as our buffer will hold */
  bytes_read = min( dev->bulk_in_size, count );
 
  /* recieve the data from the bulk port */
  /* a character device read uses GFP_KERNEL, unless a spinlock is held */
  atomic_set( &dev->read_busy, 1 );
  
  retval = usb_submit_urb( dev->read_urb, GFP_ATOMIC );
  if( retval != 0 ) // URB submission unsuccessful
    {
      atomic_set( &dev->read_busy, 0 );
      printk(DRIVER_DESC ": Failed submitting read urb, error %d (board %d)\n", retval, serial);
      return retval;
    }
  else
    {
      retval = bytes_read;
    }

  /* Debugging Information */
  //printk(DRIVER_DESC ": DEBUG: actual_length %d\n", dev->read_urb->actual_length);
  //printk(DRIVER_DESC ": DEBUG: status %d\n", dev->read_urb->status);
  usb_cypress_debug_data (__FUNCTION__, bytes_read, dev->read_urb->transfer_buffer);

  /* copy the data from our transfer buffer into buffer;
   * this is the only copy required.
   * 
   * this step should be executed in the callback
   */
  memcpy(buffer, dev->read_urb->transfer_buffer, bytes_read);
  //printk(DRIVER_DESC ": DEBUG [cypress_read]: actual_length %d\n", dev->read_urb->actual_length);


  /* this urb was already set up, except for this read size */
  dev->read_urb->transfer_buffer_length = bytes_read;

  atomic_set( &dev->read_busy, 0 );

  /* unlock the device */
  spin_unlock(&dev->lock);

  return retval;
}


/**
 * cypress_request_read
 *
 * This function submits a bulk read urb to the usb system.
 * RTAI will not allow the blocking call, so we must either
 * write our own blocking read/write routines, or use urbs
 * and callbacks. The callback should execute while RTAI
 * is sleeping, so the data is ready at the start of the
 * next loop.
 */
ssize_t cypress_request_read(int serial, char *buffer, size_t bytes_requested) 
{
  int retval = 0;
  struct usb_cypress *dev = NULL;

  //Make sure the device is active
  if( !USBBoards[serial].isActive )
    {
      printk(DRIVER_DESC ": Attempted to read from an invalid USB Board (%d)\n",serial);
      return -EINVAL;
    }
  dev = USBBoards[serial].data;

  spin_lock(&dev->lock); /* lock the USB object */
  
  /* verify that the device wasn't unplugged */
  if( !dev->present )
    {
      printk(DRIVER_DESC "Device unplugged (board %d)\n", serial);
      retval= -ENODEV;
    
      spin_unlock(&dev->lock); /* unlock the device */
      return retval;
    }

  /* verify that we actually have some data to read */
  if (bytes_requested == 0)
    {
      dbg("%s - read request of 0 bytes", __FUNCTION__);
      retval = -EINVAL;
    
      spin_unlock(&dev->lock); /* unlock the device */
      return retval;    
    }

  /* wait for a previous read to finish up; we don't use a timeout
   * and so a nonresponsive device can delay us indefinitely.
   */
  if( atomic_read(&dev->read_busy) )
    {
      printk(DRIVER_DESC ": Read already in progress (board %d)\n", serial);
      retval= -EBUSY;
      spin_unlock(&dev->lock); /* unlock the device */
      return retval;
    }

  /* recieve the data from the bulk port */
  atomic_set( &dev->read_busy, 1 );

  /* we can only read as much as our buffer will hold */
  bytes_requested = min( dev->bulk_in_size, bytes_requested );
  dev->read_urb->transfer_buffer_length = bytes_requested;
  
  /* a character device read uses GFP_KERNEL, unless a spinlock is held */
  retval = usb_submit_urb( dev->read_urb, GFP_ATOMIC );
  if( retval != 0 ) // URB submission unsuccessful
    {
      atomic_set( &dev->read_busy, 0 );
      printk(DRIVER_DESC ": Failed requesting read urb, error %d (board %d)\n", retval, serial);
    }
  else
    {
      dev->rt_buffer = buffer;
      dev->read_actual_length = 0; // set to zero here, set to the length read in callback
    }

  spin_unlock(&dev->lock); /* unlock the device */
  return retval;
}

/**
 *	cypress_read_bulk_callback
 */
void cypress_read_bulk_callback (struct urb *urb, struct pt_regs *regs)
{
  struct usb_cypress *dev = (struct usb_cypress *)urb->context;
 
  /* sync/async unlink faults aren't errors */
  if( urb->status && !(urb->status == -ENOENT || urb->status == -ECONNRESET) )
    {
      dbg("%s - nonzero read bulk status received: %d", __FUNCTION__, urb->status);
    }

  memcpy(dev->rt_buffer, 
	 urb->transfer_buffer, 
	 urb->actual_length);                    /* copy data to output buffer */
  dev->read_actual_length = urb->actual_length;  /* update value with the number of bytes read */
  atomic_set (&dev->read_busy, 0);               /* notify anyone waiting that the read has finished */

  if (atomic_read( &dev->fs_read_busy ) &&
      (dev->read_task != NULL) && 
      dev->read_task->state >= 0 )   /* running,runnable,interruptable,uninterruptable */
    {
      wake_up_process(dev->read_task);
    }
}

/**
 * cypress_get_bytes_read
 *
 * A completed read urb sets the number of bytes transferred in the callback
 * routine, but applications using the driver do not receive this information.
 * This function returns the number of bytes transferred in the most recent
 * read urb. If the requested USB board does not exist, the function returns
 * -ENODEV.
 *
 * This function can be used to check the state of the urb callback. When the
 * urb is requested, the number of bytes transferred is set to zero. When the
 * urb completes, the variable is updated with the actual length transferred.
 */
ssize_t  cypress_get_bytes_read(int serialNum)
{
  // Check if the board as active
  if( USBBoards[serialNum].isActive == TRUE )
    {
      return USBBoards[serialNum].data->read_actual_length; 
    }
  else
    {
      return -ENODEV;
    }
}

