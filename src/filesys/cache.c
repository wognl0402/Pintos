#include <debug.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"

void cache_init (void){
  list_init (&cache);
  lock_init (&cache_lock);
  cache_size = 0;
}

void cache_destroy (void){
  lock_acquire (&cache_lock);
  struct cache_entry *c;
  struct list_elem *e = list_begin (&cache);
  struct list_elem *next;
  while (e != list_end (&cache)){
	next = list_next (e);
	struct cache_entry *c = list_entry (e, struct cache_entry, c_elem);
	if (c->dirty){
	  disk_write (filesys_disk, c->sector, &c->buf);
	}
	list_remove (&c->c_elem);
	free (c);
	e= next;
  }
  lock_release (&cache_lock);

	
}
struct cache_entry *cache_lookup (disk_sector_t sector){
  struct cache_entry *c;
  struct list_elem *e;
  for (e = list_begin (&cache);
	  e != list_end (&cache);
	  e = list_next (e)){
	c = list_entry (e, struct cache_entry, c_elem);
	if (c->sector == sector){
	  return c;
	}
  }
  return NULL;
}
	
struct cache_entry *cache_return (disk_sector_t sector, bool write){
  lock_acquire(&cache_lock);
  struct cache_entry *c = cache_lookup (sector);
  
  /*struct list_elem *e;
  for (e = list_begin (&cache);
	  e != list_end (&cache);
	  e = list_next (e)){
	c = list_entry (e, struct cache_entry, c_elem);
	if (c->sector == sector){
	  c->in_use = true;
	  c->dirty |= write;
	  c->access = true;
	  lock_release (&cache_lock);
	  return c;
	}
  }*/
  if (c){
	c->in_use ++;
	c->dirty |= write;
	c->access =true;
	lock_release (&cache_lock);
	return c;
  }
  c = cache_evict_SC (sector, write);
  ASSERT (c != NULL);
  lock_release (&cache_lock);
  return c;
}
struct cache_entry *cache_evict_SC (disk_sector_t sector, bool write){
  struct cache_entry *c;
  if (cache_size < BUFFER_SIZE){
	cache_size++;
	c = malloc (sizeof *c);
	if (!c)
	  PANIC("NO MEM in cache_evict_SC");
	c->in_use = 0;
	list_push_back (&cache, &c->c_elem);
  }
  else{
	bool second = false;
	struct list_elem *e;
	for ( e = list_begin (&cache);
		e != list_end (&cache);
		e = list_next (e)){
	  c = list_entry (e, struct cache_entry, c_elem);
	  if (c->in_use > 0)
		continue;
	  if (c->access){
		c->access = false;
	  }else{
		if (c->dirty){
		  disk_write (filesys_disk, c->sector, &c->buf);
		}
		//second = true;
		break;
	  }
	}
  }
  c->in_use++;
  c->sector = sector;
  disk_read (filesys_disk, c->sector, &c->buf);
  c->dirty = write;
  c->access = true;
  return c;
}
