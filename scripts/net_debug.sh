#!/bin/bash

mv ./qemu.log ./qemu.log.bak

# Select a random port from 40000 to 50000 to listen on
RND_PORT=$(( ($RANDOM % 10000) + 40000))

qemu_line="qemu-system-i386 \
    -serial file:./qemu.log \
    -monitor unix:qemu-monitor-socket,server,nowait \
    -curses \
    -s \
    -S \
    -debugcon file:./qemu_debug.log \
    -d cpu_reset \
    -drive format=raw,file=./disk.img,cache=none,media=disk,index=0,if=ide \
    -drive format=raw,file=./disk2.img,cache=none,media=disk,index=1,if=ide \
    -net nic,model=rtl8139 \
    -net nic,model=e1000 \
    -net tap,ifname=tap0,script=no,downscript=no"

GDB_CMD="gdb"
QEMU_CMD="$qemu_line"
QEMU_MONITOR_CMD="sleep 1; stty -icanon -echo; socat - unix-connect:qemu-monitor-socket | tee ./qemu_monitor.log"
QEMU_DEBUG_CMD="unset GREP_COLORS; tail --retry -f ./qemu.log | GREP_COLOR=\"1;31\" grep --line-buffered --color=always -E \"^.*\[E\].*$|$\""
OBJDUMP_CMD="objdump -D ./imgs/protura_x86_multiboot | less"
NC_LISTEN_CMD="nc -l -p $RND_PORT"
TCPDUMP_CMD="sudo tcpdump -i tap0 -ntv port $RND_PORT"

tmux new -d -s protura-debug -x $COLUMNS -y $LINES

tmux split-window -h
tmux split-window -h

tmux select-pane -t 0
tmux split-window -v

tmux select-pane -t 2
tmux split-window -v
tmux split-window -v

tmux select-pane -t 5
tmux split-window -v

tmux new-window
tmux split-window -h

tmux select-pane -t 1
tmux send-keys "$QEMU_DEBUG_CMD" Enter

tmux select-window -t 1
tmux select-pane -t 2
tmux send-keys "$GDB_CMD" Enter

tmux select-pane -t 3
tmux send-keys "$NC_LISTEN_CMD" Enter

tmux select-pane -t 4
tmux send-keys "$TCPDUMP_CMD" Enter

tmux select-pane -t 5
tmux send-keys "$QEMU_CMD" Enter

tmux select-pane -t 1
tmux send-keys "$QEMU_MONITOR_CMD" Enter

tmux select-pane -t 6
tmux send-keys "$QEMU_DEBUG_CMD" Enter

tmux select-pane -t 1
tmux resize-pane -x 70 -y 5

tmux select-pane -t 3
tmux resize-pane -y 5

tmux select-pane -t 5
tmux resize-pane -y 26 -x 80

TTYS=( $(tmux list-panes -F "#{pane_tty}") )

tmux list-panes -F "#{pane_tty}"
echo "TTYS: $TTYS"

tmux select-pane -t ":1.2"
tmux send-keys "dashboard -output ${TTYS[0]}" Enter

tmux attach -t protura-debug
tmux kill-session -t protura-debug

