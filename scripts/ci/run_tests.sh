#!/bin/bash
#
# Runs the kernel with tests on, and then parses the output

rm ./tests.log
touch ./tests.log

echo "Running tests on QEMU..."
qemu-system-i386 \
    -serial file:./tests.log \
    -d cpu_reset \
    -drive format=raw,file=./disk.img,cache=none,media=disk,index=0,if=ide \
    -drive format=raw,file=./disk2.img,cache=none,media=disk,index=1,if=ide \
    -display none \
    -no-reboot \
    -kernel ./imgs/protura_x86_multiboot \
    -append "ktest.run=true ktests.reboot_after_run=true" \
    2> /dev/null &

QEMU_PID=$!

unset GREP_COLORS
tail --pid $QEMU_PID -n+1 -f ./tests.log | GREP_COLOR="1;32" grep --line-buffered --color=always -E "PASS|$" | GREP_COLOR="1;31" grep --line-buffered --color=always -E "FAIL|PANIC|$"

wait $QEMU_PID

exec ./scripts/ci/parse_test_output.pl < ./tests.log
