#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

typedef int mapid_t;
#define MAP_FAILED ((mapid_t)-1)
struct mmf_desc{
  mapid_t mapid;
  struct file *file;
  void *addr;
  unsigned size;
  struct list_elem mmf_elem;
};

#endif /* userprog/process.h */
