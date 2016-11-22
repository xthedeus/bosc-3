/*
Main program for the virtual memory project.
Make all of your modifications to this file.
You may add or rearrange any code or data as you need.
The header files page_table.h and disk.h explain
how to use the page table and disk interfaces.
*/

#include "page_table.h"
#include "disk.h"
#include "program.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

struct disk *disk;
const char *pageAlgo;
int *framePagePointer;
int diskWriteCounter = 0;
int diskReadCounter = 0;
int pageFaultCounter = 0;
int fifoFrameCounter = 0;

void random_algorithm(struct page_table *pt, int page);
void fifo_algorithm(struct page_table *pt, int page);

void frame_assigner(struct page_table *pt, int page, int frame) {
	int existingFrame = 0;
	int bits = 0;
	page_table_get_entry(pt, page, &existingFrame, &bits);
	char *physmem = page_table_get_physmem(pt);

	switch((int)bits) {
		case 0: {
			int existingPage = framePagePointer[frame];
			if(existingPage != -1) { //if -1 the page has not been loaded yet
				int phyFrame = 0;
				int virBits = 0;
				page_table_get_entry(pt, existingPage, &phyFrame, &virBits); //gets frame and bits of the page in memory
				if(virBits == (PROT_READ|PROT_WRITE)) { //checks if the page has the write flag meaning we need to write to disk before exchanging the page of the frame
					diskWriteCounter++; //increase write counter
					disk_write(disk, existingPage, &physmem[frame*PAGE_SIZE]); //write data to disk
				}				
				page_table_set_entry(pt, existingPage, 0, 0); //updates the page table and make the new assigned page have no rights (read/write)
			}
			//assign the memory frame to the new page
			framePagePointer[frame] = page; //updates framePagePointer
			diskReadCounter++; //increase read counter
			disk_read(disk, page, &physmem[frame*PAGE_SIZE]); //reads data from disk to memory frame
			page_table_set_entry(pt,page,frame,PROT_READ); //updates page table
			break;
		}
		case PROT_READ: { // page already has its data in memory and is requesting to write
			page_table_set_entry(pt, page, existingFrame, PROT_READ|PROT_WRITE);
			break;
		}
	}
}

void page_fault_handler( struct page_table *pt, int page )
{
	pageFaultCounter++;
	if(!strcmp(pageAlgo,"rand")) {
		random_algorithm(pt,page);
	} else if(!strcmp(pageAlgo,"fifo")) {
		fifo_algorithm(pt,page);
	} else {
		printf("Algorithm not found\n");
		exit(1);
	}
	page_table_print_entry(pt,page);
	//printf("page fault on page #%d\n",page);
	//exit(1);
}

void random_algorithm(struct page_table *pt, int page) {
	int randomTargetFrame = lrand48() % page_table_get_nframes(pt); //gets a random frame
	frame_assigner(pt, page, randomTargetFrame);
}

void fifo_algorithm(struct page_table *pt, int page) {
	int numberOfFrames = page_table_get_nframes(pt);
	int targetFrame = fifoFrameCounter; 
	if(targetFrame >= numberOfFrames) { // check if we have reached the end of the physical memory and therefore need to go back to the first frame
		// if reached the end of the physical memory then reset
		targetFrame = 0;
		fifoFrameCounter = 0;
	} else {
		// increase the physical frame counter
		fifoFrameCounter++;
	}
	frame_assigner(pt, page, targetFrame);
}

int main( int argc, char *argv[] )
{
	if(argc!=5) {
		printf("use: virtmem <npages> <nframes> <rand|fifo|custom> <sort|scan|focus>\n");
		return 1;
	}

	int npages = atoi(argv[1]);
	int nframes = atoi(argv[2]);
	pageAlgo = argv[3];
	const char *program = argv[4];
	framePagePointer = malloc(sizeof(int) * nframes);

	// initialize all values to -1 of the framePagePointer array
	int i;
	for(i = 0; i < nframes; i++) {
		framePagePointer[i] = -1;
	}

	disk = disk_open("myvirtualdisk",npages);
	if(!disk) {
		fprintf(stderr,"couldn't create virtual disk: %s\n",strerror(errno));
		return 1;
	}


	struct page_table *pt = page_table_create( npages, nframes, page_fault_handler );
	if(!pt) {
		fprintf(stderr,"couldn't create page table: %s\n",strerror(errno));
		return 1;
	}

	char *virtmem = page_table_get_virtmem(pt);

	char *physmem = page_table_get_physmem(pt);

	if(!strcmp(program,"sort")) {
		sort_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"scan")) {
		scan_program(virtmem,npages*PAGE_SIZE);

	} else if(!strcmp(program,"focus")) {
		focus_program(virtmem,npages*PAGE_SIZE);

	} else {
		fprintf(stderr,"unknown program: %s\n",argv[3]);

	}

	page_table_delete(pt);
	disk_close(disk);

	printf("Number of page faults: %d\nNumber of disk reads: %d\nNumber of page writes: %d\n", pageFaultCounter, diskReadCounter, diskWriteCounter);

	return 0;
}
