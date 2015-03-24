#ifndef INCLUDE_ARCH_CPU_H
#define INCLUDE_ARCH_CPU_H

#include <arch/task.h>
#include <arch/context.h>
#include <arch/gdt.h>

struct cpu_info {
    struct task *current;
    struct tss_entry tss;
    struct gdt_entry gdt_entries[GDT_ENTRIES];

    struct cpu_info *cpu;
};

extern struct cpu_info *curcpu asm("%gs:0");

void cpu_set_kernel_stack(struct cpu_info *c, void *kstack);
void cpu_info_init(void);

#endif
