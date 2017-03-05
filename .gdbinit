
add-symbol-file ./imgs/protura_x86_multiboot.sym 0xC0100000
directory ./src
directory ./src/fs
directory ./src/kernel
directory ./src/mm
directory ./src/net
directory ./src/pci
directory ./arch/x86
directory ./arch/x86/drivers
directory ./arch/x86/kernel
directory ./arch/x86/mm
target remote localhost:1234
#layout split
#layout regs
set pagination off

dashboard -output /tmp/dash_fifo

dashboard -layout expressions registers stack assembly source !threads !memory !history
dashboard stack -style limit 5

