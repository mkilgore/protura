#!/bin/bash
#
# Runs the kernel with tests on, and then parses the output

echo "Running tests on QEMU..."
qemu-system-i386 \
    -serial file:./tests.log \
    -d cpu_reset \
    -drive format=raw,file=./disk.img,cache=none,media=disk,index=0,if=ide \
    -drive format=raw,file=./disk2.img,cache=none,media=disk,index=1,if=ide \
    -net nic,model=rtl8139 \
    -net tap,ifname=tap0,script=no,downscript=no \
    -display none \
    -no-reboot \
    -kernel ./imgs/protura_x86_multiboot \
    -append "ktest.run=true ktests.reboot_after_run=true" \
    2> /dev/null

# cat ./tests.log
unset GREP_COLORS
GREP_COLOR="1;32" grep --color=always -E "PASS|$" ./tests.log | GREP_COLOR="1;31" grep --color=always -E "FAIL|PANIC|$"

exec ./scripts/ci/parse_test_output.pl < ./tests.log
