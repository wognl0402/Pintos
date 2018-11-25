#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "userprog/syscall.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "vm/frame.h"
#include "vm/page.h"

#define MAX_ARG 64
#define MAX_COM 128

static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

void child_close_all (void);
void file_close_all (void);
/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  tid_t tid;
  
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
  /*
  char *f_name;
  char *save_arg;
  f_name = strtok_r (fn_copy, " ", &save_arg);
  printf("strok_r check: %s\n", f_name);
  printf("save_arg check: %s\n", save_arg);
  printf("fn_copy check: %s\n", fn_copy);
  
  char *f_copy;
  f_copy = palloc_get_page (0);
  if (f_copy == NULL)
	return TID_ERROR;
  strlcpy (f_copy, fn_copy, PGSIZE);
  char *f_name;
  char *save_arg;
  printf("ORI FILENAME: %s\n", f_copy);
  for(f_name = strtok_r (f_copy, " ", &save_arg); f_name != NULL; 
	  f_name = strtok_r (NULL, " ", &save_arg))
	printf("TESTING: '%s'\n", f_name);
  printf("strtok_r check\n");
  printf("f_name: '%s'\n", f_name);
  printf("save_arg: '%s'\n", save_arg+1);
  */
  /* Create a new thread to execute FILE_NAME. */
  char *save_ptr;
  char *t_name;
  t_name = malloc(strlen(file_name)+1);
  strlcpy (t_name, file_name, strlen(file_name)+1);
  t_name = strtok_r (t_name, " ",&save_ptr);

  tid = thread_create (t_name, PRI_DEFAULT, start_process, fn_copy);
  free(t_name);
  if (tid == TID_ERROR){
    palloc_free_page (fn_copy);
	return tid;
  }//printf("sema_down pa_sema\n"); 
  
  sema_down (&thread_current ()->synch_init);
  
  //printf("child?\n");
  if(!thread_current ()->child_load)
	return -1;
  struct dead_body *db = malloc (sizeof (*db));
  db->ch_tid = tid;
  db->exit_status = -1;
  sema_init (&db->ch_sema,0);
  db->user_kill = false;
  db->used = false;
  db->alive = true;
  //printf ("child tid [%d] created \n", tid);
  list_push_back (&thread_current ()->ch_list, &db->ch_elem);
  
  //printf("sema_down done\n");
  return tid;
}

/* A thread function that loads a user process and makes it start
   running. */
static void
start_process (void *f_name)
{
  char *file_name = f_name;
  struct intr_frame if_;
  bool success;

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  acquire_filesys_lock ();
  success = load (file_name, &if_.eip, &if_.esp);
  release_filesys_lock ();
  palloc_free_page (file_name);
  
  struct thread *parent = get_thread (thread_current ()->pa_tid);
 
  parent->child_load = success;
  sema_up (&parent->synch_init);
  
  /* If load failed, quit. */
  if (!success){ 
    //printf("BAD LOADING...\n");
    //exit_ (0);	
	thread_exit ();
  }
  
  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  //printf("So... here?\n");
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.
   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid) 
{
  //printf("wait for somth\n");
  if (child_tid == TID_ERROR)
	return TID_ERROR;

  struct list_elem *e;
  struct dead_body *temp;
  int child_status = 0;
  struct thread *cur = thread_current ();
  lock_acquire (&cur->ch_lock);
  for (e=list_begin (&cur->ch_list);
	  e!=list_end (&cur->ch_list);
	  e=list_next(e)){
	temp = list_entry (e, struct dead_body, ch_elem);
	if(temp->ch_tid == child_tid){
	  //list_remove (e);
	  break;
	  /*
	  temp->is_waiting = true;
	  sema_down (&temp->ch_sema);
	  child_status = temp->exit_status; 
	  //printf("##### child_status = %d / temp->exit_status = %d\n", child_status, temp->exit_status);
	  list_remove (e);
	  free (temp); 
	  return child_status;
	  */
	}

  }
  //printf("???\n");
  if (temp == NULL)
	return -1;

  //lock_acquire (&cur->ch_lock);
  //cur->wait_tid = child_tid;
  if (temp->alive){
  //while (get_thread (child_tid) != NULL ){
	//printf("GO TO SLEEP [%d] wait for [%d] \n", cur->tid, child_tid);
	cur->wait_tid = child_tid;
	cond_wait (&cur->ch_cond, &cur->ch_lock);
    //printf("wokeup\n");
	//child_status = temp->exit_status;
  }
  
  if (!temp->user_kill || temp->used){
  //printf("userkill?\n");
    child_status = -1;
    
   //PANIC("SEX");
  }else{
  temp->used = true;
  child_status = temp->exit_status;
  //lock_release (&cur->ch_lock);
  }
  
  //while(1){}
  //list_remove (e);
  //free (temp);
  cur->wait_tid = -1;
  lock_release (&cur->ch_lock);
  return child_status;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *curr = thread_current ();
  uint32_t *pd;
  /*
  if (curr->exit_status==NULL){
	printf("?? why NULL \n");
	curr->exit_status=-1;
  }
  */
  //printf("I want to die : [%d]\n", curr->tid);
  acquire_filesys_lock ();
  if (thread_current ()->proc != NULL){
  file_allow_write (thread_current ()->proc);
  file_close (thread_current ()->proc);
  }
  file_close_all ();
  release_filesys_lock ();
  child_close_all ();

  //printf("PAPA WAKEUP\n");
//  printf("%s: exit(%d)\n", curr->name, curr->exit_status);
  //printf("IT'S exiting\n");
  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
#ifdef VM
  //acquire_frt_lock ();
  while (!list_empty (&curr->mmf_list)){
	struct list_elem *e = list_begin (&curr->mmf_list);
	struct mmf_desc *md = list_entry (e, struct mmf_desc, mmf_elem);
	if (!munmap_ (md->mapid))
	  PANIC ("can't close mmf on process_exit ()");

  }
  //release_frt_lock ();
  vm_spt_destroy (&curr->spt);
  //free (&curr->spt);
  //curr->spt= NULL;
  //printf ("spt destruction done\n");
#endif
  struct thread *parent = get_thread (curr->pa_tid);
  if (parent != NULL){
	lock_acquire (&parent->ch_lock);
	struct list_elem *ee;
	struct dead_body *db;
	for (ee = list_begin (&parent->ch_list);
		ee != list_end (&parent->ch_list);
		ee = list_next (ee)){
	  db = list_entry (ee, struct dead_body, ch_elem);

	  if (db->ch_tid == curr->tid){
		db->exit_status = curr->exit_status;
		db->alive = false;
	  }
		//lock_release (&parent->ch_lock);
  
	}
	if (parent->wait_tid == curr->tid){
	    //db->alive = false;
		cond_signal (&parent->ch_cond, &parent->ch_lock);
	}
	lock_release (&parent->ch_lock);
	/*
		}
	*/
  }



  
  pd = curr->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      curr->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);

	}
  //printf ("child tid [%d] exit \n", thread_current ()->tid);
#ifdef VM
  //vm_spt_destroy (&curr->spt);
#endif
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp, int argc, char *argv[]);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);
//P3 VM
static bool load_lazy (struct file *file, off_t ofs, uint8_t *upage,
						uint32_t read_bytes, uint32_t zero_bytes,
						bool writable);

/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *f_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofs;
  bool success = false;
  int i;

  //acquire_filesys_lock ();
/*
  char *file_name = malloc (strlen(f_name)+1);
  char *save_arg;
  file_name = strlcpy (file_name, f_name, strlen (f_name)+1);
  file_name = strtok_r (file_name, " ", &save_arg);
*/
  char **argv = palloc_get_page (0);

  //printf("PALLOCPALLOC\n");
  if (argv == NULL)
	goto done;

  char *temp;
  char *save_ptr;
  int argc = -1;
  for (temp = strtok_r (f_name, " ", &save_ptr);
	  temp != NULL;
	  temp = strtok_r (NULL, " ", &save_ptr)){
	argc++;
	argv[argc] = temp;
  }
  //printf("TOKENTOKEN\n");
/*
  char f_copy[MAX_COM] ;
  strlcpy(f_copy, f_name, strlen(f_name)+1);
  char *save_arg;
  char *argv[MAX_ARG];
  argv[0] = strtok_r (f_copy, " ", &save_arg);
  int argc = 1;
  while( argc < MAX_ARG ){
	if ((argv[argc] = strtok_r (NULL, " ", &save_arg))==NULL){
	  argc -=1;
	  break;
    }
	argc +=1;
  }
*/
  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  
  if (t->pagedir == NULL){
    printf("NULLPAGEDIR\n");	
    goto done;
  }
#ifdef VM
  if (!vm_spt_init ()){
	printf("vm_spt_init error\n");
	goto done;
  }
#endif

  process_activate ();
  
  /* Open executable file. */
  
  file = filesys_open (argv[0]);
  //free (file_name);

  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", argv[0]);
      
	  goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", argv[0]);
      goto done; 
    }
  
  /* Read program headers. */
  file_ofs = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
	  struct Elf32_Phdr phdr;

      if (file_ofs < 0 || file_ofs > file_length (file))
        goto done;
      file_seek (file, file_ofs);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofs += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              /*
			  if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
				*/
			  if (!load_lazy (file, file_page, (void *) mem_page,
					read_bytes, zero_bytes, writable))
				goto done;
            }
          else
            goto done;
          break;
        }
    }

  //printf("#JH# FROM LOADING, SETUP STACK\n");
  /* Set up stack. */
  if (!setup_stack (esp, argc, argv))
    goto done;
  
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  //printf("LOADINGLOADING\n");
  success = true;
  thread_current ()->proc = file;
  file_deny_write (file);

  done:
  
  palloc_free_page (argv);
  /*
  thread_current ()->proc = file; 
  file_deny_write (file);
  */
  /* We arrive here whether the load is successful or not. */
  //file_close (file);
  //release_filesys_lock ();
  //test_stack(*esp);
  return success;
}

/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:
        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.
        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.
   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.
   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool load_lazy (struct file *file, off_t ofs, uint8_t *upage,
	uint32_t read_bytes, uint32_t zero_bytes, bool writable){
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

	  if (!vm_put_spt_file (file, ofs, upage, page_read_bytes, page_zero_bytes, writable))
		return false;

	  read_bytes -= page_read_bytes;
	  zero_bytes -= page_zero_bytes;
	  ofs += page_read_bytes;
	  upage += PGSIZE;
	}
  return true;
}

  
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Do calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      //P3
	  //original
	  //uint8_t *kpage = palloc_get_page (PAL_USER);
      
	  /*My implementation */
	  uint8_t *kpage = vm_frame_alloc (PAL_USER);

	  
	  if (kpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, kpage, page_read_bytes) != (int) page_read_bytes)
        {
          //P3
		  //original
		  //palloc_free_page (kpage);
          vm_frame_free (kpage);
		  return false; 
        }
      memset (kpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, kpage, writable)) 
        {
          //P3
		  //original
		  //palloc_free_page (kpage);
          vm_frame_free (kpage);
		  return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp, int argc, char *argv[]) 
{
  uint8_t *kpage;
  bool success = false;
  /*
  int j;
  char sp = '\0';
  printf("NUM ARG %d \n", argc);
  for(j = argc; j > -1; j--){
	printf("PASSED %d arg: %s\n", j, argv[j]);
  }
  */
  //printf("SETTING UP?\n");
  
  //P3
  //original
  //kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  kpage = vm_frame_alloc (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success){
		//printf("STARTSETUP\n");
		*esp = PHYS_BASE;
		//printf("SIZE of UINT32_T: %d, int: %d , int8: %d \n", sizeof(uint32_t), sizeof(int), sizeof(uint8_t));
		uint32_t ref_arg[argc];
		uint32_t nul = (uint32_t) 0;
		int i;
		for( i = argc; i > -1; i--){
		  //printf("COPYING: %s \n", argv[i]);
		  //printf("strlen: %d\n", strlen(argv[i]));
		  *esp = *esp - (strlen(argv[i])+1)*sizeof(char);
		  memcpy(*esp, argv[i], strlen(argv[i])+1); 
		  ref_arg[i] = (uint32_t)*esp;
		  //printf("MIDDLE check: %s\n", (char *)ref_arg[i]);
		  //printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
		}
		int length = (int) PHYS_BASE - (int) *esp;
		int save = (4 - length % 4)%4;
		//printf("ISSTACK %d??%d:\n",length,save);
		for (i = 0; i<save; i++){
		  *esp = *esp -1;
		  *(uint8_t *) *esp = (uint8_t) 0;
		}
		//printf("%d\n", *(int *) *esp);
		//printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
		*esp = *esp -4;
		*(int *) *esp =(int) 0;
		//printf("%d\n", *(int *) *esp);
		//printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
		for( i = argc; i>-1; i--){
		  *esp = *esp -4;
		  *(uint32_t *) *esp = ref_arg[i];
		  //printf("CHECK: %s\n", (char *) ref_arg[i]);
		  //printf("CHECKCHECK: %s\n", (char *) *(uint32_t *) *esp);
		  //printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - ref_arg[i]);
		  //printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
		  //printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - *(uint32_t *) *esp);
		}
		int pt = *esp;
		*esp = *esp -4;
		memcpy(*esp,&pt, sizeof(int));
		//*(uint32_t *) *esp = (uint32_t) (*esp +4);
		*esp = *esp -4;

		argc +=1;
		memcpy(*esp,&argc,sizeof(int));

		//*(int *) *esp = argc+1;
		//printf("%d\n", *(int *) *esp);
		//printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
	    
		*esp = *esp -4;
		*(uint32_t *) *esp = (uint32_t) 0;
		//printf("%d\n", * (int * ) *esp);
		//printf("LENGTH from top: %zu\n", (uint32_t) PHYS_BASE - (uint32_t) *esp);
        //*esp = PHYS_BASE;
		//printf("SETUPSETUP\n");
	  }
      else{
        //P3
		//original
		//palloc_free_page (kpage);
		vm_frame_free (kpage);
	  }
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *t = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (t->pagedir, upage) == NULL
          && pagedir_set_page (t->pagedir, upage, kpage, writable) 
		  && vm_put_spt_entry (&t->spt, upage, kpage));
		  //&& vm_put_spt_entry (&t->spt, upage, kpage, writable));
}

void test_stack (int *t)
{
  int i;
  int argc = t[1];
  char ** argv;

  argv = (char **) t[2];
  printf("ARGC:%d ARGV:%x\n", argc, (unsigned int)argv);
  for (i = 0; i<argc; i++)
	printf("Argv[%d] = %x pointing at %s\n", i, (unsigned int)argv[i], argv[i]);
}

void file_close_all (void){
  struct list_elem *e;
  struct file_desc *temp;
  while(!list_empty (&thread_current ()->fd_list)){
	e = list_pop_back (&thread_current ()->fd_list);
	temp = list_entry (e, struct file_desc, fd_elem);
	file_close (temp->file);
	list_remove(e);
	free(temp);
  }
  //printf("THISPROB??\n");
}

void child_close_all (void){
  struct list_elem *e;
  struct dead_body *temp;
  while(!list_empty (&thread_current ()->ch_list)){
	e=list_pop_back (&thread_current ()->ch_list);
	temp=list_entry (e, struct dead_body, ch_elem);
	list_remove (e);
	free (temp);
  }
}
#include "threads/vaddr.h"
#include "userprog/syscall.h"


