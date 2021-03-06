#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
//#include "filesys/disk.h"
/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    disk_sector_t inode_sector;         /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (disk_sector_t sector, size_t entry_cnt) 
{
  return inode_create (sector, entry_cnt * sizeof (struct dir_entry), true);
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      dir->inode = inode;
      dir->pos = 0;
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

struct dir *
dir_open_path (const char *path){
  int len = strlen (path);
  char s[len+1];
  strlcpy (s, path, len+1);

//  struct dir *d = dir_open_root ();
  struct dir *d;
  if (path[0] == '/')
	d = dir_open_root ();
  else{
	//printf("...%s\n", path);
	if (thread_current ()->cwd == NULL){
	  //printf("nonsense\n");
	  d = dir_open_root ();
  	}else{
	  d = dir_reopen (thread_current ()->cwd);
  }
  }

  char *token, *save;
  for (token = strtok_r (s, "/", &save);
	  token != NULL;
	  token = strtok_r (NULL, "/", &save)){
	//printf("here?\n");
	if (strcmp (token, ".") ==0){
	  continue;
	}
	struct inode *inode = NULL;
	if (strcmp(token, "..") == 0){
	  if (!get_parent (d, &inode))
		return NULL;
	//  printf("okay\n");
	  //printf("yeah\n");
	  //continue;
	}else if (!dir_lookup (d, token, &inode)){
	  dir_close (d);
	  return NULL;
	}

	struct dir *temp = dir_open (inode);
	if (temp == NULL){
	  dir_close (d);
	  return NULL;
	}
	dir_close(d);
	d = temp;
  }
  return d;
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) {
	
		if (e.in_use && !strcmp (name, e.name)) 
      {
       
		//if (strcmp (name, "dir21")==0)
		//  printf("dir21: e.name[%s]\n",e.name); 
		if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
		return true;
      }
  }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, disk_sector_t inode_sector) 
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);
  //printf("adding %s\n", name);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL)){
	//printf("noname\n");
    goto done;
}
  if (!add_parent (inode_get_inumber (dir_get_inode (dir)), inode_sector))
	goto done;
  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

 done:
  //printf("adding done%s | %d\n", name, success);
  return success;
}
/*
bool add_parent (struct dir *dir, disk_sector_t child){
  disk_sector_t parent = inode_get_inumber (dir_get_inode (dir));

  struct inode *inode = inode_open (child);
  if (inode == NULL)
	return false;
  
  inode->parent = parent;

  struct inode_disk *id = calloc (1, sizeof *id);
  
  disk_read (filesys_disk, inode->parent, id);
  id->parent = parent;
  disk_write (filesys_disk, inode->parent, id);
  free (id);
  inode_close (inode);
  return true;
}*/
  /* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  //printf("removing %s \n", name);
  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs)){
	//printf("CAN'T LOOKUP\n");
    goto done;
  }

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL){
	//printf("inode NULL\n");
    goto done;
  }
//inode_is_dir (inode);
  if (inode_is_dir (inode) && inode_get_open_cnt (inode) >2){
	//printf("WORKING.. isdir?: %d| opencnt: %d\n", inode_is_dir (inode), inode_get_open_cnt (inode));
	goto done;
  }
  
  
  if (inode_is_dir (inode) && !dir_is_empty (inode)){
	//printf("NONEMPTY..\n");
	goto done;
  }
  
  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e){
   //printf("inode_write error\n");	
    goto done;
  }

  //printf("....\n");
  /* Remove inode. */
  inode_remove (inode);
  success = true;

  //printf("????\n");
 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

bool dir_is_root (struct dir *dir){
  if (dir == NULL)
	return false;
  if (inode_get_inumber (dir_get_inode(dir)) == ROOT_DIR_SECTOR)
	return true;

  return false;
}

bool dir_is_empty (struct inode *inode){
  struct dir_entry e;
  size_t ofs;

  for (ofs = 0; inode_read_at (inode, &e, sizeof e, ofs) == sizeof e;
	  ofs += sizeof e){
	if (e.in_use)
	  return false;
  }
  return true;
}

void get_filename (const char *path, char *filename){
  int l = strlen(path);
  char *fn = malloc (l+1);
  strlcpy (fn, path, l+1);
  char *token, *save;
  char *temp = "";
  for (token = strtok_r (fn, "/", &save);
	  token != NULL;
	  token = strtok_r (NULL, "/", &save)){
  //printf("### %s ###\n", fn);
    temp = token;
  }
  
  if (strcmp(temp, ".") ==0 || strcmp(temp, "..") == 0){
	*temp = "";
	strlcpy (filename, temp, 1);
    return;
  }
  
  //printf ("@@@ %s @@@\n", token);
  strlcpy (filename, temp, (strlen(temp)+1));
  free (fn);
}

void get_dir (const char *path, char *directory){
  int l = strlen(path);
  char *fn = malloc (l+1);
  memcpy (fn, path, l+1);

  char *dir = directory;
  if (l>0 && path[0] == '/'){
	if(dir) *dir++ = '/';
  }

  char *token, *save;
  char *temp = "";
  for (token = strtok_r (fn, "/", &save);
	  token != NULL;
	  token = strtok_r (NULL, "/", &save)){
	
	int i = strlen (temp);
	
	if (dir && i >0){
	  memcpy (dir, temp, i);
	  dir[i] = '/';
	  dir += i+1;
	}
	
	temp = token;
  }
 
  if (strcmp (temp, ".")== 0){
   memcpy (dir, temp, 1);   
//dir = ".";
   dir +=1;
  }else if (strcmp (temp, "..") == 0){
	memcpy (dir, temp, 2);
	dir +=2;
  }
  

  if(dir) *dir = '\0';
  free (fn);
}

bool get_parent (struct dir *dir, struct inode **inode){
  struct inode *temp = dir_get_inode (dir);
  disk_sector_t parent = get_parent_sector (temp);
  *inode = inode_open (parent);
  if (*inode == NULL)
	return false;

  return true;
}
