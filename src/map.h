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
#include <stdlib.h>



/**
 * The binary logarithm of the number of levels per character in the map data structure
 */
#ifndef MAP_LB_LEVELS
  #define MAP_LB_LEVELS  1
#endif

/**
 * The levels per character in the map data structure
 */
#define MAP_LEVELS  (1 << MAP_LB_LEVELS)

/**
 * The number of bits per level in the map data structure
 */
#define MAP_BIT_PER_LEVEL  (8 >> MAP_LB_LEVELS)

/**
 * The number of combinations per level in the map data structure
 */
#define MAP_PER_LEVEL  (1 << MAP_BIT_PER_LEVEL)



/**
 * char* to void* map structure
 */
typedef struct
{
  /**
   * Available keys
   */
  char** keys;
  
  /**
   * The number of available keys
   */
  long key_count;
  
  /**
   * Indefinite depth array with 16 or 17 elements per level, the last being the value at the position. The first level has 17 elements and the levels alternates between 16 and 17 elements.
   */
  void** data;
} pram_map;



/**
 * Used in `pram_map_free` and `pram__map_free` to store found values that can be freed
 */
void** pram_map_values;

/**
 * The number of elements in `pram_map_values`
 */
long pram_map_values_ptr;

/**
 * The size of `pram_map_values`
 */
long pram_map_values_size;



/**
 * Initialises a map
 * 
 * @param  map  The address of the map
 */
void map_init(pram_map* map);

/**
 * Gets the value for a key in a map
 * 
 * @param   map  The address of the map
 * @param   key  The key
 * @return       The value, `NULL` if not found
 */
void* map_get(pram_map* map, const char* key);

/**
 * Sets the value for a key in a map
 * 
 * @param  map    The address of the map
 * @param  key    The key
 * @param  value  The value, `NULL` to remove, however this does not unlist the key
 */
void map_put(pram_map* map, const char* key, void* value);

/**
 * Frees the resources of a map
 * 
 * @param   map  The address of the map
 * @return       `NULL`-terminated array of values that you may want to free,
 *               but do free this returend array before running this function again
 */
void** map_free(pram_map* map);

