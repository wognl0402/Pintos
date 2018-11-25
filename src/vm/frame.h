#ifndef VM_FRAME_H
#define VM_FRAME_H

//Prevent Multiple header

//#include <stdio.h>
//#include <string.h>
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

struct frt_entry {
  void *frame;
  void *upage;
  tid_t tid;
  bool in_use;
  bool reclaiming;
  struct list_elem frt_elem;
};

struct lock frt_lock; 
struct list frt;

void vm_frt_init (void);
void *vm_frame_alloc (enum palloc_flags);

bool vm_frame_save (struct frt_entry *);
bool vm_frame_save_swap (struct frt_entry *);
bool vm_frame_save_file (struct spt_entry *);

struct frt_entry *vm_frame_evict (void);
struct frt_entry *vm_evict_SC (void);

void vm_frame_set (void *, void *);
void vm_frame_free (void *);
void vm_frame_free_no_lock (void *);
void vm_frame_destroy (void *);

void vm_frame_reclaiming (void *);
void vm_frame_reclaimed (void *);
void acquire_frt_lock (void);
void release_frt_lock (void);
struct frt_entry *get_frt_entry (void *);

#endif
