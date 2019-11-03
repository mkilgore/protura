#!/bin/bash

sudo ip tuntap add tap mode tap dev tap0 user $USER
sudo ip addr add 192.168.2.1/24 dev tap0

