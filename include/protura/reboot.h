#ifndef INCLUDE_PROTURA_REBOOT_H
#define INCLUDE_PROTURA_REBOOT_H

#define PROTURA_REBOOT_MAGIC1 0xABCDBEEF
#define PROTURA_REBOOT_MAGIC2 152182804

#define PROTURA_REBOOT_RESTART 0x12341234

#ifdef __KERNEL__
int sys_reboot(int magic1, int magic2, int cmd);
#endif

#endif
