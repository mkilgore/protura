
add-symbol-file ./imgs/protura_x86_multiboot 0xC0100000
add-symbol-file ./protura.o 0xC0100000
target remote localhost:1234
layout split


