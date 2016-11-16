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
int bits = 0;
int frame = 0;
int currentFifoFrame = 0;
int lastPage = 0;

void random_algorithm(struct page_table *pt, int page);
void fifo_algorithm(struct page_table *pt, int page);

void page_fault_handler( struct page_table *pt, int page )
{
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
	int bitMax = 0;
	int bitMaxPage = 0;
	int bitsSwitch = 0;
	int frameSwitch = 0;
	int randomTargetFrame = lrand48() % page_table_get_nframes(pt); // get random number target
	page_table_get_entry(pt, page, &frame, &bits);

	switch(bits) {
		case 0:
			for(int i = 0; i < page_table_get_npages(pt); i++) {
				page_table_get_entry(pt, i, &frameSwitch, &bitsSwitch);
				if(bits > 0) {
					bitMax = bitsSwitch;
					bitMaxPage = i;
				}
			}
			switch(bitMax) {
				
				case 0:
					page_table_set_entry(pt,page,randomTargetFrame,PROT_READ);
					disk_read(disk, page, &page_table_get_physmem(pt)[randomTargetFrame*disk_nblocks(disk)]);
					break;
				case 1:
					page_table_set_entry(pt,page,randomTargetFrame,PROT_READ);
					page_table_set_entry(pt,bitMaxPage,0,0);
					disk_read(disk, page, &page_table_get_physmem(pt)[randomTargetFrame*disk_nblocks(disk)]);
					break;
				case 3:
					disk_write(disk,bitMaxPage,&page_table_get_physmem(pt)[randomTargetFrame*disk_nblocks(disk)]);
					disk_read(disk,page,&page_table_get_physmem(pt)[randomTargetFrame*disk_nblocks(disk)]);
					page_table_set_entry(pt,page,randomTargetFrame,PROT_READ);
					page_table_set_entry(pt,bitMaxPage,0,0);
					break;
			}
			break;
		case 1:
			page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
			break;
	}
}

void fifo_algorithm(struct page_table *pt, int page) {
	int bitMaxPage = 0;
	int existingPageBits = 0;
	int existingPageFrame = 0;
	page_table_get_entry(pt, page, &frame, &bits);
	int numberOfFrames = page_table_get_nframes(pt);
	if((currentFifoFrame % numberOfFrames) == 0 && currentFifoFrame > 0) {
		page_table_get_entry(pt, (currentFifoFrame-numberOfFrames), &existingPageFrame, &existingPageBits);
		switch(existingPageBits) {
			case 1:
				page_table_set_entry(pt,page,0,PROT_READ);
				page_table_set_entry(pt,(currentFifoFrame-numberOfFrames),0,0);
				disk_read(disk, page, &page_table_get_physmem(pt)[0*disk_nblocks(disk)]);
				break;
			case 3:
				disk_write(disk,(currentFifoFrame-numberOfFrames),&page_table_get_physmem(pt)[0*disk_nblocks(disk)]);
				disk_read(disk,page,&page_table_get_physmem(pt)[0*disk_nblocks(disk)]);
				page_table_set_entry(pt,page,0,PROT_READ);
				page_table_set_entry(pt,(currentFifoFrame-numberOfFrames),0,0);
				break;
		}
	} else {
		switch(bits) {
		case 0:
			if(currentFifoFrame < numberOfFrames) {
				page_table_set_entry(pt,page,(currentFifoFrame % numberOfFrames),PROT_READ);
				disk_read(disk, page, &page_table_get_physmem(pt)[(currentFifoFrame % numberOfFrames)*disk_nblocks(disk)]);
			} else {
				int existingPage = (currentFifoFrame % numberOfFrames);
				page_table_get_entry(pt, existingPage, &existingPageFrame, &existingPageBits);

				switch(existingPageBits) {
					case 0:
						page_table_set_entry(pt,page,existingPage,PROT_READ);
						disk_read(disk, page, &page_table_get_physmem(pt)[existingPage*disk_nblocks(disk)]);
						break;
					case 1:
						page_table_set_entry(pt,page,existingPage,PROT_READ);
						page_table_set_entry(pt,bitMaxPage,0,0);
						disk_read(disk, page, &page_table_get_physmem(pt)[existingPage*disk_nblocks(disk)]);
						break;
					case 3:
						disk_write(disk,bitMaxPage,&page_table_get_physmem(pt)[existingPage*disk_nblocks(disk)]);
						disk_read(disk,page,&page_table_get_physmem(pt)[existingPage*disk_nblocks(disk)]);
						page_table_set_entry(pt,page,existingPage,PROT_READ);
						page_table_set_entry(pt,bitMaxPage,0,0);
						break;
				}
			}
			break;
		case 1:
			page_table_set_entry(pt,page,frame,PROT_READ|PROT_WRITE);
			break;
		}
	}
	if(currentFifoFrame == page && page != lastPage) {
		currentFifoFrame += 1;
	}
	lastPage = page;
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

	return 0;
}
