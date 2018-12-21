#include <debug.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/malloc.h"
#include "threads/thread.h"

//bool can_read_ahead = true;
struct list aheads;
struct ahead{
  disk_sector_t sector;
  struct list_elem e;
};
void read_ahead (disk_sector_t );

struct condition empty;
void cache_init (void){
  list_init (&cache);
  cond_init (&empty);
  list_init (&aheads); 
  lock_init (&cache_lock);
  cache_size = 0;
  thread_create ("writeback", 0, write_back, NULL);
  thread_create ("readahead", 0, read_ahead, NULL);
}


void cache_bye (void){
  lock_acquire (&cache_lock);
  struct cache_entry *c;
  struct list_elem *e = list_begin (&cache);
  struct list_elem *next;
  while (e != list_end (&cache)){
	next = list_next (e);
	struct cache_entry *c = list_entry (e, struct cache_entry, c_elem);
//	if (c->in_use>0)
//	  continue;
	if (c->dirty){
	  disk_write (filesys_disk, c->sector, c->buf);
	  c->dirty = false;
	}
	list_remove (&c->c_elem);
	free (c);
	e= next;
	cache_size--;
  }
  lock_release (&cache_lock);	
}

void cache_destroy (void){
  lock_acquire (&cache_lock);
  struct cache_entry *c;
  struct list_elem *e = list_begin (&cache);
  struct list_elem *next;
  while (e != list_end (&cache)){
	next = list_next (e);
	struct cache_entry *c = list_entry (e, struct cache_entry, c_elem);
	if (c->in_use>0)
	  continue;
	if (c->dirty){
	  disk_write (filesys_disk, c->sector, c->buf);
	  c->dirty = false;
	}
	list_remove (&c->c_elem);
	free (c);
	e= next;
	cache_size--;
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
  //init_read_ahead (sector+1);
  //printf("let's put ahead\n");
  put_read_ahead (sector+1);
  lock_acquire(&cache_lock);
 // put_ahead (sector+1)
 // lock_release (&cache_lock);
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
	bool second = true;

	struct list_elem *e;
	while (second){
	for ( e = list_begin (&cache);
		e != list_end (&cache);
		e = list_next (e)){
	  c = list_entry (e, struct cache_entry, c_elem);
	  if (c->in_use > 0)
		continue;
	  if (c->access){
		c->access = false;
	  }else{
		//list_remove (e);
		if (c->dirty){
		  disk_write (filesys_disk, c->sector, &c->buf);
		  
		}
		//free(c->buf);
		second = false;
		break;
	  }
	}
	}
  }

  c->in_use = 1;
  c->sector = sector;
  c->dirty = write;
  c->access = true;
  disk_read (filesys_disk, sector, c->buf);
  return c;
}

void write_back (void *aux UNUSED){
  while (true)
  {
	timer_sleep (1000);
	cache_destroy ();
  }
}
/*
void read_ahead (void *aux){
  disk_sector_t ahead = * (disk_sector_t *)aux;
  lock_acquire (&cache_lock);
  struct cache_entry *c = cache_lookup (ahead);
  if (c == NULL)
	c = cache_evict_SC (ahead, false);
  lock_release (&cache_lock);

  free (aux);
}
*/
void put_read_ahead (disk_sector_t ahead){
  lock_acquire (&cache_lock);
  struct ahead *a = malloc (sizeof *a);
  if (list_empty (&aheads)){
	a->sector = ahead;
	list_push_back (&aheads, &a->e);
	cond_signal (&empty, &cache_lock);
  }else{
	a->sector = ahead;
	list_push_back (&aheads, &a->e);
  }
  lock_release (&cache_lock);
}

  

void read_ahead (disk_sector_t ahead){
  while(true){
  struct list_elem *temp;
  lock_acquire (&cache_lock);
  if (list_empty (&aheads)){
	lock_release (&cache_lock);
	//continue;
	cond_wait (&empty, &cache_lock);
  }

  temp = list_begin (&aheads);
  lock_release (&cache_lock);
  //lllldjdjdjdjdkdkdkdkdkdk
  lock_acquire (&cache_lock);
  struct ahead *ahead = list_entry (temp, struct ahead, e);
  struct cache_entry *c = cache_lookup (ahead);
  //lul
  if (c == NULL)
	c = cache_evict_SC (c->sector, false);

  list_remove (temp);
  free (ahead);
  lock_release (&cache_lock);
  /*
  disk_sector_t *temp = calloc (1, sizeof *temp);
  if (temp){
	*temp = ahead+1;
	thread_create ("read",0, read_ahead, temp);
  }
  can_read_ahead = true;
  */
  }
}
