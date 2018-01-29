#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>
#include <math.h>
#include "ext2.h"

unsigned char * disk;

int countUnallocatedInBitmap(unsigned char * bitmap, int numBytes) {

	int byte;
	int bit;
	int numUnallocated = 0;
	for (byte = 0; byte < numBytes; byte++) {
		for (bit = 0; bit < 8; bit++) {
			int inUse = ((bitmap[byte] & (1 << bit)) == 0);
			if (!inUse) {
				numUnallocated += 1;
			}
		}
	}

	return numUnallocated;

}

int main(int argc, char ** argv) {

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

	int number_of_fixes = 0;

	// PART A

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);

	// example of how to get char representation of bitmap bits
	unsigned char * bitmap_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);

	int free_blocks = countUnallocatedInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8);

	int free_inodes = countUnallocatedInBitmap(bitmap_bits, (superBlock->s_inodes_count)/8);

	//superblock blocks
	if (free_blocks != superBlock->s_free_blocks_count) {
		int difference = abs(free_inodes - superBlock->s_free_blocks_count);
		superBlock->s_free_blocks_count = free_blocks;
		printf("Fixed: superblock's free blocks counter was off by %d compared to the bitmap", difference);
		number_of_fixes += difference;
	}

	//group desc blocks
	if (free_blocks != blockGp->bg_free_blocks_count) {
		int difference = abs(free_inodes - blockGp->bg_free_blocks_count);
		blockGp->bg_free_blocks_count = free_blocks;
		printf("Fixed: block group's free blocks counter was off by %d compared to the bitmap", difference);
		number_of_fixes += difference;
	}

	//superblock inodes
	if (free_inodes != superBlock->s_free_inodes_count) {
		int difference = abs(free_inodes - superBlock->s_free_inodes_count);
		superBlock->s_free_inodes_count = free_blocks;
		printf("Fixed: superblock's free inodes counter was off by %d compared to the bitmap", difference);
		number_of_fixes += difference;
	}

	//group desc inodes
	if (free_inodes != blockGp->bg_free_blocks_count) {
		int difference = abs(free_inodes - blockGp->bg_free_blocks_count);
		blockGp->bg_free_blocks_count = free_blocks;
		printf("Fixed: block groups's free inodes counter was off by %d compared to the bitmap", difference);
		number_of_fixes += difference;
	}

	// PART B

	//TODO: understand how the mkdir dir entries loop works

	// int DSR = EXT2_BLOCK_SIZE / DISK_SECTOR; // disk sector ratio
	// unsigned char * inode_table = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);
	// int inode_ID = EXT2_ROOT_INO - 1;
	// struct ext2_inode * directory_inode = (struct ext2_inode *)(inode_table + inode_ID * sizeof(struct ext2_inode));
	//
	// int directoryExists = 0;
	// int newInodeID = -1;
	// int numbks = directory_inode->i_blocks / DSR;
	// int curr;
	//
	// for (curr = 0; curr < numbks; curr++) {
	//
	//      int directoryEntryBlock = directory_inode->i_block[curr];
	//      int dirEntBlockIterator = 0;
	//
	//      while (dirEntBlockIterator < EXT2_BLOCK_SIZE) {
	//
	//              struct ext2_dir_entry * directoryEntry = (struct ext2_dir_entry *)
	//                                                       (disk + EXT2_BLOCK_SIZE * directoryEntryBlock + dirEntBlockIterator);
	//
	//              char * dirEntName = directoryEntry->name;
	//              int dirEntInodeID = directoryEntry->inode - 1;
	//
	//              // if (strncmp(parentDir, dirEntName, lenpathname) == 0) {
	//              //      // THE NAME of the current looked directory is in this directory!
	//              //
	//              //      if (directoryEntry->file_type == EXT2_FT_REG_FILE) {
	//              //              // ERROR : NAME OF FILE
	//              //              return -2; // FIX LATER
	//              //      } else if (directoryEntry->file_type == EXT2_FT_DIR) {
	//              //              directoryExists = 1;
	//              //              newInodeID = dirEntInodeID;
	//              //              break;
	//              //      }
	//              // }
	//
	//              dirEntBlockIterator = dirEntBlockIterator + directoryEntry->rec_len;
	//
	//      } // CLOSES dirEntBlockIterator while loop
	//
	//
	//
	//      if (newInodeID != -1) {
	//              break;
	//      }
	//
	// }

	//for each file/directory/symlink
	//if(curr_inode.i_mode != directory_entry.file_type)
	//trust inode's i_mode and update file_type to match

	//I is the inode number for each object
	printf("Fixed: Entry type vs inode mismatch: inode [I]");
	number_of_fixes++;

	//// PART B can be a little tricky. it might be best to leave it for last even
	//// two ways to go about it:
	//// 1. recursion through the root directory, checking every directory for the dirEntries,
	//// and then checking the corresponding inodes
	//// note: inode number in a dir entry: subtract it by 1 to get the inode index
	//// 2. loop through all inodes and for every inode that is a directory, check its dir entries,
	//// and then check the inodes for each of those dir entries as above
	//// when looping through inodes we go i = 1; i < 32 (or s_inode_count) ; i++
	//// to check if an inode is a directory: check it's i_mode (we are trusting this to be a dir anyway)
	//// to get the dir entries of a directory:
	////	in ext2_mkdir, i placed ////-comments above and below the section that searches through dir entries in a directory inode
	////	ctrl+f in ext2_mkdir: //// LOOK HERE TO SEE HOW TO LOOP THROUGH DIR ENTRIES: and //// LOOK UP THERE TO SEE HOW TO LOOP THROUGH DIR ENTRIES

	// PART C

	int inode_index;
	int byte = 0;
	int bit = 0;
	unsigned char * inode_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_bitmap);
	for (inode_index = EXT2_ROOT_INO - 1; inode_index < superBlock->s_inodes_count; inode_index++) {
		struct ext2_inode * curr_inode = (struct ext2_inode *)(disk + inode_index * sizeof(struct ext2_inode));
		if (curr_inode->i_size != 0) {

			int inUse = ((inode_bits[byte] & (1 << bit)) == 0);
			if (inUse) {

				superBlock->s_free_inodes_count -= 1;
				blockGp->bg_Free_inodes_count -= 1;

			}

			int * inode_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_bitmap);
			*inode_bits_int = *inode_bits_int | (1 << (inode_index));

			printf("Fixed: inode [%d] not marked as in-use", inode_index + 1);
			number_of_fixes++;
		}
		bit = bit + 1;
		if (bit == 8) {
			byte = byte + 1;
			bit = 0;
		}
	}

	//for each file/directory/symlink
	//if(inode's mark != inode bitmap)
	//update inode bitmap to show that it is in use
	//update corresponding counters in block_group and superblock
	//NOTE: everything should be consistent with the bitmap at this point

	//I is the inode number for each object

	// PART D

	for (inode_index = EXT2_ROOT_INO - 1; inode_index < superBlock->s_inodes_count; inode_index++) {
		struct ext2_inode * curr_inode = (struct ext2_inode *)(disk + inode_index * sizeof(struct ext2_inode));
		if (curr_inode->i_size != 0) {
			if(curr_inode->i_dtime != 0) {
				curr_inode->i_dtime = 0;
				printf("Fixed: valid inode marked for deletion: [%d]", inode_index + 1);
				number_of_fixes++;
			}
		}
	}

	// PART E

	//for each file/directory/symlink
	//if(!allDataBlocksAllocatedInDataBitmap)
	//fix unallocated block by updating the bitmap
	//update corresponding counters in block group and superblock
	//NOTE: everything should be consistent with the bitmap at this point

	//D is the number of data blocks fixed
	//I is the inode number for each object
	printf("Fixed: D in-use data blocks not marked in data bitmap for inode: [I]");
	number_of_fixes++;

	//// go through all inodes
	//// loop through their i_block array values to get the actual int numbers of their data blocks. e.g. inode->i_block[0] = 22, inode->i_block[1] = 33, etc
	//// then just reuse the inode bitmap update code up there, but with the blocks
	//// it doesn't matter if the bitmap values for blocks are 1 or 0, if 1 then 1 | 1 = 1, but if 0 then 0 | 1 = 1, so it will always just update unallocated bitmap blocks



	if(number_of_fixes == 0) {
		printf("No file system inconsistencies detected!");
	} else {
		printf("%d file system inconsistencies repaired!", number_of_fixes);
	}

	return 0;

}
