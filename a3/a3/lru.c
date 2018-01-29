#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"


extern int memsize;

extern int debug;

extern struct frame *coremap;


/* Page to evict is chosen using the accurate LRU algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */

int lru_evict() {

	int min = coremap[0].count;
	int minID = 0;
	int id;
	for (id = 0; id < memsize; id++) {
		if (min > coremap[id].count) {
			min = coremap[id].count;
			minID = id;
		}
	}

	return minID;
}

/* This function is called on each access to a page to update any information
 * needed by the lru algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void lru_ref(pgtbl_entry_t *p) {

	int idy;

	for (idy = 0; idy < memsize; idy++) {
		if (coremap[idy].pte == p) {
			break;
		}
	}

	coremap[idy].count = coremap[idy].count + 1;

	return;
}


/* Initialize any data structures needed for this
 * replacement algorithm
 */
void lru_init() {
	return;
}