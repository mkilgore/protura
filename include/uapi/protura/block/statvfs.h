#ifndef __INCLUDE_UAPI_PROTURA_BLOCK_STATVFS_H__
#define __INCLUDE_UAPI_PROTURA_BLOCK_STATVFS_H__

#include <protura/types.h>

struct statvfs {
    unsigned long f_bsize; /* Filesystem block size */
    unsigned long f_frsize; /* Fragment size */

    __kfsblkcnt_t f_blocks; /* Size of fs in f_frsize units */
    __kfsblkcnt_t f_bfree; /* Number of free blocks */
    __kfsblkcnt_t f_bavail; /* Number of free blocks for unprivileges users */

    __kfsfilcnt_t f_files; /* Number of inodes */
    __kfsfilcnt_t f_ffree; /*Number of free inodes */
    __kfsfilcnt_t f_favail; /* Number of free inodes for unprivileges users */

    __kuint32_t f_fsid; /* Filesystem ID - See below */
    __kuint32_t f_flag; /* Mount flags */
    __kuint32_t f_namemax; /* Maximum filename length */
};

#define FSID_EXT2 0x0000ef53

#endif
