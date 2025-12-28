#ifndef __ARENA_H
#define __ARENA_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

struct arena_region;

typedef struct arena {
  size_t increment_size;
  struct arena_region* first;
} arena;

typedef struct arena_stats {
  size_t regions;
  size_t bytes_allocated;
  size_t bytes_unallocated;
} arena_stats;

arena* allocateArena(size_t initial_size, size_t increment_size);
void freeArena(arena* ap);
void resetArena(arena* ap, bool hard);
void* arenaAlloc(arena* ap, size_t size);
void* arenaRealloc(arena* ap, void* ptr, size_t size);
arena_stats getArenaStats(arena* ap);

#endif  // __ARENA_H
