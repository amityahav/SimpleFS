#include "disk.h"

#include <stdint.h>

#define MAGIC_NUMBER 0xf0f03410
#define INODES_PER_BLOCK (BLOCK_SIZE / 32)
#define POINTERS_PER_INODE 5
#define POINTERS_PER_BLOCK (BLOCK_SIZE / 4)
#define NUMBER_OF_INODE_BLOCKS(nblocks) 0.1 * nblocks
#define SUPER_BLOCK_NUMBER 0
#define INODES_FIRST_BLOCK 1
#define DATA_FIRST_BLOCK(nblocks) INODES_FIRST_BLOCK + NUMBER_OF_INODE_BLOCKS(nblocks)
#define SUPER_BLOCK_OFFSET BLOCK_OFFSET(SUPER_BLOCK_NUMBER)
#define INODE_BLOCKS_OFFSET BLOCK_OFFSET(INODES_FIRST_BLOCK)
#define INODE_BLOCK(inode_num) INODES_FIRST_BLOCK + inode_num / INODES_PER_BLOCK
#define INODE_OFFSET_IN_BLOCK(inode_num) inode_num / INODES_PER_BLOCK

typedef struct SuperBlock {

    // a signature for the file system
    uint32_t magic_number;

    // total number of blocks on the disk
    uint32_t nblocks;

    // total number of blocks reserved for storing inodes
    uint32_t inblocks;

    // total number of inodes
    uint32_t inodes_count;

} SuperBlock;

typedef struct FileSystem {

    // bitmap for indicating which inodes are not used on disk
    char *free_inodes;

    // bitmap for free blocks
    char *free_blocks; 

    Disk *disk;

    // filesystem's super block
    struct SuperBlock super;

} FileSystem;

// formats a new filesystem on the given disk.
bool format(Disk* disk);

// mounts a filesystem.
FileSystem *mount_fs(Disk* disk);

// frees the given filesystem and its resources.
void free_fs(FileSystem *fs);

typedef struct Inode {

    // whether or not the inode is valid
    uint32_t valid;

    // size of the file
    uint32_t size;

    // array of direct pointers to inode's data blocks
    uint32_t direct[POINTERS_PER_INODE];

    // pointer to an indirect block of pointers to inode's data blocks
    uint32_t indirect;

} Inode;

// creats a new inode in the file system.
ssize_t create_inode(FileSystem *fs);

// free inode with the given inode_num index.
bool remove_inode(FileSystem *fs, size_t inode_num);

union Block {
    SuperBlock super;
    Inode inodes[INODES_PER_BLOCK];
    uint32_t pointers[POINTERS_PER_BLOCK]; 
    char data[BLOCK_SIZE];
} Block;
