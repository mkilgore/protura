/*
 * Copyright (C) 2019 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_KLOG_H
#define INCLUDE_PROTURA_KLOG_H

#include <protura/types.h>
#include <protura/fs/file.h>

void klog_init(void);

extern const struct file_ops klog_file_ops;

#endif
