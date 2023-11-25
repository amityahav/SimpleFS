#include "disk.h"

Disk* open_disk(const char *path, int nblocks) {
    int fd = open(path, O_CREAT | O_RDWR); 
    if (fd == -1) {
        perror("open_disk: failed to open disk");
        return NULL;
    }

    if (lseek(fd, BLOCK_OFFSET(nblocks) - 1, SEEK_SET) == -1) {
        close(fd);
        perror("open_disk: failed sterching disk image");
        return NULL;   
    }

    if (write(fd, "\0", 1) < 0) {
        close(fd);
        perror("open_disk: failed writing to disk image file");
        return NULL;
    }

    Disk* disk = (Disk*)malloc(sizeof(disk));

    disk->fd = fd;
    disk->nblocks = nblocks;
    disk->mounted = false;

    return disk; 
}

bool write_to_disk(Disk *disk, int blocknum, char *data) {
    off_t offset = lseek(disk->fd, BLOCK_OFFSET(blocknum), SEEK_SET);
    if (offset == -1) {
        perror("write_to_disk: failed offsetting with the given offset");
        return false;
    }

    if (write(disk->fd, data, BLOCK_SIZE) == -1) {
        perror("write_to_disk: failed to write block to disk");
        return true;
    }

    return true;
}

bool read_from_disk(Disk *disk, int blocknum, char *buff) {
    off_t offset = lseek(disk->fd, BLOCK_OFFSET(blocknum), SEEK_SET);
    if (offset == -1) {
        perror("read_from_disk: failed offsetting with the given offset");
        return false;
    }

    if (read(disk->fd, buff, BLOCK_SIZE) == -1) {
        perror("read_from_disk: failed reading block from disk");
        return false;
    }

    return true;
}

void mount(Disk *disk) {
    disk->mounted = true;
}

void unmount(Disk *disk) {
    disk->mounted = false;
}

void close_disk(Disk *disk) {
    close(disk->fd);
    free(disk);
}