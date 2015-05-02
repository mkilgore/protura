#ifndef INCLUDE_ARCH_CPU_H
#define INCLUDE_ARCH_CPU_H

#include <arch/task.h>
#include <arch/context.h>
#include <arch/gdt.h>

struct cpu_info {
    struct task *current;
    struct tss_entry tss;
    struct gdt_entry gdt_entries[GDT_ENTRIES];

    struct arch_context scheduler;

    struct cpu_info *cpu;
};


#define cpu_get_local() ((struct cpu_info *)({ void *__tld; \
                           asm ("movl %%gs:0, %0": "=r" (__tld)); \
                           __tld; }))

void cpu_set_kernel_stack(struct cpu_info *c, void *kstack);
void cpu_info_init(void);

#endif
