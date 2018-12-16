#ifndef FILESYS_CACHE_H
#define FILESYS_CACHE_H

#include "devices/disk.h"
#include "filesys/filesys.h"
#include <list.h>
#include "threads/synch.h"
#define BUFFER_SIZE 64

struct list cache;
struct lock cache_lock;
uint32_t cache_size;

struct cache_entry {
  disk_sector_t sector;
  uint8_t buf[DISK_SECTOR_SIZE];
  bool empty;
  bool dirty;
  int in_use;
  bool access;
  struct list_elem c_elem;
};

void cache_init (void);
void cache_destroy (void);

struct cache_entry *cache_lookup (disk_sector_t sector);
struct cache_entry *cache_return (disk_sector_t sector, bool write);
struct cache_entry *cache_evict_SC (disk_sector_t sector, bool write);

/*
struct cache_entry *cache_lookup (disk_sector_t sector);
struct cache_entry *cache_return (disk_sector_t sector, bool write);

//struct cache_entry *cache_evict_SC (disk_sector_t sector, bool write);
struct cache_entry *cache_evict_SC (void);

void cache_add (struct cache_entry *, disk_sector_t, bool);
void cache_clear (struct cache_entry *, bool);

void cache_read (disk_sector_t sector, void *addr);

void cache_write (disk_sector_t sector, void *addr);
*/
#endif
