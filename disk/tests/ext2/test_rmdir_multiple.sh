#!/bin/run_test

# first make a few levels of directories
mkdir /mnt/slave/foo
mkdir /mnt/slave/foo/bar
mkdir /mnt/slave/foo/bar/baz
mkdir /mnt/slave/foo/bar/baz/biz
mkdir /mnt/slave/foo/bar/baz/biz/buzz

# make a very long named directory
mkdir /mnt/slave/foo/bar/baz/biz/buzz/veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongname

# Make 32 levels of directories
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/
mkdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z/

# make a very large number of directories in the same directory
mkdir /mnt/slave/foo/bar/baz1
mkdir /mnt/slave/foo/bar/baz2
mkdir /mnt/slave/foo/bar/baz3
mkdir /mnt/slave/foo/bar/baz4
mkdir /mnt/slave/foo/bar/baz5
mkdir /mnt/slave/foo/bar/baz6
mkdir /mnt/slave/foo/bar/baz7
mkdir /mnt/slave/foo/bar/baz8
mkdir /mnt/slave/foo/bar/baz9
mkdir /mnt/slave/foo/bar/baz10
mkdir /mnt/slave/foo/bar/baz11
mkdir /mnt/slave/foo/bar/baz12
mkdir /mnt/slave/foo/bar/baz13

# remove a very long named directory
rmdir /mnt/slave/foo/bar/baz/biz/buzz/veryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryveryverylongname

# remove 32 levels of directories
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/
rmdir /mnt/slave/foo/bar/baz/biz/buzz/a/b/c/d/e/f/g/h/i/j/k/l/m/n/o/p/q/r/s/t/u/v/w/x/y/z/

# remove a very large number of directories in the same directory
rmdir /mnt/slave/foo/bar/baz1
rmdir /mnt/slave/foo/bar/baz2
rmdir /mnt/slave/foo/bar/baz3
rmdir /mnt/slave/foo/bar/baz4
rmdir /mnt/slave/foo/bar/baz5
rmdir /mnt/slave/foo/bar/baz6
rmdir /mnt/slave/foo/bar/baz7
rmdir /mnt/slave/foo/bar/baz8
rmdir /mnt/slave/foo/bar/baz9
rmdir /mnt/slave/foo/bar/baz10
rmdir /mnt/slave/foo/bar/baz11
rmdir /mnt/slave/foo/bar/baz12
rmdir /mnt/slave/foo/bar/baz13

rmdir /mnt/slave/foo/bar/baz/biz/buzz
rmdir /mnt/slave/foo/bar/baz/biz
rmdir /mnt/slave/foo/bar/baz
rmdir /mnt/slave/foo/bar
rmdir /mnt/slave/foo
