#!/bin/bash

# Wait for program
wait_for_program () {
    n=0
    while true
    do
        # PID of last background command
        if xdotool search --onlyvisible --pid $!; then
            break
        else
            # 20 seconds timeout
            if [ $n -eq 20 ]; then
                xmessage "Error on executing"
            break
            else
                n=`expr $n + 1`
                sleep .1
            fi
        fi
    done
}

i3-msg workspace 2
urxvt -e bash -c "objdump -D /mnt/data/git/os-dev/protura/imgs/protura_x86_multiboot | less" &
B1_PID=$!
wait_for_program
i3-msg split v

cd /mnt/data/git/os-dev/protura
urxvt -e bash -c "sleep 2; gdb ./imgs/protura_x86_multiboot" &
B2_PID=$!
wait_for_program
i3-msg focus up
i3-msg split h

urxvt -e bash -c "qemu-system-i386 -curses -monitor telnet:127.0.0.1:2345,server -s -S -d int,cpu_reset -kernel ./imgs/protura_x86_multiboot" &
WAIT_PID=$!
wait_for_program
i3-msg split v

urxvt -e bash -c "telnet 127.0.0.1 2345" &
MON_PID=$!
wait_for_program

i3-msg focus parent
i3-msg move right
i3-msg focus left
i3-msg focus down

wait $B2_PID

kill $B1_PID
kill $B2_PID
kill $MON_PID
kill $WAIT_PID

i3-msg workspace code


