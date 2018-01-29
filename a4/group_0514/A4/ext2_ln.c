#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>
#include <math.h>
#include <string.h>
#include <errno.h>
#include "ext2.h"

unsigned char * disk;

#define SINGLE_POINTER_COUNT 12
#define DISK_SECTOR 512

typedef struct {

	int flags[2];

} duoflag;

int blockCalc(int numbytes) {

	int ratio = (numbytes - 1)/EXT2_BLOCK_SIZE;
	return ratio + 1;

}

int DSR = EXT2_BLOCK_SIZE / DISK_SECTOR; // disk sector ratio

int get_real_rl(int namelen) {

	int preliminary_rl = sizeof(struct ext2_dir_entry) + namelen;

	if ((preliminary_rl % 4) == 0) {

		preliminary_rl = preliminary_rl + 0;

	} else if ((preliminary_rl % 4) == 1) {

		preliminary_rl = preliminary_rl + 3;

	} else if ((preliminary_rl % 4) == 2) {

		preliminary_rl = preliminary_rl + 2;

	} else {

		preliminary_rl = preliminary_rl + 1;

	}

	return preliminary_rl;

}

int firstXEmptyInBitmap(unsigned char * bitmap, int numBytes, int numEmpty) {

	int byte;
	int bit;
	int marker = 0;
	int first = -1;
	for (byte = 0; byte < numBytes; byte++) {
		for (bit = 0; bit < 8; bit++) {
			int inUse = !((bitmap[byte] & (1 << bit)) == 0);
			if (!inUse) {
				if (marker == 0) {
					first = (byte*8) + bit + 1;
				} 
				if (marker != numEmpty-1) {
					marker = marker + 1;
				} else {
					// there is another empty but we already found one empty
					// so two empty exist
					return first;
				}
			}

		}
	}

	return -1; // NO TWO EMPTY BLOCKS/INODES

}

int firstTwoEmptyInBitmap(unsigned char * bitmap, int numBytes) {

	int byte;
	int bit;
	int marker = 0;
	int first = -1;
	for (byte = 0; byte < numBytes; byte++) {
		for (bit = 0; bit < 8; bit++) {
			int inUse = !((bitmap[byte] & (1 << bit)) == 0);
			if (!inUse) {
				if (marker == 0) {
					first = (byte*8) + bit + 1;
					marker = marker + 1;
				} else {
					// there is another empty but we already found one empty
					// so two empty exist
					return first;
				}
			}

		}
	}

	return -1; // NO TWO EMPTY BLOCKS/INODES

}

int firstEmptyInBitmap(unsigned char * bitmap, int numBytes) {

	int byte;
	int bit;
	for (byte = 0; byte < numBytes; byte++) {
		for (bit = 0; bit < 8; bit++) {
			int inUse = !((bitmap[byte] & (1 << bit)) == 0);
			if (!inUse) {
				return (byte*8) + bit + 1;
			}

		}
	}

	return -1; // NO EMPTY BLOCKS/INODES

}

int allocateLinkBlocks(char * pathname, int inodeNum, int numBlocks, int numbytes) {

	int targetLen = strlen(pathname);
	char target[targetLen+1];
	memset(target, 0, sizeof(target));
	strncpy(target, pathname, targetLen+1);

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	struct ext2_inode * fileInode = (struct ext2_inode *)(inodeTable + inodeNum * sizeof(struct ext2_inode));

	unsigned char * bitmap_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);
	int * bitmap_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);

	/*
	filedisk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
	if (disk == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	*/

	int needPointers = 0;

	if (numBlocks > SINGLE_POINTER_COUNT) {

		needPointers = 1;
		numBlocks = numBlocks + 1; // for a single indirection
	}

	// check if numBlocks number of blocks are available
	int emptyBlock = firstXEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8, numBlocks);
	if (emptyBlock == -1) {

		return -1; // ENOSPC
	}

	// pre-allocate all blocks
	int block_i;
	int firstLimit = SINGLE_POINTER_COUNT;

	if (numBlocks <= firstLimit ) {
		firstLimit = numBlocks;
	}

	for (block_i = 0; block_i < firstLimit; block_i++) {

		int emptyBlk = firstEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8);
		if (emptyBlk == -1) {

			return -1; //enospc

		}

		// pre-allocate this block
		int firstAvailableBlock = emptyBlk - 1;

		fileInode->i_block[block_i] = firstAvailableBlock;

		*bitmap_bits_int = *bitmap_bits_int | (1 << (firstAvailableBlock));
		superBlock->s_free_blocks_count = superBlock->s_free_blocks_count - 1;
		blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count - 1;

	}

	if (needPointers) {

		// allocate i_block[12] to be the pointer block

		int extraEmpty = firstEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8);
		if (extraEmpty == -1) {
			return -1; //enospc
		}
		int singleIndirect = extraEmpty - 1;

		fileInode->i_block[SINGLE_POINTER_COUNT] = singleIndirect;

		*bitmap_bits_int = *bitmap_bits_int | (1 << (singleIndirect));
		superBlock->s_free_blocks_count = superBlock->s_free_blocks_count - 1;
		blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count - 1 ;

		int blocksRemaining = numBlocks - SINGLE_POINTER_COUNT; 

		unsigned int * indirectBlock = (unsigned int *)(disk + EXT2_BLOCK_SIZE * fileInode->i_block[SINGLE_POINTER_COUNT]);

		if (blocksRemaining >= sizeof(indirectBlock) || blocksRemaining >= superBlock->s_free_blocks_count) {

			return -1; // enospc;

		}

		int r;
		for (r = 0; r < blocksRemaining; r++) {

			int assignEmpty = firstEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8);

			if (extraEmpty == -1) {
				return -1; //enospc

			}

			int indirBlk = assignEmpty - 1;

			indirectBlock[r] = indirBlk;

			*bitmap_bits_int = *bitmap_bits_int | (1 << (indirBlk));
			superBlock->s_free_blocks_count = superBlock->s_free_blocks_count - 1;
			blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count - 1;
		}

	}

	int bytesRemaining = numbytes;
	int inodeBlockIndex = 0;
	int inodeIndirectIndex = 0;
	int inc = 0;

	while (bytesRemaining > 0) {

		int numToSend = EXT2_BLOCK_SIZE;

		if (bytesRemaining >= EXT2_BLOCK_SIZE) {

			numToSend = EXT2_BLOCK_SIZE;

		} else {

			numToSend = EXT2_BLOCK_SIZE - bytesRemaining;

		}

		if (needPointers && numBlocks > SINGLE_POINTER_COUNT && inodeBlockIndex == 11) {

			unsigned int * block;
			block = (unsigned int *)(disk + EXT2_BLOCK_SIZE * fileInode->i_block[SINGLE_POINTER_COUNT]);

			int indBlock;
			indBlock = block[inodeIndirectIndex];

			memcpy(disk + EXT2_BLOCK_SIZE * indBlock, (unsigned char *) target + EXT2_BLOCK_SIZE * inc, numToSend);
			inodeIndirectIndex = inodeIndirectIndex + 1;
			inc = inc + 1;


		} else {

			int block;
			block = fileInode->i_block[inodeBlockIndex];

			memcpy(disk + EXT2_BLOCK_SIZE * block, (unsigned char *) target + EXT2_BLOCK_SIZE * inc, numToSend);
			inodeBlockIndex = inodeBlockIndex + 1;
			inc = inc + 1;

		}

		bytesRemaining = bytesRemaining - numToSend;


	}

	return 0;

}

int allocateNewLinkInode(int filesizeBytes, int requiredBlocks) {

	// here: filesizeBytes is going to be the length of the pathname + 1

	// REMOVETHE "2" AND REPLACE WITH THE OLD FUNCTION WHEN DONE

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	
	unsigned char * inode_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_bitmap);
	unsigned char * block_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);
	
	int emptyInode = firstEmptyInBitmap(inode_bits, (superBlock->s_inodes_count)/8);
	int emptyBlocks = firstXEmptyInBitmap(block_bits, (superBlock->s_inodes_count)/8, requiredBlocks);
	int firstEmptyNode = emptyInode - 1;

	if (emptyInode == -1 || emptyBlocks == -1) {
		//ENOSPC
		return -1;
	}

	struct ext2_inode * newInode = (struct ext2_inode *)(inodeTable + firstEmptyNode * sizeof(struct ext2_inode));

	// allocation here

	int blocks = blockCalc(filesizeBytes);

	newInode->i_mode = EXT2_S_IFLNK;
	newInode->i_uid = 0;
	newInode->i_size = filesizeBytes; // default for new directories
	newInode->i_gid = 0;
	newInode->i_links_count = 0; // . and ..
	newInode->i_blocks = blocks * DSR; // currently set to 2
	newInode->osd1 = 0;

	newInode->i_generation = 0;
	newInode->i_file_acl = 0;
	newInode->i_dir_acl = 0;
	newInode->i_faddr = 0;

	newInode->extra[0] = 0;
	newInode->extra[1] = 0;
	newInode->extra[2] = 0;

	// INODE BITMAP EDITING:

	int * inode_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_bitmap);
	*inode_bits_int = *inode_bits_int | (1 << (firstEmptyNode));

	// CHANGING SOME OF THE STUFF HERE:

	superBlock->s_free_inodes_count = superBlock->s_free_inodes_count - 1;

	blockGp->bg_free_inodes_count = blockGp->bg_free_inodes_count - 1;
	// blockGp->bg_used_dirs_count = blockGp->bg_used_dirs_count + 1;

	return firstEmptyNode; // RETURNS the inodeID (inode number - 1) (b/c inode 2 is in fact index 1, index 0 is inode 1)

}

int addNewLnkToParentDirEntList(int dirInodeNum, int dirParentInodeNum, char * name, int * parentFreeBlocks, int type) {

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int linktype = EXT2_FT_UNKNOWN;
	if (type == 1) {
		linktype = EXT2_FT_REG_FILE;
	} else if (type == 2) {
		linktype = EXT2_FT_DIR;
	} else if (type == 3) {
		linktype = EXT2_FT_SYMLINK;
	}

	struct ext2_inode * parentDirInode = (struct ext2_inode *)(inodeTable + dirParentInodeNum * sizeof(struct ext2_inode));

	unsigned char * bitmap_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);

	int numblocksPar = parentDirInode->i_blocks / DSR;

	if (parentFreeBlocks[0] == 0) {

		// can add the new entry to the i_block index in parentFreeBlocks[i]

		int currBlk = parentDirInode->i_block[parentFreeBlocks[1]];
		int currBlkIterator = 0;


		while (currBlkIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * dE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * currBlk + currBlkIterator);


			if (currBlkIterator + dE->rec_len >= EXT2_BLOCK_SIZE) {

				// this is the last directory entry
				int dE_real_rl = get_real_rl(dE->name_len);
				
				struct ext2_dir_entry * newDE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * currBlk + currBlkIterator + dE_real_rl);	

				newDE->inode = dirInodeNum + 1;
				newDE->rec_len = EXT2_BLOCK_SIZE - (currBlkIterator + dE_real_rl);
				newDE->name_len = strlen(name);
				newDE->file_type = linktype;
				strncpy(newDE->name, name, EXT2_NAME_LEN);

				dE->rec_len = dE_real_rl;
				
				return 0;

			}

			// printf("iter: %d reclen: %d \n", currBlkIterator, dE->rec_len);

			currBlkIterator = currBlkIterator + dE->rec_len;

		}	

	} else if (parentFreeBlocks[0] > 0) {

		// need to allocate the new block to the parent directory
		// this is after we have allocated a block to mkdir

		int emptyBlock = firstEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8);
		int firstAvailableBlock = emptyBlock - 1;
		if (emptyBlock == -1) {
			// enospc
			return -1;
		}

		int * bitmap_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);

		parentDirInode->i_block[numblocksPar] = firstAvailableBlock;
		int currBlk = parentDirInode->i_block[numblocksPar];

		struct ext2_dir_entry * newDE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * currBlk + 0);

		newDE->inode = dirInodeNum + 1;
		newDE->rec_len = EXT2_BLOCK_SIZE;
		newDE->name_len = strlen(name);
		newDE->file_type = linktype;
		strncpy(newDE->name, name, EXT2_NAME_LEN);

		*bitmap_bits_int = *bitmap_bits_int | (1 << (firstAvailableBlock)); // marks the bitmap
		parentDirInode->i_blocks = parentDirInode->i_blocks + DSR; // added an extra block to the directory

		superBlock->s_free_blocks_count = superBlock->s_free_blocks_count - 1;
		blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count - 1;

		// ext case

		return 0;

	}


	return -1; // weird failure of some kind
	
	
	
}


duoflag checkParentDirEntListSpaceExists(int dirParentInodeNum, char * name) {

	// FLAGS starts as {0,0}
	// if FLAGS is {-1,0} then some enospc error
	// if FLAGS is {0,3} then we need to go into i_blocks[3] for mkdir
	// if flags is something like {1,23} then we create i_block[1] and assign it to block 22 (note: 23 - 1)

	duoflag d;

	int namelength = strlen(name);
	int tentative_size = get_real_rl(namelength);
	d.flags[0] = 0; // flags[1] == 0 means returning an i_block index, flags[1] == 1 means returning block 
	d.flags[1] = 0;

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);

	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	unsigned char * bitmap_bits = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);

	struct ext2_inode * parentDirInode = (struct ext2_inode *)(inodeTable + dirParentInodeNum * sizeof(struct ext2_inode));

	int numblocksPar = parentDirInode->i_blocks / DSR;
	int currblk;

	for (currblk = 0; currblk < numblocksPar; currblk++) {

		int currBlk = parentDirInode->i_block[currblk];
		int currBlkIterator = 0;

		while (currBlkIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * dE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * currBlk + currBlkIterator);

			if (currBlkIterator + dE->rec_len >= EXT2_BLOCK_SIZE) {
				// this is the last directory entry
				int dE_real_rl = get_real_rl(dE->name_len);
				if ((EXT2_BLOCK_SIZE - (currBlkIterator + dE_real_rl)) >= tentative_size) {

					d.flags[1] = currblk;
					return d;

				}
			}

			currBlkIterator = currBlkIterator + dE->rec_len;

		}

	}

	if (numblocksPar < SINGLE_POINTER_COUNT) {

		
		// we need to store the new entry in a new block of the parent directory inode
		// allocate a new block
		int emptyBlock = firstTwoEmptyInBitmap(bitmap_bits, (superBlock->s_blocks_count)/8); 
		if (emptyBlock == -1) {
			d.flags[1] = -1;
			return d; //enospc
		}
		d.flags[0] = numblocksPar;
		d.flags[1] = emptyBlock;
		
		return d;

	}

	d.flags[1] = -1;
	return d; // enospc


}

int goodPathToCreate(char * basePath) {

	// SOMETHING TO CHANGE FOR LATER:
	// MAKE SURE ALL INODE NUMBERS ARE IN FACT INODE NUMBERS - 1 WHEN GOING BETWEEN INODES

	// returns:
	// -2 if: already an entry with the same name (EEXIST)
	// -1 if: the pathname does not exist (ENOENT)
	// -1 
	// inodeID -> need to make sure that this is the actual id, not the name

	// struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);

	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int inodeID = EXT2_ROOT_INO - 1;

	int basePathLen = strlen(basePath);
	char basepath[basePathLen+1];
	memset(basepath, 0, sizeof(basepath));
	strncpy(basepath, basePath, basePathLen+1);

	char * directoryToCreateConstant = basename(basepath);
	char * pathnameConstant = dirname(basepath);

	int lenpathname = strlen(pathnameConstant);

	char dirToCreate[EXT2_NAME_LEN+1];
	char pathname[lenpathname+1];

	memset(dirToCreate, 0, sizeof(dirToCreate));
	memset(pathname, 0, sizeof(pathname));

	strncpy(dirToCreate, directoryToCreateConstant, EXT2_NAME_LEN+1);
	strncpy(pathname, pathnameConstant, lenpathname+1);

	char * parentDir;
	char * childDir;

	parentDir = strtok(pathname, "/");
	childDir = strtok(NULL, "/");

	char dotRef[EXT2_NAME_LEN] = ".";

	
	while (parentDir != NULL && inodeID != -1) {

		if (strncmp(parentDir, dotRef, lenpathname) == 0 && childDir == NULL) {
			// this is the directory in which we have to create what is currently the parent
			// have to check that the name of the parentDir isn't identical to any entries here!

			struct ext2_inode * rootInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

			int numBlocks = rootInode->i_blocks / DSR; 
			int blocknum;

			for (blocknum = 0; blocknum < numBlocks; blocknum++) {

				int rootEntBlock = rootInode->i_block[blocknum];
				int rootEntBlockIterator = 0;

				while (rootEntBlockIterator < EXT2_BLOCK_SIZE) {
					
					struct ext2_dir_entry * dirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * rootEntBlock + rootEntBlockIterator);

					char * dirEntryName = dirEntry->name;
					int dirEntryLen = dirEntry->name_len;
					
					if (strncmp(dirToCreate, dirEntryName, dirEntryLen) == 0) {
						// ALREADYIN HERE
						return -2; // FIX for error stuff 
					}

					rootEntBlockIterator = rootEntBlockIterator + dirEntry->rec_len;
					
				}

			}

			
			// we are good to go for trying to create the directory here. return the inode number

			return EXT2_ROOT_INO - 1;
		}

		struct ext2_inode * directoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

		// need to iterate through entries in directoryInode 

		int directoryExists = 0;
		int newInodeID = -1;
		int numbks = directoryInode->i_blocks / DSR;
		int curr;

		for (curr = 0; curr < numbks; curr++) {

			int directoryEntryBlock = directoryInode->i_block[curr];
			int dirEntBlockIterator = 0;

		// ADDTHE FOR LOOP HERE LATER 

			while (dirEntBlockIterator < EXT2_BLOCK_SIZE) {
		
				struct ext2_dir_entry * directoryEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * directoryEntryBlock + dirEntBlockIterator);
			
				char * dirEntName = directoryEntry->name;
				int dirEntInodeID = directoryEntry->inode - 1;

				if (strncmp(parentDir, dirEntName, lenpathname) == 0) {
				// THE NAME of the current looked directory is in this directory!

					if (directoryEntry->file_type == EXT2_FT_REG_FILE) {
					// ERROR : NAME OF FLE
						return -2; // FIX LATER
					} else if (directoryEntry->file_type == EXT2_FT_DIR) {
						directoryExists = 1;
						newInodeID = dirEntInodeID;
						break;
					}

				}

				dirEntBlockIterator = dirEntBlockIterator + directoryEntry->rec_len;

			} // CLOSES dirEntBlockIterator while loop

			if (newInodeID != -1) {
				break;
			}

		}

		if ((!directoryExists) || newInodeID == -1) {
			// pathname is not linked
			// ERROR
			return -1;
		} else {

			parentDir = childDir;
			childDir = strtok(NULL,"/");
			inodeID = newInodeID;
			directoryExists = 0;
			
		}


	} // CLOSES inodeID while loop

	// FINALLY, NEED TO CHECK THE LAST DIRECTORY.
	// this is stored in inodeID.

	struct ext2_inode * finalDirectoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

	int numblocksFinalDir = finalDirectoryInode->i_blocks / DSR;
	int currblock;

	for (currblock = 0; currblock < numblocksFinalDir; currblock++) {
		
		int finalEntBlock = finalDirectoryInode->i_block[currblock];
		int finalEntBlockIterator = 0;

		while (finalEntBlockIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * currDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalEntBlock + finalEntBlockIterator);

			char * currDirEntName = currDirEntry->name;
			int currDirEntNameLen = currDirEntry->name_len;

			if (strncmp(dirToCreate, currDirEntName, currDirEntNameLen) == 0) {

				// ALREADY SOMETHING WITH THE SAME NAME, RETURN ERROR	
				return -2; // FIX LATER I GUESS

			}

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}
	
	return inodeID; // path is valid?

	// we might need to return something about which inode to make or something ...

}

int goodPathExists(char * basePath) {

	// SOMETHING TO CHANGE FOR LATER:
	// MAKE SURE ALL INODE NUMBERS ARE IN FACT INODE NUMBERS - 1 WHEN GOING BETWEEN INODES

	// returns:
	// -2 if: already an entry with the same name (EEXIST)
	// -1 if: the pathname does not exist (ENOENT)
	// -1 
	// inodeID -> need to make sure that this is the actual id, not the name

	// struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);

	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int inodeID = EXT2_ROOT_INO - 1;

	int basePathLen = strlen(basePath);
	char basepath[basePathLen+1];
	memset(basepath, 0, sizeof(basepath));
	strncpy(basepath, basePath, basePathLen+1);

	char * directoryToCreateConstant = basename(basepath);
	char * pathnameConstant = dirname(basepath);

	int lenpathname = strlen(pathnameConstant);

	char dirToCreate[EXT2_NAME_LEN+1];
	char pathname[lenpathname+1];

	memset(dirToCreate, 0, sizeof(dirToCreate));
	memset(pathname, 0, sizeof(pathname));

	strncpy(dirToCreate, directoryToCreateConstant, EXT2_NAME_LEN+1);
	strncpy(pathname, pathnameConstant, lenpathname+1);

	char * parentDir;
	char * childDir;

	parentDir = strtok(pathname, "/");
	childDir = strtok(NULL, "/");

	char dotRef[EXT2_NAME_LEN] = ".";

	
	while (parentDir != NULL && inodeID != -1) {

		if (strncmp(parentDir, dotRef, lenpathname) == 0 && childDir == NULL) {
			// this is the directory in which we have to create what is currently the parent
			// have to check that the name of the parentDir isn't identical to any entries here!

			struct ext2_inode * rootInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

			int numBlocks = rootInode->i_blocks / DSR; 
			int blocknum;

			for (blocknum = 0; blocknum < numBlocks; blocknum++) {

				int rootEntBlock = rootInode->i_block[blocknum];
				int rootEntBlockIterator = 0;

				while (rootEntBlockIterator < EXT2_BLOCK_SIZE) {
					
					struct ext2_dir_entry * dirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * rootEntBlock + rootEntBlockIterator);

					char * dirEntryName = dirEntry->name;
					int currDirEntNameLen = dirEntry->name_len;
					
					if (strncmp(dirToCreate, dirEntryName, currDirEntNameLen) == 0) {
						// ALREADYIN HERE
						if (dirEntry->file_type == EXT2_FT_DIR) {
							return -2; //EISDIR
						}

						return EXT2_ROOT_INO - 1; // FIX for error stuff 
					}

					rootEntBlockIterator = rootEntBlockIterator + dirEntry->rec_len;
					
				}

			}

			
			// we are good to go for trying to create the directory here. return the inode number

			return -1;
		}

		struct ext2_inode * directoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

		// need to iterate through entries in directoryInode 

		int directoryExists = 0;
		int newInodeID = -1;
		int numbks = directoryInode->i_blocks / DSR;
		int curr;

		for (curr = 0; curr < numbks; curr++) {

			int directoryEntryBlock = directoryInode->i_block[curr];
			int dirEntBlockIterator = 0;

		// ADDTHE FOR LOOP HERE LATER 

			while (dirEntBlockIterator < EXT2_BLOCK_SIZE) {
		
				struct ext2_dir_entry * directoryEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * directoryEntryBlock + dirEntBlockIterator);
			
				char * dirEntName = directoryEntry->name;
				int dirEntInodeID = directoryEntry->inode - 1;

				if (strncmp(parentDir, dirEntName, lenpathname) == 0) {
				// THE NAME of the current looked directory is in this directory!

					if (directoryEntry->file_type == EXT2_FT_REG_FILE) {
					// ERROR : NAME OF FLE
						return -1; // FIX LATER
					} else if (directoryEntry->file_type == EXT2_FT_DIR) {
						directoryExists = 1;
						newInodeID = dirEntInodeID;
						break;
					}

				}

				dirEntBlockIterator = dirEntBlockIterator + directoryEntry->rec_len;

			} // CLOSES dirEntBlockIterator while loop

			if (newInodeID != -1) {
				break;
			}

		}

		if ((!directoryExists) || newInodeID == -1) {
			// pathname is not linked
			// ERROR
			return -1;
		} else {

			parentDir = childDir;
			childDir = strtok(NULL,"/");
			inodeID = newInodeID;
			directoryExists = 0;
			
		}


	} // CLOSES inodeID while loop

	// FINALLY, NEED TO CHECK THE LAST DIRECTORY.
	// this is stored in inodeID.

	struct ext2_inode * finalDirectoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

	int numblocksFinalDir = finalDirectoryInode->i_blocks / DSR;
	int currblock;

	for (currblock = 0; currblock < numblocksFinalDir; currblock++) {
		
		int finalEntBlock = finalDirectoryInode->i_block[currblock];
		int finalEntBlockIterator = 0;

		while (finalEntBlockIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * currDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalEntBlock + finalEntBlockIterator);

			char * currDirEntName = currDirEntry->name;
			int currDirEntNameLen = currDirEntry->name_len;

			if (strncmp(dirToCreate, currDirEntName, currDirEntNameLen) == 0) {

				// ALREADY SOMETHING WITH THE SAME NAME, RETURN ERROR
				if (currDirEntry->file_type == EXT2_FT_DIR) {
					return -2; // EISDIR
				}	
				return inodeID; // FIX LATER I GUESS

			}

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}
	
	return -1; // path is valid?

	// we might need to return something about which inode to make or something ...

}

int getInodeIDFromParent(int inodeParentID, char * sourcename) {

	int snl = strlen(sourcename);
	char source[snl+1];
	memset(source, 0, sizeof(source));
	strncpy(source, sourcename, snl+1);

	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	struct ext2_inode * finalDirectoryInode = (struct ext2_inode *)(inodeTable + inodeParentID * sizeof(struct ext2_inode));

	int numblocksFinalDir = finalDirectoryInode->i_blocks / DSR;
	int currblock;

	for (currblock = 0; currblock < numblocksFinalDir; currblock++) {
		
		int finalEntBlock = finalDirectoryInode->i_block[currblock];
		int finalEntBlockIterator = 0;

		while (finalEntBlockIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * currDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalEntBlock + finalEntBlockIterator);

			char * currDirEntName = currDirEntry->name;
			int inodeID = currDirEntry->inode - 1;
			int currDirEntNameLen = currDirEntry->name_len;

			if (strncmp(source, currDirEntName, currDirEntNameLen) == 0) {

				// ALREADY SOMETHING WITH THE SAME NAME, RETURN ERROR	
				return inodeID; // FIX LATER I GUESS

			}

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}
	
	return -1; // path is valid?

}

void lnk(char * path1, char * path2, int isSoft) {

	int pathLen1 = strlen(path1);
	char pathname1[pathLen1+1];
	memset(pathname1, 0, sizeof(pathname1));
	strncpy(pathname1, path1, pathLen1+1);

	if (pathname1[pathLen1-1] == '/') {
		pathname1[pathLen1-1] = 0;
	}

	if (pathname1[0] == '/') {
		memmove(pathname1, pathname1+1, pathLen1);
	}

	int pathLen2 = strlen(path2);
	char pathname2[pathLen2+1];
	memset(pathname2, 0, sizeof(pathname2));
	strncpy(pathname2, path2, pathLen2+1);

	if (pathname2[pathLen2-1] == '/') {
		pathname2[pathLen2-1] = 0;
	}

	if (pathname2[0] == '/') {
		memmove(pathname2, pathname2+1, pathLen2);
	}

	// need to make sure both paths are valid

	int linkSourceParentInodeID = goodPathExists(pathname1);
	int linkLocationParentInodeID = goodPathExists(pathname2);

	if (linkSourceParentInodeID < 0) {
		if (linkSourceParentInodeID == -1) {
			errno = ENOENT;
			perror("Path does not exist.\n");
			exit(EXIT_FAILURE);
		} else {
			errno = EISDIR;
			perror("The target is a directory hardlink.\n");
			exit(EXIT_FAILURE);
		}

	}

	if (linkLocationParentInodeID < 0) {
		if (linkLocationParentInodeID == -1) {
			errno = ENOENT;
			perror("Path does not exist.\n");
			exit(EXIT_FAILURE);
		} else {
			errno = EEXIST;
			perror("The attempted name for the link exists.\n");
			exit(EXIT_FAILURE);
		}

	}

	char * sourcenameConstant = basename(pathname1);
	int snC = strlen(sourcenameConstant);
	char sourcename[snC+1];
	memset(sourcename, 0, sizeof(sourcename));
	strncpy(sourcename, sourcenameConstant, snC+1);

	char * linknameConstant = basename(pathname2);
	int lnC = strlen(linknameConstant);
	char linkname[lnC+1];
	memset(linkname, 0, sizeof(linkname));
	strncpy(linkname, linknameConstant, lnC+1);

	int linkSourceInodeID = getInodeIDFromParent(linkSourceParentInodeID, sourcename);

	if (linkSourceInodeID < 0) {

		errno = ENOENT;
		perror("No inode found.\n");
		exit(EXIT_FAILURE);

	}

	if (!isSoft) {

		struct ext2_group_desc * blockgp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
		unsigned char * inodetable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockgp->bg_inode_table);

		// hard link

		int spaceExists[2];

		duoflag dflag = checkParentDirEntListSpaceExists(linkLocationParentInodeID, linkname);

		spaceExists[0] = dflag.flags[0];
		spaceExists[1] = dflag.flags[1];

		if (spaceExists[0] == -1) {

			//ENOSPC
			errno = ENOSPC;
			perror("Not enough space.\n");
			exit(EXIT_FAILURE);
		}

		int ftype = 0;

		struct ext2_inode * sourceInode = (struct ext2_inode *)(inodetable + linkSourceInodeID * sizeof(struct ext2_inode));

		if (sourceInode->i_mode & EXT2_S_IFREG) {
			ftype = 1;

		} else if (sourceInode->i_mode & EXT2_S_IFDIR) {
			ftype = 2;

		}


		if (addNewLnkToParentDirEntList(linkSourceInodeID, linkLocationParentInodeID, linkname, spaceExists, ftype) < 0 ){
			errno = ENOSPC;
			perror("Not enough space in blocks.\n");
			exit(EXIT_FAILURE);

		}

		sourceInode->i_links_count = sourceInode->i_links_count + 1;

		// check if there is space in the dir 
		// if so, then do this:
		// get the inode of the link Source paren		// get its number
		// create a dir entry with the inode number above as a link

	} else {

		// soft link

		// 1. check if dir entry space exists
		// 2. allocateNewLinkInode
		// 3. allocateSoftLinkBlocks
		// 4. addNewLnkToParentDirEntList

		int spaceExists[2];

		duoflag dflag = checkParentDirEntListSpaceExists(linkLocationParentInodeID, linkname);

		spaceExists[0] = dflag.flags[0];
		spaceExists[1] = dflag.flags[1];

		if (spaceExists[0] == -1) {

			//ENOSPC
			errno = ENOSPC;
			perror("Not enough space.\n");
			exit(EXIT_FAILURE);
		}

		int linkBytes = pathLen1;
		int linkBlocks = blockCalc(linkBytes);

		int newFileInodeIndex = allocateNewLinkInode(linkBytes, linkBlocks);

		if (newFileInodeIndex < 0) {

			//ENOSPC
			errno = ENOSPC;
			perror("Not enough space in inode table or blocks.\n");
			exit(EXIT_FAILURE);


		}

		// ALLOCATE LINK BLOCKS

		if (allocateLinkBlocks(pathname1, newFileInodeIndex, linkBlocks, linkBytes) < 0) {
			errno = ENOSPC;
			perror("Not enough space in blocks. \n");
			exit(EXIT_FAILURE);

		}

		// ADD THE DIRECTORY ENTRY



		if (addNewLnkToParentDirEntList(newFileInodeIndex, linkLocationParentInodeID, linkname, spaceExists, 3) < 0) {

			//ENOSPC
			errno = ENOSPC;
			perror("Not enough space in blocks.\n");
			exit(EXIT_FAILURE);

		}


	}



}

int main(int argc, char ** argv) {

    if((argc != 4) && (argc != 5)) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

    char flagcheck[5] = "-s";

	if (argc == 5 && strncmp(argv[2], flagcheck, strlen(argv[2])+1) == 0) {

		lnk(argv[3], argv[4], 1);

	} else {

		lnk(argv[2], argv[3], 0);
	}

	return 0;
}
