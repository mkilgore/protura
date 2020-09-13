
#include <protura/types.h>
#include <protura/debug.h>
#include <protura/initcall.h>

static void core_init(void)
{
    kp(KP_NORMAL, "init: Core\n");
}
initcall(core, core_init);

static void arch_init(void)
{
    kp(KP_NORMAL, "init: Arch\n");
}
initcall(arch, arch_init);

static void subsys_init(void)
{
    kp(KP_NORMAL, "init: Subsys\n");
}
initcall(subsys, subsys_init);

static void device_init(void)
{
    kp(KP_NORMAL, "init: Device\n");
}
initcall(device, device_init);

initcall_dependency(arch, core);
initcall_dependency(subsys, arch);
initcall_dependency(device, subsys);

