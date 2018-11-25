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

void vm_frt_init (void){
  list_init (&frt);
  lock_init (&frt_lock);
  return;
}


void *vm_frame_alloc (enum palloc_flags flags){
  void *page;
  void *frame = palloc_get_page (flags);
  if (frame == NULL){
	printf("vm_frame_alloc: WE HAVE TO EVICT SOME FRAME\n");
	return NULL;
  }

  struct frt_entry *f;
  f = malloc (sizeof (*f));
  if (f == NULL){
	printf("vm_frame_alloc: CAN'T MAKE NEW frt_entry\n");
	return NULL;
  }
  f->frame = frame;
  f->tid = thread_current ()->tid;
  f->in_use = true;
  acquire_frt_lock ();
  list_push_back (&frt, &f->frt_elem);
  release_frt_lock ();
  return frame;
}

void vm_frame_set (void *kpage, void*upage){
  //printf("I'm setting up\n");
  //acquire_frt_lock ();
  
  struct frt_entry *f = get_frt_entry (kpage);
  
  ASSERT (f->tid == thread_current ()->tid);
  ASSERT (f->in_use);
  acquire_frt_lock ();
  f->upage = upage;
  f->in_use = false;

  release_frt_lock ();
  return;
}

void vm_frame_free (void *frame)
{
  //acquire_frt_lock ();
  struct frt_entry *f = get_frt_entry (frame);
  if(f == NULL){
	printf ("vm_frame_free: NO SUCH FRAME EXIST\n");
	//release_frt_lock ();
	return;
  }
  //Remove frt_entry from the frt
  acquire_frt_lock ();
  list_remove (&f->frt_elem);
  free (f);
  //Free the frame
  palloc_free_page (frame);

  release_frt_lock ();
  return;
}
//HELP TO ACCESS frt, frt_lock 

void acquire_frt_lock (void){
  //printf("try to acq\n");
  lock_acquire (&frt_lock);
  //printf("succeed \n");
}

void release_frt_lock (void){
  //printf("try to acq\n");
  lock_release (&frt_lock);
}

struct frt_entry *get_frt_entry (void *frame){
  struct list_elem *e;
  struct frt_entry *temp;

  acquire_frt_lock ();
  for (e=list_begin (&frt); e!=list_end (&frt); e=list_next (e)){
	temp = list_entry (e, struct frt_entry, frt_elem);
	if (temp->frame == frame){
	  release_frt_lock ();
	  return temp;
	}
  }
  release_frt_lock ();
  return NULL;
}



