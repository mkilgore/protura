Design Overview
===============

Current existing components/subsystems:

- Memory Management
  - The kernel exists in the lower 1GB of memory, which is identity-mapped to the highest 1GB of virtual memory
  - Every page is represented by a `struct page`.
  - `palloc`: A buddy-allocator for physical pages.
  - `kmalloc`: A more general-purpose allocator for smaller-sized structures
    - Implemented by the combination of multiple `slab` allocators, which allocate fixed-sized objects.
    - The `slab` allocators are backed by `palloc`.
  - Uses `struct address_space` objects to represent the full memory layout of a process (IE. A full page-table).
    - Contains a list of `struct vm_map` objects, which represents a single mapped entity (memory-mapped file, anonymous mapping, etc.)
    - The `struct vm_map` objects are used to do dynamic loading of pages when they are used.
- Processes
  - Task switching is software-based, though the TSS has to be used to swap the stack pointer and segment
  - Every user-space process has a corresponding kernel thread.
  - Every process has it's own address-space, with the kernel mapped in the higher area
- Scheduler
  - Very simple round-robin design
  - All tasks exist on the same list, which is continually looped over
  - Supports fork() and exec() for loading and executing new programs
- Block device layer
  - Currently, IDE is the only block device
  - A block cache sits in between the file systems and block devices
    - A `bdflush` daemon runs in the kernel and periodically syncs blocks to disk
    - `sync()` can also be used to sync the blocks and FS on demand.
  - Supports MBR partition table
  - No I/O Scheduler, so each I/O block request is submitted one at a time.
- Char device layer
  - Basic devices like /dev/null, /dev/zero, and /dev/full
- File System
  - Supports a combined VFS filesystem, with one device at the root, and other file systems can be mounted at various points
  - Current supported filesystems:
    - EXT2
      - Support is more or less complete, though support for most extra EXT2 features is not there yet.
      - Inode and Block allocation lacks an algorithm to prevent fragmentation.
      - There's a small test suite that verifies the FS modifications.
    - Proc
      - A virtual file system device that exposes various kernel information and APIs
  - Supports executing ELF and #! programs.
- TTY Layer
  - A TTY layer exists, with support for the most common `termios` settings
  - There is a "VT" layer for implementing generic VT102 support
    - This understands a fair amount of the VT102 escape sequences, and ncurses includes a terminfo entry for Protura
  - Currently no PTY support, the only supported TTYs are serial and keyboard/console
- PCI
  - Currently supports a small set of PCI devices, though the basic framework is there to support more
  - Current supported devices:
    - E1000 (Network card)
    - RTL8139 (Network card)
    - PIIX3 (chipset, IDE DMA Controller)
- Network Layer
  - IPv4 support is fairly full featured
    - An internal IPv4 routing table exists, along with userspace APIs for implementing `route`
      - ARP is used to discover external MAC addresses according to the configured routes
    - Another userspace API existing for network interfaces, which is used to implement `ifconfig`
    - Supports several protocol selections:
      - UDP
        - Pretty simple, this is more or less complete
      - "Raw" IP
        - This protocol recieves a copy of any packet matching the supplied protocol ID.
        - Used internally and externally to implement ICMP
      - TCP
        - About half-done, the 3-way TCP handshake and data rx/tx on an established connection are complete
        - TCP connection closing is not yet implemented, so closing the socket drops the connection without actually closing it
        - Retransmission of sent packets is not yet implemented, but the framework is there and timers are implemented.
        - Listening sockets are not implemented
  - Loopback device

External OS components:

- i686-protura-gcc
  - A version of GCC that is ported to i686-protura
    - Note it's lots of versions behind at the moment because I haven't udpated it
- protura-newlib
  - A version of newlib with lots of extra code to provide Protura support
    - Provides a full `libc` along with lots of extra Unix/POSIX syscalls/interfaces that are not implemented in `newlib`.
- /bin/init
  - A very basic init program that can parse `/etc/inittab` to determine what programs to start on boot.
  - Also runs `/etc/rc.local`
  - Does not support `/etc/fstab` (Or rather, `mount` does not support `/etc/fstab`).
    - Automatically mounts `proc` at `/proc`.
  - Sets up a few environment variables like the `PATH`.
- Coreutils
  - See `userspace/coreutils` folder.
- klogd
  - A daemon that reads from /dev/kmsg and writes the contents to a persistent log on the disk at /var/log/klog

Extra ported utilities:

- ncurses
  - Includes a terminfo entry for Protura
- vim
  - Compiles the 'normal' feature set, which supports a fair amount of stuff.
    - Can also be compiled as `tiny`. This reduces load time, which is fairly slow.
- ed
  - Mostly just useful before `vim` was ported.
- less
  - Really nice for viewing files, much nicer to use then `more` provided by our coreutils

