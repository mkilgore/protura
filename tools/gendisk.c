
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>

#include <protura/userspace_inc.h>
#include <fs/stat.h>
#include <fs/simple_fs.h>
#include <fs/dirent.h>
#include <protura/list.h>
#include <protura/userspace_inc_done.h>

#define SECTOR_SIZE 512

struct inode_desc {
    struct list_node node;
    struct simple_fs_disk_inode i;
    struct simple_fs_disk_inode_map map;
    int sectors;
    char *data;
};

struct disk_map {
    uint32_t inode_count;
    uint32_t inode_map_sector;
    uint32_t inode_root;
    struct list_head descs;

    uint32_t next_ino;
    uint32_t next_sector;
};

struct disk_map root = {
    .descs = LIST_HEAD_INIT(root.descs),
};

void add_inode_desc(struct disk_map *map, struct inode_desc *desc)
{
    map->inode_count++;
    list_add_tail(&map->descs, &desc->node);
}

void add_dir_entry(struct inode_desc *dir, const char *name, uint32_t ino)
{
    struct kdirent ent;
    uint32_t cur;

    strcpy(ent.name, name);
    ent.ino = ino;

    cur = dir->i.size;

    dir->i.size += sizeof(struct kdirent);
    dir->data = realloc(dir->data, dir->i.size);
    memcpy(dir->data + cur, &ent, sizeof(ent));
}

void read_file(struct inode_desc *file, int fd)
{
    char devtype;
    short major, minor;
    off_t len;

    len = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    file->i.size = len;
    file->i.mode = S_IFREG;
    file->i.major = 0;
    file->i.minor = 0;
    file->map.sector = root.next_sector++;
    file->data = malloc(len + 1);

    read(fd, file->data, len);

    file->data[len] = '\0';

    if (sscanf(file->data, "Device:%c %hd,%hd", &devtype, &major, &minor) == 3) {
        if (devtype == 'b')
            file->i.mode = S_IFBLK;
        file->i.major = major;
        file->i.minor = minor;
        free(file->data);
        file->data = NULL;
        file->i.size = 0;
        len = 0;
    }

    file->sectors = (len + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int i;

    for (i = 0; i < file->sectors; i++)
        file->i.sectors[i] = root.next_sector++;
}

uint32_t map_dir(uint32_t parent)
{
    struct inode_desc *inode_dir;
    struct dirent *ent, *prev;
    DIR *d = opendir(".");

    inode_dir = malloc(sizeof(*inode_dir));
    memset(inode_dir, 0, sizeof(*inode_dir));

    inode_dir->map.ino = root.next_ino++;
    inode_dir->map.sector = root.next_sector++;
    inode_dir->i.mode = S_IFDIR;

    prev = malloc(offsetof(struct dirent, d_name) + 255);

    while ((readdir_r(d, prev, &ent)) == 0 && ent != NULL) {
        uint32_t ino;

        if (strcmp(ent->d_name, "..") == 0) {
            ino = parent;
        } else if(strcmp(ent->d_name, ".") == 0) {
            ino = inode_dir->map.ino;
        } else if (ent->d_type == DT_REG) {
            struct inode_desc *f;
            int fd;

            f = malloc(sizeof(*f));
            memset(f, 0, sizeof(*f));

            f->map.ino = root.next_ino++;

            fd = open(ent->d_name, O_RDONLY);
            read_file(f, fd);
            close(fd);

            add_inode_desc(&root, f);
            ino = f->map.ino;
        } else if (ent->d_type == DT_DIR) {
            chdir(ent->d_name);
            ino = map_dir(inode_dir->map.ino);
            chdir("..");
        }

        add_dir_entry(inode_dir, ent->d_name, ino);
    }

    closedir(d);

    free(prev);

    inode_dir->sectors = (inode_dir->i.size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    int i;
    for (i = 0; i < inode_dir->sectors; i++)
        inode_dir->i.sectors[i] = root.next_sector++;

    add_inode_desc(&root, inode_dir);

    return inode_dir->map.ino;
}

void print_map(struct disk_map *map)
{
    struct inode_desc *d;

    printf("Files: %d\n", map->inode_count);
    printf("Inode sec: %d\n", map->inode_map_sector);

    list_foreach_entry(&map->descs, d, node) {
        printf("Desc: i:%d s:%d se:%d\n", d->map.ino, d->i.size, d->map.sector);
    }
}

void write_dir(int fd, struct disk_map *root)
{
    struct inode_desc *d;
    struct simple_fs_disk_sb sb;
    memset(&sb, 0, sizeof(sb));

    sb.inode_count = root->inode_count;
    sb.root_ino = root->inode_root;
    sb.inode_map_sector = root->inode_map_sector;

    ftruncate(fd, root->next_sector * SECTOR_SIZE);

    lseek(fd, 0, SEEK_SET);
    write(fd, &sb, sizeof(sb));

    lseek(fd, root->inode_map_sector * SECTOR_SIZE, SEEK_SET);
    list_foreach_entry(&root->descs, d, node)
        write(fd, &d->map, sizeof(d->map));

    list_foreach_entry(&root->descs, d, node) {
        lseek(fd, d->map.sector * SECTOR_SIZE, SEEK_SET);
        write(fd, &d->i, sizeof(d->i));

        if (!S_ISDIR(d->i.mode) && !S_ISREG(d->i.mode))
            continue;

        int i;
        for (i = 0; i < d->sectors; i++) {
            int len;
            lseek(fd, d->i.sectors[i] * SECTOR_SIZE, SEEK_SET);

            if (d->i.size - i * SECTOR_SIZE < SECTOR_SIZE)
                len = d->i.size - i * SECTOR_SIZE;

            write(fd, d->data + i * SECTOR_SIZE, len);
        }
    }
}

int main(int argc, char **argv)
{
    const char *dir;
    const char *file;
    int fd;

    if (argc < 3) {
        printf("%s: Missing arguments\n", argv[0]);
        printf("%s <directory> <file>\n", argv[0]);
        return 1;
    }

    dir = argv[1];
    file = argv[2];

    fd = open(file, O_CREAT | O_TRUNC | O_WRONLY, 0644);

    /* Sector 0 is the super-block, so we skip it */
    root.next_sector = 1;
    root.inode_map_sector = root.next_sector++;

    chdir(dir);
    root.inode_root = map_dir(root.next_ino);

    write_dir(fd, &root);
    close(fd);

    return 0;
}

