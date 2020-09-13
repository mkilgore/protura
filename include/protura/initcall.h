#ifndef INCLUDE_INITCALL_H
#define INCLUDE_INITCALL_H

/* Used to forward declare an initcall in a header
 * This is used to expose the initcall for taking a dependency on it */
#define extern_initcall(name) \
    void __init_##name (void)

/* Declares a function to be an initcall by that name.
 * This does not provide any extra dependency information. */
#define initcall(name, fn) \
    void __init_##name (void) __attribute__((alias(#fn)))

/* Declares that a paticular initcall has a dependency on another initcall.
 *
 * The extra pointers serve to ensure the initcall dependencies actually exist.
 * This would eventually get caught at linking time, but this gives you the
 * error earlier at compile time and thus tells you the specific file with the
 * error. */
#define initcall_dependency(init, dep) \
    static void __attribute__((used, section(".discard.initcall.ptrs"))) (*TP(__test_ptr, __COUNTER__)) (void) = &__init_##dep; \
    static void __attribute__((used, section(".discard.initcall.ptrs"))) (*TP(__2test_ptr, __COUNTER__)) (void) = &__init_##init; \
    static char __attribute__((used, section(".discard.initcall.deps"), aligned(1))) TP(__dependency_info, __COUNTER__)[] = #dep " " #init

#define initcall_core(name, fn) \
    initcall(name, fn); \
    initcall_dependency(name, core); \
    initcall_dependency(subsys, name)

#define initcall_subsys(name, fn) \
    initcall(name, fn); \
    initcall_dependency(name, subsys); \
    initcall_dependency(device, name)

#define initcall_device(name, fn) \
    initcall(name, fn); \
    initcall_dependency(name, device)

/* The NULL terminated list of initcalls to run on boot */
extern void (*initcalls[]) (void);

/* These are defined hooks during the boot sequence you can take a depedency or
 * be a 'part-of', to ensure you run with certain facilities setup */
extern_initcall(core);
extern_initcall(subsys);
extern_initcall(device);

#endif
