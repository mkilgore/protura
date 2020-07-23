---
layout: post
title: "Kernel Testing"
parent: Documentation
---

Kernel Testing
==============
{: .no_toc }

Protura has a fair amount of different test suites that run as part of the CI
process and can be run locally. This page details how to run them and details on each one.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Running Test Suites via make
----------------------------

All of the test suites can be run via make. Some of them have extra
dependencies that need to be installed beforehand as noted below, but generally
speaking they should just work:

| Makefile command | Description |
| :--------------: | :---------: |
| `check` | Runs every test suite the kernel has |
| `check-kernel` | Runs the ktest test suite, which are compiled as part of the kernel. More information about ktest is [here](api/ktest.md) |
| `check-uapi` | Verifies the uapi headers do not reference any headers not in the uapi |
| `check-ext2` | Runs tests against the ext2 driver and verifies the resulting disk is not corrupted |
| `check-pci` | Verifies the PCI enumeration code |
| `check-symbol-table` | Verifies the symbol table compiled into the kernel matches the one the kernel actually has |

Test suites
-----------

Everything used as part of the testing is located in the `./tests` directory. That then splits into a `Makefile` which actually kicks off the tests, a `./scripts` directory containing most of the logic for the tests, and a `./testcases` directory containing test case information used by the scripts. The details of each test suite are explained in further detail below.

### ktest

ktest is explained in detail on the [ktest API page](api/ktest.md).

### uapi

The uapi is the collection of user-facing headers that are shared with the
kernel. Effectively, these are the headers installed on the system when `make
install-kernel-headers` is run. The design is such that every uapi header has a
non-uapi counter-part, such that if header `uapi/protura/types.h` exists, then
header `protura/types.h` must *also* exist. The first line in `protura/types.h`
includes the uapi version of the header, which ensures the correct order of the
kernel-specific and non-kernel-specific definitions.

In the uapi headers, they do *not* include other uapi headers, because the uapi
prefix will not be maintained when `make install-kernel-headers` is run.
Instead, they simply include the headers without the uapi prefix, which will do
the right thing both for userspace code and kernel code - for kernel code, it
pulls in all the kernel-specific stuff, for userspace code, it just includes
the uapi header (Which is now not located in uapi after being copied onto the
filesystem).

The catch is that, when compiling the kernel, it is possible a uapi header is
missing but the kernel version exists. That will compile file for the kernel
(Since the uapi headers will just include the kernel version) but break for
userspace because it will then be trying to include the corresponding uapi
header that doesn't exist.

The solution is a simple script that utilizes `gcc` and sets up the proper
include locations to mimic what userspace will see. We then run `gcc
-fsyntax-only` on every header and verify that `gcc` can parse it correctly. If
a header is missing at this point, `gcc` will complain when it can't find it
and the test will fail.

### ext2

The ext2 tests verify the behavior of the ext2 driver by running several
operations against it and then verifying the resulting disk's ext2 file system
is still valid. The validity is checked by using `e2fsck` on the disk after
Protura has modified it and verifying that `e2fsck` did not find any errors
with the disk.

The test cases are split into two separate sets:

1. `./tests/testcases/test-disks`
  - Each entry in this directory is a script that generates an ext2 file system
    with various different parameters (such as disk size or inode size). All of
    the ext2 testcases are run against every test disk.
2. `./userspace/root/tests/ext2`
  - These scripts run commands to modify the file system in various ways that
    will hopefully introduces errors if they are there. They run all the
    various file system operations single or multiple times, and in small sizes
    or large sizes, or a combination, etc.

As noted, every test in `./userspace/root/tests/ext2` is run against every test
disk.

### PCI

Comparatively, the PCI tests are pretty simple. The kernel contains an endpoint
called `/proc/pci_devices` which outputs the IDs of every PCI devices the
kernel was able to find during bootup. The testcases in `./tests/testcases/pci`
are different sets of `qemu` flags which create a different set of PCI devices
and bridges. The test then runs `qemu` and gets the contents of
`/proc/pci_devices` back the kernel, and verifies it matches what we expected
for the arguments we passed to `qemu`.

### Symbol Table

The kernel contains a built-in symbol table containing the names and addresses
of every symbol in the kernel. To achieve this, there is a fairly convoluted
process of linking the kernel multiple times and generating multiple symbol
tables to end on the right one. This step takes the final kernel, generates a
symbol table from it, and verifies the symbol we got from it matches the one
that was linked in, which tells us the final linking step didn't change the
address or add any new symbols. Generally speaking, it is not expected this
will ever fail, but this catches the situation where the compiler does
something silly, or a new situation came up that we didn't account for, and the
symbol table is no longer accurate.
