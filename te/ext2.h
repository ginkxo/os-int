#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include "ext2.h"

unsigned char *disk;

void printbin(unsigned char * bitmap, int numBytes) {
	int byte;
	int bit;
	for (byte = 0; byte < numBytes; byte++) {
		for (bit = 0; bit < 8; bit++) {
			int inUse = !((bitmap[byte] & (1 << bit)) == 0);
			printf("%d", inUse);
		}
		printf(" ");
	}
	printf("\n");

	return;
}


int main(int argc, char **argv) {

    if(argc != 2) {
        fprintf(stderr, "Usage: %s <image file name>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    struct ext2_super_block *sb = (struct ext2_super_block *)(disk + 1024);
    printf("Inodes: %d\n", sb->s_inodes_count);
    printf("Blocks: %d\n", sb->s_blocks_count);

	struct ext2_group_desc *bgd = (struct ext2_group_desc *)(disk + (1024+EXT2_BLOCK_SIZE));
	printf("%4sblock bitmap: %d\n","",bgd->bg_block_bitmap);
	printf("%4sinode bitmap: %d\n","",bgd->bg_inode_bitmap);
	printf("%4sinode table: %d\n","",bgd->bg_inode_table);
	printf("%4sfree blocks: %d\n","",bgd->bg_free_blocks_count);
	printf("%4sfree inodes: %d\n","",bgd->bg_free_inodes_count);
	printf("%4sused dirs: %d\n","",bgd->bg_used_dirs_count);

	unsigned char *bitmap_bits = (unsigned char *)(disk + 1024 * bgd->bg_block_bitmap);
	unsigned char *inode_bits = (unsigned char *)(disk + 1024 * bgd->bg_inode_bitmap);
	printf("Block bitmap: ");
	printbin(bitmap_bits, (sb->s_blocks_count)/8);
	printf("Inode bitmap: ");
	printbin(inode_bits, (sb->s_inodes_count)/8);

	int dirblocks[sb->s_inodes_count];
	int inodenums[sb->s_inodes_count];
	int dirsizes[sb->s_inodes_count];
	// int diriblocks[sb->s_inodes_count];
	int dirc = 0;

	printf("\n");
	printf("Inodes: \n");
	unsigned char * inodetable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * bgd->bg_inode_table);
	int id;
	for (id = EXT2_ROOT_INO - 1; id < sb->s_inodes_count; id++) {

		if (id == EXT2_ROOT_INO - 1 || id >= EXT2_GOOD_OLD_FIRST_INO) {

			struct ext2_inode * in = (struct ext2_inode *) (inodetable + id*sizeof(struct ext2_inode));

			if (in->i_size != 0) {
	
				char type = '\0';
				if (in->i_mode & EXT2_S_IFREG) type = 'f';
				else if (in->i_mode & EXT2_S_IFDIR) {
					type = 'd';
					dirblocks[dirc] = in->i_block[0];
					dirsizes[dirc] = in->i_size;
					inodenums[dirc] = id+1;
					dirc++;
				}

				printf("[%d] type: %c size: %d links: %d blocks: %d\n", id+1, type, in->i_size, in->i_links_count, in->i_blocks);

				int blox;
				printf("[%d] Blocks: ", id+1);
				for (blox = 0; blox < in->i_blocks / 2; blox++) {
					printf(" %d", in->i_block[blox]);
				}
				printf("\n");
				
			}

		}

	}
	printf("\n");
	printf("Directory Blocks: \n");
	int l;
	for (l = 0; l < dirc; l++) {
		printf("%4sDIR BLOCK NUM: %d ","",dirblocks[l]);
		// ...
		printf("(for inode %d)\n", inodenums[l]);
		int linkIndex = 0;
		int base = dirblocks[l];
		while (linkIndex < dirsizes[l]) {
			struct ext2_dir_entry * dirent = (struct ext2_dir_entry *) (disk + EXT2_BLOCK_SIZE * base + linkIndex );
			char dirtype = '\0';
			unsigned char ft = dirent->file_type;
			if (ft == EXT2_FT_DIR) dirtype = 'd';
			else if (ft == EXT2_FT_REG_FILE) dirtype = 'f';
			else if (ft == EXT2_FT_SYMLINK) dirtype = 's'; 
			printf("Inode: %d rec_len: %d name_len: %d type= %c name= %s\n", dirent->inode, dirent->rec_len, dirent->name_len, dirtype, dirent->name);
			linkIndex = linkIndex + dirent->rec_len;
		}
	}
	
    
    return 0;
}