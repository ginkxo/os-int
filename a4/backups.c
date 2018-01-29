	/*
	char * etc1 = "HELLO.txt"; 
    char * etc2 = "another/hello/HELLO.txt";
    
    int etc1len = strlen(etc1);
    int etc2len = strlen(etc2);
    
    char etc01[etc1len+1];
    char etc02[etc2len+1];
    
    memset(etc01, 0, sizeof(etc01));
    memset(etc02, 0, sizeof(etc02));
    
    strncpy(etc01, etc1, etc1len+1);
    strncpy(etc02, etc2, etc2len+1);
    
    
    printf("%s %s \n", dirname(etc01), basename(etc01)); --> . HELLO.txt
    printf("%s %s \n", dirname(etc02), basename(etc02)); --> another/hello HELLO.txt

    */

    /*

    potential ext2Path options:

    	hello.txt a/b/file.txt
    		- if file.txt exists there, then return EEXIST
    		- if file.txt does not exist there, then return inode ID of b

    	in this case:
    		dirname: a/b
    		basename: file.txt

		have to iterate to directory b, and then check entries for file.txt

    	hello.txt a/b
    		- if hello.txt exists in b, then return EEXIST
    		- if hello.txt does not exist there, then return inode ID of b

    	in this case:
    		dirname: a
    		basename: b

    	have to iterate to directory b, and then check entries for 



    */

/*
duoflag fileGoodPath (char * ext2Path, char * localPath) {

	// INITIALIZATIONS

	duoflag d;
	d.flags[0] = 0;
	d.flags[1] = 0;

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE *2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int inodeID = EXT2_ROOT_INO - 1;
	//int previousInodeID = EXT2_ROOT_INO - 1;
	int FINAL_TOKEN_FLAG = 0;

	// CONVERTING FROM STRING CONSTANTS

	int ext2PathLen = strlen(ext2Path);
	char basepath[ext2PathLen+1];
	memset(basepath, 0, sizeof(basepath));
	strncpy(basepath, ext2Path, ext2PathLen+1);

	// unlike mkdir, here we are going to work with the entire basepath a/b/hello.txt or a/b

	int localPathLen = strlen(localPath);
	char localpath[localPathLen+1];
	memset(localpath, 0, sizeof(localpath));
	strncpy(localpath, localPath, localPathLen+1);

	// ext2 path name to find parent dir, file to create name, and real file name

	// char * fileToCreateConstant = basename(basepath); // "HELLO.txt"
	 //char * pathnameConstant = dirname(basepath); // "another/hello" or "." if root

	char * realFileConstant = basename(localpath); // "HELLO.txt" or w/e

	// convert the above to string arrays

	// int fTCLen = strlen(fileToCreateConstant);
	// int pCLen = strlen(pathnameConstant);
	int rFLen = strlen(realFileConstant);

	// char fileToCreate[];
	// char pathname[];
	char realFile[rFLen+1];
	memset(realFile, 0 sizeof(realFile));
	strncpy(realFile, realFileConstant, rFLen+1);

	char * parentDir;
	char * childDir;

	parent = strtok(basepath, "/");
	child = strtok(NULL, "/");

	char dotRef[EXT2_NAME_LEN+1] = ".";

	while (parent != NULL && inodeID != -1) {

		// CASE ONE: parentDir == ".":
			// if childDir != NULL:
				// search parentDir for the childDir name
			// else:
				// search parentDir for the realFile name

		if (strncmp(parent, dotRef, ext2PathLen) == 0) {

			// search parentDir for the childDir name

			struct ext2_inode * rootInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));
			int numBlocks = rootInode->i_blocks / DSR;
			int blocknum;
			int filenamelen = 1; // just in case

			if (child != NULL) {

				filenamelen = strlen(child);

			}

			// search parentDir for the realFile name

			else {

				filenamelen = strlen(realFile);

			}

			char targetName[filenamelen+1];
			memset(targetName, 0, sizeof(targetName));

			if (child != NULL) {

				strncpy(targetName, child, filenamelen+1);


			} else {

				strncpy(targetName, realFile, filenamelen+1);

			}

			for (blocknum = 0; blocknum < numBlocks; blocknum++) {

				int rootEntBlock = rootInode->i_block[blocknum];
				int rootEntBlockIterator = 0;

				while (rootEntBlockIterator < EXT2_BLOCK_SIZE) {

					struct ext2_dir_entry * dirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * rootEntBlock + rootEntBlockIterator);
					char * dirEntryName = dirEntry->name;

					if (strncmp(targetName, dirEntryName, EXT2_NAME_LEN) == 0) {
						
						// get its inode
						// if directory, return {0, its inode}
						// if file, return {0, -2}

						if (dirEntry->file_type == EXT2_FT_REG_FILE) {

							d.flags[0] = 0;
							d.flags[1] = -2;
							return d;

						} else if (dirEntry->file_type == EXT2_FT_DIR) {

							struct ext2_inode * finalDirInode = (struct ext2_inode *)(inodeTable + dirEntInodeID * sizeof(struct ext2_inode));

							int finalBlks = currInode->i_blocks / DSR;
							int finalCurr;

							for (finalCurr = 0; finalCurr < finalBlks; finalCurr++) {

								int finalDEBlock = finalDirInode->i_block[finalCurr];
								int finalDEBlockIterator = 0;

								while (finalDEBlockIterator < EXT2_BLOCK_SIZE) {

									struct ext2_dir_entry * finalDE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalDEBlock + finalDEBlockIterator);

									char * finalDEName = finalDE->name;

									if (strncmp(targetName,finalDEName, EXT2_NAME_LEN) == 0) {

										d.flags[0] = 0;
										d.flags[1] = -2;
										return d;

									}


								} // while


							} // for



						}
						

					}

					rootEntBlockIterator = rootEntBlockIterator + dirEntry->rec_len;


				}


			}

			d.flags[0] = 1;
			d.flags[1] = EXT2_ROOT_INO - 1;

			return d;


		} // closes case 1

		// now on to the other cases
		// this is the situation where
		// parent is NOT "." 

		struct ext2_inode * currInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

		int directoryExists = 0;
		int newInodeID = -1;
		int numbks = currInode->i_blocks / DSR;
		int curr;

		if (child == NULL) {
			// in parent, we have the last token
			// options:
			// we will iterate through the dir entries of currInode
			// we need to find parent as a last token
			// if parent exists and we find it, we do two things:
				// check its inode
				// if it is a directory:
					// we have to check this directory for the input file name
					// if it exists, return the duoflag {0,-2}
					// if it doesn't, return the duoflag {1,directoryInode}
				// if it is a file, return the duoflag {0,-2}
					// this implies {EXISTS, EEXIST} returns out in copy
			// if parent DOES not exist
				// we are going to treat it like creating a new file 
				// return the duoflag {1,currInodeNumber}
					// this implies {DOESNTEXIST, create target file with this name in this inode dir}

			FINAL_TOKEN_FLAG = 1;

		}

		for (curr = 0; curr < numbks; curr++) {

			int directoryEntryBlock = currInode->i_block[curr];
			int dirEntBlockIterator = 0;


			while (dirEntBlockIterator < EXT2_BLOCK_SIZE) {
		
				struct ext2_dir_entry * directoryEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * directoryEntryBlock + dirEntBlockIterator);
			
				char * dirEntName = directoryEntry->name;
				int dirEntInodeID = directoryEntry->inode - 1;

				if (strncmp(parent, dirEntName, lenpathname) == 0) {
				// THE NAME of the current looked directory is in this directory!

					if (directoryEntry->file_type == EXT2_FT_REG_FILE) {

						if (FINAL_TOKEN_FLAG) {
							d.flags[0] = 0;
							d.flags[1] = -2;
							return d;
						} else {
							// ERROR : NAME OF FLE
							d.flags[0] = -1;
							d.flags[1] = 0;
							return d; // ENOENT
						}

					} else if (directoryEntry->file_type == EXT2_FT_DIR) {

						if (!FINAL_TOKEN_FLAG) {
							directoryExists = 1;
							newInodeID = dirEntInodeID;
							break;

						} else {

							// we have to check this directory for the input file name
							// if it exists, return the duoflag {0,-2}
							// if it doesn't, return the duoflag {1,directoryInode}

							struct ext2_inode * finalDirInode = (struct ext2_inode *)(inodeTable + dirEntInodeID * sizeof(struct ext2_inode));

							int finalBlks = currInode->i_blocks / DSR;
							int finalCurr;

							for (finalCurr = 0; finalCurr < finalBlks; finalCurr++) {

								int finalDEBlock = finalDirInode->i_block[finalCurr];
								int finalDEBlockIterator = 0;

								while (finalDEBlockIterator < EXT2_BLOCK_SIZE) {

									struct ext2_dir_entry * finalDE = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * finalDEBlock + finalDEBlockIterator);

									char * finalDEName = finalDE->name;

									if (strncmp(realFile,finalDEName, EXT2_NAME_LEN) == 0) {

										d.flags[0] = 0;
										d.flags[1] = -2;
										return d; // EEXIST {0,-2}

									}


								}


							}

							d.flags[0] = 1;
							d.flags[1] = dirEntInodeID;
							return d; // {1, dirEntInodeID} <- needs to go in here

						}
					}

				}

				dirEntBlockIterator = dirEntBlockIterator + directoryEntry->rec_len;

			} // CLOSES dirEntBlockIterator while loop

			if (newInodeID != -1) {
				break;
			}

		} // closes the for block loop

		if (FINAL_TOKEN_FLAG && (!directoryExists)) {

			// THE ENTRY WITHT HAT NAME DIDNT EXIST
			// but this is the final token
			// we return the duoflag right here

			d.flags[0] = 1;
			d.flags[1] = inodeID;
			return d;

		}

		if ((!directoryExists) || newInodeID == -1) {

			d.flags[0] = -1;
			d.flags[1] = 0;

			return d; // {-1,-1} ENOENT

		} else {

			parent = child;
			child = strtok(NULL,"/");
			inodeID = newInodeID;
			directoryExists = 0;
		}




	} // closes the main inodeID while loop

	d.flags[0] = 1;
	d.flags[1] = inodeID;
	return d;

}

*/

/*

int fileGoodpath (char * basepath, char * localPath) {

	// localPath: the relative path of the file you wanna copy
	// basepath: the absolute path of where you are copying 

	// will either return an error code, an EEXIST code, or the parent inode number

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);
	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);

	int inodeID = EXT2_ROOT_INO - 1;

	char * lastItemConstant = basename(basepath);
	char * pathnameConstant = dirname(basepath);

	char * lastItemLocal = basename(localPath);

	char lastItem[EXT2_NAME_LEN];
	char pathnameSansEnd[EXT2_NAME_LEN * 10];
	char pathname[EXT2_NAME_LEN * 1000];

	char lastItemLocalA[EXT2_NAME_LEN];

	memset(lastItem, 0, EXT2_NAME_LEN);
	memset(pathnameSansEnd, 0, EXT2_NAME_LEN * 10);
	memset(pathname, 0, EXT2_NAME_LEN * 1000);

	memset(lastItemLocalA, 0, EXT2_NAME_LEN);

	strncpy(lastItem, lastItemConstant, EXT2_NAME_LEN);
	strncpy(pathnameSansEnd, pathnameConstant, EXT2_NAME_LEN * 10);
	strncpy(pathname, basepath, EXT2_NAME_LEN * 1000);
	
	strncpy(lastItemLocalA, lastItemLocal, EXT2_NAME_LEN);

	char * parentDir;
	char * childDir;

	parentDir = strtok(pathname, "/");
	childDir = strtok(NULL, "/");

	struct stat lbuf;

	int status = lstat(basepath, &lbuf);
	if (status < 0) {
		fprintf(stderr, "Failure \n");
		return -1;
	} 

	int isfile = S_ISREG(lbuf.st_mode);
	int isdir = S_ISDIR(lbuf.st_mode);
	int islnk = S_ISLNK(lbuf.st_mode);

	int size = lbuf.st_size;

	if (isfile || islnk) {
		
		if (strncmp(lastItemLocalA, lastItem, EXT2_NAME_LEN) == 0) {
			return -2 // EEXIST
		} else {
			return -1 // trying to copy into invalid path
		}

	}

	char dotRef[EXT2_NAME_LEN] = ".";

	// how do we handle links ...

	// get to while loop only if isdir
	while (parentDir != NULL && inodeID != -1) {

		////////////
		// CASE ONE: the target dir IS JUST the root (a dot)
		///////////

		if (strncmp(parentDir, dotRef, EXT2_NAME_LEN) == 0 & childDir == NULL) {

			struct ext2_inode * rootInode (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

			int numBlocks = rootInode->i_blocks / DSR;
			int blocknum;

			for (blocknum = 0; blocknum < numBlocks; blocknum++) {

				int rootEntBlock = rootInode->i_blocks[blocknum];
				int rootEntBlockIterator = 0;

				while (rootEntBlockIterator < EXT2_BLOCK_SIZE) {
					
					struct ext2_dir_entry * dirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * rootEntBlock + rootEntBlockIterator);

					char * dirEntryName = dirEntry->name;
					
					if (strncmp(lastItemLocalA, dirEntryName, EXT2_NAME_LEN) == 0) {
						// ALREADYIN HERE
						return -1; // FIX for error stuff 
					}

					rootEntBlockIterator = rootEntBlockIterator + dirEntry->rec_len;
					
				} // closes the while loop

			} // closes the block loop

			return EXT2_ROOT_INO; // the file will be copied as an element of the root directory
			
		} // closes case one if statement


		////////////
		// CASE TWO: the target is on a path with at least one directory 
		///////////

		struct ext2_inode * directoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

		int numBlk = directoryInode->i_blocks / DSR;
		int blknm;

		for (blknm = 0; blknm < numBlk; blknm++) {

			int directoryEntryBlock = directoryInode->i_blocks[blknm];
			int dirEntBlockIterator = 0;

			int directoryExists = 0;
			int newInodeID = -1;

			while (dirEntBlockIterator < EXT2_BLOCK_SIZE) {

				struct ext2_dir_entry * directoryEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE * directoryEntryBlock + dirEntBlockIterator);

				char * dirEntName = directoryEntry->name;
				int dirEntNodeID = directoryEntry->inode;

				if (strncmp(parentDir, dirEntName, EXT2_NAME_LEN) == 0) {

					if (directoryEntry->file_type == EXT2_FT_REG_FILE) {

						// error: invalid path
						return -1;

					} else if (directoryEntry->file_type == EXT2_FT_DIR) {
						directoryExists = 1;
						newInode = dirEntInodeID;
					}

				}

				dirEntBlockIterator = dirEntBlockIterator + directoryEntry->rec_len;
			} // closes the while loop

		

		} // closes the block loop

		if (!directoryExists) {

			// error: invalid path
			return -1;

		} else {
			parentDir = childDir;
			childDir = strtok(NULL,"/");
			inodeID = newInodeID;
			directoryExists = 0;

		}
		
		

	} // closes inodeID while loop

	/////////
	// FINAL PART: now, the inodeID should point to the inode ID of the target directory
	// we just check in here if something already has the same name as the file to copy
	// if it doesn't, we return the inode ID of this parent directory
	/////////

	struct ext2_inode * finalDirectoryInode = (struct ext2_inode *)(inodeTable + inodeID * sizeof(struct ext2_inode));

	int numbloxFinalDir = finalDirectoryInode->i_blocks / DSR;
	int currblk;

	for (currblk = 0; currblk < numbloxFinalDir; currblk++) {

		int finalEntBlock = finalDirectoryInode->i_block[currblock];
		int finalEntBlockIterator = 0;

		while (finalEntBlockIterator < EXT2_BLOCK_SIZE) {

			struct ext2_dir_entry * currDirEntry = (struct ext2_dir_entry *)(disk + EXT2_BLOCK_SIZE* finalEntBlock + finalEntBlockIterator);

			char * currDirEntName = currDirEntry->name; // CAN WE STRNCMP WITH A CONSTANT CAR LIKE THIS ...

			if (strncmp(lastItemLocalA, currDirEntName, EXT2_NAME_LEN) == 0) {

				// already something with the same name, return error of some kind
				return -1; // change later

			}

			finalEntBlockIterator = finalEntBlockIterator + currDirEntry->rec_len;

		}

	}

	return inodeID; // should be the inodeID of the target directory

}

*/

int allocateNewFileInode(int filesizeBytes) {

	struct ext2_super_block * superBlock = (struct ext2_super_block *)(disk + EXT2_BLOCK_SIZE * 1);
	struct ext2_group_desc * blockGp = (struct ext2_group_desc *)(disk + EXT2_BLOCK_SIZE * 2);

	if (superBlock->s_free_inodes_count == 0 || superBlock->s_free_blocks_count == 0) {
		// FAILURE, NO SPACE	
		return -1; // ENOSPC, fix later
	}

	unsigned char * inodeTable = (unsigned char *)(disk + EXT2_BLOCK_SIZE * blockGp->bg_inode_table);
	
	int freeInodes = superBlock->s_free_inodes_count;
	int numInodes = superBlock->s_inodes_count;
	int totalInodes = freeInodes + numInodes; // is this actually equal
	int currInodeID;
	int firstEmptyInode = -1;

	for (currInodeID = EXT2_ROOT_INO - 1; currInodeID < totalInodes; currInodeID++) {

		if (currInodeID >= EXT2_GOOD_OLD_FIRST_INO) {

			struct ext2_inode * inode = (struct ext2_inode *)(inodetable + currInodeID * sizeof(struct ext2_inode));

			if (!inode->i_size) { // THIS COULD SEGFAULT ...

				firstEmptyInode = currInodeID;
				break; // the first inode slot from the front that is "empty"

			}

		}

	} // if this does segfault, we will change iteration to numInodes instead

	if (firstEmptyInode == -1) {
		// some kind of error i guess
		return -1;
	}

	struct ext2_inode * newInode = (struct ext2_inode *)(inodetable + firstEmptyNode * sizeof(struct ext2_inode));	
	
	double quo = (double) filesizeBytes / (double) EXT2_BLOCK_SIZE;
	int blocks = (int) ceil(quo);

	newInode->i_mode = EXT2_S_IFREG;
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

	// NEED TO ALLOCATE BLOCKS HERE
	
	/////// Might move these to until after block allocation is successful! ////////
	superBlock->s_inodes_count = superBlock->s_inodes_count + 1;
	superBlock->s_free_inodes_count = superBlock->s_free_inodes_count - 1;

	blockGp->bg_free_inodes_count = blockGp->bg_free_inodes_count - 1;
	// blockGp->bg_used_dirs_count = blockGp->bg_used_dirs_count + 1;

	return firstEmptyNode;
}