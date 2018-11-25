#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"
#include "vm/page.h"
#include "vm/swap.h"
#include "vm/frame.h"
//#include "tests/lib.h"

static struct lock frt_evict_lock;

void vm_frt_init (void){
  list_init (&frt);
  lock_init (&frt_lock);
  lock_init (&frt_evict_lock);
  return;
}


void *vm_frame_alloc (enum palloc_flags flags){
  acquire_frt_lock ();
  //void *page;
  void *frame = palloc_get_page (PAL_USER | flags);
  if (frame == NULL){
	//PANIC ("WE WHOULD EVICT!!!");
	//printf("vm_frame_alloc: WE HAVE TO EVICT SOME FRAME\n");
	struct frt_entry *victim  = vm_frame_evict ();
	//ASSERT (temp != NULL);
    ASSERT (victim != NULL);
    
	if (!vm_frame_save (victim)){
	  PANIC ("can't save the victim");
	}

	/*
	struct thread *t = get_thread (victim->tid);
	
	//pagedir_clear_page (t->pagedir, victim->upage);
	//if (!vm_is_in_spt (&t->spt, victim->upage))
	//  PANIC ("line 40 \n");
	//printf ("upage: [%x], tid: [%d]\n", victim->upage, t->tid);
	
	ASSERT (t != NULL);
	if (pg_ofs (victim->upage) != 0)
	  PANIC ("WHY NOW?");
	int swap_index = vm_swap_out (victim->frame);
	ASSERT (pg_ofs (victim->upage) == 0);
	
	//ASSERT (vm_is_in_spt (&t->spt, victim->upage));
	
	if (!vm_set_swap (&t->spt, victim->upage, swap_index)){
	  //printf ("upage: [%x], tid: [%d]\n", victim->upage, t->tid);
	  PANIC ("vm_set_swap: NO SUCH spt entry");
	}
	pagedir_clear_page (t->pagedir, victim->upage);
	//frame = victim->kpage;
	//PANIC ("WE WHOULD EVICT!!!");
	vm_frame_free_no_lock (victim->frame);
	
    //don't need this//
	
	struct frt_entry *temp;
	struct list_elem *e;
	for (e= list_begin (&frt);
		e!= list_end (&frt); e=list_next (e)){
	  temp = list_entry (e, struct frt_entry, frt_elem);
	  if (temp->frame == victim->frame){
		list_remove (e);
		free (temp);
		break;
	  }
	}
	//================//

    */
    frame = palloc_get_page (PAL_USER | flags);
	if (frame == NULL)
	  PANIC ("Eviction failed again");
  }

  //acquire_frt_lock ();
  struct frt_entry *f;
  f = malloc (sizeof (*f));
  if (f == NULL){
	PANIC ("vm_frame_alloc: CAN'T MAKE NEW frt_entry");
	return NULL;
  }
  f->frame = frame;
  f->tid = thread_current ()->tid;
  f->in_use = true;
  list_push_back (&frt, &f->frt_elem);
  release_frt_lock ();
  return frame;
}

bool vm_frame_save (struct frt_entry *f){
  struct thread *t = get_thread (f->tid);
  struct spt_entry *spte = vm_get_spt_entry (&t->spt, f->upage);
  ASSERT (!f->in_use);
  ASSERT (!f->reclaiming);
  if (spte->status == ON_MMF){
	if (pagedir_is_dirty (t->pagedir, spte->upage)){
	  acquire_filesys_lock ();
	  vm_frame_save_file (spte);
	  acquire_filesys_lock ();
	}
	
	goto saved;
  }

  if (!vm_frame_save_swap (f))
	return false;
  else
	goto saved;

saved:
  spte->is_in_disk = false;
  spte->kpage = NULL;
  pagedir_clear_page (t->pagedir, f->upage);
  vm_frame_free_no_lock (f->frame);
  return true;
}
bool vm_frame_save_file (struct spt_entry *s){
  file_seek (s->file.file, s->file.ofs);
  file_write (s->file.file, s->upage, s->file.read_bytes);
}

bool vm_frame_save_swap (struct frt_entry *f){
  struct thread *t = get_thread (f->tid);
  ASSERT (t != NULL);
  //ASSERT (pg_ofs(f->upage) == 0); 
  if (pg_ofs(f->upage) != 0)
	PANIC ("WHY NOW?");

  int swap_index = vm_swap_out (f->frame);
  

  if (!vm_set_swap (&t->spt, f->upage, swap_index)){
	printf("owner: %d, trier: %d\n", f->tid, thread_current ()->tid);
	PANIC ("vm_frame_save - vm_set_swap: NO SUCH spt");
  }
/*
  pagedir_clear_page (t->pagedir, f->upage);
  vm_frame_free_no_lock (f->frame);
  */
  return true;
}
struct frt_entry *vm_frame_evict (void){

  struct frt_entry *victim;

 // lock_acquire (&frt_evict_lock);

  victim = vm_evict_SC ();
  ASSERT (victim != NULL);

 // lock_release (&frt_evict_lock);
  return victim;
}

struct frt_entry *vm_evict_SC (void){
  struct list_elem *e;
  bool second = false;
  struct frt_entry *f;
  struct thread *t;
  //struct thread *t = thread_current ();
  if (list_empty (&frt)){
	printf("vm_evict_SC: frt empty\n");
  }

SCAN:
  for (e = list_begin (&frt);
	  e != list_end (&frt);
	  e = list_next (e)){
	
	f = list_entry (e, struct frt_entry, frt_elem);
	if (f->in_use)
	  continue;
    if (f->reclaiming)
	  continue;
    /*	
	if (pg_ofs (f->upage) != 0){
	  printf("upage value: %x / owner: %d / accessed by : %d  \n", (f->upage), f->tid, thread_current ()->tid);
	}
	*/
	ASSERT (pg_ofs (f->upage) == 0);
	

	//msg ("[%d] directory checked", f->tid);
	//printf ("[%d] directory checked", f->tid);
	
	t = get_thread (f->tid);
	
	if (pagedir_is_accessed (t->pagedir, f->upage)){
	  if (second)
		printf("CAN'T be happend\n");
	  pagedir_set_accessed (t->pagedir, f->upage, false);
	}else{
	  //list_remove (e);
	  return f;
	  //break;
	}
	
  }

  if (!second){
//	printf("FUCKYOU\n");
	second = true;
	goto SCAN;
  }else{
	return NULL;
  }
  return NULL;
}

void vm_frame_set (void *kpage, void*upage){
  acquire_frt_lock ();
  struct frt_entry *f = get_frt_entry (kpage);
  
//  ASSERT (f->tid == thread_current ()->tid);
  if (f->tid != thread_current ()->tid){
	printf("IN USE? %d \n", f->in_use);
	printf(" [entry's tid: %d] while [current tid: %d]\n", f->tid, thread_current ()->tid);
	PANIC ("TID error");
  }
  ASSERT (f->in_use);

  ASSERT (pg_ofs (upage) == 0);
  //acquire_frt_lock ();
  
  f->upage = upage;
  f->in_use = false;

  release_frt_lock ();
  return;
}

void vm_frame_free (void *frame){
  acquire_frt_lock ();
  vm_frame_free_no_lock (frame);
  release_frt_lock ();
}

void vm_frame_free_no_lock (void *frame)
{
  struct frt_entry *f = get_frt_entry (frame);
  if(f == NULL){
	printf ("vm_frame_free: NO SUCH FRAME EXIST\n");
	return;
  }
  //Remove frt_entry from the frt
  //acquire_frt_lock ();
  list_remove (&f->frt_elem);
  free (f);
  //Free the frame
  //printf("vm_frame_palloc?\n");
  palloc_free_page (frame);

  //release_frt_lock ();
  return;
}

void vm_frame_destroy (void *frame)
{
  //acquire_frt_lock ();
  struct frt_entry *f = get_frt_entry (frame);
  if(f == NULL){
	printf ("vm_frame_destroy: NO SUCH FRAME EXIST\n");
	return;
  }
  //Remove frt_entry from the frt
  //acquire_frt_lock ();
  list_remove (&f->frt_elem);
  free (f);
  //Free the frame
  //printf("vm_frame_palloc?\n");
  //release_frt_lock ();
  //palloc_free_page (frame);

  return;
}
//HELP TO ACCESS frt, frt_lock 

void vm_frame_reclaiming (void *frame){
  acquire_frt_lock ();
  
  struct frt_entry *f = get_frt_entry (frame);
  f->reclaiming = true;
  
  release_frt_lock ();

  return;
}
void vm_frame_reclaimed (void *frame){
  acquire_frt_lock ();
  
  struct frt_entry *f = get_frt_entry (frame);
  f->reclaiming = false;
  
  release_frt_lock ();
  
  return;
}

void acquire_frt_lock (void){
  lock_acquire (&frt_lock);
}

void release_frt_lock (void){
  lock_release (&frt_lock);
}

struct frt_entry *get_frt_entry (void *frame){
  struct list_elem *e;
  struct frt_entry *temp;

  //acquire_frt_lock ();
  for (e=list_begin (&frt); e!=list_end (&frt); e=list_next (e)){
	temp = list_entry (e, struct frt_entry, frt_elem);
	if (temp->frame == frame){
	  //release_frt_lock ();
	  return temp;
	}
  }
  //release_frt_lock ();
  return NULL;
}

