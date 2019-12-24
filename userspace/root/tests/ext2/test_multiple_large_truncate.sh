#!/bin/run_test

head -c 10000000 /dev/zero > /mnt/slave/new_file1
truncate /mnt/slave/new_file1

head -c 10000000 /dev/zero > /mnt/slave/new_file2
truncate /mnt/slave/new_file2

head -c 10000000 /dev/zero > /mnt/slave/new_file3
truncate /mnt/slave/new_file3

head -c 10000000 /dev/zero > /mnt/slave/new_file4
truncate /mnt/slave/new_file4
