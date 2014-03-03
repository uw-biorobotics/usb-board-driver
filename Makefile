#KERNEL_SRC = /usr/src/linux
KERNEL_SRC = /lib/modules/$(shell uname -r)/build
#KERNEL_SRC = /usr/src/linux-headers-$(shell uname -r)/include/linux
SUBDIR = $(PWD)

EXTRA_FLAGS += -DDEBUG=1
DEBUG = y

obj-m += brl_usb.o 
brl_usb-objs := brl_usb_fops.o \
	cypress_read_ops.o \
	cypress_write_ops.o \
	bulk_cypress.o 

all:	
	$(MAKE) -C $(KERNEL_SRC) SUBDIRS=$(SUBDIR) modules

clean:
	rm -f *.o *~ core *.mod.c *.ko
#TODO make an install target that copies init.d and udev rules
install: 
	cp -R ./etc /

uninstall: 
	rm /etc/init.d/brl_usb && rm /etc/udev/rules.d/*brl*
