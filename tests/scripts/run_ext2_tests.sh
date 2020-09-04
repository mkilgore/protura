#!/bin/bash
#
# Runs the kernel and execute a ext2 test, and then parses the output
#
# Argument 1: Kernel image
# Argument 2: First disk image
# Argument 3: Second disk image
# Argument 4: test results directory

PREFIX="ext2"

KERNEL=$1
DISK_ONE=$2
DISK_TWO=$3
TEST_PREFIX=$4
TEST_NAME=$5

QEMU_PID=

mkdir -p ./obj/$TEST_NAME

DISK_CPY=./obj/$TEST_NAME/disk_cpy.img
MAIN_DISK_CPY=./obj/$TEST_NAME/main_disk_cpy.img

cp $DISK_ONE $MAIN_DISK_CPY

TESTS=$(find ./userspace/root/tests/ext2/ -name "test_*.sh" | xargs basename -a)

for test in $TESTS; do
    TESTCASE="$TEST_NAME: $test"
    TEST_LOG=${TEST_PREFIX}/$(basename -s .sh $test).qemu.log
    TEST_ERR_LOG=${TEST_PREFIX}/$(basename -s .sh $test).qemu.err.log
    TEST_E2FSCK_LOG=${TEST_PREFIX}/$(basename -s .sh $test).e2fsck.log

    rm -fr $DISK_CPY
    cp $DISK_TWO $DISK_CPY

    rm -fr $TEST_LOG
    touch $TEST_LOG

    rm -fr $TEST_E2FSCK_LOG
    touch $TEST_E2FSCK_LOG

    start_time=$(date +%s.%2N)

    timeout --foreground 180 qemu-system-i386 \
        -serial file:$TEST_LOG \
        -d cpu_reset \
        -m 512M \
        -drive format=raw,file=$MAIN_DISK_CPY,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_CPY,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "init=/tests/ext2/$test com1.loglevel=debug reboot_on_panic=1 ext2.loglevel=debug" \
        2> $TEST_ERR_LOG &

    wait $!
    RET=$?

    end_time=$(date +%s.%2N)
    time_length=$(echo "scale=2; $end_time - $start_time" | bc)

    [ "$RET" -eq "0" ]
    assert_success_named "Timeout (${time_length}s)" "Disk likely not synced!"

    [ -s "$TEST_LOG" ]
    assert_success_named "Kernel Log Not Empty"

    ! grep -q "PANIC" $TEST_LOG
    assert_success_named "Kernel Didn't Panic" "Kernel Log:" cat $TEST_LOG

    e2fsck -nfv $DISK_CPY > $TEST_E2FSCK_LOG 2>&1
    assert_success_named "e2fsck" "File System errors:" cat $TEST_E2FSCK_LOG
done

rm $DISK_CPY
rm $MAIN_DISK_CPY
