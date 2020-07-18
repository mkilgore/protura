#ifndef INCLUDE_PROTURA_EVENT_KEYBOARD_H
#define INCLUDE_PROTURA_EVENT_KEYBOARD_H

#include <uapi/protura/event/keyboard.h>
#include <protura/types.h>
#include <protura/fs/file.h>

void keyboard_event_queue_submit(int keysym, int is_release);

extern struct file_ops keyboard_event_file_ops;

#endif
