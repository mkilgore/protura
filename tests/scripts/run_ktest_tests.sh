#!/bin/bash
#
# Runs the kernel with tests on, and then parses the output
#
# Argument 1: Kernel image
# Argument 2: First disk image
# Argument 3: Second disk image
# Argument 4: test results directory
# Argument 5: Extra kernel arguments

KERNEL=$1
DISK_ONE=$2
DISK_TWO=$3
TEST_RESULTS_DIR=$4
EXTRA_TEST_ARGS=$5

QEMU_PID=
RET=

. ./tests/scripts/colors.sh

function test_run {
    TEST_LOG=$2
    args=""

    . $1

    qemu-system-i386 \
        $args \
        -d cpu_reset \
        -display none \
        -no-reboot \
        -kernel $KERNEL \
        -append "ktest.run=true ktest.reboot_after_run=true $EXTRA_TEST_ARGS" \
        2> "$3" &

    QEMU_PID=$!
}

function test_verify {
    unset GREP_COLORS
    tail --pid $QEMU_PID -n+1 -f $TEST_LOG | GREP_COLOR="1;32" grep --line-buffered --color=always -E "PASS|$" | GREP_COLOR="1;31" grep --line-buffered --color=always -E "FAIL|PANIC|$"

    wait $QEMU_PID

    ./tests/scripts/parse_ktest_output.pl < $1
    RET=$?
}

TOTAL_RESULT=0

TESTS=$(find ./tests/testcases/ktest/ -name "*.args" | xargs basename -a -s .args)

for test in $TESTS; do
    TEST_ARGS=./tests/testcases/ktest/$test.args
    TEST_QEMU_LOG=$TEST_RESULTS_DIR/$test.qemu.log
    TEST_QEMU_ERR_LOG=$TEST_RESULTS_DIR/$test.qemu.err.log

    echo "Running test environment: $test"

    rm -fr $TEST_QEMU_LOG
    touch $TEST_QEMU_LOG

    rm -fr $TEST_QEMU_ERR_LOG
    touch $TEST_QEMU_ERR_LOG

    test_run "$TEST_ARGS" "$TEST_QEMU_LOG" "$TEST_QEMU_ERR_LOG"
    test_verify "$TEST_QEMU_LOG"

    TOTAL_RESULT=$(($TOTAL_RESULT + $RET))
done

if [ "$TOTAL_RESULT" == "0" ]; then
    echo "${GREEN}ALL TESTS PASSED!$RESET"
else
    echo "${RED}TESTS FAILURE!$RESET"
fi

exit $TOTAL_RESULT
