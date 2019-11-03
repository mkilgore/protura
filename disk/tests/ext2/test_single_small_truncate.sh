#!/bin/run_test

head -c 1 /dev/zero       > /mnt/slave/new_file1
head -c 10 /dev/zero      > /mnt/slave/new_file2
head -c 512 /dev/zero     > /mnt/slave/new_file3
head -c 513 /dev/zero     > /mnt/slave/new_file4
head -c 4096 /dev/zero    > /mnt/slave/new_file5
head -c 8192 /dev/zero    > /mnt/slave/new_file6
head -c 16384 /dev/zero   > /mnt/slave/new_file7
head -c 98304 /dev/zero   > /mnt/slave/new_file8
head -c 106496 /dev/zero  > /mnt/slave/new_file9
head -c 8486912 /dev/zero > /mnt/slave/new_file10
head -c 8495104 /dev/zero > /mnt/slave/new_file11

truncate /mnt/slave/new_file1
truncate /mnt/slave/new_file2
truncate /mnt/slave/new_file3
truncate /mnt/slave/new_file4
truncate /mnt/slave/new_file5
truncate /mnt/slave/new_file6
truncate /mnt/slave/new_file7
truncate /mnt/slave/new_file8
truncate /mnt/slave/new_file9
truncate /mnt/slave/new_file10
truncate /mnt/slave/new_file11
