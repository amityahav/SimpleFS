#include "src/fs.h"

int main(int agrc, char **argv) {
    // just for testing purposes
    const char *tmp_disk_path = "./disk";
    int nblocks = 10;

    Disk* disk = open_disk(tmp_disk_path, nblocks);
    assert(disk != NULL);
    assert(!disk->mounted);
    assert(disk->nblocks == nblocks);

    // assert size of disk
    struct stat st;
    stat(tmp_disk_path, &st);
    assert(st.st_size == nblocks * BLOCK_SIZE);

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

    union Block block;

    // mounting before formatting should fail
    assert(mount_fs(disk) == NULL);

    // assert proper formating 
    SuperBlock expected = {
        .magic_number = MAGIC_NUMBER,
        .nblocks = nblocks,
        .inblocks = NUMBER_OF_INODE_BLOCKS(nblocks),
        .inodes_count = NUMBER_OF_INODE_BLOCKS(nblocks) * INODES_PER_BLOCK,
    };

    assert(format(disk));
    assert(read_from_disk(disk, SUPER_BLOCK_NUMBER, block.data));

    // assert proper super block
    assert(expected.magic_number == block.super.magic_number);
    assert(expected.inblocks == block.super.inblocks);
    assert(expected.inodes_count == block.super.inodes_count);
    assert(expected.nblocks == block.super.nblocks);

    // assert proper mounting
    FileSystem *fs = mount_fs(disk);
    assert(fs != NULL);
    
    // assert all inodes are unused since its the first time mount
    for (int i = 0; i < block.super.inodes_count; i ++) {
        assert(fs->free_inodes[i]);
    }

    // aserrt all data blocks are unused since its the first time mount
    for (int i = 0; i < NUMBER_OF_DATA_BLOCKS(block.super.nblocks); i++) {
        assert(fs->free_blocks[i]);
    }

    // try format a formatted disk
    assert(!format(disk));

    // create inode (should return the first inode#0)
    assert(create_inode(fs) == 0);

    // cleanup
    close_disk(disk);
    remove(tmp_disk_path);

    return 0;
}
