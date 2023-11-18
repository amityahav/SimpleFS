#include <stdio.h>
#include "fs.h"

bool format(Disk* disk) {
    if (disk->mounted) {
        printf("format: there's a filesystem mounted already on the disk.\n");
        return false;
    }

    // clean any data already presented on disk
    char zeros[BLOCK_SIZE] = {0};
    for (int i = 0; i < disk->nblocks; i++) {
        if (!write_to_disk(disk, i, zeros)) {
            printf("format: failed cleaning disk\n"); 
            return false;
        }
    }

    union Block block;

    // create super block and persist
    block.super.magic_number = MAGIC_NUMBER;
    block.super.nblocks = disk->nblocks;
    block.super.inblocks = NUMBER_OF_INODE_BLOCKS(disk->nblocks);
    block.super.inodes_count = NUMBER_OF_INODE_BLOCKS(disk->nblocks) * INODES_PER_BLOCK;

    if (!write_to_disk(disk, SUPER_BLOCK_OFFSET, block.data)) {
        printf("format: failed writing super block to disk\n");
        return false;
    }

    return true;
}

FileSystem* mount_fs(Disk* disk) {
    union Block block;

    // read super block from disk
    if (!read_from_disk(disk, SUPER_BLOCK_NUMBER, block.data)) {
        printf("mount: failed reading super block from disk");
        return NULL;
    }

    SuperBlock super = block.super;

    // validate magic number
    if (super.magic_number != MAGIC_NUMBER) {
        printf("mount: magic number is invalid\n");
        return NULL;
    }

    // create new filesystem
    FileSystem *fs = (FileSystem*)malloc(sizeof(FileSystem));

    // create bitmap of free inodes
    fs->free_inodes = (char*)malloc(super.inodes_count * sizeof(char));

    // iterate over all inode blocks
    for (int i = 0; i < super.inblocks; i++) {
        if (!read_from_disk(disk, INODES_FIRST_BLOCK + i, block.data)) {
            printf("mount: failed reading inodes block from disk\n");
            return NULL;
        }

        // iterate over all inodes in the block and build the bitmap
        for (int j = 0; j < INODES_PER_BLOCK; j++) {
            fs->free_inodes[i * INODES_PER_BLOCK + j] = block.inodes[j].valid;
        }
    }

    mount(disk);

    return fs;
}

void free_fs(FileSystem *fs) {
    free(fs->free_inodes);
    free(fs);
}