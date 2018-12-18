/* Reads a 128 kB file into static data and "sorts" the bytes in
   it, using counting sort, a single-pass algorithm.  The sorted
   data is written back to the same file in-place. */

#include <debug.h>
#include <syscall.h>
#include "tests/lib.h"
#include "tests/main.h"
#include <string.h>

const char *test_name = "child-sort";

unsigned char buf[128 * 1024];
size_t histogram[256];

static bool
is_sorted (const unsigned char *buf, size_t size){
  size_t i;
  bool temp = true;
  for (i=1; i<size; i++)
	if (buf[i-1] > buf[i]){
	  printf("FUCKED UP AT %d\n", i);
	  temp = false;
	}//return i;
  return temp;
}


int
main (int argc UNUSED, char *argv[]) 
{
  int handle;
  unsigned char *p;
  size_t size;
  size_t i;

  quiet = true;

  CHECK ((handle = open (argv[1])) > 1, "open \"%s\"", argv[1]);

  size = read (handle, buf, sizeof buf);
  for (i = 0; i < size; i++)
  
    histogram[buf[i]]++;
  p = buf;
  for (i = 0; i < sizeof histogram / sizeof *histogram; i++) 
    {
      size_t j = histogram[i];
      while (j-- > 0)
        *p++ = i;
    }
  /*//buf[0] = buf[size-1];
  msg ("argv[1]=[%s], %d", argv[1], strcmp(argv[1], "buf1"));
  */
  seek (handle, 0);
  /*
   * size_t temp = -1;
  //if ((temp = is_sorted (buf, size)) != -1)
  if (!is_sorted (buf,size))
	msg("SORTING FUCKED UP");
  
  if (strcmp (argv[1], "buf4") == 0)
	for (i=0; i<size; i++){
	  if (i%10000==0 || i == size-1)
		msg("buf1[%d] = %d", i, buf[i]);
	}
	
  msg ("%s) buf before %d", argv[1], buf);
  */
  write (handle, buf, size);
  /*
  if (!is_sorted (buf,size))
  //if ((temp = is_sorted (buf, size)) != -1)
	msg("SORTING FUCKEDDDDDDDDD UP at %s / idx: %d", argv[1], temp);
  
 seek (handle, 0);
  read (handle, buf, size);
  
  msg ("%s) buf after %d", argv[1], buf);
  if (strcmp (argv[1], "buf4") == 0)
	for (i=0; i<size; i++){
	  if (i%10000==0 || i == size-1)
		msg("buf1[%d] = %d", i, buf[i]);
	}
  if (!is_sorted (buf,size))
  //if ((temp = is_sorted (buf, size)) != -1)
	msg("SORTING FUCKED UP at %s / idx: %d", argv[1], temp);
 */ close (handle);
  
  return 123;
}
