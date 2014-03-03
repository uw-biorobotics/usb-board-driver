/**
 * BRL USB Driver - 2.3
 * Copyright (C) 2003-2009 BioRobotics Laboratory
 *
 * written by Phil Roan, Ken Fodero, Arash Aminpour, Hawkeye King
 * BioRobotics Lab, University of Washington
 *
 * derived from USB Skeleton driver - 1.1
 * Copyright (C) 2001-2003 Greg Kroah-Hartman (greg@kroah.com)
 *
 */

/**
  23-sep-2010 HK: 
  For compatibility w/ new kernels, changed dev->lock = SPIN_LOCK_UNLOCKED to spin_lock_init(dev->lock)
  Bug fix: conflicting return types for cypres_request_read.  Was ssize_t in bulk_cypress.h and int in bulk_cypress.c

  29-Sep-2010 HK:
  Note: add the following line to udev to automatically generate /dev/BRL_USB_<x> when board x is attached.
  SUBSYSTEMS=="usb",ATTRS{manufacturer}=="UW BioRobotics Lab",NAME="BRL_USB_%s{serial}"

*/

#include "bulk_cypress.h"

// Variable storing the number of attached boards
char usb_board_count = 0; 
struct usb_cypress_node USBBoards[MAX_BOARDS];

//Symbol showing number of USB boards
EXPORT_SYMBOL(usb_board_count);

//Symbols related to cypress read/write
EXPORT_SYMBOL(cypress_get_bytes_read);
EXPORT_SYMBOL(cypress_get_bytes_written);
EXPORT_SYMBOL(cypress_listActiveBoards);
EXPORT_SYMBOL(cypress_read);
//EXPORT_SYMBOL(cypress_read_no_urb);
EXPORT_SYMBOL(cypress_request_read);
//EXPORT_SYMBOL(cypress_write);
//EXPORT_SYMBOL(cypress_write_no_urb);

/**
 * define file operations and stuff. 
 */ 
static const struct file_operations cypress_fops = {
  .owner =	THIS_MODULE,
  .read =	test_read,
  .write =	test_write,
  .open =	test_open,
  .release=	test_release,
  .flush =	test_flush,
  .ioctl =      test_ioctl,
};

/**
 * table of devices that work with this driver 
 */
static struct usb_device_id cypress_table [] = {
  {USB_DEVICE(BRL_USB_VENDOR_ID, BRL_USB_PRODUCT_ID)},
  { }
};
MODULE_DEVICE_TABLE (usb, cypress_table);

/**
 * usb class driver info in order to get a minor number from the usb core,
 * and to have the device registered with the driver core
 */
static struct usb_class_driver cypress_class = {
  .name =		"brl_usb%d",
  .fops =		&cypress_fops,
  .minor_base =	USB_CYPRESS_MINOR_BASE,
};

/**
 * addNode - Add data for a USB node to the linked list data structure
 * 
 *  result - success 0 or failure -1
 */
int addNode(struct usb_cypress *dev)
{
  int serialNum = getSerialNum(dev);
  USBBoards[serialNum].isActive = TRUE;      //Set the board as active
  USBBoards[serialNum].data = dev;           //Set pointer with data to point to dev struct
  usb_board_count++;                         //Update count of number of USB boards attached

  printk(DRIVER_DESC ": USB Board #%d Successfully Attached\n",serialNum);
  return 0;
}

/**
 *	cypress_delete
 */
inline void cypress_delete (struct usb_cypress *dev)
{
  /* TODO: check semaphores for completion */
  removeNode(dev);
  usb_buffer_free (dev->udev, dev->bulk_in_size,
		   dev->bulk_in_buffer,
		   dev->read_urb->transfer_dma);
  usb_buffer_free (dev->udev, dev->bulk_out_size,
		   dev->bulk_out_buffer,
		   dev->write_urb->transfer_dma);
  usb_free_urb (dev->read_urb);
  usb_free_urb (dev->write_urb);
  kfree(dev);
}

/**
 *	cypress_disconnect
 *
 *	Pointer to the disconnect function in the USB driver. This function is
 *      called by the USB core when the struct usb_interface has been removed
 *      from the system or when the driver is being unloaded from the USB core.
 *
 *	This routine guarantees that the driver will not submit any more urbs
 *	by clearing dev->udev.  It is also supposed to terminate any currently
 *	active urbs.
 */
void cypress_disconnect(struct usb_interface *interface)
{
  struct usb_cypress *dev;
  
  usb_deregister_dev(interface, &cypress_class);    // Disconnect devfs and give back minor
  dev = usb_get_intfdata(interface);               //  "
  usb_set_intfdata (interface, NULL);               //  "

  spin_lock(&dev->lock);
  if(atomic_read(&dev->read_busy))
    {
      usb_unlink_urb(dev->read_urb);                // terminate an ongoing read
    }
  if(atomic_read(&dev->write_busy))
    {
      usb_unlink_urb(dev->write_urb);               // terminate an ongoing write
    }
  dev->present = 0;                                 // prevent device read, write and ioctl
  spin_unlock(&dev->lock);
  cypress_delete (dev);
}

/**
 * cypress_listActiveBoards - function to return a list of all active boards
 *
 */
void cypress_listActiveBoards(int *list)
{
  int i, count = 0;
  for (i = 0; i < MAX_BOARDS; i++)
    {
      if (USBBoards[i].isActive)        // Check for an active board if so add it to list
	{
	  list[count] = i;
	  count++;
	}
      if (count == usb_board_count)    // Stop if we have found all the boards
	return;
    }
}

/**
 *	cypress_probe
 *
 *	Called by the usb core when a new device is connected that it thinks
 *	this driver might be interested in.
 *
 *      Pointer to the probe function in the USB driver. This function is
 *      called by the USB core when it thinks it has a struct usb_interface
 *      that this driver can handle. A pointer to the struct usb_device_id
 *      that the USB core used to make this decision is also passed to this
 *      function. If the USB driver claims the struct usb_interface that is
 *      passed to it, it should initialize the device properly and return
 *      0. If the driver does not want to claim the device, or an error
 *      occurs, it should return a negative error value.
 */
int cypress_probe(struct usb_interface *interface, const struct usb_device_id *id)
{
  struct usb_device *udev = interface_to_usbdev(interface);
  struct usb_cypress *dev = NULL;
  struct usb_host_interface *iface_desc;
  struct usb_endpoint_descriptor *endpoint;
  size_t buffer_size;
  int i, retval = -ENOMEM;

  /* See if the device offered us matches what we can accept */
  if ((udev->descriptor.idVendor != BRL_USB_VENDOR_ID) || 
      (udev->descriptor.idProduct != BRL_USB_PRODUCT_ID))
    {
      return -ENODEV;
    }

  dev = kmalloc(sizeof(struct usb_cypress), 
		GFP_ATOMIC);  /* allocate memory for our device state and initialize it */
  if( dev == NULL )
    {
      err("cypress_probe: out of memory.");
      return -ENOMEM;
    }
  memset(dev, 0x00, sizeof (*dev));

  dev->udev = udev;
  dev->interface = interface;

  /* Set up the endpoint information */
  /* check out the endpoints */
  /* use only the first bulk-in and bulk-out endpoints */
  iface_desc = &interface->altsetting[0];
  for( i = 0; i < iface_desc->desc.bNumEndpoints; ++i )
    {
      endpoint = &iface_desc->endpoint[i].desc;
      if( !dev->bulk_in_endpointAddr &&
	  (endpoint->bEndpointAddress & USB_DIR_IN) &&
	  ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) )
	{
	  /* we found a bulk in endpoint */
	  buffer_size = endpoint->wMaxPacketSize;
	  dev->bulk_in_size = buffer_size;      
	  dev->bulk_in_endpointAddr = endpoint->bEndpointAddress;
	  dev->read_urb = usb_alloc_urb(0, GFP_ATOMIC);
	  if( dev->read_urb == NULL )
	    {
	      err("No free urbs available");
	      goto error;
	    }
	  dev->read_urb->transfer_flags = (URB_NO_TRANSFER_DMA_MAP);
	  dev->bulk_in_buffer = usb_buffer_alloc (udev,
						  buffer_size, GFP_ATOMIC,
						  &dev->read_urb->transfer_dma);
	  if( dev->bulk_in_buffer == NULL )
	    {
	      err("Couldn't allocate bulk_in_buffer");
	      goto error;
	    }
	  usb_fill_bulk_urb(dev->read_urb, udev,
			    usb_rcvbulkpipe(udev, endpoint->bEndpointAddress),
			    dev->bulk_in_buffer, buffer_size,
			    (usb_complete_t)cypress_read_bulk_callback, dev);
	}

      if( !dev->bulk_out_endpointAddr &&
	  !(endpoint->bEndpointAddress & USB_DIR_IN) &&
	  ((endpoint->bmAttributes & USB_ENDPOINT_XFERTYPE_MASK) == USB_ENDPOINT_XFER_BULK) )
	{
	  /* we found a bulk out endpoint */
	  /* a probe() may sleep and has no restrictions on memory allocations */
	  dev->write_urb = usb_alloc_urb(0, GFP_ATOMIC);
	  if( dev->write_urb == NULL )
	    {
	      err("No free urbs available");
	      goto error;
	    }
	  dev->bulk_out_endpointAddr = endpoint->bEndpointAddress;

	  /* on some platforms using this kind of buffer alloc
	   * call eliminates a dma "bounce buffer".
	   *
	   * NOTE: you'd normally want i/o buffers that hold
	   * more than one packet, so that i/o delays between
	   * packets don't hurt throughput.
	   */
	  buffer_size = endpoint->wMaxPacketSize;
	  dev->bulk_out_size = buffer_size;
	  dev->write_urb->transfer_flags = (URB_NO_TRANSFER_DMA_MAP);
	  dev->bulk_out_buffer = usb_buffer_alloc (udev,
						   buffer_size, GFP_ATOMIC,
						   &dev->write_urb->transfer_dma);
	  if( dev->bulk_out_buffer == NULL )
	    {
	      err("Couldn't allocate bulk_out_buffer");
	      goto error;
	    }
	  usb_fill_bulk_urb(dev->write_urb, udev,
			    usb_sndbulkpipe(udev,
					    endpoint->bEndpointAddress),
			    dev->bulk_out_buffer, buffer_size,
			    (usb_complete_t)cypress_write_bulk_callback, dev);
	}
    }
  if (!(dev->bulk_in_endpointAddr && dev->bulk_out_endpointAddr))
    {
      err("Couldn't find both bulk-in and bulk-out endpoints");
      goto error;
    }

  dev->present = 1;                   /* allow device read, write and ioctl */
  usb_set_intfdata (interface, dev);  /* we can register the device now, as it is ready */
  spin_lock_init(&(dev->lock));       /* initialize spinlock to unlocked (new kerenel method) */

  /* HK: Begin- connect filesystem hooks */
  /* we can register the device now, as it is ready */
  retval = usb_register_dev(interface, &cypress_class);
  if (retval) {
    /* something prevented us from registering this driver */
    err("Not able to get a minor for this device.");
    usb_set_intfdata(interface, NULL);
    goto error;
  }
  dev_info(&interface->dev,
	   "BRL USB device now attached to minor: %d\n",
	   interface->minor);                            /* let the user know the device minor */
  dev->read_task = NULL;                                 /* Initialize fs read_task. */
  
  addNode(dev);
  return 0;

 error: // please please please remove goto statements!    HK:Why?
  printk("cypress_probe: error occured!\n");
  cypress_delete (dev);
  return retval;
}

/**
 * getSerialNum - function to read the serial number from a USB board
 *   
 *  result - int - serial number of the board
 *                 Returns (negative) error code on failure.
 */
int getSerialNum(struct usb_cypress *dev)
{
  int i, result, len;
  char buffer[MAX_SERIAL_LENGTH] = {0};

  if( dev == NULL )
    {
      printk("getSerialNum error: Passed in NULL pointer\n");
      return -1;
    }

  //Read in USB iSerialNumber descriptor string
  len = usb_string(dev->udev, dev->udev->descriptor.iSerialNumber, buffer, MAX_SERIAL_LENGTH); 
  
  result = 0;

  //Return error code
  if( len <= 0 )
    {
      printk("Error reading USB serial number:%d\n",len);
      return len;  // return failure.
    }

  //Loop through and convert it to an integer
  for (i = 0; i < len; i++)
    {
      if( (buffer[i] < '0') || (buffer[i] > '9') )
	{
	  printk("Error in serial Number - Non Numeral Digit '%c' found!!!\n",buffer[i]);
	  return -1;
	}
      result = result*10 + buffer[i] - '0';
    }

  //Return serial number
  return result;
}


/**
 * removeNode - Remove data for a USB node from the linked list data structure
 * 
 *  result - success 0 or failure -1
 */
int removeNode(struct usb_cypress *dev)
{
  int i, serialNum = 0;

  serialNum = getSerialNum(dev);

  //A device has been disconnected
  if( serialNum < 0 )
    {
      for( i = 0; i < MAX_BOARDS; i++ )
	{
	  //Look for an active node that has dev structure incorrect
	  if( (USBBoards[i].isActive == TRUE) && (getSerialNum(USBBoards[i].data) < 0) )
	    {
	      USBBoards[i].isActive = FALSE;
	      USBBoards[i].data = NULL;
	
	      usb_board_count--;

	      printk(DRIVER_DESC ": USB board #%d removed from system\n",i);
	      return 0;
	    }
	}
      return -1;
    
    }
  //The driver is being powered off
  else
    {
      for (i = 0; i < MAX_BOARDS; i++)
	{
	  //Look for the board and remove it's data structure
	  if ((USBBoards[i].isActive == TRUE) && (i == serialNum))
	    {
	      USBBoards[i].isActive = FALSE;
	      USBBoards[i].data = NULL;
	
	      usb_board_count--;

	      printk(DRIVER_DESC ": USB board #%d detached from driver\n",i);
	      return 0;
	    }
	}
      return -1;
    }
}


/**
 * traverse_list - function to traverse list of active usb boards
 *   
 */
void traverse_list(void)
{
  int i = 0;
  struct usb_cypress *dev = NULL;

  printk("Starting List Traversal\n");

  for (i = 0; i < MAX_BOARDS; i++)
    {
      dev = USBBoards[i].data;

      if (USBBoards[i].isActive == TRUE)
	printk("USB Serial = %d active. Read Lock = %d, Write Lock = %d\n", i, 
	       atomic_read(&dev->read_busy), atomic_read(&dev->write_busy));
    }
  printk("List Traversal Complete\n");
}

/* usb specific object needed to register this driver with the usb subsystem */
struct usb_driver cypress_driver = {
  //  .owner =	THIS_MODULE,
  .name =	"brl_usb",
  .id_table =	cypress_table,
  .probe =	cypress_probe,
  .disconnect =	cypress_disconnect,  
};

/**
 * cypress_reset_encdac() - 
 *    reset board encoders and DACs
 */
/*
int cypress_reset_encdac(int serial){
  int cnt = 0,   ret;

  char buffer_out[USB_MAX_OUT_LEN] = {0},     buffer_in[USB_MAX_IN_LEN] = {0};
  struct usb_cypress *dev = USBBoards[serial].data;

  buffer_out[0] = ENCDAC_RESET;                        //Command board to reset ENCS and DACS
  while( cnt <= USB_MAX_LOOPS )
    {
      printk("Resetting write...\n");
      cnt++;
      msleep(5);

      if ( cypress_write(serial, buffer_out, 1) != 1 ) // write reset packet to board
	{
	  printk("Write failed in cypress_reset_encdac (%d).\n", cnt);  // write failed
	  continue;
	}
      msleep(500);
      break;
    }

  cnt = 0;
  while( cnt <= USB_MAX_LOOPS )
    {
      printk("Resetting write...\n");
      cnt++;

      if ( cypress_read(serial, buffer_in, 1) <= 0 )
	{
	  printk("Read failed in cypress_reset_encdac (%d)\n", cnt);
	  continue;
	}
      msleep(500);

      if (buffer_in[0] == ENCDAC_RESET_ACK)       // Rec'd ACK packet
	return 0;
      else {                                      //Found some other type of packet
	printk("Failed ACK in cypress_reset_encdac rec'd %X (%d)\n", buffer_in[0], cnt);
	continue;
      }
    }

  printk("%d timeouts in reading reset [ACK] on (board %d)\n", cnt, serial);
  return USB_INIT_ERROR;
}*/

void usb_cypress_debug_data (const char *function, int size, const unsigned char *data)
{
  int i;

  if( !debug )
    return;

  printk("debug:%d : %s - length = %d, data = ", debug, function, size);
  for( i = 0; i < size; ++i )
    {
      printk("%.2x ", data[i]);
    }
  printk("\n");
}

/**
 *	usb_cypress_init
 */
int __init usb_cypress_init(void)
{
  int result, i;

  for (i = 0; i < MAX_BOARDS; i++)
    {
      USBBoards[i].isActive = FALSE;
    }

  /* register this driver with the USB subsystem */
  result = usb_register(&cypress_driver);
  if (result) {
    err("usb_register failed. Error number %d", result);
    return result;
  }

  printk(DRIVER_DESC " " DRIVER_VERSION " - Now Installed\n");
  return 0;
}

/**
 *	usb_cypress_exit
 *
 *      When the USB driver is to be unloaded, the struct usb_driver
 *      needs to be unregistered from the kernel. This is done with
 *      a call to usb_deregister_driver. When this call happens,
 *      any USB interfaces that were currently bound to this driver
 *      are disconnected, and the disconnect function is called for
 *      them.
 */
void __exit usb_cypress_exit(void)
{
  /* deregister this driver with the USB subsystem */
  usb_deregister(&cypress_driver);
}


module_init (usb_cypress_init);
module_exit (usb_cypress_exit);



MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
