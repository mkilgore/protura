/*
 * Copyright (C) 2015 Matt Kilgore
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License v2 as published by the
 * Free Software Foundation.
 */
#ifndef INCLUDE_PROTURA_USERSPACE_INC_H
#define INCLUDE_PROTURA_USERSPACE_INC_H

#define off_t     koff_t
#define pid_t     kpid_t
#define mode_t    kmode_t
#define dev_t     kdev_t
#define sector_t  ksector_t
#define ino_t     kino_t
#define umode_t   kumode_t
#define dirent    kdirent
#define size_t    ksize_t

#undef S_IFREG
#undef S_IFDIR
#undef S_IFBLK
#undef S_IFMT

#endif
