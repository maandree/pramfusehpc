/* -*- coding: utf-8 -*- */
/**
 * pramfusehpc — Persistent RAM FUSE filesystem
 * 
 * Copyright (C) 2013  André Technology (mattias@andretechnology.com)
 * 
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#define bool   long
#define true   1
#define false  0
#define null   0
#define throw  return -


/**
 * Buffer for converting a file name in this mountpoint to the one in on the HDD
 */
static char* pathbuf = null;

/**
 * The length of the HDD path
 */
static long hddlen = 0;

/**
 * The size of `pathbuf`
 */
static long pathbufsize = 0;



/**
 * Compare two NUL-terminated strings against each other
 * 
 * @param   a  The first comparand
 * @param   b  The second comparand
 * @return     The comparands' equality
 */
static bool eq(char* a, char* b)
{
  while (*a && *b)
    if (*a++ != *b++)
      return false;
  return !(*a | *b);
}


/**
 * Return a path as it is named on the HDD relative to the root mount path
 * 
 * @param   path  The path in RAM
 * @return        The path on HDD
 */
char* p(const char* path)
{
  long n = 0;
  while ((*(path + n++)))
    ;
  if (n + hddlen > pathbufsize)
    {
      while (n + hddlen > pathbufsize)
	pathbufsize <<= 1;
      char* old = pathbuf;
      pathbuf = (char*)malloc(pathbufsize);
      for (long i = 0; i < hddlen; i++)
	*(pathbuf + i) = *(old + i);
      free(old);
    }
  pathbuf += hddlen;
  for (long i = 0; i < n; i++)
    *(pathbuf + i) = *(path + i);
  return pathbuf -= hddlen;
}

/**
 * Get the value to return for a FUSE operation
 * 
 * @param   rc  The returned value from the native operation
 * @return      `rc`, or the thrown error negative.
 */
inline int r(int rc)
{
  return rc < 0 ? -errno : rc;
}




/**
 * Clean up the file system for exit
 */
void pram_destroy()
{
  free(pathbuf);
}

/**
 * Change the permission bits of a file
 * 
 * @param   path  The file
 * @param   mode  The permission bits
 * @return        Error code
 */
int pram_chmod(const char* path, mode_t mode)
{
  return r(chmod(p(path), mode));
}

/**
 * Change the ownership of a file
 * 
 * @param   path   The file
 * @param   owner  The owner
 * @param   group  The group
 * @return         Error code
 */
int pram_chown(const char* path, uid_t owner, gid_t group)
{
  return r(lchown(p(path), owner, group));
}

/**
 * Get file attributes
 * 
 * @param   path  The file
 * @param   attr  The stat storage
 * @return        Error code
 */
int pram_getattr(const char* path, struct stat* attr)
{
  return r(lstat(p(path), attr));
}

/**
 * Get extended file attributes
 * 
 * @param   path   The file
 * @param   name   The attribute name
 * @param   value  The attribute value storage
 * @param   size   The size of `value`
 * @return         Error code or value size
 */
int pram_getxattr(const char* path, const char* name, char* value, size_t size)
{
  return r(lgetxattr(p(path), name, value, size));
}

/**
 * Create a hard link to a file
 * 
 * @param   target  The target
 * @param   path    The link
 * @return          Error code
 */
int pram_link(const char* target, const char* path)
{
  char* _target = p(target);
  pathbuf = (char*)malloc(pathbufsize);
  for (int i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(_target + i);
  char* _path = p(path);
  int rc = r(lgetxattr(_target, _path));
  free(_target);
  return rc;
}

/**
 * List extended file attributes
 * 
 * @param   path   The file
 * @param   list   The list storage
 * @param   size   The size of `list`
 * @return         Error code or value size
 */
int pram_listxattr(const char* path, char* list, size_t size)
{
  return r(llistxattr(p(path), list, size));
}

/**
 * Create a directory
 * 
 * @param   path   The file
 * @param   mode   The file's protection bits
 * @return         Error code
 */
int pram_mkdir(const char* path, mode_t mode)
{
  return r(mkdir(p(path), mode));
}

/**
 * Create a file node
 * 
 * @param   path   The file
 * @param   mode   The file's protection bits
 * @param   mode   The file's device specifications
 * @return         Error code
 */
int pram_mknod(const char* path, mode_t mode, dev_t dev)
{
  return r(mknod(p(path), mode, dev));
}


/**
 * The file system operations
 */
static struct fuse_operations pram_oper = {
  .destroy = pram_destroy,
  .chmod = pram_chmod,
  .chown = pram_chown,
  .getattr = pram_getattr,
  .getxattr = pram_getxattr,
  .link = pram_link,
  .listxattr = pram_listxattr,
  .mkdir = pram_mkdir,
  .mknod = pram_mknod,
};


/**
 * This is the main entry point of the program
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command line arguments, including executed program
 * @return        The exit value, zero on success
 */
int main(int argc, char** argv)
{
  char* hdd = null;
  
  for (int i = 1; i < argc; i++)
    if (eq(*(argv + i), "--hdd"))
      {
	if ((hdd))
	  {
	    fputs("pramfusehpc: error: use of multiple --hdd", stderr);
	    return 1;
	  }
	else if (i + 1 == argc)
	  {
	    fputs("pramfusehpc: error: --hdd without argument", stderr);
	    return 1;
	  }
	else
	  hdd = argv[++i];
      }
  
  if (hdd == null)
    {
      fputs("pramfusehpc: error: --hdd is not specified", stderr);
      return 1;
    }
  
  hddlen = 0;
  for (long i = 0; (*(hdd + i)); i++)
    hddlen++;
  long bufsize = hddlen | hddlen >> 1;
  for (long s = 0; s < 32; s <<= 1)
    bufsize |= bufsize >> s;
  bufsize += 1;
  pathbuf = (char*)malloc(bufsize);
  for (long i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(hdd + i);
  
  return fuse_main(argc, argv, &pram_oper, null);
}

