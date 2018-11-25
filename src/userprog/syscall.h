#ifndef USERPROG_SYSCALL_H
#define USERPROG_SYSCALL_H

#include "threads/thread.h"
#include "threads/interrupt.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"

void syscall_init (void);
void exit_ (int status);
bool munmap_ (int mapid);
#endif /* userprog/syscall.h */
