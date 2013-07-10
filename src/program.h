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
#include <pthread.h>
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
#include <attr/xattr.h>

#include "map.h"



/** Auxiliary syntax **/
#define true     1
#define false    0
#define throw    return -



/**
 * Buffer for converting a file name in this mountpoint to the one in on the HDD
 */
static char* pathbuf = NULL;

/**
 * The length of the HDD path
 */
static long hddlen = 0;

/**
 * The size of `pathbuf`
 */
static long pathbufsize = 0;

/**
 * Thread mutex
 */
static pthread_mutex_t pram_mutex;

/**
 * File cache map
 */
static pram_map* pram_file_cache;



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
 * Information for cached files
 */
struct pram_file_info
{
  /**
   * Contains information such as for example proctection, inode number and ownership
   */
  struct stat attr;
  
  /**
   * File description
   */
  uint64_t fd;
};



/**
 * Lock mutex
 */
#define _lock  pthread_mutex_lock(&pram_mutex)

/**
 * Unlock mutex
 */
#define _unlock  pthread_mutex_unlock(&pram_mutex)

/**
 * Perform a synchronised call and return its return value
 * 
 * @param   INSTRUCTION:→int  The instruction
 * @reutrn                    `r(INSTRUCTION)`
 */
#define sync_return(INSTRUCTION)  	\
  _lock;				\
  int rc = INSTRUCTION;			\
  _unlock;				\
  return r(rc);

/**
 * Perform a synchronised call
 * 
 * @param  INSTRUCTION:→void  The instruction
 */
#define sync_call(INSTRUCTION)  	\
  _lock;				\
  INSTRUCTION;				\
  _unlock;

/**
 * Perform a synchronised call with two paths as sole arguments
 * 
 * @param  METHOD:→int    The method
 * @param  A:const char*  The first argument
 * @param  B:const char*  The other argument
 */
#define sync_call_duo_return(METHOD, A, B)	\
  _lock;					\
  char* a = p(A);				\
  int rc = METHOD(a, q(a, B));			\
  _unlock;					\
  free(a);					\
  return r(rc);



/**
 * Compare two NUL-terminated strings against each other
 * 
 * @param   a  The first comparand
 * @param   b  The second comparand
 * @return     The comparands' equality
 */
static inline long eq(char* a, char* b);

/**
 * Return a path as it is named on the HDD relative to the root mount path
 * 
 * @param   path  The path in RAM
 * @return        The path on HDD
 */
static char* p(const char* path);

/**
 * Return a path as it is named on the HDD relative to the root mount path,
 * but use a new allocation for the path buffer.
 * 
 * @parma   hdd   The old path buffer
 * @param   path  The path in RAM
 * @return        The path on HDD
 */
static inline char* q(char* hdd, const char* path);

/**
 * Get the value to return for a FUSE operation
 * 
 * @param   rc  The returned value from the native operation
 * @return      `rc`, or the thrown error negative.
 */
static inline int r(int rc);



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
int get_user_info(uid_t* user, gid_t* group, mode_t* umask, pid_t* process, gid_t* supplemental, int n);

