#include <string.h>
#include <debug.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
/*
#define BUFFER_SIZE 64

struct cache_entry {
  disk_sector_t disk_sector;
  uint8_t buf[DISK_SECTOR_SIZE];
  bool empty;
  bool dirty;
  bool in_use;
  bool access;
};
*/
static struct cache_entry cache[BUFFER_SIZE];

struct lock cache_lock;

void cache_init (void){
  lock_init (&cache_lock);

  int i;
  for (i=0; i<BUFFER_SIZE; i++){
	cache[i].empty = true;
  }
}

void cache_destroy (void){
  int i = 0;
  for (i = 0; i< BUFFER_SIZE; i++){
	if (!cache[i].empty){
	  cache_clear (&(cache[i]), true);
	}
  }
}

struct cache_entry *cache_lookup (disk_sector_t sector){
  int i;
  for (i = 0; i<BUFFER_SIZE; i++){
	if (cache[i].empty){
	  continue;
	}
	if (cache[i].disk_sector == sector){
	  //cache[i].in_use = true;
	  return (&cache[i]);
	}
  }
  return NULL;
}

struct cache_entry *cache_return (disk_sector_t sector, bool write){
  lock_acquire (&cache_lock);

  struct cache_entry *e = cache_lookup (sector);
  if (e != NULL){
	printf("cache_return: cache hit!\n");
  }else{
	printf("let's evict\n");
	e = cache_evict_SC ();
	ASSERT (e != NULL);
	//printf("gottit\n");
    cache_add (e, sector, write);
  }
  //ASSERT (e != NULL);
  e->in_use = true;
  //printf("really?\n");
  lock_release (&cache_lock);
  return e;
}

struct cache_entry *cache_evict_SC (void){
  struct cache_entry *e;
  bool second = false;
  int i = 0;
SCAN:
  //find
  for (i = 0; i < BUFFER_SIZE; i++){
	if (cache[i].empty){
	  printf("eviction scheme : [%d]\n", i);
	  return (&cache[i]);
	}
  }
  for (i=0; i<BUFFER_SIZE; i++){
	if (cache[i].in_use){
	  continue;
	}
	if (cache[i].access){
	  cache[i].access = false;
	  continue;
	}else{
	  printf("in SC: changing slot [%d] \n",i);
	  cache_clear (&cache[i], true);
	  return (&cache[i]);
	}
  }

  if (!second){
	second = true;
	goto SCAN;
  }else{
	return NULL;
  }
  return NULL;
}

void cache_add (struct cache_entry *e, disk_sector_t sector, bool write){
  e->disk_sector = sector;
  e->empty = false;
  e->dirty = write;
  e->access = true;
  disk_read (filesys_disk, e->disk_sector, e->buf);
}

void cache_clear (struct cache_entry *victim, bool delete){
  if (victim->dirty){
	//printf("save dirty...\n");
	disk_write (filesys_disk, victim->disk_sector, &victim->buf);
	victim->dirty = false;
  }
  //victim->disk_sector = NULL;
  if (delete){
	victim->disk_sector = NULL;
	victim->empty = true;
  }
  //sdfsd
}
void cache_read (disk_sector_t sector, void *addr){
  disk_read (filesys_disk, sector, addr);
}

void cache_write (disk_sector_t sector, void *addr){
  disk_read (filesys_disk, sector, addr);
}


