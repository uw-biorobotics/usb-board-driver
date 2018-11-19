usb-board-driver
================

## Source Files ##
- bulk_cypress.c
- cypress_read_ops.c
- brl_usb_fops.c

## Headers ##
- bulk_cypress.h
- cypress_read_ops.h
- brl_usb_fops.h

## Prerequisites ##
- recent kernel (2.6 or 3.x series should work fine)
- kernel headers installed

## Compile ##
Ensure that you have your current kernel's headers installed:

> sudo apt-get update

> sudo apt-get install linux-headers-$(uname -r)

Build the kernel module

> make

## Install ##
Copy usb driver folder to usr/src with the current version number, for example (11/2018):
> cp -R ../usb-board-driver /usr/src/brl_usb-2.4

Use dkms to install and manage the module. This should load the module on startup.
> sudo dkms install -m brl_usb -v 2.4

## Uninstall ##
> sudo make uninstall

## API ##
- usb_board_count
- cypress_get_bytes_read
- cypress_get_bytes_written
- cypress_listActiveBoards
- cypress_read
- cypress_request_read
