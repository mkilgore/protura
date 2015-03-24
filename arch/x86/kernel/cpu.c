
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>

#include <arch/asm.h>
#include <arch/gdt.h>
#include <arch/cpu.h>

static struct cpu_info cpu;

static void loadgs(uint16_t gs)
{
    asm volatile("movw %0, %%gs": : "r" (gs));
}

static void cpu_gdt(struct cpu_info *c)
{
    c->gdt_entries[_GDT_NULL] = (struct gdt_entry){ 0 };
    c->gdt_entries[_KERNEL_CS_N] = GDT_ENTRY(GDT_TYPE_EXECUTABLE | GDT_TYPE_READABLE, 0, 0xFFFFFFFF, GDT_DPL_KERNEL);
    c->gdt_entries[_KERNEL_DS_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, 0, 0xFFFFFFFF, GDT_DPL_KERNEL);
    c->gdt_entries[_USER_CS_N] = GDT_ENTRY(GDT_TYPE_EXECUTABLE | GDT_TYPE_READABLE, 0, 0xFFFFFFFF, GDT_DPL_USER);
    c->gdt_entries[_USER_DS_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, 0, 0xFFFFFFFF, GDT_DPL_USER);

    /* Setup CPU-local variable */
    c->gdt_entries[_CPU_VAR_N] = GDT_ENTRY(GDT_TYPE_WRITABLE, (uintptr_t)&c->cpu, sizeof(&c->cpu) - 1, GDT_DPL_KERNEL);

    /* Setup CPU-tss */
    c->gdt_entries[_GDT_TSS_N] = GDT_ENTRY16(GDT_STS_T32A, (uintptr_t)&c->tss, sizeof(c->tss) - 1, GDT_DPL_KERNEL);
    c->gdt_entries[_GDT_TSS_N].des_type = 0;

    gdt_flush(c->gdt_entries, sizeof(c->gdt_entries));

    loadgs(_CPU_VAR);
}

static void cpu_tss(struct cpu_info *c)
{
    memset(&c->tss, 0, sizeof(c->tss));
    c->tss.iomb = sizeof(c->tss);
}

void cpu_set_kernel_stack(struct cpu_info *c, void *kstack)
{
    /* We rewrite this GDT entry since old TSS will have a type of 'TSS busy'.
     * We just overwrite it with a new TSS segment to make sure it's right
     * again. */
    c->gdt_entries[_GDT_TSS_N] = GDT_ENTRY16(GDT_STS_T32A, (uintptr_t)&c->tss, sizeof(c->tss) - 1, GDT_DPL_KERNEL);
    c->gdt_entries[_GDT_TSS_N].des_type = 0;

    c->tss.ss0 = _KERNEL_DS;
    c->tss.esp0 = kstack;

    ltr(_GDT_TSS);
}

void cpu_info_init(void)
{
    cpu_gdt(&cpu);
    cpu_tss(&cpu);
    cpu.cpu = &cpu;
}

