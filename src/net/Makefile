
ifeq ($(CONFIG_NET_SUPPORT),y)
objs-y += net.o
objs-y += packet.o
objs-y += netdevice.o
objs-y += packet_queue.o
objs-y += arp.o
objs-y += sys.o
objs-y += sys_file.o
objs-y += sys_socket.o
objs-y += linklayer.o
objs-y += address_family.o
objs-y += protocol.o
objs-y += socket.o

subdir-y += ipv4

else

objs-y += sys_net_stub.o

endif


