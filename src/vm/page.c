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
#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "filesys/file.h"

bool vm_spt_init (void){
  struct thread *t = thread_current ();
  //printf("init!!!\n");
  return hash_init (&t->spt, spt_hash_func, spt_hash_less_func, NULL);
}

void vm_spt_destroy (struct hash *h){
//  acquire_frt_lock ();
  hash_destroy (h, vm_spt_free);
  //free (h);
//  release_frt_lock ();
}

//bool hash_init ();
//void hash_clear ();
//hash_destroy ();
//size_t hash_size ();
//bool hash_empty ();

/* HASH OPERATION FUNC */
struct spt_entry *vm_get_spt_entry (struct hash *h, void *upage){
  struct spt_entry spte;

  spte.upage = upage;
  //PANIC("DURING vm_get");
  struct hash_elem *e = hash_find (h, &spte.spt_elem);
  
  if (e == NULL)
	return NULL;
  
  return hash_entry (e, struct spt_entry, spt_elem);
}

bool vm_is_in_spt (struct hash *h, void *upage){
  struct spt_entry *spte = vm_get_spt_entry (h, upage);
  //printf("??????\n");
  if (spte == NULL)
	return false;
  
  return true;
}

//bool vm_put_spt_entry (struct hash *h, void *upage, void *kpage, bool writable){
bool vm_put_spt_entry (struct hash *h, void *upage, void *kpage){
  struct spt_entry *spte;
  spte = malloc (sizeof *spte);
  
  if (spte == NULL){
	printf("vm_put_spt_entry: CAN'TMAKE spt_entry\n");
	return false;
  }

  spte->upage = upage;
  spte->kpage = kpage;
  spte->swap_index = -1;
  spte->status = ON_FRAME;
  //spte->writable = writable;

  struct hash_elem *e = hash_insert (h, &spte->spt_elem);
  if (e == NULL){
	return true;
  }else{
	free (spte);
	return false;
  }
}

bool vm_put_spt_file (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
  struct thread *t = thread_current ();
  struct spt_entry *spte;
  spte = malloc (sizeof *spte);

  if (spte == NULL){
	PANIC ("vm_put_spt_file spte NULL error");
  }

  spte->upage = upage;
  spte->kpage = NULL;
  spte->swap_index = -1;
  spte->status = ON_FILE;
  spte->is_in_disk = false;  
  spte->writable = writable;
 
  spte->file.file = file;
  spte->file.ofs = ofs;
  spte->file.read_bytes = read_bytes;
  spte->file.zero_bytes = zero_bytes;
  spte->file.writable = writable;
  
/*
  spte->file = file;
  spte->ofs = ofs;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  //spte->writable = writable;
*/
  struct hash_elem *e = hash_insert (&t->spt, &spte->spt_elem);
  if (e == NULL){
	return true;
  }else{
	free (spte);
	return false;
  }
}
bool vm_put_spt_mmf (struct file *file, off_t ofs, uint8_t *upage, uint32_t read_bytes, uint32_t zero_bytes, bool writable){
  struct thread *t = thread_current ();
  struct spt_entry *spte;
  spte = malloc (sizeof *spte);

  if (spte == NULL){
	PANIC ("vm_put_spt_mmf spte NULL error");
  }

  spte->upage = upage;
  spte->kpage = NULL;
  spte->swap_index = -1;
  spte->status = ON_MMF;
  spte->is_in_disk = false;
  spte->writable = writable;

  spte->file.file = file;
  spte->file.ofs = ofs;
  spte->file.read_bytes = read_bytes;
  spte->file.zero_bytes = zero_bytes;
  spte->file.writable = writable;

  struct hash_elem *e = hash_insert (&t->spt, &spte->spt_elem);
  if (e == NULL){
	return true;
  }else{
	free (spte);
	return false;
  }
}
bool vm_spt_reclaim (struct hash *h, struct spt_entry *spte){
  if (spte->status == ON_FILE){
	if(!vm_spt_reclaim_file (h, spte)){
	  PANIC ("Can't reclaim file");
	}
	goto done;
  }

  if (spte->status == ON_MMF){
	//PANIC( "MMF retrieval");
	if (!vm_spt_reclaim_mmf (h, spte)){
	  PANIC ("Can't reclaim mmf");
	}
	goto done;
  }

  if (!vm_spt_reclaim_swap (h, spte)){
	PANIC ("Can't reclaim swap");
    goto done;
  }


done:
  spte->is_in_disk = true;
  //printf("retrieval done\n");
  return true;
}
/*
void vm_spt_reclaim_start (struct hash *h, struct spt_entry *spte){
  
}
void vm_spt_reclaim_done (struct hash *h, struct spt_entry *spte){
  
}*/

bool vm_spt_reclaim_mmf (struct hash *h, struct spt_entry *spte){
  //void *kpage = vm_frame_alloc (PAL_USER);
  struct thread *t = thread_current ();
  
  //acquire_filesys_lock ();

  file_seek (spte->file.file, spte->file.ofs);
  void *kpage = vm_frame_alloc (PAL_USER);

  if (kpage == NULL){
	PANIC ("Can't make kpage at vm_spt_reclaim_mmf");
  }

  vm_frame_reclaiming (kpage);
  if (file_read (spte->file.file, kpage, spte->file.read_bytes) 
	  != (int) spte->file.read_bytes){
	vm_frame_free (kpage);
	return false;
  }
  memset (kpage + spte->file.read_bytes, 0, spte->file.zero_bytes);

  //P3-2
  
  if (!pagedir_set_page (t->pagedir, spte->upage, kpage, spte->file.writable)){
	vm_frame_free (kpage);
	return false;
  }

  spte->kpage =kpage;
  //release_filesys_lock ();
  //PANIC ("FINE");
  return true;
}
bool vm_spt_reclaim_swap (struct hash *h, struct spt_entry *spte){
  void *kpage = vm_frame_alloc (PAL_USER);
  //acquire_frt_lock ();
  if (kpage == NULL)
	PANIC ("HAVE YOU IMPLEMENTED EVCITION?");

  //P3-2
  vm_frame_reclaiming (kpage);

  if (!pagedir_set_page (thread_current ()->pagedir, spte->upage, kpage, true)){
	vm_frame_free (kpage);
	return false;
  }
   spte->kpage = kpage;
  vm_swap_in (spte->swap_index, kpage);
  
  
  //spte->status = ON_FRAME;
  //spte->is_in_disk = true;
  //release_frt_lock ();
  return true;
}

bool vm_spt_reclaim_file (struct hash *h, struct spt_entry *spte){
  struct thread *t = thread_current ();

  //acquire_filesys_lock ();

  file_seek (spte->file.file, spte->file.ofs);
  
  uint8_t *kpage = vm_frame_alloc (PAL_USER);
  
  if (kpage == NULL){
	PANIC ("CAN't make kapge at vm_spt_reclaim_file (page.c)");
  }

  vm_frame_reclaiming (kpage);
  
  if (file_read (spte->file.file, kpage, spte->file.read_bytes) != (int) spte->file.read_bytes){
	vm_frame_free (kpage);
	return false;
  }
  memset (kpage + spte->file.read_bytes, 0, spte->file.zero_bytes);
  
  //P3-2

  if (!pagedir_set_page (t->pagedir, spte->upage, kpage, spte->file.writable)){
	vm_frame_free (kpage);
	PANIC ("WTF?");
	//return false;
  }
  spte->kpage = kpage;
//  spte->status = ON_FRAME;
  //release_filesys_lock ();
  return true;
}

bool vm_del_spt_mmf (struct thread *t, void *upage){
  acquire_frt_lock ();

  struct spt_entry *spte = vm_get_spt_entry (&t->spt, upage);
  if (spte == NULL){
	PANIC ("can't unmap - vm_del_spt_mmf");
  }
 
 
  if (spte->is_in_disk){
	ASSERT (spte->kpage != NULL);
	if (pagedir_is_dirty (t->pagedir, spte->upage))
		vm_frame_save_file (spte);
    pagedir_clear_page (t->pagedir, spte->upage);
	vm_frame_free_no_lock (spte->kpage);
  }
  hash_delete (&t->spt, &spte->spt_elem);
	
//vm_frame_free_no_lock (kpage);
  release_frt_lock ();
  return true;
}

bool vm_set_swap (struct hash *h, void *upage, int swap_index){
  struct spt_entry *spte = vm_get_spt_entry (h, upage);
  if (spte == NULL)
	return false;
	//PANIC ("vm_set_swap: NO SUCH spt entry");
  //spte->kpage = NULL;
  spte->status = ON_SWAP;
  spte->swap_index = swap_index;
  return true;
}
void vm_stack_grow (struct hash *h, void *fault_page){
  //printf("GO TO GROWING!!!!!!\n");
  //struct thread *t = thread_current ();
  void *frame = vm_frame_alloc (PAL_USER | PAL_ZERO);
  if (frame == NULL){
	PANIC ("NO FRAME FOR GROW");
	//frame = 
	//printf("Still no eviction...\n");
  }//else{

  //struct spt_entry *spte;
  //spte = malloc (sizeof (*spte));
  //spte->upage = pg_round_down (fault_addr);
  //spte->upage = fault_page;
  //spte->kpage = frame;
  //spte->status = CLEARED;
  //P3-2
  vm_frame_reclaiming (frame);

  struct thread *t = thread_current ();
  if (pagedir_get_page (t->pagedir, fault_page) != NULL)
	PANIC ("WHILE GROW, ALREADY IN PAGEDIR");
  if (!pagedir_set_page (t->pagedir, fault_page, frame, true))
	PANIC ("CANNOT SET PAGEDIR");
  //if (!vm_put_spt_entry (&t->spt, fault_page, frame, true))
  if (!vm_put_spt_entry (&t->spt, fault_page, frame))
	PANIC ("CANNOT PUT INTO SPT");
  
  return;
	  /*
  struct hash_elem *e = hash_insert (h, &spte->spt_elem);
  if (e == NULL){
	PANIC ("WHILE GROW, DUPLICATED HASH ELEM");
  }
  struct thread *t = thread_current ();
  //pagedir_set_page (t->pagedir, fault_page)
  
  return;
  */
 }

//struct hash_elem *vm_put_spt_entry (struct hash *h, void *upage){

/* HASH INIT FUNC */

unsigned spt_hash_func (const struct hash_elem *h, void *aux){
  //Hash the vaddr
  struct spt_entry *temp;
  temp = hash_entry (h, struct spt_entry, spt_elem);
  return hash_bytes (&temp->upage, sizeof temp->upage);
  //return;
}

bool spt_hash_less_func (const struct hash_elem *a,
	const struct hash_elem *b,
	void *aux){

  const struct spt_entry *sa = hash_entry (a, struct spt_entry, spt_elem);
  const struct spt_entry *sb = hash_entry (b, struct spt_entry, spt_elem);
  //return (sa->upage - sb->upage) > 0;
  return sa->upage > sb->upage;
}

void vm_spt_free (struct hash_elem *e, void *aux UNUSED){
  struct spt_entry *spte;
  spte = hash_entry (e, struct spt_entry, spt_elem);
  acquire_frt_lock ();

  if (spte->kpage != NULL){
	//PANIC ("WHY ARE YOU STILL HERE!!!\n");
	//vm_frame_free (spte->kpage);
	vm_frame_destroy (spte->kpage);
	//if (!vm_frame_free (spte->kpage)){
	  //printf ("vm_spt_free: vm_frame_free error\n");
  }else if (spte->status == ON_SWAP){
	vm_swap_free (spte->swap_index);
  }

  free (spte);
  release_frt_lock ();
}

