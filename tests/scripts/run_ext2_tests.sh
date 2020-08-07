#!/bin/bash
#
# Runs the kernel and execute a ext2 test, and then parses the output
#
# Argument 1: Kernel image
# Argument 2: First disk image
# Argument 3: Second disk image
# Argument 4: test results directory

KERNEL=$1
DISK_ONE=$2
DISK_TWO=$3
TEST_PREFIX=$4
TEST_NAME=$5

QEMU_PID=
RET=

mkdir -p ./obj/$TEST_NAME

DISK_CPY=./obj/$TEST_NAME/disk_cpy.img
MAIN_DISK_CPY=./obj/$TEST_NAME/main_disk_cpy.img

cp $DISK_ONE $MAIN_DISK_CPY

. ./tests/scripts/colors.sh

TOTAL_RESULT=0

TESTS=$(find ./userspace/root/tests/ext2/ -name "test_*.sh" | xargs basename -a)

for test in $TESTS; do
    TEST_LOG=${TEST_PREFIX}/$(basename -s .sh $test).qemu.log
    TEST_ERR_LOG=${TEST_PREFIX}/$(basename -s .sh $test).qemu.err.log
    TEST_E2FSCK_LOG=${TEST_PREFIX}/$(basename -s .sh $test).e2fsck.log

    rm -fr $DISK_CPY
    cp $DISK_TWO $DISK_CPY

    rm -fr $TEST_LOG
    touch $TEST_LOG

    rm -fr $TEST_E2FSCK_LOG
    touch $TEST_E2FSCK_LOG

    result_str="EXT2: $TEST_NAME: $test:"

    timeout --foreground 120 qemu-system-i386 \
        -serial file:$TEST_LOG \
        -d cpu_reset \
        -m 512M \
        -drive format=raw,file=$MAIN_DISK_CPY,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_CPY,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "init=/tests/ext2/$test reboot_on_panic=1" \
        2> $TEST_ERR_LOG &

    wait $!
    RET=$?

    if [ "$RET" -ne "0" ]; then
        echo "QEMU TIMEOUT" >> "$TEST_LOG"

        result_str+="$RED QEMU TIMEOUT, DISK LIKELY NOT SYNCED, FAILURE!!$RESET ..."

        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    fi

    if [ ! -s "$TEST_LOG" ]; then
        result_str+="$RED KERNEL LOG EMPTY, AUTOMATIC FAILURE!!$RESET ..."

        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    fi

    if grep -q "PANIC" $TEST_LOG; then
        result_str+="$RED KERNEL PANIC, AUTOMATIC FAILURE!!$RESET ..."

        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    fi

    e2fsck -nfv $DISK_CPY > $TEST_E2FSCK_LOG 2>&1

    if [ "${PIPESTATUS[0]}" -ne "0" ]; then
        echo "EXT2 FAILURE" >> "$TEST_E2FSCK_LOG"

        result_str+="$RED EXT2 FILE SYSTEM ERRORS!$RESET"

        echo "$result_str"

        echo "e2fsck results:"
        cat $TEST_E2FSCK_LOG

        TOTAL_RESULT=$(($TOTAL_RESULT + 1))
    else
        echo "EXT2 PASS" >> "$TEST_E2FSCK_LOG"
        result_str+="$GREEN PASS!$RESET"

        echo "$result_str"
    fi
done

rm $DISK_CPY
rm $MAIN_DISK_CPY

exit $TOTAL_RESULT
