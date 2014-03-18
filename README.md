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
> sudo make install

## Uninstall ##
> sudo make uninstall

## Loading the module ##
> /etc/init.d/brl_usb start

## API ##
- usb_board_count
- cypress_get_bytes_read
- cypress_get_bytes_written
- cypress_listActiveBoards
- cypress_read
- cypress_request_read
