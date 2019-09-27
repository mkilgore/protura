#!/bin/bash
#
# Configure a tuntap device for your user
#

sudo ip tuntap add tap dev tap0 mode tap user $USER
