#ifndef INCLUDE_FS_STAT_H
#define INCLUDE_FS_STAT_H

/* Regular file */
#define S_IFREG 0100000
#define S_IFDIR 0040000

#define S_ISREG(m) (((m) & S_IFREG) == S_IFREG)
#define S_ISDIR(m) (((m) & S_IFDIR) == S_IFDIR)

#endif
