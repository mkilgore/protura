#ifndef SRC_DRIVERS_CHAR_VT_INTERNAL_H
#define SRC_DRIVERS_CHAR_VT_INTERNAL_H

#include <protura/stdarg.h>
#include <protura/drivers/vt.h>

struct vt_kp_output {
    struct kp_output output;
    struct vt *vt;
};

void vt_early_init(struct vt *vt);
void vt_init(struct vt *vt);

void vt_early_printf(struct kp_output *output, const char *fmt, va_list lst);
int vt_write(struct vt *vt, const char *buf, size_t len);
int vt_tty_write(struct tty *tty, const char *buf, size_t len);

#endif
