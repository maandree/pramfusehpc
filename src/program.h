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
#ifdef HAVE_SETXATTR
  #include <attr/xattr.h>
#endif



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


