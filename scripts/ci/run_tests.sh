#!/bin/bash
#
# Runs the kernel with tests on, and then parses the output
#
# Argument 1: Kernel image
# Argument 2: First disk image
# Argument 3: Second disk image
# Argument 4: logs directory

KERNEL=$1
DISK_ONE=$2
DISK_TWO=$3
LOGS_DIR=$4

TEST_LOG=$LOGS_DIR/tests.log
QEMU_PID=
RET=

function test_prepare {
    rm $TEST_LOG
    touch $TEST_LOG
}

function test_two_disks {
    echo "QEMU: Two IDE disks, one serial"
    qemu-system-i386 \
        -serial file:$TEST_LOG \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_one_disk {
    echo "QEMU: One IDE disk, one serial"
    qemu-system-i386 \
        -serial file:$TEST_LOG  \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_two_serial {
    echo "QEMU: One IDE disk, two serial"
    qemu-system-i386 \
        -serial file:$TEST_LOG  \
        -serial file:./test2.log \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_two_network_cards {
    echo "QEMU: One IDE disk, one serial, two network cards"
    qemu-system-i386 \
        -serial file:$TEST_LOG  \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -display none \
        -no-reboot \
        -net nic,model=rtl8139 \
        -net nic,model=e1000 \
        -net tap,ifname=tap0,script=no,downscript=no \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_mem_64m {
    echo "QEMU: One IDE disk, 64MB of memory"
    qemu-system-i386 \
        -m 64M \
        -serial file:$TEST_LOG  \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_mem_256m {
    echo "QEMU: One IDE disk, 256MB of memory"
    qemu-system-i386 \
        -m 256M \
        -serial file:$TEST_LOG  \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}

function test_mem_1g {
    echo "QEMU: One IDE disk, 1GB of memory"
    qemu-system-i386 \
        -m 1G \
        -serial file:$TEST_LOG  \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,cache=none,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true" \
        2> /dev/null &

    QEMU_PID=$!
}


function test_verify {
    unset GREP_COLORS
    tail --pid $QEMU_PID -n+1 -f $TEST_LOG | GREP_COLOR="1;32" grep --line-buffered --color=always -E "PASS|$" | GREP_COLOR="1;31" grep --line-buffered --color=always -E "FAIL|PANIC|$"

    wait $QEMU_PID

    ./scripts/ci/parse_test_output.pl < $TEST_LOG
    RET=$?
}

TOTAL_RESULT=0

test_prepare
test_two_disks
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_one_disk
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_two_serial
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_two_network_cards
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_mem_64m
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_mem_256m
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

test_prepare
test_mem_1g
test_verify
TOTAL_RESULT=$(($TOTAL_RESULT + $RET))

if [ "$TOTAL_RESULT" == "0" ]; then
    echo "ALL TESTS PASSED!"
else
    echo "TESTS FAILURE!"
fi

exit $TOTAL_RESULT
