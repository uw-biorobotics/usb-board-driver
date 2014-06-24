/*
 * USB Cypress driver - 2.0
 *
 */

//#include <linux/config.h> // No longer in existence
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/usb.h>
#include "brl_usb_fops.h"
#include <asm/io.h>
#define PARPORT  0x378

#define debug 0

// USB PRODUCT and VENDOR ID Information
#define BRL_USB_VENDOR_ID	0x04B4
#define BRL_USB_PRODUCT_ID	0x4000
#define USB_CYPRESS_MINOR_BASE  192

// Version Information
#define DRIVER_VERSION "2.4"
#define DRIVER_AUTHOR "Phil Roan, Kenneth Fodero, Hawkeye King, Arash Aminpour"
#define DRIVER_DESC "BRL USB DRIVER"

#ifndef FALSE
#define FALSE 0
#endif
#ifndef TRUE
#define TRUE !FALSE
#endif

#define CY_READ  0
#define CY_WRITE 1
#define ERROR 1

/* imported from USB_packets.h */
/* Part: LSI LS7266R1 dual 24-bit quadrature counters */
#define ENC_RESET         0x01  //OUT: Reset Encoder
#define ENC_REQ           0x02  //OUT: Request Encoder packet
#define ENC_READ          0x03  //IN:  Data packet: encoder counts
#define ENC_VEL           0x04  //IN:  Data packet: encoder counts and velocity
/* Part: TI DAC7731E 16-Bit DACs */
#define DAC_RESET         0x05  //OUT: Reset Dac
#define DAC_WRITE          0x06  //OUT: Data packet: Dac value
#define ENCDAC_RESET      0x07  //OUT: Reset Encs and Dac
/* Acks sent back to host */
#define ESTOP_ACK         0x08  //IN: Ack E_STOP
#define ENC_RESET_ACK     0x09  //IN: Ack ENC_RESET
#define DAC_RESET_ACK     0x0A  //IN: Ack DAC_RESET
#define ENCDAC_RESET_ACK  0x0B  //IN: Ack ENC_RESET and DAC_RESET
#define USB_MAX_OUT_LEN 512
#define USB_MAX_IN_LEN  512
#define USB_MAX_LOOPS   4
#define USB_INIT_ERROR  -1

#undef CONFIG_USB_DEBUG
/*
#ifdef CONFIG_USB_DEBUG
static int debug = 1;
#else
static int debug = 0;
#endif
*/

/* Use our own dbg macro */
#undef dbg
#define dbg(format, arg...) do { if (debug) printk(KERN_DEBUG __FILE__ ": " format "\n" , ## arg); } while (0)

#define MAX_SERIAL_LENGTH 10  // Maximum length of a serial number
#define MAX_BOARDS 99         // Maximum number of connected boards

/* Structure to hold all of our device specific stuff */
struct usb_cypress
{
  struct usb_device *	udev;			/* save off the usb device pointer */
  struct usb_interface * interface;		/* the interface for this device */
  unsigned char		num_ports;		/* the number of ports this device has */
  char			num_interrupt_in;	/* number of interrupt in endpoints we have */
  char			num_bulk_in;		/* number of bulk in endpoints we have */
  char			num_bulk_out;		/* number of bulk out endpoints we have */

  struct urb *		read_urb;		/* the urb used to read data */
  __u8			bulk_in_endpointAddr;	/* the address of the bulk in endpoint */
  unsigned char *       bulk_in_buffer;		/* the buffer to receive data */
  size_t		bulk_in_size;		/* the size of the receive buffer */
  atomic_t		read_busy;		/* true iff read urb is busy */
  size_t                read_actual_length;     /* the number of bytes transfered in the read operation */
  unsigned char *       rt_buffer;              /* pointer to buffer in RT space to receive inbound data */

  struct urb *		write_urb;		/* the urb used to send data */
  __u8			bulk_out_endpointAddr;	/* the address of the bulk out endpoint */
  unsigned char *	bulk_out_buffer;	/* the buffer to send data */
  size_t		bulk_out_size;		/* the size of the send buffer */
  atomic_t		write_busy;		/* true iff write urb is busy */
  size_t                write_actual_length;    /* the number of bytes transfered in the write operation */

  int			present;		/* if the device is not disconnected */
  spinlock_t            lock;                   /* locks this structure */
  struct task_struct *  read_task;              /* task pointer. Used for wake_up_process in callback */
  atomic_t		fs_read_busy;		/* true iff a read file operation is in prog. */
  atomic_t		fs_operable;		/* true iff the filesystem node is "open". */
  int                   boardSerialNum;                 /* Board serial number */
};

//Data Structure
struct usb_cypress_node
{
  char isActive;
  struct usb_cypress *data;
};

/* local function prototypes */
int     addNode(struct usb_cypress *dev);
void    cypress_disconnect(struct usb_interface *interface);
ssize_t cypress_get_bytes_read(int serial);
ssize_t cypress_get_bytes_written(int serial);
void    cypress_listActiveBoards(int *list);
int     cypress_probe(struct usb_interface *interface, const struct usb_device_id *id);
ssize_t cypress_read(int serial, char *buffer, size_t count);
void    cypress_read_bulk_callback(struct urb *urb, struct pt_regs *regs);
ssize_t cypress_read_no_urb(int serial, char *buffer, size_t count);
ssize_t cypress_request_read(int, char*, size_t);
ssize_t cypress_write(int serial, const char *buffer, size_t count);
void    cypress_write_bulk_callback(struct urb *urb, struct pt_regs *regs);
ssize_t cypress_write_no_urb(int serial, char *buffer, size_t count);
int     getSerialNum(struct usb_cypress *dev);
int     removeNode(struct usb_cypress *dev);
void    traverseList(void);
void    usb_cypress_debug_data (const char *function, int size, const unsigned char *data);
int     cypress_reset_encdac(int);

