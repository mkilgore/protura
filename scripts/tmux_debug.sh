#!/bin/bash

mv ./qemu.log ./qemu.log.bak

if [ -z "${@:0}" ]; then
    kernel_cmdline=
else
    kernel_cmdline="-append \"${@:0}\""
fi

qemu_line="qemu-system-i386 \
    -serial file:./qemu.log \
    -serial tcp:localhost:4567,server,nowait \
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
# QEMU_LOG_CMD="sleep .1; stty -icanon -echo; socat file:\$(tty),raw,echo=0 unix-connect:qemu-serial-socket | tee ./com2.log"
QEMU_LOG_CMD="sleep 1; stty raw -echo; socat - tcp:localhost:4567 | tee ./com2.log"
QEMU_MONITOR_CMD="sleep 1; stty -icanon -echo; socat - unix-connect:qemu-monitor-socket | tee ./qemu_monitor.log"
QEMU_DEBUG_CMD="unset GREP_COLORS; tail --retry -f ./qemu.log | GREP_COLOR=\"1;31\" grep --line-buffered --color=always -E \"^.*\[E\].*$|$\""
OBJDUMP_CMD="objdump -D ./imgs/protura_x86_multiboot | less"

tmux new -d -s protura-debug -x $COLUMNS -y $LINES

tmux split-window -h
tmux split-window -h

tmux select-pane -t 0
tmux split-window -v

tmux select-pane -t 2
tmux split-window -v

tmux select-pane -t 4
tmux split-window -v

tmux new-window
tmux split-window -h

tmux select-pane -t 1
tmux send-keys "$QEMU_DEBUG_CMD" Enter

tmux select-window -t 1
tmux select-pane -t 2
tmux send-keys "$GDB_CMD" Enter

tmux select-pane -t 3
tmux send-keys "$QEMU_LOG_CMD" Enter

tmux select-pane -t 4
tmux send-keys "$QEMU_CMD" Enter

tmux select-pane -t 1
tmux send-keys "$QEMU_MONITOR_CMD" Enter

tmux select-pane -t 5
tmux send-keys "$QEMU_DEBUG_CMD" Enter

tmux select-pane -t 1
tmux resize-pane -x 70 -y 15

tmux select-pane -t 4
tmux resize-pane -y 26 -x 80

TTYS=( $(tmux list-panes -F "#{pane_tty}") )

tmux list-panes -F "#{pane_tty}"
echo "TTYS: $TTYS"

tmux select-pane -t ":1.2"
tmux send-keys "dashboard -output ${TTYS[0]}" Enter

tmux attach -t protura-debug
tmux kill-session -t protura-debug

