#!/bin/bash
echo -n "Enter the user account to enable real-time and parallel port privileges for"
read user
sudo adduser $user lp
sudo adduser $user realtime
