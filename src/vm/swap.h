#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <stdio.h>
#include <string.h>
#include "lib/kernel/list.h"
#include "lib/kernel/hash.h"
#include "lib/kernel/bitmap.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/pte.h"
#include "userprog/pagedir.h"

void vm_swt_init (void);
void vm_swap_free (int);

bool vm_swap_in (int, void *);
int vm_swap_out (const void *);


#endif
