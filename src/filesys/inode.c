#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
#define DIRECT_BLOCK 122
#define BLOCK_PER_INDIRECT_BLOCK 128

/* On-disk inode.
   Must be exactly DISK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    disk_sector_t block[DIRECT_BLOCK];                /* First data sector. */
    disk_sector_t sindirect;
	disk_sector_t dindirect;
    disk_sector_t start;

	bool is_dir;
	off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    //uint32_t unused[125];               /* Not used. */
  };

struct indirect_block{
  disk_sector_t block[BLOCK_PER_INDIRECT_BLOCK];
};



static disk_sector_t index_to_sector (struct inode_disk *i, off_t index){
  disk_sector_t target;
  
  // direct
  if (index < DIRECT_BLOCK){
	target = i->block[index];
	//ASSERT(target != -1);
	return target;
  }
  index = index - DIRECT_BLOCK;
  
  // single indirect
  if (index < BLOCK_PER_INDIRECT_BLOCK){
	struct indirect_block *temp = calloc (1, sizeof *temp);
	disk_read (filesys_disk, i->sindirect, temp);
	
	target = temp->block[index];
	free(temp);
	return target;
  }
  index = index - BLOCK_PER_INDIRECT_BLOCK;

  // double indirect
  off_t sindex = index / BLOCK_PER_INDIRECT_BLOCK;
  off_t dindex = index % BLOCK_PER_INDIRECT_BLOCK;

  struct indirect_block *tem = calloc (1, sizeof *tem);
  struct indirect_block *tem2 = calloc (1, sizeof *tem2);
  disk_read (filesys_disk, i->dindirect, tem);
  disk_read (filesys_disk, tem->block[sindex], tem2);
  
  target = tem2->block[dindex];
  free (tem);
  free (tem2);
  if (target == -1)
	printf("index = [%d], sindex = [%d], dindex = [%d]\n", index, sindex, dindex);
  ASSERT (target != -1);
  return target;
}


/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, DISK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    disk_sector_t sector;               /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
    disk_sector_t start;
	off_t length;
	struct lock lock;
  };

/* Returns the disk sector that contains byte offset POS within
   INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static disk_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  /*
  if (pos < inode->data.length)
    return inode->data.start + pos / DISK_SECTOR_SIZE;
  */
  disk_sector_t ret;
  if (0 <= pos && pos < inode->length){
	off_t index = pos / DISK_SECTOR_SIZE;
	struct inode_disk *temp = calloc (1, sizeof (*temp));
	if (temp == NULL)
	  PANIC("byte_to_sector can't calloc");

	disk_read (filesys_disk, inode->sector, temp);
//HAVE to do
	//ret = index_to_sector (&inodei->data, index);
	ret = index_to_sector (temp, index);
	free (temp);
	return ret;

	//return index_to_sector (inode, index);
	//return inode->start + pos / DISK_SECTOR_SIZE;
  }else
	return -1;
    //printf("BTS: pos = [%d], length = [%d]\n", pos, inode->length);
	//PANIC("WHAT'sup");
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
  lock_init (&i_lock);
}

bool
inode_extend (struct inode_disk *i, off_t current, off_t new);
bool inode_alloc (struct inode_disk *i, off_t length);


/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   disk.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool inode_extend (struct inode_disk *i, off_t current, off_t new){
  static char zeros[DISK_SECTOR_SIZE];
  /*
  size_t current = bytes_to_sectors (length1);
  size_t new = bytes_to_sectors (length2);
  */
  if (new <current)
	return false;
  if (new == current)
	return true;
  bool success = false;
  while (current < new){
	current ++;
	/*
	if (current % 1000 == 0)
	  printf("inode_extend: [current = %d], [new = %d]\n", current, new);
	*/
	if (current < DIRECT_BLOCK){
	  success = free_map_allocate (1, &i->block[current]);
	  disk_write (filesys_disk, i->block[current], zeros);
	  continue;
	}
   /*
	if (current == DIRECT_BLOCK){
	  success = free_map_allocate (1, &i->sindirect);
	  
	}*/
	if (current < DIRECT_BLOCK + BLOCK_PER_INDIRECT_BLOCK){
//	  success = inode_extension_sindirect (i, index-DIRECT_BLOCK);
	  struct indirect_block *temp = calloc (1, sizeof *temp);
	  if (current == DIRECT_BLOCK)
		success = free_map_allocate (1, &i->sindirect);
	  //else
		disk_read (filesys_disk, i->sindirect, temp);

	  success = free_map_allocate (1, &temp->block[(current-DIRECT_BLOCK)]);
	  //PANIC("here?");
	  disk_write (filesys_disk, temp->block[(current-DIRECT_BLOCK)], zeros);
	  disk_write (filesys_disk, i->sindirect, temp);
	  free(temp);
      
	  continue;
	}
	
	
    off_t sindex = (current - DIRECT_BLOCK - BLOCK_PER_INDIRECT_BLOCK) / BLOCK_PER_INDIRECT_BLOCK;
	off_t dindex = (current - DIRECT_BLOCK - BLOCK_PER_INDIRECT_BLOCK) % BLOCK_PER_INDIRECT_BLOCK;
	
	struct indirect_block *temp = calloc (1, sizeof *temp);
	struct indirect_block *temp2 = calloc (1, sizeof *temp2);
	if (sindex == 0 && dindex == 0)
	  success = free_map_allocate (1, &i->dindirect);
	//else
	disk_read (filesys_disk, i->dindirect, temp);
	
	//if (dindex == 0)
	  //success = free_map_allocate (1, &temp->block[sindex]);

	disk_read (filesys_disk, temp->block[sindex], temp2);
	success= free_map_allocate (1, &temp2->block[dindex]);
	disk_write (filesys_disk, temp2->block[dindex], zeros);
	disk_write (filesys_disk, temp->block[sindex], temp2);
	disk_write (filesys_disk, i->dindirect, temp);

	free (temp);
	free (temp2);

	continue;
  }
	
  return success; 
}

bool inode_alloc (struct inode_disk *i, off_t length){
  if (length <0)
	return false;
  off_t size = bytes_to_sectors (length);
  return inode_extend (i, -1, size);
}

bool
inode_create (disk_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == DISK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {

      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
 	  if (inode_alloc (disk_inode, length)){
		//PANIC("WHAT");
		disk_write (filesys_disk, sector, disk_inode);
	    success = true;
	  }
 	  /*
	  if (free_map_allocate (sectors, &disk_inode->start))
        {
          disk_write (filesys_disk, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[DISK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                disk_write (filesys_disk, disk_inode->start + i, zeros); 
            }
          success = true; 
        }
	   */
 	  
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (disk_sector_t sector) 
{
  struct list_elem *e;
  struct inode *inode;

  //lock_acquire (&i_lock);
  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }
  

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  lock_init (&inode->lock);
  //disk_read (filesys_disk, inode->sector, &inode->data);
  struct inode_disk *data = calloc (1, sizeof *data);
  disk_read (filesys_disk, inode->sector, data);
  //disk_read (filesys_disk, inode->sector, &inode->data);
  //inode->start = datastart;
  inode->length = data->length;
  
  //lock_release (&i_lock);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
disk_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void inode_free (struct inode *inode);
bool inode_dealloc (struct inode_disk *i, off_t index);

void
inode_close (struct inode *inode) 
{

  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  //lock_acquire (&i_lock);
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);

      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
//	      inode_lock (inode);
	      //printf("time to close\n");
          inode_free (inode);
		  free_map_release (inode->sector, 1);
//	      inode_unlock (inode);
		  /*
		  free_map_release (inode->sector, 1);
          free_map_release (inode->start,
                            bytes_to_sectors (inode->length)); 
       
		  */
		}


      free (inode); 
    }
  //lock_release (&i_lock);
}

void inode_free (struct inode *inode){
  struct inode_disk *ib = calloc (1, sizeof *ib);

  disk_read (filesys_disk, inode->sector, ib);
  off_t index = bytes_to_sectors (inode->length);
  inode_dealloc (ib, index);
  free (ib);
}


bool inode_dealloc (struct inode_disk *i, off_t index){
  if (index < 0)
	return false;

  off_t current = index+1;
  while (current != 0){
	current++;

	if (current < DIRECT_BLOCK){
	  free_map_release (i->block[current], 1);  
	  continue;
	}
	if (current < DIRECT_BLOCK + BLOCK_PER_INDIRECT_BLOCK){
	  struct indirect_block *temp = calloc (1, sizeof *temp);
	  disk_read (filesys_disk, i->sindirect, temp);
	  free_map_release (temp->block[(current-DIRECT_BLOCK)], 1);
	  if (current == DIRECT_BLOCK)
		free_map_release (i->sindirect, 1);

	  free (temp);
	  continue;
	}
	off_t sindex = (current - DIRECT_BLOCK - BLOCK_PER_INDIRECT_BLOCK) / BLOCK_PER_INDIRECT_BLOCK;
	off_t dindex = (current - DIRECT_BLOCK - BLOCK_PER_INDIRECT_BLOCK) % BLOCK_PER_INDIRECT_BLOCK;

	struct indirect_block *tem = calloc (1, sizeof *tem);
	struct indirect_block *tem2 = calloc (1, sizeof *tem2);

	disk_read (filesys_disk, i->dindirect, tem);
	disk_read (filesys_disk, tem->block[sindex], tem2);
	free_map_release (tem2->block[dindex], 1);

	if (dindex == 0)
	  free_map_release (tem->block[sindex], 1);
	if (sindex == 0)
	  free_map_release (i->dindirect, 1);

	free (tem);
	free (tem2);
  }
  return true;
}



 
/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  //printf("remove!\n");
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  //ASSERT (inode != NULL);
  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;
      /*
      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
           //Read full sector directly into caller's buffer. 
          disk_read (filesys_disk, sector_idx, buffer + bytes_read); 
        }
      else 
        {
          //Read sector into bounce buffer, then partially copy
          //   into caller's buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          disk_read (filesys_disk, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
*/
	  
	  struct cache_entry *e = cache_return (sector_idx, false);
	  memcpy (buffer + bytes_read, e->buf + sector_ofs, chunk_size);
	  e->in_use--;
	  e->access = true;
	  

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;

  if ( offset + size > inode->length){
	
	//inode_lock (inode);

	off_t len1 = bytes_to_sectors (inode->length);
	off_t len2 = bytes_to_sectors (offset + size);

	struct inode_disk *id = calloc (1, sizeof *id);
	disk_read (filesys_disk, inode->sector, id);
	if (!inode_extend (id, len1, len2))
	  PANIC ("write extension failed");
	inode->length = offset+size;
	id->length = offset+size;
	disk_write (filesys_disk, inode->sector, id);
	free (id);
	
	//inode_unlock (inode);
  }
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      disk_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % DISK_SECTOR_SIZE;

	  if (sector_idx == -1){
		printf("inode error: pos = [%d]. inode_length = [%d]\n", offset, inode->length);
	  printf("[%d]\n", byte_to_sector (inode, offset));
	  }
	  //ASSERT (sector_idx != -1);
	  ASSERT (sector_idx < 0xcccccccc);
      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = DISK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

	  /*
      if (sector_ofs == 0 && chunk_size == DISK_SECTOR_SIZE) 
        {
          // Write full sector directly to disk. 
          disk_write (filesys_disk, sector_idx, buffer + bytes_written); 
        }
      else 
        {
           //We need a bounce buffer. 
          if (bounce == NULL) 
            {
              bounce = malloc (DISK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          // If the sector contains data before or after the chunk
           //  we're writing, then we need to read in the sector
            // first.  Otherwise we start with a sector of all zeros. 
          if (sector_ofs > 0 || chunk_size < sector_left) 
            disk_read (filesys_disk, sector_idx, bounce);
          else
            memset (bounce, 0, DISK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          disk_write (filesys_disk, sector_idx, bounce); 
        }
	  */	
	  //printf("here and "); 
	  //ASSERT (sector_idx != 0xcccccccc)
	  struct cache_entry *c = cache_return (sector_idx, true);
	  //printf("tehre\n");
	  memcpy (c->buf + sector_ofs, buffer + bytes_written, chunk_size);
	  c->in_use--;
	
	  //printf("writting at \n");
      
	  /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->length;
}

void inode_lock (const struct inode *inode){
  lock_acquire(&((struct inode *) inode)->lock);
}

void inode_unlock (const struct inode *inode){
  lock_release(&((struct inode *) inode)->lock);
}
