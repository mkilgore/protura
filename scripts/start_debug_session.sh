#!/bin/bash
#
# Argument 1: Directory to store logs in
# Argument 2: The kernel to run in qemu
# Argument 3: The master disk to attach
# Argument 4: The slave disk to attach
# Argument 5: Any extra arguments to provide to the kernel

LOGS_DIR=$1
KERNEL=$2
DISK_ONE=$3
DISK_TWO=$4
KERNEL_ARGS=$5

qemu_line="qemu-system-i386 \
    -serial file:$LOGS_DIR/qemu.log \
    -serial tcp:localhost:4567,server,nowait \
    -monitor unix:qemu-monitor-socket,server,nowait \
    -curses \
    -s \
    -S \
    -debugcon file:$LOGS_DIR/qemu_debug.log \
    -d cpu_reset \
    -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
    -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
    -net nic,model=rtl8139 \
    -net nic,model=e1000 \
    -net tap,ifname=tap0,script=no,downscript=no \
    -kernel $KERNEL \
    -append \"$KERNEL_ARGS\""

GDB_CMD="gdb"
QEMU_CMD="$qemu_line"
# QEMU_LOG_CMD="sleep .1; stty -icanon -echo; socat file:\$(tty),raw,echo=0 unix-connect:qemu-serial-socket | tee ./com2.log"
QEMU_LOG_CMD="sleep 1; stty raw -echo; socat - tcp:localhost:4567 | tee $LOGS_DIR/com2.log"
QEMU_MONITOR_CMD="sleep 1; stty -icanon -echo; socat - unix-connect:qemu-monitor-socket | tee $LOGS_DIR/qemu_monitor.log"
QEMU_DEBUG_CMD="unset GREP_COLORS; tail --retry -f $LOGS_DIR/qemu.log | GREP_COLOR=\"1;31\" grep --line-buffered --color=always -E \"^.*\[E\].*$|$\""

tmux new -d -s protura-debug -x $(tput cols) -y $(tput lines)

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

# We add this so if a test fails the kernel automatically breaks at that point
tmux send-keys "break ktest_fail_test" Enter
tmux send-keys "dashboard -output ${TTYS[0]}" Enter

tmux attach -t protura-debug
tmux kill-session -t protura-debug

