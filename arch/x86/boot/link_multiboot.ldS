
#include <mm/memlayout.h>

ENTRY(start)

SECTIONS
{
    . = KMEM_LINK;
    
    .multiboot ALIGN(8) : AT(ADDR(.multiboot) - KMEM_KBASE) {
        *(.multiboot)
    }

    .text : AT(ADDR(.text) - KMEM_KBASE) {
        *(.text)
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : AT(ADDR(.data) - KMEM_KBASE) {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss : AT(ADDR(.bss) - KMEM_KBASE) {
        *(COMMON)
        *(.bss)
        *(.bootstrap_stack)
    }

    kern_end = .;
}

