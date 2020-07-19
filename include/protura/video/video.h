#ifndef INCLUDE_PROTURA_VIDEO_VIDEO_H
#define INCLUDE_PROTURA_VIDEO_VIDEO_H

int video_is_disabled(void);

/* This is used on bootup if for some reason the video drivers should not be
 * loaded. The typical reason would be the bootloader providing graphics, in
 * which case you may not know which adaptor and don't want to initialize it a
 * second time. */
void video_mark_disabled(void);

void video_init(void);

#endif
