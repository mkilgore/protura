Project Structure
=================

The Protura OS project structure is described below:

- `toolchain`
  - Submodule to the `protura-toolchain` git repository
- `docs`
  - Documentation on the OS
- `include`
  - Global header files for the kernel
- `src`
  - source of the kernel
- `arch`
  - architecture-specific kernel headers and source
- `scripts`
  - Holds generic scripts, such as build helper scripts, debug scripts, and some CI specific scripts
- `userspace`
  - `coreutils`
    - Contains implementations of a number of "coreutil" programs. Also includes more important programs like `init` and `sh`
  - `extras`
    - Contains "extra" programs, like `vim` and `less`, which can be optionally compiled as part of the OS disk image
  - `utils`
    - Extra random utility programs, mostly test programs.
  - `root`
    - Contains the "root" of the disk's file system. This is combined along with the contents of `./obj/disk_root`.
    - `misc`
      - Files in here are ignored by `git`, which makes it an easy spot to drop files you want present in the created disk

When building the OS, the below directories are creating as needed:

- `bin`
  - `imgs`
    - Will contain created full disk images
  - `kernel`
    - Will contain the compiled kernel
  - `test_results`
    - Will contain some logs from tests, if they are run
  - `logs`
    - Contains logs from debug sessions
  - `toolchain`
    - Contains the compiled toolchain
- `obj`
  - `disk_mount`
    - Mountpoint for disk images when they are being created
  - `disk_root`
    - Contains the base filesystem for the disk images. The cross-compiler toolchain references libraries from this location, and any compiled programs get placed into this location
