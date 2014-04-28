
#include <mm/memlayout.h>

ENTRY(start)

SECTIONS
{
    . = KMEM_LINK;

    .text : AT(V2P_WO(ADDR(.text))) {
        *(.text)
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : AT(V2P_WO(ADDR(.data))) {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss : AT(V2P_WO(ADDR(.bss))) {
        *(COMMON)
        *(.bss)
        *(.bootstrap_stack)
    }

    kern_end = .;
}

