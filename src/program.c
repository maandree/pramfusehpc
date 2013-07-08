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
#define _GNU_SOURCE
#include <fuse.h>
#include <ulockmgr.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memset */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
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
 * Information for opened directories
 */
struct pram_dir_info
{
  /**
   * DIR object create when opening the directory
   */
  DIR* dp;
  
  /**
   * Read directory information
   */
  struct dirent* entry;
  
  /**
   * Offset
   */
  off_t offset;
};



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
int pram_fgetattr(const char* path, struct stat* attr, struct fuse_file_info* fi)
{
  (void) path;
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
int pram_ftruncate(const char* path, off_t length, struct fuse_file_info* fi)
{
  (void) path;
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
 * Apply or remove an advisory lock on a file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @param   op    The lock operation
 * @return        Error code
 */
int pram_flock(const char* path, struct fuse_file_info* fi, int op)
{
  (void) path;
  return r(flock(fi->fh, op));
}

/**
 * Synchronize changes to a file to the underlaying storage device
 * 
 * @param   path        The file
 * @param   isdatasync  Whether to use `fdatasync` rather than `fsync` if available
 * @param   fi          File information
 * @return              Error code
 */
int pram_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
  (void) path;
  
  #ifdef HAVE_FDATASYNC
    if (isdatasync)
      return r(fdatasync(fi->fh));
    else
      return r(fsync(fi->fh));
  #else
    (void) isdatasync;
    return r(fsync(fi->fh));
  #endif
}

/**
 * Close an open file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
int pram_release(const char* path, struct fuse_file_info* fi)
{
  (void) path;
  return r(close(fi->fh));
}

/**
 * Preallocate file space for writing
 * 
 * @param   path  The file
 * @param   mode  The allocation mode
 * @param   off   The offset in the file
 * @param   len   The length of the allocation
 * @param   fi    File information
 * @return        Error code
 */
int pram_fallocate(const char* path, int mode, off_t off, off_t len, struct fuse_file_info* fi)
{
  (void) path;
  #ifdef linux
    return r(fallocate(fi->fh, mode, off, len));
  #else
    if (mode)
      throw EOPNOTSUPP;
    throw posix_fallocate(fi->fh, off, len);
  #endif
}

/**
 * Apply or remove an advisory lock on a file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @param   cmd   Lock command
 * @param   lock  Lock object
 * @return        Error code
 */
int pram_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* lock)
{
  (void) path;
  return ulockmgr_op(fi->fh, cmd, lock, &(fi->lock_owner), sizeof(fi->lock_owner));
}

/**
 * Flush file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
int pram_flush(const char* path, struct fuse_file_info* fi)
{
  (void) path;
  /* FILE* is needed for fflush, and there is not flush, so we need to duplicate and close  */
  return r(close(dup(fi->fh)));
}

/**
 * Write a buffer to a position in a file
 * 
 * @param    path  The file
 * @param    buf   The buffer to write
 * @param    len   The number of bytes to write
 * @param    off   The offset in the file at which to start the write
 * @param    fi    File information
 * @return         Error code if negative, and number of written bytes if non-negative
 */
int pram_write(const char* path, const char* buf, size_t len, off_t off, struct fuse_file_info* fi)
{
  (void) path;
  return r(pwrite(fi->fh, buf, len, off));
}

/**
 * Read a part of a file
 * 
 * @param    path  The file
 * @param    buf   The buffer to which to write read bytes
 * @param    len   The number of bytes to read
 * @param    off   The offset in the file at which to start the read
 * @param    fi    File information
 * @return         Error code if negative, and number of read bytes if non-negative
 */
int pram_read(const char* path, char* buf, size_t len, off_t off, struct fuse_file_info* fi)
{
  (void) path;
  return r(pread(fi->fh, buf, len, off));
}

/**
 * Write a buffer to a position in a file
 * 
 * @param    path  The file
 * @param    buf   The buffer to write
 * @param    off   The offset in the file at which to start the write
 * @param    fi    File information
 * @return         Error code if negative, and number of written bytes if non-negative
 */
int pram_write_buf(const char* path, struct fuse_bufvec* buf, off_t off, struct fuse_file_info* fi)
{
  (void) path;
  struct fuse_bufvec dest = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
  dest.buf->flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
  dest.buf->fd = fi->fh;
  dest.buf->pos = off;
  return fuse_buf_copy(&dest, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

/**
 * Read a part of a file
 * 
 * @param    path  The file
 * @param    bufp  The buffer to which to write read bytes
 * @param    len   The number of bytes to read
 * @param    off   The offset in the file at which to start the read
 * @param    fi    File information
 * @return         Error code
 */
int pram_read_buf(const char* path, struct fuse_bufvec** bufp, size_t len, off_t off, struct fuse_file_info* fi)
{
  (void) path;
  struct fuse_bufvec* src = (struct fuse_bufvec*)malloc(sizeof(struct fuse_bufvec));
  if (src == null)
    throw ENOMEM;
  *src = FUSE_BUFVEC_INIT(len);
  src->buf->flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
  src->buf->fd = fi->fh;
  src->buf->pos = off;
  *bufp = src;
  return 0;
}

/**
 * Close a directory
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
int pram_releasedir(const char* path, struct fuse_file_info* fi)
{
  (void) path;
  struct pram_dir_info* di = (struct pram_dir_info*)(uintptr_t)(fi->fh);
  int err = r(closedir(di->dp));
  free(di);
  return err;
}

/**
 * Open a directory
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
int pram_opendir(const char* path, struct fuse_file_info* fi)
{
  struct pram_dir_info* di = (struct pram_dir_info*)malloc(sizeof(struct pram_dir_info));
  if (di == null)
    throw ENOMEM;
  if ((di->dp = opendir(path)) == null)
    {
      int error = errno;
      free(di);
      throw error;
    }
  di->entry = null;
  di->offset = 0;
  fi->fh = (long)di;
  return 0;
}

/**
 * Read a directory
 * 
 * @param   path    The file
 * @param   buf     Read buffer
 * @param   filler  Function used to fill the buffer
 * @param   off     Read offset
 * @param   fi      File information
 * @return          Error code
 */
int pram_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info* fi)
{
  (void) path;
  struct pram_dir_info* di = (struct pram_dir_info*)(uintptr_t)(fi->fh);
  if (off != di->offset)
    {
      seekdir(di->dp, off);
      di->entry = null;
      di->offset = off;
    }
  for (;;)
    {
      struct stat st;
      off_t next_offset;
      if (di->entry == null)
	  if ((di->entry = readdir(di->dp)) == null)
	    break;
      memset(&st, 0, sizeof(struct stat));
      st.st_ino = di->entry->d_ino;
      st.st_mode = di->entry->d_type << 12;
      next_offset = telldir(di->dp);
      if (filler(buf, di->entry->d_name, &st, next_offset))
	break;
      di->entry = null;
      di->offset = next_offset;
    }
  return 0;
}

/**
 * Create a file
 * 
 * @param   path  The file
 * @param   mode  File mode
 * @param   fi    File information
 * @return        Error code
 */
int pram_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
  int fd = open(path, fi->flags, mode);
  if (fd < 0)
    throw fd;
  fi->fh = fd;
  return 0;
}

/**
 * Open a file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
int pram_open(const char* path, struct fuse_file_info* fi)
{
  int fd = open(path, fi->flags);
  if (fd < 0)
    throw fd;
  fi->fh = fd;
  return 0;
}

#if HAVE_UTIMENSAT
/**
 * Update nanosecond precise timestamps
 * 
 * @param   path  The file
 * @param   ts    Time stamps
 * @return        Error code
 */
int pram_utimens(const char* path, const struct timespec ts[2])
{
  return r(utimensat(0, p(path), ts, AT_SYMLINK_NOFOLLOW));
}
#endif



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
  .flock = pram_flock,
  .fsync = pram_fsync,
  .release = pram_release,
  .lock = pram_lock,
  .flush = pram_flush,
  .write = pram_write,
  .read = pram_read,
  .write_buf = pram_write_buf,
  .read_buf = pram_read_buf,
  .releasedir = pram_releasedir,
  .opendir = pram_opendir,
  .readdir = pram_readdir,
  .create = pram_create,
  .open = pram_open,
  #ifdef HAVE_POSIX_FALLOCATE
    .fallocate = pram_fallocate,
  #endif
  #ifdef HAVE_SETXATTR
    .getxattr = pram_getxattr,
    .listxattr = pram_listxattr,
    .removexattr = pram_removexattr,
    .setxattr = pram_setxattr,
  #endif
  #if HAVE_UTIMENSAT
    .utimens = pram_utimens,
  #endif
  
  .flag_nullpath_ok = true, /* TODO should this be true or false? */
  #if HAVE_UTIMENSAT
    .flag_utime_omit_ok = true,
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
  
  int rc = fuse_main(argc, argv, &pram_oper, null);
  free(pathbuf);
  return rc;
}

  /*
what is poll? (used by fsel)
ioctl (used by fioc)
http://fuse.sourceforge.net/doxygen/structfuse__operations.html#ae3f3482e33a0eada0292350d76b82901

use of pthread
read cusexmp
what about fadvice?
   */
