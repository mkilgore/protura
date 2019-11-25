#ifndef SRC_DRIVERS_CHAR_VT_INTERNAL_H
#define SRC_DRIVERS_CHAR_VT_INTERNAL_H

#include <protura/stdarg.h>
#include <protura/drivers/vt.h>

#define SCR_DEF_FORGROUND (SCR_WHITE)
#define SCR_DEF_BACKGROUND (SCR_BLACK)

struct vt_kp_output {
    struct kp_output output;
    struct vt *vt;
};

void vt_early_init(struct vt *vt);
void vt_init(struct vt *vt);

void vt_early_printf(struct kp_output *output, const char *fmt, va_list lst);
int vt_write(struct vt *vt, const char *buf, size_t len);
int vt_tty_write(struct tty *tty, const char *buf, size_t len);

void __vt_clear_color(struct vt *vt, uint8_t color);
void __vt_clear_to_end(struct vt *vt);
void __vt_clear_to_cursor(struct vt *vt);
void __vt_clear_line_to_end(struct vt *vt);
void __vt_clear_line_to_cursor(struct vt *vt);
void __vt_clear_line(struct vt *vt);
void __vt_clear(struct vt *vt);
void __vt_updatecur(struct vt *vt);
void __vt_scroll(struct vt *vt, int lines);
void __vt_scroll_from_cursor(struct vt *vt, int lines);
void __vt_scroll_up_from_cursor(struct vt *vt, int lines);
void __vt_shift_left_from_cursor(struct vt *vt, int chars);
void __vt_shift_right_from_cursor(struct vt *vt, int chars);

#endif
