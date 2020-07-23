---
layout: post
title: "Kernel Debugging"
parent: Documentation
author: "Matt Kilgore"
---

Debugging Protura
=================
{: .no_toc }

Protura has decently mature debugging facilities, in the form of fairly
easy-to-use debugging environments when developing on Linux. All of them are
accessible via `make` and are described below.

## Table of contents
{: .no_toc .text-delta }

1. TOC
{:toc}

---

Makefile Usage
--------------

| Makefile Command | Description |
| :--------------: | :---------: |
| `make debug` | Simplest environment. `gdb`, 80x25 video console, serial console |
| `make debug-ktest` | Same as `debug`, but runs the ktest suite on startup, breaks on test failures. See the [ktest documentation](api/ktest.md) for more arguments. |
| `make debug-vga` | Forgoes the 80x25 video console for an external window with real video output |
| `make debug-net` | Similar to `debug`, but also wires up `netcat` and `tcpdump` listening on a random port, for testing TCP connections |

The dependencies required for each environment are listed below:

| Dependency | Description |
| :--------------: | :---------: |
| `qemu-system-i386` | `qemu` is the main way of testing Protura. We use lots of features from it, with the main win being the ease of scriptability in regards to new environments and configurations. |
| `tmux` | The entire environment is spun up in a `tmux` environment to make setup and teardown simple and repeatable with the same layout each time. Note that some `tmux` settings in your `~/.tmux.conf` may interfere with the script (`base-index` in particular) |
| `gdb` | `gdb` is the main debugger used for Protura development. It hooks into `qemu` and thus you can control the starting and stopping of `qemu` from `gdb` (And set breakpoints, example tasks and stack traces, etc...) |
| `gdb-dashboard` | Optional, but very useful. `gdb-dashboard` is a `gdb` plugin which can display a colorful display of the current system state in a separate console. This is configured automatically. |
| `netcat` | For network debugging, `debug-net` only |
| `tcpdump` | For network debugging, `debug-net` only |

Setting up the Tap Network Device
---------------------------------

Testing and debugging networking requires setting up a `tap` network device on
Linux. If this is not setup, then `qemu` will give you an error such as "tap0"
does not exist, or a permissions issue trying to access it. To fix this, you
can run the `./scripts/ci/setup_tuntap.sh` script, which will setup the `tap0`
interface for you with the correct ip settings for the defaults Protura
provides in `./userspace/root/etc/rc.local`.

gdb Quick Start Guide
---------------------

`gdb` can be fairly daunting at first, but it's not really that hard to use it for some basic testing. A few useful `qemu` commands along with some custom Protura ones are below:

| Command | Custom | Description |
| :-----: | :----: | :---------: |
| `cont` | No | Continues execution of the VM. Note that `qemu` starts in this state, so to start `qemu` after doing `make debug` you do a `cont`. |
| `^C` | No | Doing a Ctrl-C in the Qemu debug window will stop execution and allow you to inspect the system |
| `bt` | No | Prints a backtrace of where the CPU is currently executing |
| `print X` | No | outputs the result of the expression `X`, which more or less be any valid C expression |
| `frame X` | No | Switches the current stack frame selected, which allows you to print local variables from that frame |
| `list_tasks` | Yes | Lists in order the names and PIDs of every task Protura is running |
| `task1`, `task2`, ... `task20` | Yes | These identifiers refer to the `struct task` values for the entries listed by `list_tasks` |
| `switch_task X` | Yes | Switches the current stack and instruction pointer to match those of the task X. This allows `bt` and friends to display a stack trace of task X instead of the currently running task. X should be a task identifier (`task1`, `task2`, ...) |
| `end_switch_task` | Yes | Undoes the `switch_task` command (as long as `switch_task` was only done once!) |

