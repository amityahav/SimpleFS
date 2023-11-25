#include <stdio.h>
#include <string.h>

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
        printf("mount: failed reading super block from disk\n");
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

    fs->super = super;

    // create bitmap of free inodes
    fs->free_inodes = (char*)malloc(super.inodes_count * sizeof(char));

    // create bitmap of free blocks
    fs->free_blocks = (char*)malloc((super.nblocks - super.inblocks) * sizeof(char));
    memset(fs->free_blocks, 1, (super.nblocks - super.inblocks));

    // iterate over all inode blocks
    for (int i = 0; i < super.inblocks; i++) {
        if (!read_from_disk(disk, INODES_FIRST_BLOCK + i, block.data)) {
            printf("mount: failed reading inodes block from disk\n");
            return NULL;
        }

        for (int j = 0; j < INODES_PER_BLOCK; j++) {
            Inode* inode = &block.inodes[j];

            // scan inode's direct pointers for used blocks
            for (int l = 0; l < POINTERS_PER_INODE; l++) {
                // block is in-use
                if (inode->direct[l] != 0) {
                    int norm = inode->direct[l] - DATA_FIRST_BLOCK(super.nblocks);
                    fs->free_blocks[norm] = 0;
                }
            }

            // scam inode's indirect pointers for used blocks
            if (inode->indirect != 0) {
                int norm = inode->indirect - DATA_FIRST_BLOCK(super.nblocks);
                fs->free_blocks[norm] = 0;

                union Block indirect_block;
                if (!read_from_disk(disk, inode->indirect, indirect_block.data)) {
                    printf("mount: failed reading inode %d indirect block from disk\n", j);
                    return NULL;
                }

                for (int l = 0; l < POINTERS_PER_BLOCK; l++) {
                    if (indirect_block.pointers[l] != 0) {
                        int norm = indirect_block.pointers[l] - DATA_FIRST_BLOCK(super.nblocks);
                        fs->free_blocks[norm] = 0;
                    }
                }
            }

            fs->free_inodes[i * INODES_PER_BLOCK + j] = !inode->valid;
        }
    }

    mount(disk);

    fs->disk = disk;

    return fs;
}

void free_fs(FileSystem *fs) {
    fs->disk = NULL;
    free(fs->free_inodes);
    free(fs->free_blocks);
    free(fs);
}

ssize_t create_inode(FileSystem *fs) {
    // find first invalid inode and return its index
    for (int i = 0; i < fs->super.inodes_count; i++) {
        if(fs->free_inodes[i]) {
            fs->free_inodes = 0;
            return i;
        }
    }

    return -1;
}

bool remove_inode(FileSystem *fs, size_t inode_num) {
    if (inode_num < 0 || inode_num > fs->super.inodes_count) {
        return false;
    }

    if (fs->free_inodes[inode_num]) {
        return true // idempotent
    }


}

Inode* load_inode(FileSystem *fs, size_t inode_num) {
    union Block block;

    if (!read_from_disk(fs->disk, INODE_BLOCK(inode_num), block.data)) {
        printf("load_inode: failed to load inode %d to memory\n", inode_num);
        return NULL;
    }

    Inode inode = block.inodes[INODE_OFFSET_IN_BLOCK(inode_num)];

    Inode* new_inode = (Inode*)malloc(sizeof(Inode));
    memcpy(new_inode, &inode, sizeof(inode));

    return new_inode;
}