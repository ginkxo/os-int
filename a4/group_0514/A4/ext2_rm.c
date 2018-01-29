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
#include <time.h>
#include "ext2.h"

unsigned char * disk;

#define SINGLE_POINTER_COUNT 12
#define DISK_SECTOR 512

typedef struct {

	int flags[2];

} duoflag;

int DSR = EXT2_BLOCK_SIZE / DISK_SECTOR; // disk sector ratio

int goodPathRM(char * basePath) {

	// SOMETHING TO CHANGE FOR LATER:
	// MAKE SURE ALL INODE NUMBERS ARE IN FACT INODE NUMBERS - 1 WHEN GOING BETWEEN INODES

	// returns:
	// -2 if: already an entry with the same name (EEXIST)
	// -1 if: the pathname does not exist (ENOENT)
	// -1 
	// inodeID -> need to make sure that this is the actual id, not the name

	// struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);

	// NOTE: here, dirToCreate -> fileToDelete (for later)

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
			// have to check that the name of the parentDir isn't identical to any entries here

			struct ext2_inode * rootInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

			int numBlocks = rootInode->i_blocks / DSR; 
			int blocknum;

			for (blocknum = 0; blocknum < numBlocks; blocknum++) {

				int rootEntBlock = rootInode->i_block[blocknum];
				int rootEntBlockIterator = 0;

				while (rootEntBlockIterator < EXT2_BLOCK_SIZE) {
					
					struct ext2_dir_entry * dirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * rootEntBlock + rootEntBlockIterator);

					char * dirEntryName = dirEntry->name;
					int dirEntryNameLen = dirEntry->name_len;
					// printf("direntname: %s %d \n", dirEntryName, dirEntryNameLen);
					
					if (((strncmp(dirToCreate, dirEntryName, dirEntryNameLen) == 0) && (dirEntry->file_type == EXT2_FT_REG_FILE)) || (dirEntry->file_type == EXT2_FT_SYMLINK)) {
						// ALREADYIN HERE
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
			int dirEntryNameLen = currDirEntry->name_len;
			if (((strncmp(dirToCreate, currDirEntName, dirEntryNameLen) == 0) && (currDirEntry->file_type == EXT2_FT_REG_FILE)) || (currDirEntry->file_type == EXT2_FT_SYMLINK)) {

				// ALREADY SOMETHING WITH THE SAME NAME, RETURN ERROR	
				return inodeID; // FIX LATER I GUESS

			}

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}
	
	return -1; // path is valid?

	// we might need to return something about which inode to make or something ...

}

int delEntry(int inodeParentID, char * fileToDeleteName) {

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int ftdnLen = strlen(fileToDeleteName);
	char fileToDelete[ftdnLen+1];
	memset(fileToDelete, 0, sizeof(fileToDelete));
	strncpy(fileToDelete, fileToDeleteName, ftdnLen+1);

	struct ext2_inode * finalDirectoryInode = (struct ext2_inode *)(inodeTable + inodeParentID * sizeof(struct ext2_inode));

	int numblocksFinalDir = finalDirectoryInode->i_blocks / DSR;
	int currblock;

	for (currblock = 0; currblock < numblocksFinalDir; currblock++) {
		
		int finalEntBlock = finalDirectoryInode->i_block[currblock];
		int finalEntBlockIterator = 0;
		int entryNum = 0;
		int previousRecLen = 0;

		while (finalEntBlockIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * currDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalEntBlock + finalEntBlockIterator);
			entryNum = entryNum + 1;

			char * currDirEntName = currDirEntry->name;
			int currDirEntNameLen = currDirEntry->name_len;
			if (((strncmp(fileToDeleteName, currDirEntName, currDirEntNameLen) == 0) && (currDirEntry->file_type == EXT2_FT_REG_FILE)) || (currDirEntry->file_type == EXT2_FT_SYMLINK)) {

				int inodeNum = currDirEntry->inode - 1;

				struct ext2_inode * fileToDeleteInode = (struct ext2_inode *)(inodeTable + inodeNum * sizeof(struct ext2_inode));
				// set the inode's dtime
				// construct a list of data blocks used and remove them and unallocate the blocks
				// unallocate the inode

				int realBlockNum = fileToDeleteInode->i_blocks / DSR;
				int allocatedBlocks[realBlockNum];
				int blockiter;
				int limit = 0;

				if (realBlockNum <= SINGLE_POINTER_COUNT) {
					limit = realBlockNum;
				} else {
					limit = SINGLE_POINTER_COUNT;
				}

				for (blockiter = 0; blockiter < limit; blockiter++) {

					allocatedBlocks[blockiter] = fileToDeleteInode->i_block[blockiter];
				}

				if (realBlockNum > SINGLE_POINTER_COUNT) {

					int extblk = blockiter + 1;
					unsigned int * indirectBlock = (unsigned int *)(disk + EXT2_BLOCK_SIZE * fileToDeleteInode->i_block[SINGLE_POINTER_COUNT]);

					int biterext;
					for (biterext=0; biterext < realBlockNum-SINGLE_POINTER_COUNT; biterext++) {
						allocatedBlocks[extblk] = indirectBlock[biterext];
						extblk++;

					}


				}

				int * inode_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_bitmap);
				int * bitmap_bits_int = (int *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_block_bitmap);
				*inode_bits_int = *inode_bits_int & ~(1 << (inodeNum));
				superBlock->s_free_inodes_count = superBlock->s_free_inodes_count + 1;
				blockGp->bg_free_inodes_count = blockGp->bg_free_inodes_count + 1;

				int iBlk;
				for (iBlk = 0; iBlk < realBlockNum; iBlk++) {

					int blockToDeallocate = allocatedBlocks[iBlk];
					*bitmap_bits_int = *bitmap_bits_int & ~(1 << (blockToDeallocate));
					superBlock->s_free_blocks_count = superBlock->s_free_blocks_count + 1;
					blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count + 1;


				}

				// now have to delete the directory entry

				fileToDeleteInode->i_links_count = 0; // i guess we reset it to zero
				fileToDeleteInode->i_dtime = (unsigned) time(NULL);

				if (entryNum == 1) {

					if (currDirEntry->rec_len == EXT2_BLOCK_SIZE) {

						currDirEntry->inode = 0;

						// *bitmap_bits_int = *bitmap_bits_int & !(1 << (finalEntBlock));
						// superBlock->s_free_blocks_count = superBlock->s_free_blocks_count + 1;
						// blockGp->bg_free_blocks_count = blockGp->bg_free_blocks_count + 1;	
						// finalDirectoryInode->i_blocks = finalDirectoryInode->i_blocks - DSR;

						return 0;	


					} else {

						currDirEntry->inode = 0;

						return 0;

					}

				} else {

					struct ext2_dir_entry * prevDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalEntBlock + finalEntBlockIterator - previousRecLen);
					prevDirEntry->rec_len = prevDirEntry->rec_len + currDirEntry->rec_len; // skips over

					return 0;
				}

				// return 0;
				

			}

			previousRecLen = currDirEntry->rec_len;

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}



	// removing the file:
	// 1. enter the directory of this parent
	// 2. look for the directory location of the child
	// 3. Get it's inode
	// 4. Unallocate the inode from bitmap
	// 5. Set the inode's dtime
	// 6. Get a list of data blocks it points to
	// 7. For each data block, unallocate it from the block bitmap
	

	// removing the dir entry:
	// 1. keep a flag for if it is the first element in a block
	// 2. keep a flag for if it is the only element in a block
	// 3. if it is the first element in a block
	//		if it is the only element in a block:
	//			get the block number of this dir data block
	//			unallocate it from the bitmap
	//			return 
	//		set the next element's 


	return -1;


}


void rmove(char * path) {

	int pathLen = strlen(path);
	char pathname[pathLen+1];
	memset(pathname, 0, sizeof(pathname));
	strncpy(pathname, path, pathLen+1);

	if (pathname[pathLen-1] == '/') {
		pathname[pathLen-1] = 0;
	}

	if (pathname[0] == '/') {
		memmove(pathname, pathname+1, pathLen);
	}

	char * fileToDeleteName = basename(pathname);

	// REMOVE THE LAST / FROM THE PATH NAME OR IT WILL SEGFAULT 

	// for test purposes
	//struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	//unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);
	//struct ext2_inode * parentDirInode1 = (struct ext2_inode *)(inodeTable + 1 * sizeof(struct ext2_inode));
	// for test purposes

	//printf("blockloc: %d \n", parentDirInode1->i_block[0]);

	int inodeIndexOfParentDir = goodPathRM(pathname);

	if (inodeIndexOfParentDir < 0) {
		if (inodeIndexOfParentDir == -1) {
			//ENOENT
			errno = ENOENT;
			perror("Path does not exist.\n");
			exit(EXIT_FAILURE);
		} else {
			//EEXIST
			errno = EEXIST;
			perror("File/directory with same name as directory you are attempting to create exists.\n");
			exit(EXIT_FAILURE);
		}
	} 

	// need to go into this inode and check for entry

	if (delEntry(inodeIndexOfParentDir, fileToDeleteName) < 0) {
		errno = ENOENT;
		perror("Unable to delete the file. \n");
		exit(EXIT_FAILURE);

	}


}

int main(int argc, char ** argv) {

    if(argc != 3) {
        fprintf(stderr, "Usage: %s <image file name> <absolute path>\n", argv[0]);
        exit(1);
    }
    int fd = open(argv[1], O_RDWR);

    disk = mmap(NULL, 128 * 1024, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if(disk == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }

	rmove(argv[2]);

	return 0;
}
