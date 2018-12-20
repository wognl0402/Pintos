#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "devices/disk.h"
#include "filesys/cache.h"
#include "threads/thread.h"

/* The disk that contains the file system. */
struct disk *filesys_disk;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  filesys_disk = disk_get (0, 1);
  if (filesys_disk == NULL)
    PANIC ("hd0:1 (hdb) not present, file system initialization failed");

  inode_init ();
  free_map_init ();
  cache_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  cache_bye ();
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  disk_sector_t inode_sector = 0;
  //struct dir *dir = dir_open_root ();
  //char *path = malloc (strlen(name)+1);
  //char *filename = malloc (strlen(name)+1); 
  char path [strlen(name)+1];
  char filename [strlen(name)+1];
  get_dir (name, path);
  get_filename (name, filename);

  struct dir *dir = dir_open_path (path);

  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size, is_dir)
                  && dir_add (dir, filename, inode_sector));
  /*
  bool success = (dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (dir, name, inode_sector));
  */
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  char path [strlen(name)+1];
  char filename [strlen(name)+1];

  if (strlen(name) == 0)
	return NULL;
  get_dir (name, path);
  get_filename (name, filename);

  struct dir *dir = dir_open_path (path);

  //struct dir *dir = dir_open_root ();
  struct inode *inode = NULL;
  
  /*
  if (dir != NULL){
    //printf("in FILESYS_OPEN: name %s @@@@\n", name);
	if (!dir_lookup (dir, filename, &inode)){
	  //printf("can't look up #####\n");
    }
  }dir_close (dir);
*/
  if (dir == NULL){
	//printf("that\n");
	return NULL;
  }
  if (strlen(filename) >0){
    dir_lookup (dir, filename, &inode);
    dir_close (dir);
  }else{
	//printf("this\n");
	inode = dir_get_inode (dir);
  }
  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  char path [strlen(name)+1];
  char filename [strlen(name)+1];
  get_dir (name, path);
  get_filename (name, filename);

  struct dir *dir = dir_open_path (path);

  //struct dir *dir = dir_open_root ();
  bool success = (dir != NULL && dir_remove (dir, filename));
  //printf("fs_remove...|%d \n", success);
  dir_close (dir); 

  return success;
}

bool
filesys_chdir (const char *name){
  //printf("chdir init\n");
  char path [strlen(name)+1];
  char filename [strlen(name)+1];
  get_dir(name,path);
  get_filename (name, filename);
  struct dir *dir = dir_open_path (path);
  struct inode *inode = NULL;

  if (dir == NULL)
	return false;
  if (strlen (filename)>0){
	if (!dir_lookup (dir, filename, &inode))
	//  printf("no dir..%s\n", path);
//	dir_close (dir);
	//if (!inode_is_dir (inode))
	  //return false;
	dir_close (dir);
	dir = dir_open (inode);
	//printf("let's go to [%s]\n", filename);
  }
  if (dir){
  //printf("go\n");
	dir_close (thread_current ()->cwd);
  thread_current ()->cwd = dir;
  return true;
  }
// printf("no\n");
  return false;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}
