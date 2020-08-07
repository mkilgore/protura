#!/bin/run_test

head -c 10000 /dev/zero   > /mnt/slave/new_file1
head -c 10000 /dev/zero   > /mnt/slave/new_file2
head -c 51200 /dev/zero   > /mnt/slave/new_file3
head -c 51300 /dev/zero   > /mnt/slave/new_file4
head -c 4096 /dev/zero    > /mnt/slave/new_file5
head -c 8192 /dev/zero    > /mnt/slave/new_file6
head -c 16384 /dev/zero   > /mnt/slave/new_file7
head -c 98304 /dev/zero   > /mnt/slave/new_file8
head -c 106496 /dev/zero  > /mnt/slave/new_file9
head -c 8486912 /dev/zero > /mnt/slave/new_file10
head -c 8495104 /dev/zero > /mnt/slave/new_file11

truncate -s 1000 /mnt/slave/new_file1
truncate -s 1000 /mnt/slave/new_file2
truncate -o -s 10 /mnt/slave/new_file3
truncate -o -s 10 /mnt/slave/new_file4
truncate -s 100 /mnt/slave/new_file5
truncate -o -s 1 /mnt/slave/new_file6
truncate -o -s 3 /mnt/slave/new_file7
truncate -o -s 5 /mnt/slave/new_file8
truncate -o -s 20 /mnt/slave/new_file9
truncate -o -s 400 /mnt/slave/new_file10
truncate -o -s 1050 /mnt/slave/new_file11
