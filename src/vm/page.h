#ifndef VM_PAGE_H
#define VM_PAGE_H

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
#include "filesys/file.h"

enum spt_status{
  CLEARED,
  ON_FRAME,
  ON_SWAP,
  ON_FILE,
  ON_MMF
};

struct spt_file{
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
  bool writable;
};

//Supplemental page table entry
struct spt_entry
{
  //uvaddr will be hashed
  void *upage;
  void* kpage;  /*do we need this?*/
  enum spt_status status;
  int swap_index;
  bool writable;
  bool is_in_disk;
  //P3-2
  struct spt_file file;
  /*
  struct file *file;
  off_t ofs;
  uint32_t read_bytes;
  uint32_t zero_bytes;
*/
  struct hash_elem spt_elem;
};

bool vm_spt_init (void);
void vm_spt_destroy (struct hash *);

struct spt_entry *vm_get_spt_entry (struct hash *, void *);
bool vm_is_in_spt (struct hash *, void *);
bool vm_put_spt_entry (struct hash *, void *, void *);
//bool vm_put_spt_entry (struct hash *, void *, void *, bool);

bool vm_put_spt_file (struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);
bool vm_put_spt_mmf (struct file *, off_t, uint8_t *, uint32_t, uint32_t, bool);


bool vm_spt_reclaim (struct hash *, struct spt_entry *);
bool vm_spt_reclaim_swap (struct hash *, struct spt_entry *);
bool vm_spt_reclaim_file (struct hash *, struct spt_entry *);
bool vm_spt_reclaim_mmf (struct hash *, struct spt_entry *);

bool vm_del_spt_mmf (struct thread *t, void *upage);

bool vm_set_swap (struct hash *, void *, int);
void vm_stack_grow (struct hash *, void *);

unsigned spt_hash_func (const struct hash_elem *, void *);
bool spt_hash_less_func (const struct hash_elem *,
	const struct hash_elem*, void *);
void vm_spt_free (struct hash_elem *, void *);

#endif
