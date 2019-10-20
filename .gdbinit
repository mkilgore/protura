
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

dashboard -layout expressions stack assembly source !threads !memory !history
dashboard stack -style limit 5


macro define offsetof(_type, _memb) ((long)(&((_type *)0)->_memb))
macro define container_of(_ptr, _type, _memb) ((_type *)((void *)(_ptr) - offsetof(_type, _memb)))
