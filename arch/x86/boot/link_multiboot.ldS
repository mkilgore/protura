
#include <protura/mm/memlayout.h>

ENTRY(start)

SECTIONS
{
    . = KMEM_LINK;

    /* multiboot gets it own section because it needs to be placed at the very
     * top of the executable */
    .text : AT(V2P_WO(ADDR(.text))) {
        *(.multiboot)
        *(.text)
        *(.rodata)
        *(.rodata.*)
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : AT(V2P_WO(ADDR(.data))) {
        *(.data)
        *(.debug*)
    }

    /* Read-write data (uninitialized) and stack */
    .bss : AT(V2P_WO(ADDR(.bss))) {
        *(COMMON)
        *(.bss)
        *(.bootstrap_stack)
    }

    .stab : {
        *(.stab)
    }
    kern_end = .;
}

