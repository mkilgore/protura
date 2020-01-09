#!/bin/bash
#
# Configure a tuntap device for your user
#

sudo ip tuntap add tap dev tap0 mode tap user $USER
sudo ip addr add 192.168.56.1/24 dev tap0
sudo ip link set dev tap0 up
