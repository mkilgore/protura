#!/bin/bash
#
# Runs the kernel and execute a pci test, and then checks the output
#
# Argument 1: Kernel image
# Argument 2: First disk image
# Argument 3: Second disk image
# Argument 4: test results directory

KERNEL=$1
DISK_ONE=$2
DISK_TWO=$3
TEST_PREFIX=$4

QEMU_PID=
RET=
IGNORED=

. ./tests/scripts/colors.sh

# Arg 1: Argument file
# Arg 2: Qemu debug log
# Arg 3: Test output
function run_ext2_test {
    args=""

    . $1

    if [ "$IGNORED" -ne "0" ]; then
        return
    fi

    timeout 120 qemu-system-i386 \
        -serial file:$2 \
        -debugcon file:$3 \
        -d cpu_reset \
        -drive format=raw,file=$DISK_ONE,cache=none,media=disk,index=0,if=ide \
        -drive format=raw,file=$DISK_TWO,media=disk,index=1,if=ide \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "init=/tests/pci/dump_pci.sh reboot_on_panic=1" \
        $args \
        2> "$4" &

    QEMU_PID=$!
}

function dump_kernel_log {
    unset GREP_COLORS

    cat "$1" | GREP_COLOR="1;32" grep --line-buffered --color=always -E "PASS|$" | GREP_COLOR="1;31" grep --line-buffered --color=always -E "FAIL|PANIC|$"
}

TOTAL_RESULT=0

TESTS=$(find ./tests/testcases/pci/ -name "*.args" | xargs basename -a -s .args)

for test in $TESTS; do
    TEST_ARGS=./tests/testcases/pci/$test.args
    TEST_EXPECTED=./tests/testcases/pci/$test.table

    TEST_QEMU_LOG=${TEST_PREFIX}/$test.qemu.log
    TEST_QEMU_ERR_LOG=${TEST_PREFIX}/$test.qemu.err.log
    TEST_OUTPUT_LOG=${TEST_PREFIX}/$test.output.log

    printf "Running PCI test: $test ..."

    rm -fr $TEST_QEMU_LOG
    touch $TEST_QEMU_LOG

    rm -fr $TEST_OUTPUT_LOG
    touch $TEST_OUTPUT_LOG

    IGNORED=0

    run_ext2_test "$TEST_ARGS" "$TEST_QEMU_LOG" "$TEST_OUTPUT_LOG" "$TEST_QEMU_ERR_LOG"

    if [ "$IGNORED" -eq "0" ]; then
        wait $QEMU_PID
        fail=0
        RET=$?

        if [ "$RET" -ne "0" ]; then
            echo "QEMU TIMEOUT" >> "$TEST_QEMU_LOG"
            echo "$RED QEMU TIMEOUT, FAILURE!!$RESET"

            echo "Kernel log:"
            dump_kernel_log "$TEST_QEMU_LOG"

            TOTAL_RESULT=$(($TOTAL_RESULT + 1))
            fail=1
        fi

        if ! cmp -s -- $TEST_EXPECTED $TEST_OUTPUT_LOG; then
            echo "$RED Incorrect table!$RESET"

            echo "Expected:"
            cat $TEST_EXPECTED | sed -e 's/^/    /'

            echo "Actual:"
            cat $TEST_OUTPUT_LOG | sed -e 's/^/    /'

            TOTAL_RESULT=$(($TOTAL_RESULT + 1))
            fail=1
        fi

        if [ "$fail" -eq "0" ]; then
            echo "$GREEN PASS!$RESET"
        fi
    else
        echo "$YELLOW Ignored.$RESET"
    fi
done

if [ "$TOTAL_RESULT" == "0" ]; then
    echo "${GREEN}ALL TESTS PASSED!$RESET"
else
    echo "${RED}TESTS FAILURE!$RESET"
fi

exit $TOTAL_RESULT
