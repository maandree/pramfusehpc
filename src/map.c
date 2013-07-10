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
#include "map.h"



/**
 * Initialises a map
 * 
 * @param  map  The address of the map
 */
void pram_map_init(pram_map* map)
{
  long i;
  void** level;
  map->data = level = (void**)malloc((MAP_PER_LEVEL + 1) * sizeof(void*));
  for (i = 0; i <= MAP_PER_LEVEL; i++)
    *(level + i) = NULL;
}


/**
 * Gets the value for a key in a map
 * 
 * @param   map  The address of the map
 * @param   key  The key
 * @return       The value, `NULL` if not found
 */
void* pram_map_get(pram_map* map, const char* key)
{
  void** at = map->data;
  long lv;
  while (*key)
    {
      #define __(L)											\
	lv = (long)((*key >> ((MAP_LEVELS - L - 1) * MAP_BIT_PER_LEVEL)) & (MAP_PER_LEVEL - 1));	\
	if (*(at + lv))											\
	  at = (void**)*(at + lv);									\
	else												\
	  return NULL
      __(0);
      #if MAP_LB_LEVELS >= 1
      __(1);
      #endif
      #if MAP_LB_LEVELS >= 2
      __(2);  __(3);
      #endif
      #if MAP_LB_LEVELS >= 3
      __(4);  __(5);  __(6);  __(7);
      #endif
      #undef __
      key++;
    }
  return *(at + MAP_PER_LEVEL);
}


/**
 * Sets the value for a key in a map
 * 
 * @param  map    The address of the map
 * @param  key    The key
 * @param  value  The value, `NULL` to remove
 */
void pram_map_put(pram_map* map, const char* key, void* value)
{
  void** at = map->data;
  long i, lv;
  while (*key)
    {
      #define __(L, END)										\
	lv = (long)((*key >> ((MAP_LEVELS - L - 1) * MAP_BIT_PER_LEVEL)) & (MAP_PER_LEVEL - 1));	\
        if (*(at + lv))											\
	  at = (void**)*(at + lv);					       				\
	else												\
	  {												\
	    at = (void**)(*(at + lv) = (void*)malloc((MAP_PER_LEVEL + (END)) * sizeof(void*)));		\
	    for (i = 0; i < MAP_PER_LEVEL + (END); i++)							\
	      *(at + i) = NULL;										\
	  }
      __(0, MAP_LB_LEVELS == 0);
      #if MAP_LB_LEVELS >= 1
      __(1, MAP_LB_LEVELS == 1);
      #endif
      #if MAP_LB_LEVELS >= 2
      __(2, 0);  __(3, MAP_LB_LEVELS == 2);
      #endif
      #if MAP_LB_LEVELS >= 3
      __(4, 0);  __(5, 0);  __(6, 0);  __(7, MAP_LB_LEVELS == 3);
      #endif
      #undef __
      key++;
    }
  *(at + MAP_PER_LEVEL) = value;
}


/**
 * Frees a level and all sublevels in a map
 * 
 * @param  level        The level
 * @param  level_depth  The level measured in depth
 */
static void pram__map_free(void** level, long level_depth)
{
  long i, next_level_depth = level_depth + 1;
  void* value;
  if (level == NULL)
    return;
  for (i = 0; i < MAP_PER_LEVEL; i++)
    pram__map_free((void**)*(level + i), next_level_depth);
  if ((level_depth & 1) == 0)
    if ((value = *(level + MAP_PER_LEVEL)))
      {
	if (pram_map_values_ptr == pram_map_values_size)
	  pram_map_values = (void**)realloc(pram_map_values, (pram_map_values_size <<= 1) * sizeof(void*));
	*(pram_map_values + pram_map_values_ptr++) = value;
      }
  free(level);
}


/**
 * Frees the resources of a map
 * 
 * @param   map  The address of the map
 * @return       `NULL`-terminated array of values that you may want to free,
 *               but do free this returend array before running this function again
 */
void** pram_map_free(pram_map* map)
{
  pram_map_values_ptr = 0;
  pram_map_values_size = 64;
  pram_map_values = (void**)malloc(512 * sizeof(void*));
  pram__map_free(map->data, 0);
  if (pram_map_values_ptr == pram_map_values_size)
    pram_map_values = (void**)realloc(pram_map_values, (pram_map_values_size + 1) * sizeof(void*));
  *(pram_map_values + pram_map_values_ptr) = NULL;
  return pram_map_values;
}

