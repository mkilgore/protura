#ifndef INCLUDE_ARCH_SIGNAL_H
#define INCLUDE_ARCH_SIGNAL_H

#include <uapi/arch/signal.h>

#define SIG_BIT(x) (F((x) - 1))
#define SIG_UNBLOCKABLE (SIG_BIT(SIGKILL) | SIG_BIT(SIGSTOP))

#include <protura/bits.h>
#define SIGSET_SET(set, sig) bit_set(set, sig - 1)
#define SIGSET_UNSET(set, sig) bit_clear(set, sig - 1)

typedef void (*sighandler_t) (int);

struct irq_frame;
struct task;

/* Handles any pending signals on the current task */
int signal_handle(struct task *current, struct irq_frame *iframe);
void sys_sigreturn(struct irq_frame *iframe);

extern char *x86_trampoline_code;
extern uint32_t x86_trampoline_len;

#endif
