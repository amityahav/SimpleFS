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
        printf("mount_fs: failed reading super block from disk\n");
        return NULL;
    }

    SuperBlock super = block.super;

    // validate magic number
    if (super.magic_number != MAGIC_NUMBER) {
        printf("mount_fs: magic number is invalid\n");
        return NULL;
    }

    // create new filesystem
    FileSystem *fs = (FileSystem*)malloc(sizeof(FileSystem));

    fs->super = super;

    // create bitmap of free inodes
    fs->free_inodes = (char*)malloc(super.inodes_count * sizeof(char));

    // create bitmap of free blocks
    fs->free_blocks = (char*)malloc(NUMBER_OF_DATA_BLOCKS(super.nblocks) * sizeof(char));
    memset(fs->free_blocks, true, NUMBER_OF_DATA_BLOCKS(super.nblocks));

    // iterate over all inode blocks
    for (int i = 0; i < super.inblocks; i++) {
        if (!read_from_disk(disk, INODES_FIRST_BLOCK + i, block.data)) {
            printf("mount_fs: failed reading inodes block from disk\n");
            return NULL;
        }

        for (int j = 0; j < INODES_PER_BLOCK; j++) {
            Inode* inode = &block.inodes[j];

            // scan inode's direct pointers for used blocks
            for (int l = 0; l < POINTERS_PER_INODE; l++) {
                // block is in-use
                if (inode->direct[l] != 0) {
                    int norm = inode->direct[l] - DATA_FIRST_BLOCK(super.nblocks);
                    fs->free_blocks[norm] = false;
                }
            }

            // scan inode's indirect pointers for used blocks
            if (inode->indirect != 0) {
                int norm = inode->indirect - DATA_FIRST_BLOCK(super.nblocks);
                fs->free_blocks[norm] = false;

                union Block indirect_block;
                if (!read_from_disk(disk, inode->indirect, indirect_block.data)) {
                    printf("mount_fs: failed reading inode %d indirect block from disk\n", j);
                    return NULL;
                }

                for (int l = 0; l < POINTERS_PER_BLOCK; l++) {
                    if (indirect_block.pointers[l] != 0) {
                        int norm = indirect_block.pointers[l] - DATA_FIRST_BLOCK(super.nblocks);
                        fs->free_blocks[norm] = false;
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

ssize_t block_alloc(FileSystem *fs) {
    for (int i = 0; i < NUMBER_OF_DATA_BLOCKS(fs->super.nblocks); i++) {
        if (fs->free_blocks[i]) {
            fs->free_blocks[i] = false;
            return DATA_FIRST_BLOCK(fs->super.nblocks) + i;
        }
    }

    return -1;
}

bool block_dealloc(FileSystem *fs, int block_num) {
    char zeros[BLOCK_SIZE] = {0};

    if (!write_to_disk(fs->disk, block_num, zeros)) {
        printf("block_dealloc: failed deallocating block %d\n", block_num);
        return false;
    }

    int norm = block_num - DATA_FIRST_BLOCK(fs->super.nblocks);
    fs->free_blocks[norm] = true;

    return true;
}

void free_fs(FileSystem *fs) {
    fs->disk = NULL;
    free(fs->free_inodes);
    free(fs->free_blocks);
    free(fs);
}

ssize_t create_inode(FileSystem *fs) {
    union Block block;

    for (int i = 0; i < fs->super.inodes_count; i++) {
        if(fs->free_inodes[i]) {
            Inode *inode = load_inode(fs, i, &block);
            if (inode == NULL) {
                printf("create_inode: failed loading inode %d\n", i);
                return -1;
            }

            inode->valid = true;

            if (!save_inode(fs, inode, i, &block)) {
                printf("create_inode: failed saving inode %d\n", i);
                free(inode);
                return -1;
            }

            free(inode);
            fs->free_inodes[i] = false;
            return i;
        }
    }

    return -1;
}

ssize_t stat_inode(FileSystem *fs, size_t inode_num) {
    union Block block;
    ssize_t size = 0;

    Inode *inode = load_inode(fs, inode_num, &block);
    if (inode == NULL) {
        printf("stat: failed to load inode\n");
        return -1;
    }

    size = inode->size;

    free(inode);

    return size;
}

bool remove_inode(FileSystem *fs, size_t inode_num) {
    if (inode_num < 0 || inode_num > fs->super.inodes_count) {
        return false;
    }

    if (fs->free_inodes[inode_num]) {
        return true; // idempotent
    }

    union Block inodes_block;
    Inode* inode = load_inode(fs, inode_num, &inodes_block);
    if (inode == NULL) {
        printf("load_inode: failed to load inode %ld into memory\n", inode_num);
        return false;
    }

    // free direct pointers if any
    for (int i = 0; i < POINTERS_PER_INODE; i++) {
        if (inode->direct[i] != 0) {
            if (!block_dealloc(fs, inode->direct[i])) {
                printf("remove_inode: failed cleaning block %d for inode %ld\n", inode->direct[i], inode_num);
                free(inode);
                return false;
            }

            inode->direct[i] = 0;
        }
    }

    // free indirect pointer and indirect blocks if any
    if (inode->indirect != 0) {
        union Block indirect_block; 

        if (!read_from_disk(fs->disk, inode->indirect, indirect_block.data)) {
            printf("remove_inode: failed reading indirect block for inode %ld\n", inode_num);
            free(inode);
            return false;
        }

        for (int i = 0; i < POINTERS_PER_BLOCK; i++) {
            if (indirect_block.pointers[i] != 0) { // free indirect blocks
                if (!block_dealloc(fs, indirect_block.pointers[i])) {
                    printf("remove_inode: failed cleaning block %d for inode %ld\n", indirect_block.pointers[i], inode_num);
                    free(inode);
                    return false;
                }
            }
        }

        // free indirect block
        if (!block_dealloc(fs, inode->indirect)) {
            printf("remove_inode: failed cleaning block %d for inode %ld\n", inode->indirect, inode_num);
            free(inode);
            return false;
        }

        inode->indirect = 0;
    }

    inode->size = 0;
    inode->valid = 0;

    if (!save_inode(fs, inode, inode_num, &inodes_block)) {
        printf("remove_inode: failed saving inode %ld on disk\n", inode_num);
        return false;
    }

    fs->free_inodes[inode_num] = true;

    free(inode);

    return true;
}

Inode* load_inode(FileSystem *fs, size_t inode_num, union Block *block) {
    if (!read_from_disk(fs->disk, INODE_BLOCK(inode_num), block->data)) {
        printf("load_inode: failed to load inodes block for inode %ld\n", inode_num);
        return NULL;
    }

    Inode inode = block->inodes[INODE_OFFSET_IN_BLOCK(inode_num)];

    Inode* new_inode = (Inode*)malloc(sizeof(Inode));
    memcpy(new_inode, &inode, sizeof(inode));

    return new_inode;
}

bool save_inode(FileSystem *fs, Inode *inode, size_t inode_num, union Block *block) {
   block->inodes[INODE_OFFSET_IN_BLOCK(inode_num)] = *inode;
   
   if (!write_to_disk(fs->disk, INODE_BLOCK(inode_num), block->data)) {
        printf("save_inode: failed to save inode's block for inode %ld\n", inode_num);
        return false;
   }

   return true;
}

ssize_t read_from_inode(FileSystem *fs, size_t inode_num, char *data, size_t length, size_t offset) {
    union Block block;

    Inode *inode = load_inode(fs, inode_num, &block);
    if (inode == NULL) {
        printf("read_from_inode: failed to load inode %ld\n", inode_num);
        return -1;
    }

    if (!inode->valid) {
        printf("read_from_inode: inode %ld is invalid\n", inode_num);
        free(inode);
        return -1;
    }

    if (offset >= inode->size) {
        printf("read_from_inode: inode %ld size is less than the given offset %ld\n", inode_num, offset);
        free(inode);
        return -1;
    }

    // fix length in case we surpass the inode's size
    if (offset + length >= inode->size) {
        length = inode->size - offset;
    }
   
    size_t starting_block = offset / BLOCK_SIZE;
    size_t ending_block = (offset + length) / BLOCK_SIZE;
    size_t current_block = starting_block;
    ssize_t n = 0;
    union Block indirect_block;
    bool loaded = false;

    while (current_block <= ending_block) {
        uint32_t bp = 0;
        size_t off = 0;
        size_t s = 0;

        if (current_block < POINTERS_PER_INODE) {
            bp = inode->direct[current_block];
        } else {
            if (!loaded) {
                // if inode's indirect block is not allocated,
                // then we know we've read all we can from this inode
                if (!inode->indirect) {
                    break;
                }

                // load indirect block only once
                if (!read_from_disk(fs->disk, inode->indirect, indirect_block.data)) {
                    printf("read_from_inode: failed reading indirect block\n");
                    free(inode);
                    return -1;
                }

                loaded = true;
            }

            if (current_block - POINTERS_PER_INODE >= POINTERS_PER_BLOCK) {
                printf("read_from_inode: trying to read from an out of bounds pointer in indirect block for inode %ld\n", inode_num);
                break;
            }

            bp = indirect_block.pointers[current_block - POINTERS_PER_INODE];
        }

        // if current block is not allocated,
        // skip to the next one. in case of a sparse file 
        // this unallocated block should be treated as a block filled with zeros
        // and be copied on the fly to the caller's buffer. 
        // this is not implemented currently.
        if (!bp) {
            current_block++;
            continue;
        }

        if (!read_from_disk(fs->disk, bp, block.data)) {
            printf("read_from_inode: failed reading block of inode %ld", inode_num);
            free(inode);
            return -1;
        }

        s = BLOCK_SIZE <= length ? BLOCK_SIZE : length;
        off = current_block == starting_block ? offset % BLOCK_SIZE : 0;

        // adjust s in case we overflow from the current block
        s = s + off >= BLOCK_SIZE ? BLOCK_SIZE - off : s;

        memcpy(data + n, block.data + off, s);

        n += s;
        length -= s;
        current_block++;
    }

    return n;
}

// TODO: update inode's size upon writing
ssize_t write_to_inode(FileSystem *fs, size_t inode_num, char *data, size_t length, size_t offset) {
    union Block inode_block;

    Inode *inode = load_inode(fs, inode_num, &inode_block);
    if (inode == NULL) {
        printf("write_to_inode: failed loading inode %ld\n", inode_num);
        return -1;
    }

    if (!inode->valid) {
        printf("write_to_inode: inode %ld is invalid\n", inode_num);
        free(inode);
        return -1;
    }

    size_t starting_block = offset / BLOCK_SIZE;
    size_t ending_block = (offset + length) / BLOCK_SIZE;
    size_t current_block = starting_block;
    size_t n = 0;
   //uint32_t new_size = inode->size;
    bool loaded = false;
    bool inode_modified = false;
    bool indirect_modified = false;
    union Block indirect_block;

    while (current_block <= ending_block) {
        union Block cb;
        ssize_t block_ptr = 0;
        uint32_t bp = 0;
        size_t off = 0;
        size_t s = 0;

        if (current_block < POINTERS_PER_INODE) {
            if (!inode->direct[current_block]) {
                block_ptr = block_alloc(fs);
                if (block_ptr == -1) {
                    printf("write_to_inode: disk is full\n");
                    break;
                }

                inode->direct[current_block] = block_ptr;
                inode->size += BLOCK_SIZE;
                inode_modified = true;
            }

            bp = inode->direct[current_block] = block_ptr;
        } else {
            if (!inode->indirect) {
                // indirect node is not allocated
                block_ptr = block_alloc(fs);
                if (block_ptr == -1) {
                    printf("write_to_inode: disk is full\n");
                    break;
                }

                inode->indirect = block_ptr;
                inode_modified = true;
            } 
            
            if (!loaded) {
                // load indirect node to memory
                if (!read_from_disk(fs->disk, inode->indirect, indirect_block.data)) {
                    printf("write_to_inode: failed reading indirect block\n");
                    free(inode);
                    return -1;
                }

                loaded = true;
            }

            if (current_block - POINTERS_PER_INODE >= POINTERS_PER_BLOCK) {
                printf("write_to_inode: trying to write to an out of bounds pointer in indirect block for inode %ld\n", inode_num);
                break;    
            }

            if (!indirect_block.pointers[current_block - POINTERS_PER_INODE]) {
                block_ptr = block_alloc(fs);
                if (block_ptr == -1) {
                    printf("write_to_inode: disk is full\n");
                    break;
                }

                indirect_block.pointers[current_block - POINTERS_PER_INODE] = block_ptr;
                inode->size += BLOCK_SIZE;
                indirect_modified = true;
            }          
            
            bp = indirect_block.pointers[current_block - POINTERS_PER_INODE];
        }

        s = BLOCK_SIZE <= length ? BLOCK_SIZE : length;
        off = current_block == starting_block ? offset % BLOCK_SIZE : 0;

        // adjust s in case we overflow from the current block
        s = s + off >= BLOCK_SIZE ? BLOCK_SIZE - off : s;

        char *b = data + n;

        if (off > 0 || s < BLOCK_SIZE) {
            // perform read-modify-write in cases where we want to update only a part
            // of a data block. caused by the fact that we only operate with BLOCK_SIZE granularity
            if (!read_from_disk(fs->disk, bp, cb.data)) {
                printf("write_to_inode: failed reading data block of inode %ld\n", inode_num);
                free(inode);
                return -1;
            }

            memcpy(cb.data + off, data + n, s);

            b = cb.data;
        }

        if (!write_to_disk(fs->disk, bp, b)) {
            printf("write_to_inode: failed writing data block of inode %ld\n", inode_num);
            free(inode);
            return -1;
        }

        n += s;
        length -= s;
        current_block++;
    }

    // writing modified indirect block back to disk
    if (indirect_modified && inode->indirect > 0 && !write_to_disk(fs->disk, inode->indirect, indirect_block.data)) {
        printf("write_to_inode: failed writing indirect block for inode %ld\n", inode_num);
        free(inode);
        return -1;
    }

    // write modified inode back to disk
    if (inode_modified && !save_inode(fs, inode, inode_num, &inode_block)) {
        printf("write_to_inode: failed saving inode %ld\n", inode_num);
        free(inode);
        return -1;
    }

    free(inode);

    return n;
}
