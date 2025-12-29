#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arena.h"

typedef struct arena_allocation {
  size_t size;
  uint8_t data[];
} arena_allocation;

typedef struct arena_region {
  void* end;
  void* current;
  struct arena_region* next;
  uint8_t data[];
} arena_region;

arena_region* allocateArenaRegion(size_t size)
{
  arena_region* region = malloc(sizeof(arena_region) + size);
  if (!region) {
    return NULL;
  }

  region->end = region->data + size;
  region->current = region->data;
  region->next = NULL;

  return region;
}

arena* allocateArena(size_t initial_size, size_t increment_size)
{
  arena* ap = malloc(sizeof(arena));
  if (!ap) {
    return NULL;
  }

  ap->first = allocateArenaRegion(initial_size);
  if (!ap->first) {
    free(ap);
    return NULL;
  }

  ap->increment_size = increment_size;

  return ap;
}

void resetArena(arena* ap, bool hard)
{
  arena_region* region = ap->first;

  if (hard) {
    // First region stays.
    region->current = region->data;
    region = region->next;
    arena_region* prev;
    while (region) {
      prev = region;
      region = region->next;
      free(prev);
    }
    ap->first->next = NULL;
    return;
  }

  while (region) {
    region->current = region->data;
    region = region->next;
  }
}

void freeArena(arena* ap)
{
  arena_region* current = ap->first;
  arena_region* prev;

  while (current) {
    prev = current;
    current = current->next;

    free(prev);
  }

  free(ap);
}

void* arenaAlloc(arena* ap, size_t size)
{
  arena_region* current = ap->first;

  size = size + sizeof(arena_allocation);

  while (current) {
    if (current->current + size <= current->end) {
      arena_allocation* allocation = current->current;
      allocation->size = size - sizeof(arena_allocation);
      current->current += size;
      return allocation->data;
    }
    if (!current->next) {
      arena_region* region;

      if (size > ap->increment_size) {
        region = allocateArenaRegion(size);
      }
      else {
        region = allocateArenaRegion(ap->increment_size);
      }

      /* NOTE: region could be NULL if allocaation failed. This will cause NULL
       * to be returned. */
      current->next = region;
    }
    current = current->next;
  }

  return NULL;
}

// Realloc memory. The original ptr should be considered invalid afterwards
// even though it does not get 'free'd.
void* arenaRealloc(arena* ap, void* ptr, size_t new_size)
{
  if (ptr == NULL) {
    return arenaAlloc(ap, new_size);
  }

  // Find which region memory is in.
  arena_region* region = ap->first;

  while (region) {
    if ((void*)region->data < ptr && region->end > ptr) {
      break;
    }
    region = region->next;
  }

  assert(region);  // Not in one of the regions. ptr may not even be arena
                   // allocated.

  arena_allocation* allocation = ptr - sizeof(arena_allocation);
  size_t size = allocation->size;

  // Leave it where it is. Also handles realloc of zero size.
  if (size >= new_size) {
    return ptr;
  }

  /*
  printf("Region:         %p\n", region);
  printf("Region data:    %p\n", region->data);
  printf("Ptr:            %p\n", ptr);
  printf("Allocation:     %p\n", allocation);
  printf("Size:           %u\n", (uint32_t)size);
  printf("New size:       %u\n", (uint32_t)new_size);
  printf("Allocation+size:%p\n", (void*)allocation + size);
  printf("Region current: %p\n", region->current);
  printf("Region end:     %p\n", region->end);
  */

  // Can we just extend the allocation?
  if ((void*)allocation->data + size == region->current) {
    if ((void*)allocation->data + new_size <= region->end) {
      allocation->size = new_size;
      region->current = ((void*)allocation) + new_size;
      return allocation->data;
    }
  }

  // Grab some new memory of sufficient size, and copy the data.
  void* new_alloc = arenaAlloc(ap, new_size);

  memcpy(new_alloc, ptr, size);

  return new_alloc;
}

arena_stats getArenaStats(arena* ap)
{
  arena_stats stats = {
      .bytes_allocated = 0, .bytes_unallocated = 0, .regions = 0};
  arena_region* region = ap->first;

  while (region) {
    stats.bytes_allocated += region->current - (void*)region->data;
    stats.bytes_unallocated += region->end - region->current;
    stats.regions++;

    region = region->next;
  }

  return stats;
}

#define TESTING
#ifdef TESTING

#define COUNT 0x100

int main(int argc, char* argv[])
{
  arena* ap = allocateArena(0x1000, 0x2000);

  for (int i = 0; i < 101; i++) {
    if (i % 50 == 0) {
      arena_stats stats = getArenaStats(ap);
      printf("Alloc: %ld, Unalloc: %ld, Regions: %ld\n", stats.bytes_allocated,
             stats.bytes_unallocated, stats.regions);
      resetArena(ap, false);
    }
    uint8_t* ptr = arenaAlloc(ap, COUNT);
    if (ptr) {
      for (int j = 0; j < COUNT; j++) {
        ptr[j] = i;
      }
    }
  }

  arena_stats stats = getArenaStats(ap);
  printf("Alloc: %ld, Unalloc: %ld, Regions: %ld\n", stats.bytes_allocated, stats.bytes_unallocated, stats.regions);

  printf("realloc Tests\n");

  char* message = arenaAlloc(ap, 6);

  message[0] = 0;
  strcat(message, "Hello");

  printf("Message: %s\n", message);

  message = arenaRealloc(ap, message, 13);

  printf("Message: %s\n", message);

  strcat(message, " World!");

  printf("Message: %s\n", message);

  // Get in the way of simply extending.
  arenaAlloc(ap, 1);

  message = arenaRealloc(ap, message, 80);

  printf("Message: %s\n", message);

  strcat(message, " Have a nice day.");

  printf("Message: %s\n", message);

  stats = getArenaStats(ap);
  printf("Alloc: %ld, Unalloc: %ld, Regions: %ld\n", stats.bytes_allocated, stats.bytes_unallocated, stats.regions);

  // Really big alloc test (bigger than region increment_size)

  void* big = arenaAlloc(ap, 0x4000);

  assert(big);

  stats = getArenaStats(ap);
  printf("Alloc: %ld, Unalloc: %ld, Regions: %ld\n", stats.bytes_allocated, stats.bytes_unallocated, stats.regions);

  freeArena(ap);

  return EXIT_SUCCESS;
}

#endif
