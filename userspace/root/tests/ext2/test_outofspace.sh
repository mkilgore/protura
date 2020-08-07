#!/bin/run_test

# 300MB worth of files, should introduce a 'no space left of disk' condition
# That condition should not result in disk corruption or crashing though.
head -c 100000000 /dev/zero > /mnt/slave/new_file1
head -c 100000000 /dev/zero > /mnt/slave/new_file2
head -c 100000000 /dev/zero > /mnt/slave/new_file3
