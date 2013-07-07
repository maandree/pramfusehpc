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
#ifdef HAVE_CONFIG_H
  #include <config.h>
#endif
#include <fuse.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef HAVE_SETXATTR
  #include <attr/xattr.h>
#endif

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
      pathbufsize = n + hddlen + 128;
      pathbuf = (char*)realloc(pathbuf, pathbufsize * sizeof(char));
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
int r(int rc)
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
 * Get file attributes
 * 
 * @param   path  The file
 * @param   attr  The stat storage
 * @param   fi    File information
 * @return        Error code
 */
int pram_fgetattr(const char* path, struct stat* attr, struct fuse_file_info *fi)
{
return r(fstat(fi->fh, attr));
}

#ifdef HAVE_SETXATTR
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
#endif

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
  pathbuf = (char*)malloc(pathbufsize * sizeof(char));
  for (int i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(_target + i);
  char* _path = p(path);
  int rc = r(link(_target, _path));
  free(_target);
  return rc;
}

#ifdef HAVE_SETXATTR
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
#endif

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
 * @param   rdev   The file's device specifications
 * @return         Error code
 */
int pram_mknod(const char* path, mode_t mode, dev_t rdev)
{
  return r(mknod(p(path), mode, rdev));
}

#ifdef HAVE_SETXATTR
/**
 * Remove extended file attribute
 * 
 * @param   path  The file
 * @param   name  The attribute
 * @return        Error code
 */
int pram_removexattr(const char* path, const char* name)
{
  return r(lremovexattr(p(path), name));
}
#endif

/**
 * Rename file
 * 
 * @param   source  The source file
 * @param   path    The destination file
 * @return          Error code
 */
int pram_rename(const char* source, const char* path)
{
  char* _source = p(source);
  pathbuf = (char*)malloc(pathbufsize * sizeof(char));
  for (int i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(_source + i);
  char* _path = p(path);
  int rc = r(rename(_source, _path));
  free(_source);
  return rc;
}

/**
 * Remove a directory
 * 
 * @param   path  The file
 * @return        Error code
 */
int pram_rmdir(const char* path)
{
  return r(rmdir(p(path)));
}

#ifdef HAVE_SETXATTR
/**
 * Set an extended file attribute
 * 
 * @param   path   The file
 * @param   name   The attribute
 * @param   value  The value
 * @param   size   The size of the value
 * @param   flags  Rules
 * @return         Error code
 */
int pram_setxattr(const char* path, const char* name, const char* value, size_t size, int flags)
{
  return r(lsetxattr(p(path), name, value, size, flags));
}
#endif

/**
 * Get file system statistics
 * 
 * @param   path   The path
 * @param   value  The statistics storage
 * @return         Error code
 */
int pram_statfs(const char* path, struct statvfs* value)
{
  return r(statvfs(p(path), value));
}

/**
 * Create a symbolic link
 * 
 * @param   target  The string contained in the link
 * @param   path    The link to create
 * @return          Error code
 */
int pram_symlink(const char* target, const char* path)
{
  return r(symlink(target, p(path)));
}

/**
 * Truncate or extend a file to a specified length
 * 
 * @param   path    The file
 * @param   length  The length
 * @return          Error code
 */
int pram_truncate(const char* path, off_t length)
{
  return r(truncate(p(path), length));
}

/**
 * Truncate or extend a file to a specified length
 * 
 * @param   path    The file
 * @param   length  The length
 * @param   fi      File information
 * @return          Error code
 */
int pram_ftruncate(const char* path, off_t length, struct fuse_file_info *fi)
{
  return r(ftruncate(fi->fh, length));
}

/**
 * Remove a link to a file, that is, a file path,
 * and only the inode if it has no more paths
 * 
 * @param   path  The file
 * @return        Error code
 */
int pram_unlink(const char* path)
{
  return r(unlink(p(path)));
}

/**
 * Read value of a symbolic link
 * 
 * @param   path    The file
 * @param   target  The target storage
 * @param   size    The size of `target`
 * @return          Error code
 */
int pram_readlink(const char* path, char* target, size_t size)
{
  long n = readlink(p(path), target, size - 1);
  if (n < 0)
    throw errno;
  *(target + n) = 0;
  return 0;
}

/**
 * Check real user's permissions for a file
 * 
 * @param   path  The file
 * @param   mode  The permission to test
 * @return        Error code
 */
int pram_access(const char* path, int mode)
{
  return r(access(p(path), mode));
}



/**
 * The file system operations
 */
static struct fuse_operations pram_oper = {
  .destroy = pram_destroy,
  .chmod = pram_chmod,
  .chown = pram_chown,
  .getattr = pram_getattr,
  .fgetattr = pram_fgetattr,
  .link = pram_link,
  .mkdir = pram_mkdir,
  .mknod = pram_mknod,
  .rename = pram_rename,
  .rmdir = pram_rmdir,
  .statfs = pram_statfs,
  .symlink = pram_symlink,
  .truncate = pram_truncate,
  .ftruncate = pram_ftruncate,
  .unlink = pram_unlink,
  .readlink = pram_readlink,
  .access = pram_access,
  #ifdef HAVE_POSIX_FALLOCATE
    .getxattr = pram_getxattr,
    .listxattr = pram_listxattr,
    .removexattr = pram_removexattr,
    .setxattr = pram_setxattr,
  #endif
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
  pathbuf = (char*)malloc(bufsize * sizeof(char));
  for (long i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(hdd + i);
  
  return fuse_main(argc, argv, &pram_oper, null);
}

