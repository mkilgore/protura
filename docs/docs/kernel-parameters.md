---
layout: post
title: "Kernel Parameters"
parent: Documentation
---

Kernel Testing
==============
{: .no_toc }

Protura has a decenter number of different parameters that can be passed to the
kernel via the "command line" provided by the bootloader. These different
options are all grouped and listed below.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Misc Kernel Parameters
----------------------

| Parameter | Default | Description |
| --- | --- | --- |
| `major` | CONFIG_ROOT_MAJOR | The major device number of the block device holding the root file-system |
| `minor` | CONFIG_ROOT_MINOR | The minor device number of the block device holding the root file-system |
| `fstype` | CONFIG_ROOT_FSTYPE | The file system type of the root file-system |
| `video` | `true` | When `false`, no video drivers will be loaded (The kernel will be text only) |
| `bdflush.delay` | CONFIG_BDFLUSH_DELAY | Number of seconds in-between syncs of the block cache |
| `reboot_on_panic` | `false` | If `true`, the kernel will attempt a reboot if a panic happens |

Kernel Log Level Parameters
---------------------------

These kernel parameters modify both which log messages are passed to debug
outputs (console, COM1, etc...), and what log levels those debug outputs
accept. This allows for a fairly flexable configuration. Ex. `klog` recieves
everything (`trace`), `console` only recieves warnings and errors, `COM1` gets
debug, etc... And you can also individual enabling logging for drivers or
subsystems like `ext2` or `net` so you get more verbose logging for those
systems without getting verbose logging for the rest of the kernel (Which can
get out of hand pretty quick).

As a general rule, the config keys in `./protura.conf` are generally set to
`KP_WARNING` in regards to restricting the log messaging from most of the
subsystems/drivers.

| Parameter | Default | Description |
| --- | --- | --- |
| `com1.loglevel` | `normal` | The max level of log message to write to the COM1 port |
| `console.loglevel` | `normal` | The max level of log message to write to the console |
| `ext2.loglevel` | CONFIG_EXT2_LOG_LEVEL | The max level of log messages from the ext2 driver |
| `elf.loglevel` | CONFIG_ELF_LOG_LEVEL | The max level of log messages from the ELF binfmt |
| `vfs.loglevel` | CONFIG_VFS_LOG_LEVEL | The max level of log messages from the VFS layer |

Kernel Testing Parameters
-------------------------

ktest is explained in detail on the [ktest API page](api/ktest.md).
