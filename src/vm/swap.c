#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#include "devices/disk.h"
#include "threads/vaddr.h"

static const size_t SECTORS_PER_PAGE = PGSIZE / DISK_SECTOR_SIZE;

struct disk *swap_disk;
struct bitmap *swap_map;

void vm_swt_init (void){
  swap_disk = disk_get (1, 1);

  if (swap_disk == NULL){
	PANIC ("NO SWAP DISK");
  }
  
  disk_sector_t swap_size = disk_size (swap_disk) / SECTORS_PER_PAGE;

  swap_map = bitmap_create ((size_t) swap_size);
  if (swap_map == NULL)
	PANIC ("NO SWAP MAP");

  bitmap_set_all (swap_map, true);
  return;
}
bool vm_swap_in (int swap_index, void *upage){
  disk_sector_t start = (disk_sector_t) swap_index;
  int i = 0;
  for (i = 0; i < SECTORS_PER_PAGE; i++)
	disk_read (swap_disk, start * SECTORS_PER_PAGE + i,
		upage + i * DISK_SECTOR_SIZE);

  //Do some I/O Ops

  bitmap_flip (swap_map, start);
  return true;
}

int vm_swap_out (const void *upage){
  size_t swap_index = bitmap_scan_and_flip (swap_map, 0, 1, true);

  if (swap_index == BITMAP_ERROR ) 
    return -1;

  int i ;
  for (i = 0; i < SECTORS_PER_PAGE; i++)
	disk_write (swap_disk, swap_index * SECTORS_PER_PAGE + i,
		upage + DISK_SECTOR_SIZE * i);

  return (int) swap_index;
}
/*	
//bitmap_flip (swap_map, swap_index);
  return swap_index;
}
*/

void vm_swap_free (int swap_index){
  bitmap_set (swap_map, (size_t) swap_index, true);
}
