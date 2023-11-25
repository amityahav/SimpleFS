#include <assert.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>

#define BLOCK_SIZE 4096
#define BLOCK_OFFSET(blocknum) blocknum * BLOCK_SIZE

typedef struct {
    
    // file descriptor representing the disk image
    int fd;

    // total number of blocks in the disk image
    int nblocks;

    // indicator whether there is a filesystem mounted on the disk
    bool mounted;

} Disk;

// opens a new emulated disk at the given path of size BLOCK_SIZE * nblocks.
Disk* open_disk(const char *path, int nblocks);

// writes data block to the given disk at block #blocknum.
bool write_to_disk(Disk *disk, int blocknum, char *data);

// reads data block from the given disk at block #blocknum into buff.
bool read_from_disk(Disk *disk, int blocknum, char *buff);

// mount an arbitrary filesystem to disk.
void mount(Disk *disk);

// unmount existing filesystem (if any).
void unmount(Disk *disk);

// closes the given disk and free all of its resources.
void close_disk(Disk *disk);