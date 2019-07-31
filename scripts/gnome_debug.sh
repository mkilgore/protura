#!/bin/bash
#
# Use with ./script work1 work2 [arguments]
#
# Where 'work1' is the name of the workspace to start the debugging on
# 'work2' is the name of your current workspace (So the script can switch back to it)
# [arguments] are any extra arguments you want to pass on to the kernel when it is run.

mv ./qemu.log ./qemu.log.bak

mkfifo /tmp/dash_fifo

#xfce4-terminal -title objdump -e bash -c "objdump -D ./imgs/protura_x86_multiboot | less" &
#B1_PID=$!

xfce4-terminal --title objdump -e "bash -c \"cat /tmp/dash_fifo\"" &
B1_PID=$!

xfce4-terminal --title gdb -e "bash -c \"gdb\"" &
B2_PID=$!

if [ -z "${@:2}" ]; then
    kernel_cmdline=
else
    kernel_cmdline="-append \"${@:2}\""
fi

qemu_line="qemu-system-i386 \
    -serial file:./qemu.log \
    -serial telnet:127.0.0.1:5346,nowait \
    -monitor telnet:127.0.0.1:5345,nowait \
    -curses \
    -s \
    -S \
    -debugcon file:./qemu_debug.log \
    -d cpu_reset \
    -drive format=raw,file=./disk.img,media=disk,index=0,if=ide \
    -drive format=raw,file=./disk2.img,media=disk,index=1,if=ide \
    -net nic,model=rtl8139 \
    -net nic,model=e1000 \
    -net tap,ifname=tap0,script=no,downscript=no \
    -kernel ./imgs/protura_x86_multiboot \
    $kernel_cmdline"

xfce4-terminal --title qemu-log -e "bash -c \"stty -icanon -echo; nc -v -l -p 5346 | tee ./com2.log\"" &
TERM_PID=$!

xfce4-terminal --title qemu-monitor -e "bash -c \"stty -icanon -echo; nc -v -l -p 5345 | tee ./qemu_monitor.log\"" &
MON_PID=$!

sleep .1

echo "Executing QEMU:"
$qemu_line
#xfce4-terminal --title qemu-console -e "bash -c \"$qemu_line\"" &
#WAIT_PID=$!

xfce4-terminal --title qemu-debug -e "bash -c \"tail --retry -f ./qemu.log\"" &
B3_PID=$!

wait $B2_PID

kill $B1_PID
kill $B2_PID
kill $B3_PID
kill $MON_PID
kill $WAIT_PID
kill $TERM_PID
