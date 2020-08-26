/*
 * Copyright (C) 2020 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */

#include <protura/types.h>
#include <protura/debug.h>
#include <protura/string.h>
#include <protura/kparam.h>

static int __video_is_enabled = 1;
KPARAM("video", &__video_is_enabled, KPARAM_BOOL);

int video_is_disabled(void)
{
    return !__video_is_enabled;
}

void video_mark_disabled(void)
{
    __video_is_enabled = 0;
}

void video_init(void)
{
    if (!__video_is_enabled)
        kp(KP_NORMAL, "Video output is disabled, text only!\n");
}
