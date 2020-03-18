#ifndef INCLUDE_PROTURA_REBOOT_H
#define INCLUDE_PROTURA_REBOOT_H

#include <uapi/protura/reboot.h>

int sys_reboot(int magic1, int magic2, int cmd);

#endif
