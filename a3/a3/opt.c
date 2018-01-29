#include <stdio.h>
#include <assert.h>
#include <unistd.h>
#include <getopt.h>
#include <stdlib.h>
#include "pagetable.h"
#include "sim.h"


extern unsigned memsize;

extern int debug;

extern struct frame *coremap;

typedef struct vaddr_node {
	char type;
	addr_t vaddr;
	struct vaddr_node * next;
} vnode;

vnode * head_vnode;
vnode * curr_vnode;

int * distance_array; //store distances to next access of potential_evict

//helper function to return greatest distance
int return_greatest_distance_index(int * array){
	int j;
	int greatest_distance = 0;
	int greatest_distance_index;

	for (j = 0; j < memsize; j++) {
		if(greatest_distance < array[j]) {
			greatest_distance = array[j];
			greatest_distance_index = j;
		}
	}

	return greatest_distance_index;
}

/* Page to evict is chosen using the optimal (aka MIN) algorithm.
 * Returns the page frame number (which is also the index in the coremap)
 * for the page that is to be evicted.
 */
int opt_evict() {

	int i;
	int index_to_evict;

	for (i = 0; i < memsize; i++) {

		int coremap_potential = coremap[i].pte->frame >> PAGE_SHIFT;
		char * mem_ptr = &physmem[coremap_potential * SIMPAGESIZE];
		addr_t * potential_evict = (addr_t *)(mem_ptr + sizeof(int));

		int distance_to_next_access = 0;

		vnode * runner_vnode = head_vnode->next;

		while(runner_vnode != NULL && runner_vnode->vaddr != *potential_evict) {
			distance_to_next_access += 1;
			runner_vnode = runner_vnode->next;
		}

		if (runner_vnode == NULL) { //page never accessed again
			return i;
		} else { //store the distance in the distance_array
			distance_array[i] = distance_to_next_access;
		}

	}

	index_to_evict = return_greatest_distance_index(distance_array);

	return index_to_evict;
}

/* This function is called on each access to a page to update any information
 * needed by the opt algorithm.
 * Input: The page table entry for the page that is being accessed.
 */
void opt_ref(pgtbl_entry_t *p) {

	vnode * new_head = head_vnode->next;
	free(head_vnode);
	head_vnode = new_head;

	return;
}

/* Initializes any data structures needed for this
 * replacement algorithm.
 */
void opt_init() {

	FILE * traceFilePointer;
	char buffer[MAXLINE];
	char accessType;
	addr_t virtualAddress;

	head_vnode = NULL;
	curr_vnode = NULL;

	distance_array = malloc(sizeof(int) * memsize);

	//open tracefile and check for errors
	if(tracefile != NULL) {
		if((traceFilePointer = fopen(tracefile, "r")) == NULL) {
			perror("Error opening tracefile!");
			exit(1);
		}
	}

	//read one line at a time and create a linkedlist of virtual addresses
	while(fgets(buffer, MAXLINE, traceFilePointer) != NULL) {

		sscanf(buffer, "%c %lx", &accessType, &virtualAddress);

		//create a new node
		vnode * new_vnode = malloc(sizeof(vnode));
		new_vnode->type = accessType;
		new_vnode->vaddr = virtualAddress;
		new_vnode->next = NULL;

		//assign node to linkedlist
		if(head_vnode == NULL) {

			head_vnode = new_vnode;
			curr_vnode = head_vnode; //// either set to head or new

		}else{

			curr_vnode->next = new_vnode;
			curr_vnode = new_vnode;

		}
	}

	//set the current vnode back to head
	curr_vnode = head_vnode;

}