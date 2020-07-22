---
layout: post
title: "Kernel Testing"
parent: Documentation
---

Kernel Testing
==============

The kernel currently has two different testing frameworks, the internal kernel
testing framework, `ktest`, and also external test scripts located inside of
the `./userspace/root/tests` folder. The big difference between the two is that the
`ktest` framework supports tests that can run completely internally in the
kernel (mainly, but not always, unit tests). The test scripts are designed for
more end-to-end tests that run commands against the full kernel, and then
analyze some result. At the moment, the only test scripts are `ext2` tests,
which cover just about every `ext2` operation. The resulting disk after the
script is run is checked against `e2fsck` to verify it is still consistent.

To run tests on a built kernel, simply run the `check` target:

    make check

The tests are run via QEMU. You can also use `make debug-ktest` to start a
debug environment that will run all of the ktest tests automatically, and break
on any test failures.
