
add-symbol-file ./imgs/protura_x86_multiboot 0xC0100000
directory ./src
directory ./src/fs
directory ./src/kernel
directory ./src/mm
directory ./arch/x86
directory ./arch/x86/drivers
directory ./arch/x86/kernel
directory ./arch/x86/mm
target remote localhost:1234
layout split
layout regs

