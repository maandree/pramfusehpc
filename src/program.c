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
#include "program.h"



/**
 * Change the permission bits of a file
 * 
 * @param   path  The file
 * @param   mode  The permission bits
 * @return        Error code
 */
static int pram_chmod(const char* path, mode_t mode)
{
  struct pram_file* cache = NULL;
  _lock;
  int error = get_file_cache(path, &cache);
  if (!error)
    if (cache->attr.st_mode != mode)
      {
	error = chmod(pathbuf, mode);
	cache->attr.st_mode = mode;
	/* TODO update ctime */
      }
  _unlock;
  return error;
}

/**
 * Change the ownership of a file
 * 
 * @param   path   The file
 * @param   owner  The owner
 * @param   group  The group
 * @return         Error code
 */
static int pram_chown(const char* path, uid_t owner, gid_t group)
{
  struct pram_file* cache = NULL;
  _lock;
  int error = get_file_cache(path, &cache);
  if (!error)
    if ((cache->attr.st_uid != owner) || (cache->attr.st_gid != group))
      {
	error = lchown(pathbuf, owner, group);
	cache->attr.st_uid = owner;
	cache->attr.st_gid = group;
	/* TODO update ctime */
      }
  _unlock;
  return error;
}

/**
 * Get file attributes
 * 
 * @param   path  The file
 * @param   attr  The stat storage
 * @return        Error code
 */
static int pram_getattr(const char* path, struct stat* attr)
{
  struct pram_file* cache = NULL;
  _lock;
  int error = get_file_cache(path, &cache);
  if (!error)
    *attr = cache->attr;
  _unlock;
  return error;
}

/**
 * Get file attributes
 * 
 * @param   path  The file
 * @param   attr  The stat storage
 * @param   fi    File information
 * @return        Error code
 */
static int pram_fgetattr(const char* path, struct stat* attr, struct fuse_file_info* fi)
{
  (void) path;
  _lock;
  *attr = fcache(fi)->attr;
  _unlock;
  return 0;
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
static int pram_getxattr(const char* path, const char* name, char* value, size_t size)
{
  sync_return(lgetxattr(p(path), name, value, size));  /* TODO xattr is not cached */
}

/**
 * Create a hard link to a file
 * 
 * @param   target  The target
 * @param   path    The link
 * @return          Error code
 */
static int pram_link(const char* target, const char* path)
{
  /* TODO hard linking is currently a problem for our cache */
  sync_call_duo_return(link, target, path);
}

/**
 * List extended file attributes
 * 
 * @param   path   The file
 * @param   list   The list storage
 * @param   size   The size of `list`
 * @return         Error code or value size
 */
static int pram_listxattr(const char* path, char* list, size_t size)
{
  sync_return(llistxattr(p(path), list, size));  /* TODO xattr is not cached */
}

/**
 * Create a directory
 * 
 * @param   path   The file
 * @param   mode   The file's protection bits
 * @return         Error code
 */
static int pram_mkdir(const char* path, mode_t mode)
{
  sync_return(mkdir(p(path), mode));  /* TODO dir is not cached */
}

/**
 * Create a file node
 * 
 * @param   path   The file
 * @param   mode   The file's protection bits
 * @param   rdev   The file's device specifications
 * @return         Error code
 */
static int pram_mknod(const char* path, mode_t mode, dev_t rdev)
{
  sync_return(mknod(p(path), mode, rdev));  /* TODO dir is not cached */
}

/**
 * Remove extended file attribute
 * 
 * @param   path  The file
 * @param   name  The attribute
 * @return        Error code
 */
static int pram_removexattr(const char* path, const char* name)
{
  sync_return(lremovexattr(p(path), name));  /* TODO xattr is not cached */
  /* TODO update ctime */
}

/**
 * Rename file
 * 
 * @param   source  The source file
 * @param   path    The destination file
 * @return          Error code
 */
static int pram_rename(const char* source, const char* path)
{
  _lock;  /* TODO dir is not cached */
  char* _source = p(source);
  int error = rename(_source, q(_source, path));
  if (!error)
    if (!eq(source, path))
      {
	pram_map_put(pram_file_cache, path, pram_map_get(pram_file_cache, source));
	pram_map_put(pram_file_cache, source, NULL);
	/* TODO update ctime */
      }
  _unlock;
  free(_source);
  return r(error);
}

/**
 * Remove a directory
 * 
 * @param   path  The file
 * @return        Error code
 */
static int pram_rmdir(const char* path)
{
  sync_return(rmdir(p(path)));  /* TODO dir is not cached */
}

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
static int pram_setxattr(const char* path, const char* name, const char* value, size_t size, int flags)
{
  sync_return(lsetxattr(p(path), name, value, size, flags));  /* TODO xattr is not cached */
  /* TODO update ctime */
}

/**
 * Get file system statistics
 * 
 * @param   path   The path
 * @param   value  The statistics storage
 * @return         Error code
 */
static int pram_statfs(const char* path, struct statvfs* value)
{
  sync_return(statvfs(p(path), value));  /* TODO statfs is not cached */
}

/**
 * Create a symbolic link
 * 
 * @param   target  The string contained in the link
 * @param   path    The link to create
 * @return          Error code
 */
static int pram_symlink(const char* target, const char* path)
{
  _lock;
  void* ret = pram_map_get(pram_file_cache, path);
  if (ret)
    {
      _unlock;
      throw EEXIST;
    }
  int rc = symlink(target, p(path));  /* TODO dir is not cached */
  _unlock;
  return r(rc);
}

/**
 * Truncate or extend a file to a specified length
 * 
 * @param   path    The file
 * @param   length  The length
 * @return          Error code
 */
static int pram_truncate(const char* path, off_t length)
{
  _lock;
  struct pram_file* cache;
  int error = get_file_cache(path, &cache);
  if (!error)
    if (!(error = truncate(pathbuf, length)))
      {
	off_t size = cache->attr.st_size;
	off_t addition = size >= length ? 0 : (length - size);
	blkcnt_t blocks = cache->attr.st_blocks;
	size += (!!(size & 511)) << 9;
	blocks -= size >> 9;
	size = length;
	size += (!!(size & 511)) << 9;
	blocks += size >> 9;
	cache->attr.st_size = length;
	cache->attr.st_blocks = blocks;
	if (cache->allocated)
	  {
	    if (addition > 0)
	      {
		off_t off = length - addition;
		off_t end = (unsigned long)length < cache->allocated ? (unsigned long)length : cache->allocated;
		char* buf = cache->buffer;
		for (; off < end; off++)
		  *(buf + off) = 0;
	      }
	    else if (length == 0)
	      {
		cache->allocated = 0;
		free(cache->buffer);
	      }
	    else if (cache->allocated >= (unsigned long)(length << 1))
	      {
		cache->allocated = length;
		cache->buffer = (char*)realloc(cache->buffer, length * sizeof(char));
	      }
	  }
	/* TODO update ctime */
	/* TODO update mtime */
      }
  _unlock;
  return r(error);
}

/**
 * Truncate or extend a file to a specified length
 * 
 * @param   path    The file
 * @param   length  The length
 * @param   fi      File information
 * @return          Error code
 */
static int pram_ftruncate(const char* path, off_t length, struct fuse_file_info* fi)
{
  (void) path;
  struct pram_file_info* file = (struct pram_file_info*)(uintptr_t)(fi->fh);
  int error = ftruncate(file->fd, length);
  if (!error)
    {
      struct pram_file* cache = file->cache;
      off_t size = cache->attr.st_size;
      off_t addition = size >= length ? 0 : (length - size);
      blkcnt_t blocks = cache->attr.st_blocks;
      size += (!!(size & 511)) << 9;
      blocks -= size >> 9;
      size = length;
      size += (!!(size & 511)) << 9;
      blocks += size >> 9;
      _lock;
      cache->attr.st_size = length;
      cache->attr.st_blocks = blocks;
      if (cache->allocated)
	{
	  if (addition > 0)
	    {
	      off_t off = length - addition;
	      off_t end = (unsigned long)length < cache->allocated ? (unsigned long)length : cache->allocated;
	      char* buf = cache->buffer;
	      for (; off < end; off++)
		*(buf + off) = 0;
	    }
	  else if (length == 0)
	    {
	      cache->allocated = 0;
	      free(cache->buffer);
	    }
	  else if (cache->allocated >= (unsigned long)(length << 1))
	    {
	      cache->allocated = length;
	      cache->buffer = (char*)realloc(cache->buffer, length * sizeof(char));
	    }
	}
      _unlock;
      /* TODO update ctime */
      /* TODO update mtime */
    }
  return r(error);
}

/**
 * Remove a link to a file, that is, a file path,
 * and only the inode if it has no more paths
 * 
 * @param   path  The file
 * @return        Error code
 */
static int pram_unlink(const char* path)
{
  _lock;  /* TODO dir is not cached */
  void* ret = pram_map_get(pram_file_cache, path);
  if (ret != NULL)
    {
      struct pram_file* cache = (struct pram_file*)ret;
      cache->attr.st_nlink--;
      if (cache->attr.st_nlink == 0)
	{
	  free(cache->buffer);
	  free(cache->link);
	  free(cache);
	  pram_map_put(pram_file_cache, path, NULL);
	}
    }
  int rc = unlink(p(path));
  _unlock;
  return r(rc);
}

/**
 * Check real user's permissions for a file
 * 
 * @param   path  The file
 * @param   mode  The permission to test
 * @return        Error code
 */
static int pram_access(const char* path, int mode)
{
  /* TODO */
  sync_return(access(p(path), mode));
}

/**
 * Read value of a symbolic link
 * 
 * @param   path    The file
 * @param   target  The target storage
 * @param   size    The size of `target`
 * @return          Error code
 */
static int pram_readlink(const char* path, char* target, size_t size)
{
  if (size <= 0)
    throw EINVAL;
  struct pram_file* cache = NULL;
  _lock;
  int error = r(get_file_cache(path, &cache));
  _unlock;
  if (!error)
    {
      if (S_ISLNK(cache->attr.st_mode) == false)
	throw EINVAL;
      else if (!(error = pram_access(path, R_OK | X_OK)))
	{
	  _lock;
	  if ((cache->link) == false)
	    {
	      char* link = (char*)malloc(1024 * sizeof(char));
	      long n = readlink(p(path), link, 1023);
	      if (n < 0)
		{
		  _unlock;
		  free(link);
		  throw errno;
		}
	      else if (n > 1023)
		{
		  link = (char*)realloc(link, (n + 1) * sizeof(char));
		  if ((n = readlink(pathbuf, link, 1023)) < 0)
		    {
		      _unlock;
		      free(link);
		      throw errno;
		    }
		}
	      else if (n < 1023)
		link = (char*)realloc(link, (n + 1) * sizeof(char));
	      *(link + n) = 0;
	      cache->link = link;
	      cache->linkn = n + 1;
	    }
	  char* link = cache->link;
	  for (size_t i = 0; i < size; i++)
	    if ((*(target + i) + *(link + i)) == 0)
	      break;
	  *(target + size - 1) = 0;
	  error = cache->linkn;
	  _unlock;
	}
    }
  return error;
}

/**
 * Apply or remove an advisory lock on a file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @param   op    The lock operation
 * @return        Error code
 */
static int pram_flock(const char* path, struct fuse_file_info* fi, int op)
{
  /* TODO */
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
static int pram_fsync(const char* path, int isdatasync, struct fuse_file_info* fi)
{
  /* TODO */
  (void) path;
  return r(isdatasync ? fdatasync(fi->fh) : fsync(fi->fh));
}

/**
 * Synchronize changes to a directory to the underlaying storage device
 * 
 * @param   path        The file
 * @param   isdatasync  Whether to use `fdatasyncdir` rather than `fsyncdir` if available
 * @param   fi          File information
 * @return              Error code
 */
static int pram_fsyncdir(const char* path, int isdatasync, struct fuse_file_info* fi)
{
  /* TODO */
  (void) path;
  return r(isdatasync ? fdatasync(fi->fh) : fsync(fi->fh));
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
static int pram_fallocate(const char* path, int mode, off_t off, off_t len, struct fuse_file_info* fi)
{
  /* TODO */
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
static int pram_lock(const char* path, struct fuse_file_info* fi, int cmd, struct flock* lock)
{
  /* TODO */
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
static int pram_flush(const char* path, struct fuse_file_info* fi)
{
  /* TODO */
  (void) path;
  /* FILE* is needed for fflush, and there is not flush, so we need to duplicate and close  */
  return r(close(dup(fi->fh)));
}

/**
 * Close an open file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
static int pram_release(const char* path, struct fuse_file_info* fi)
{
  pram_flush(path, fi);
  struct pram_file_info* file = (struct pram_file_info*)(uintptr_t)(fi->fh);
  int rc = close(file->fd);
  free(file);
  return r(rc);
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
static int pram_write(const char* path, const char* buf, size_t len, off_t off, struct fuse_file_info* fi)
{
  /* TODO what if this is not a regular file */
  (void) path;
  if (len == 0)
    return 0;
  struct pram_file* cache = fcache(fi);
  if (cache->allocated)
    {
      _lock;
      char* wbuf;
      if (off + len > (unsigned long)(cache->attr.st_size))
	cache->attr.st_size = off + len;
      if (off + len <= cache->allocated)
	wbuf = cache->buffer;
      else
	{
	  wbuf = (char*)realloc(cache->buffer, (cache->allocated = off + len) * sizeof(char));
	  if (wbuf)
	    cache->buffer = wbuf;
	  else
	    {
	      /* TODO free old cache to get more memory */
	      _unlock;
	      return r(pwrite(ffd(fi), buf, len, off));
	    }
	}
      _unlock;
      wbuf += off;
      for (size_t i = 0; i != len; i++)
	*(wbuf + i) = *(buf + i);
      return len;
    }
  else
    return r(pwrite(ffd(fi), buf, len, off));
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
static int pram_read(const char* path, char* buf, size_t len, off_t off, struct fuse_file_info* fi)
{
  /* TODO what if this is not a regular file */
  (void) path;
  if (len == 0)
    return 0;
  struct pram_file* cache = fcache(fi);
  _lock;
  unsigned long n = off + len;
  if (n > (unsigned long)(cache->attr.st_size))
    n = cache->attr.st_size;
  if (cache->allocated == 0)
    {
      char* buffer = (char*)malloc(n * sizeof(buf));
      if (buf == NULL)
	{
	  /* TODO free old cache to get more memory */
	  _unlock;
	  return r(pread(ffd(fi), buf, len, off));
	}
      cache->allocated = n;
      cache->buffer = buffer;
      _unlock;
      unsigned long ptr = 0;
      uint64_t fd = ffd(fi);
      while (ptr < n)
	{
	  ssize_t r = pread(fd, buffer, n - ptr, ptr);
	  if (r < 0)
	    {
	      if ((cache->allocated = ptr) == 0)
		{
		  _lock;
		  free(buffer);
		  cache->buffer = NULL;
		  _unlock;
		  throw errno;
		}
	    }
	  if (r == 0)
	    {
	      n = ptr;
	      _lock;
	      cache->allocated = n;
	      buffer = (char*)realloc(buffer, n * sizeof(char*));
	      if (buffer)
		cache->buffer = buffer;
	      _unlock;
	      break;
	    }
	  ptr += r;
	}
    }
  n -= off;
  if (n >> 31)
    n = len & ((1L << 32) - 1L);
  char* buffer = cache->buffer + off;
  for (unsigned long i = 0; i != n; i++)
    *(buf + i) = *(buffer + i);
  return n;
}

/**
 * Close a directory
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
static int pram_releasedir(const char* path, struct fuse_file_info* fi)
{
  /* TODO dir is not cached */
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
static int pram_opendir(const char* path, struct fuse_file_info* fi)
{
  /* TODO dir is not cached */
  struct pram_dir_info* di = (struct pram_dir_info*)malloc(sizeof(struct pram_dir_info));
  if (di == NULL)
    throw ENOMEM;
  sync_call(di->dp = opendir(p(path)));
  if (di->dp == NULL)
    {
      int error = errno;
      free(di);
      throw error;
    }
  di->entry = NULL;
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
static int pram_readdir(const char* path, void* buf, fuse_fill_dir_t filler, off_t off, struct fuse_file_info* fi)
{
  /* TODO dir is not cached */
  (void) path;
  struct pram_dir_info* di = (struct pram_dir_info*)(uintptr_t)(fi->fh);
  if (off != di->offset)
    {
      seekdir(di->dp, off);
      di->entry = NULL;
      di->offset = off;
    }
  for (;;)
    {
      struct stat st;
      off_t next_offset;
      if (di->entry == NULL)
	  if ((di->entry = readdir(di->dp)) == NULL)
	    break;
      memset(&st, 0, sizeof(struct stat));
      st.st_ino = di->entry->d_ino;
      st.st_mode = di->entry->d_type << 12;
      next_offset = telldir(di->dp);
      if (filler(buf, di->entry->d_name, &st, next_offset))
	break;
      di->entry = NULL;
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
static int pram_create(const char* path, mode_t mode, struct fuse_file_info* fi)
{
  /* TODO dir is not cached*/
  _lock;
  int fd = open(p(path), fi->flags, mode);
  if (fd < 0)
    throw fd;
  struct pram_file* cache = (struct pram_file*)malloc(sizeof(struct pram_file));
  int error = get_file_cache(path, &cache);
  _unlock;
  if (error)
    {
      free(cache);
      close(fd);
      return error;
    }
  struct pram_file_info* file = (struct pram_file_info*)malloc(sizeof(struct pram_file_info));
  file->fd = fd;
  file->cache = cache;
  fi->fh = (uint64_t)(void*)file;
  return 0;
}

/**
 * Open a file
 * 
 * @param   path  The file
 * @param   fi    File information
 * @return        Error code
 */
static int pram_open(const char* path, struct fuse_file_info* fi)
{
  _lock;
  int fd = open(p(path), fi->flags);
  if (fd < 0)
    throw fd;
  struct pram_file* cache = (struct pram_file*)malloc(sizeof(struct pram_file));
  int error = get_file_cache(path, &cache);
  _unlock;
  if (error)
    {
      free(cache);
      close(fd);
      return error;
    }
  struct pram_file_info* file = (struct pram_file_info*)malloc(sizeof(struct pram_file_info));
  file->fd = fd;
  file->cache = cache;
  fi->fh = (uint64_t)(void*)file;
  return 0;
}

/**
 * Update nanosecond precise timestamps
 * 
 * @param   path  The file
 * @param   ts    Time stamps
 * @return        Error code
 */
static int pram_utimens(const char* path, const struct timespec ts[2])
{
  _lock;
  struct pram_file* cache = NULL;
  int error = get_file_cache(path, &cache);
  if (!error)
    if (!(error = utimensat(0, pathbuf, ts, AT_SYMLINK_NOFOLLOW)))
      {
	if (ts == NULL)
	  {
	    struct stat attr;
	    error = lstat(p(path), &attr);
	    if (!error)
	      cache->attr = attr;
	  }
	else
	  {
	    #if defined __USE_MISC || defined __USE_XOPEN2K8
	      cache->attr.st_atim = ts[0];
	      cache->attr.st_mtim = ts[1];
	    #else
	      cache->attr.st_atime = ts[0].tv_sec;
	      cache->attr.st_mtime = ts[1].tv_sec;
	      cache->attr.st_atimensec = ts[0].tv_nsec;
	      cache->attr.st_mtimensec = ts[1].tv_nsec;
	    #endif
	    /* TODO update ctime? */
	  }
      }
  _unlock;
  return r(error);
}



/**
 * The file system operations
 */
static struct fuse_operations pram_oper = {
  /* .destroy = pram_destroy,  :  void() // Clean up the file system for exit */
  /* .init = pram_init,  :  void*(struct fuse_conn_info *conn) // Initialise filesystem */
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
  .fsyncdir = pram_fsyncdir,
  .release = pram_release,
  .lock = pram_lock,
  .flush = pram_flush,
  .write = pram_write,
  .read = pram_read,
  .releasedir = pram_releasedir,
  .opendir = pram_opendir,
  .readdir = pram_readdir,
  .create = pram_create,
  .open = pram_open,
  .fallocate = pram_fallocate,
  .getxattr = pram_getxattr,
  .listxattr = pram_listxattr,
  .removexattr = pram_removexattr,
  .setxattr = pram_setxattr,
  .utimens = pram_utimens,
  
  /*
    what about fadvice?
  */
  
  /* Operations with fd does not need path, HOWEVER, unlinked files cannot be accessed. */
  .flag_nullpath_ok = false,
  /* Operations with fd does not need path, and unlinked files cannot be accessed. */
  .flag_nopath = true,
  
  .flag_utime_omit_ok = true,
};



/**
 * Pthread start routine for background thread
 * 
 * @param   data  Private use input data
 * @return        Private use output data
 */
/*void* pram_background(void* data)
{
  (void) data;
  return NULL;
}*/



/**
 * This is the main entry point of the program
 * 
 * @param   argc  The number of elements in `argv`
 * @param   argv  The command line arguments, including executed program
 * @return        The exit value, zero on success
 */
int main(int argc, char** argv)
{
  int _argc = argc - 2, i;
  char* hdd = NULL;
  for (i = 1; i < argc; i++)
    if (eq(*(argv + i), "--hdd"))
      {
	if ((hdd))
	  fputs("pramfusehpc: error: use of multiple --hdd", stderr);
	else if (i + 1 == argc)
	  fputs("pramfusehpc: error: --hdd without argument", stderr);
	else
	  {
	    hdd = argv[++i];
	    break;
	  }
	return 1;
      }
  if (hdd == NULL)
    {
      fputs("pramfusehpc: error: --hdd is not specified", stderr);
      return 1;
    }
  
  /* pthread_t background_thread; */
  pthread_attr_t pth_attr;
  
  if ((errno = pthread_mutex_init(&pram_mutex, NULL)))
    {
      perror("pthread_mutex_init");
      return 1;
    }
  if ((errno = pthread_attr_init(&pth_attr)))
    {
      perror("pthread_attr_init");
      return 1;
    }
  /*
  if ((errno = pthread_create(&background_thread, &pth_attr, pram_background, NULL)))
    {
      perror("pthread_create");
      return 1;
    }
  */
  
  hdd = realpath(hdd, NULL);
  if (hdd == NULL)
    {
      perror("realpath");
      return 1;
    }
  hddlen = 0;
  for (long j = 0; (*(hdd + j)); j++)
    hddlen++;
  long bufsize = hddlen | (hddlen >> 1);
  for (long s = 1; s < 32; s <<= 1)
    bufsize |= bufsize >> s;
  bufsize += 1;
  pathbuf = (char*)malloc(bufsize * sizeof(char));
  for (long j = 0; j < hddlen; j++)
    *(pathbuf + j) = *(hdd + j);
  if ((hddlen > 0) && (*(hdd + hddlen - 1) == '/'))
    hddlen--;
  
  i--;
  char** _argv = (char**)malloc(_argc * sizeof(char*));
  for (long j = 0, k = 0; j < argc; j++)
    if (j == i)
      j++;
    else
      *(_argv + k++) = *(argv + j);
  
  pram_file_cache = (pram_map*)malloc(sizeof(pram_map));
  pram_map_init(pram_file_cache);
  
  int rc = fuse_main(_argc, _argv, &pram_oper, NULL);
  free(hdd);
  free(_argv);
  free(pathbuf);
  struct pram_file** file_caches = (struct pram_file**)pram_map_free(pram_file_cache);
  struct pram_file* file_cache;
  while ((file_cache = *file_caches++))
    {
      free(file_cache->link);
      free(file_cache->buffer);
      free(file_cache);
    }
  free(file_caches);
  free(pram_file_cache);
  /* pthread_cancel(background_thread); */
  /* pthread_join(background_thread, NULL); */
  pthread_mutex_destroy(&pram_mutex);
  return rc;
}



/**
 * Compare two NUL-terminated strings against each other
 * 
 * @param   a  The first comparand
 * @param   b  The second comparand
 * @return     The comparands' equality
 */
static inline long eq(const char* a, const char* b)
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
static char* p(const char* path)
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
 * Return a path as it is named on the HDD relative to the root mount path,
 * but use a new allocation for the path buffer.
 * 
 * @parma   hdd   The old path buffer
 * @param   path  The path in RAM
 * @return        The path on HDD
 */
static inline char* q(const char* hdd, const char* path)
{
  pathbuf = (char*)malloc(pathbufsize * sizeof(char));
  for (int i = 0; i < hddlen; i++)
    *(pathbuf + i) = *(hdd + i);
  return p(path);
}

/**
 * Get the value to return for a FUSE operation
 * 
 * @param   rc  The returned value from the native operation
 * @return      `rc`, or the thrown error negative.
 */
static inline int r(int rc)
{
  return rc < 0 ? -errno : rc;
}



/**
 * Get information about the use as well as the ID of the process accessing the file system
 * 
 * @param   user          Where to store the ID of the user accessing the file system
 * @param   group         Where to store the group ID of the user accessing the file system
 * @param   umask         Where to store the current umask
 * @param   process       Where to store the ID of the process accessing the file system
 * @param   supplemental  List to fill with supplemental groups of the user accessing the file system, ignored if `NULL`
 * @param   n             The size of `supplemental`
 * @return                Error code or the total (can exceed `supplemental`) number of supplemental groups
 */
int get_user_info(uid_t* user, gid_t* group, mode_t* umask, pid_t* process, gid_t* supplemental, int n)
{
  struct fuse_context* context = fuse_get_context();
  *user = context->uid;
  *group = context->gid;
  *umask = context->umask;
  *process = context->pid;
  
  if (supplemental == NULL)
    return 0;
  
  int ret = fuse_getgroups(n, supplemental);
  return ret == -ENOSYS ? 0 : ret;
}


/**
 * Gets the file cache for a file by its name
 * 
 * @param   path   The file
 * @param   cache  Area to put the cache in
 * @return         Error code
 */
int get_file_cache(const char* path, struct pram_file** cache)
{
  void* ret = pram_map_get(pram_file_cache, path);
  if (ret == NULL)
    {
      struct stat attr;
      int error = lstat(p(path), &attr);
      if (error)
	throw errno;
      struct pram_file* c = (struct pram_file*)malloc(sizeof(struct pram_file));
      memset(c, 0, sizeof(struct pram_file));
      (*cache = c)->attr = attr;
      c->buffer = NULL;
      c->allocated = 0;
      pram_map_put(pram_file_cache, path, c);
    }
  else
    *cache = (struct pram_file*)ret;
  return 0;
}

