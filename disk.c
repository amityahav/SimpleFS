#include "disk.h"

Disk* open_disk(const char *path, int nblocks) {
    int fd = open(path, O_CREAT | O_WRONLY); 
    if (fd == -1) {
        perror("failed to open disk");
        return NULL;
    }

    if (lseek(fd, nblocks * BLOCK_SIZE - 1, SEEK_SET) == -1) {
        close(fd);
        perror("failed sterching disk image");
        return NULL;   
    }

    if (write(fd, "\0", 1) < 0) {
        close(fd);
        perror("failed writing to disk image file");
        return NULL;
    }

    Disk* disk = (Disk*)malloc(sizeof(disk));

    disk->fd = fd;

    return disk; 
}

bool write_to_disk(Disk *disk, int blocknum, char *data) {
    off_t offset = lseek(disk->fd, blocknum * BLOCK_SIZE, SEEK_SET);
    if (offset == -1) {
        perror("failed offsetting with the given offset");
        return false;
    }

    if (write(disk->fd, data, BLOCK_SIZE) == -1) {
        perror("failed to write block to disk");
        return true;
    }

    return true;
}

bool read_from_disk(Disk *disk, int blocknum, char *buff) {
    off_t offset = lseek(disk->fd, blocknum * BLOCK_SIZE, SEEK_SET);
    if (offset == -1) {
        perror("failed offsetting with the given offset");
        return false;
    }

    if (read(disk->fd, buff, BLOCK_SIZE) == -1) {
        perror("failed reading block from disk");
        return false;
    }

    return true;
}

void close_disk(Disk *disk) {
    close(disk->fd);
    free(disk);
}

int main(int agrc, char **argv) {
    // just for testing purposes
    const char *tmp_disk_path = "./disk_file";

    Disk* disk = open_disk(tmp_disk_path, 10);
    assert(disk != NULL);

    // assert size of disk
    struct stat st;
    stat(tmp_disk_path, &st);
    assert(st.st_size == 10 * BLOCK_SIZE);

    // assert proper writing/reading from disk
    char data[BLOCK_SIZE];
    for (int i = 0; i < BLOCK_SIZE; i++) {
        data[i] = 'x';
    }

    assert(write_to_disk(disk, 5, data));

    char buff[BLOCK_SIZE];
    assert(read_from_disk(disk, 5, buff));
    
    for (int i = 0; i < BLOCK_SIZE; i++) {
        assert(buff[i] == 'x');
    }

    // cleanup
    close_disk(disk);
    remove(tmp_disk_path);

    return 0;
}
