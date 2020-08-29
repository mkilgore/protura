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

| Parameter | Default | Description |
| --- | --- | --- |
| `com1.loglevel` | `normal` | The max level of log message to write to the COM1 port |
| `console.loglevel` | `normal` | The max level of log message to write to the console |

Kernel Testing Parameters
-------------------------

ktest is explained in detail on the [ktest API page](api/ktest.md).
