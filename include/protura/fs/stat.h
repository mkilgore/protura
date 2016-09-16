#ifndef INCLUDE_FS_STAT_H
#define INCLUDE_FS_STAT_H

#ifdef __KERNEL__
# include <protura/types.h>
# include <protura/time.h>
#endif

struct stat {
    uint32_t st_dev;
    uint32_t st_ino;
    uint16_t st_mode;
    uint16_t st_nlink;
    uint16_t st_uid;
    uint16_t st_gid;
    uint32_t st_rdev;
    uint32_t st_size;

    time_t st_atime;
    time_t st_mtime;
    time_t st_ctime;
    uint32_t st_blksize;
    uint32_t st_blocks;
};

/* mode flags */
#define	_IFMT 0170000	/* type of file */
#define	_IFDIR 0040000	/* directory */
#define	_IFCHR 0020000	/* character special */
#define	_IFBLK 0060000	/* block special */
#define	_IFREG 0100000	/* regular */
#define	_IFLNK 0120000	/* symbolic link */
#define	_IFSOCK 0140000	/* socket */
#define	_IFIFO 0010000	/* fifo */

#define S_IFMT  _IFMT
#define S_IFREG _IFREG
#define S_IFDIR _IFDIR
#define S_IFBLK _IFBLK
#define S_IFCHR _IFCHR
#define S_IFLNK _IFLNK
#define S_IFSOCK _IFSOCK
#define S_IFIFO _IFIFO

#define S_ISREG(m) (((m) & S_IFMT) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFMT) == S_IFDIR)
#define S_ISBLK(m) (((m) & S_IFMT) == S_IFBLK)
#define S_ISCHR(m) (((m) & S_IFMT) == S_IFCHR)
#define S_ISLNK(m) (((m) & S_IFMT) == S_IFLNK)
#define S_ISSOCK(m) (((m) & S_IFMT) == S_IFSOCK)
#define S_ISFIFO(m) (((m) & S_IFMT) == S_IFIFO)

#define	S_IRWXU 	(S_IRUSR | S_IWUSR | S_IXUSR)
#define		S_IRUSR	0000400	/* read permission, owner */
#define		S_IWUSR	0000200	/* write permission, owner */
#define		S_IXUSR 0000100/* execute/search permission, owner */
#define	S_IRWXG		(S_IRGRP | S_IWGRP | S_IXGRP)
#define		S_IRGRP	0000040	/* read permission, group */
#define		S_IWGRP	0000020	/* write permission, grougroup */
#define		S_IXGRP 0000010/* execute/search permission, group */
#define	S_IRWXO		(S_IROTH | S_IWOTH | S_IXOTH)
#define		S_IROTH	0000004	/* read permission, other */
#define		S_IWOTH	0000002	/* write permission, other */
#define		S_IXOTH 0000001/* execute/search permission, other */

#define MODE_TO_DT(mode) (((mode) >> 12) & 15)

#define DT_UNKNOWN 0
#define DT_CHR     MODE_TO_DT(_IFCHR)
#define DT_DIR     MODE_TO_DT(_IFDIR)
#define DT_BLK     MODE_TO_DT(_IFBLK)
#define DT_REG     MODE_TO_DT(_IFREG)
#define DT_LNK     MODE_TO_DT(_IFLNK)
#define DT_FIFO    MODE_TO_DT(_IFIFO)

#endif
