#ifndef INCLUDE_ARCH_CPU_H
#define INCLUDE_ARCH_CPU_H

#include <arch/context.h>
#include <arch/gdt.h>

struct task;

struct cpu_info {
    int cpu_id;
    int intr_count;
    int reschedule;

    struct task *current;
    struct tss_entry tss;
    struct gdt_entry gdt_entries[GDT_ENTRIES];

    struct arch_context scheduler;
    struct task *kidle;

    /* This is actually a self-referencial pointer that just points back to the
     * containing cpu_info. It's useful for implementing cpu local data,
     * because the GDT entry refers to the location of the address of the cpu_info object.
     * This is an easy place to store that address since we know we'll have a
     * cpu_info per cpu, and we need one of these pointers per cpu. The GDT
     * entry refers to the address of this 'cpu' entry in this struct */
    struct cpu_info *cpu;
};


#define cpu_get_local() ((struct cpu_info *)({ void *__tld; \
                           asm ("movl %%gs:0, %0": "=r" (__tld)); \
                           __tld; }))

void cpu_set_kernel_stack(struct cpu_info *c, void *kstack);
void cpu_start_scheduler(void);
void cpu_init_early(void);
void cpu_info_init(void);
void cpu_setup_idle(void);

#endif
