
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include <fs/simple_fs.h>
#include <fs/dirent.h>

/* Root directory sector */
int root_files = 0;
struct kdirent root_ents[512 / sizeof(struct kdirent)];

void read_dir(const char *dir)
{
    struct dirent *ent;
    DIR *d = opendir(dir);

    while ((ent = readdir(d))) {
        if (strcmp(ent->d_name, "..") == 0 || strcmp(ent->d_name, ".") == 0)
            continue;
        strncpy(root_ents[root_files].name, ent->d_name, sizeof(root_ents[root_files].name));
        root_files++;
    }

    closedir(d);
}

int main(int argc, char **argv)
{
    char sector[512];
    struct simple_fs_disk_sb sb;
    struct simple_fs_disk_inode inode;
    int fd;

    fd = open("./disk.img", O_CREAT | O_TRUNC | O_WRONLY, 0644);

    read_dir("./disk");

    sb.file_count = root_files;
    sb.root_ino = 1;

    memset(sector, 0, sizeof(sector));
    memcpy(sector, &sb, sizeof(sb));

    write(fd, sector, sizeof(sector));

    memset(sector, 0, sizeof(sector));
    memset(&inode, 0, sizeof(inode));

    inode.size = root_files * sizeof(struct kdirent);
    inode.sectors[0] = 2;

    memcpy(sector, &inode, sizeof(inode));
    write(fd, sector, sizeof(sector));

    /* Reserve space for the root directory entries */
    memset(sector, 0, sizeof(sector));

    write(fd, sector, sizeof(sector));

    chdir("./disk");

    int i;
    int cur_sector = 3;

    for (i = 0; i < root_files; i++) {
        int file = open(root_ents[i].name, O_RDONLY);
        size_t size;

        memset(&inode, 0, sizeof(inode));

        lseek(file, 0, SEEK_END);
        size = lseek(file, 0, SEEK_CUR);
        lseek(file, 0, SEEK_SET);

        inode.size = size;
        inode.sectors[0] = cur_sector + 1;

        root_ents[i].ino = cur_sector;

        memset(sector, 0, sizeof(sector));
        memcpy(sector, &inode, sizeof(inode));
        write(fd, sector, sizeof(sector));
        cur_sector++;

        memset(sector, 0, sizeof(sector));
        read(file, sector, sizeof(sector));
        write(fd, sector, sizeof(sector));
        cur_sector++;
    }

    chdir("..");

    lseek(fd, 1024, SEEK_SET);

    memcpy(sector, root_ents, sizeof(root_ents));
    write(fd, sector, sizeof(sector));

    close(fd);

    return 0;
}

