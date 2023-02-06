#define _DEFAULT_SOURCE
#define _BSD_SOURCE 
#include <malloc.h> 
#include <stdio.h> 
#include <unistd.h>
#include <assert.h>
#include <string.h>

// Include any other headers we need here
#include <sys/mman.h>
#include <pthread.h>

// NOTE: You should NOT include <stdlib.h> in your final implementation

#include <debug.h> // definition of debug_printf

// Definition of the free list
typedef struct free_list_elm {
  size_t size;
  struct free_list_elm* next;
} free_list_elm;

// creates the free list 
static free_list_elm* free_list = 0;

// lock for the free list
pthread_mutex_t list_lock;

// page size from man page of mmap
const size_t PAGE_SIZE = 4096;

static free_list_elm* findFreeSize(size_t size);
static free_list_elm* addFreeList(free_list_elm* start, free_list_elm* toAdd);

// Looks for a block of memory in the free list that are big enough for accomodate the request
// Otherwise, returns NULL.
// Analogous to findNextFree from A5
static free_list_elm* findFreeSize(size_t s){
  free_list_elm** current = &free_list;
  // Searches through the list 
  while(*current && (*current)->size < s){
    current = &(*current)->next;
  }

  // remove the memory from the list if found
  free_list_elm* block = 0;
  if(current && *current){
    // the next pointer is the blokc we want (the memory that is large enough)
    block = *current;
    *current = (*current)->next;
  }
  return block;
}

// Adds an element to the free list
// Coalesces adjacent blcks
// Keeps the list sorted by memory address of the blocks
// Analogous to addMoreSpace from A5
static free_list_elm* addFreeList(free_list_elm* start, free_list_elm* toAdd){
  // No starting list, no problem
  if(!start){
    toAdd->next = 0;
    return toAdd;
  }

  // Free list is sorted, so position matters
  // toAdd > start, add toAdd to next in line
  // toAdd < start, coalesce
  if(start < toAdd){
    // add toAdd to the next in line element
    start->next = addFreeList(start->next, toAdd);

    // Coalesce the elements of the free list if you can
    char* end = ((char*)start + start->size);
    if(start->next && end >= (char*)start->next) {
      start->size += start->next->size;
      start->next = start->next->next;
    }
    return start;
  } else {
    // Coalesce the elements if possible
    // OR add start to toAdd
    char* end = (char*)toAdd + toAdd->size;
    if(end >= (char*)start) {
      toAdd->size += start->size;
      toAdd->next = start->next;
    } else {
      // add start to toAdd
      toAdd->next = start;
    }
    return toAdd;
  }
}

// Cuts up the request
// Use when the request is LARGER than one page, because this will tell you how many pages to allocate
static size_t divide(size_t x, size_t y){
  // pretty simple division and remainder math
  size_t z = x / y;

  if(z * y == x){
    return z;
  } else {
    return z + 1;
  }
}

// Implementation of malloc using mmap and threads
void *mymalloc(size_t s){
  s += sizeof(size_t);
  // make sure block is smaller than a free list elment
  if(s < sizeof(free_list_elm)){
    s = sizeof(free_list_elm);
  }
  // malloc value
  free_list_elm* allocated = 0;

  // is memory block smaller than a page?
  if(s < PAGE_SIZE) {
    // Case 2 from A5
    // is there space in our free list?
    pthread_mutex_lock(&list_lock);
    free_list_elm* memoryBlock = findFreeSize(s);
    pthread_mutex_unlock(&list_lock);

    // was a space found?
    if(!memoryBlock){
      // not found, use mmap
      memoryBlock = mmap(0, PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
      memoryBlock->size = PAGE_SIZE;
    }

    // new case: Case 3: the requested space is too much for the current element of the free list
    // block too big, the extra space becomes a new free list element
    long left_over = memoryBlock->size - (s + sizeof(free_list_elm));
    if(left_over >= 0){
      // leftovers go into the free list
      free_list_elm* extraBlock = (free_list_elm*)((char*)memoryBlock + s);
      extraBlock->size = memoryBlock->size - s;
      // add to free list
      pthread_mutex_lock(&list_lock);
      free_list = addFreeList(free_list, extraBlock);
      pthread_mutex_unlock(&list_lock);

      // update size of the block here
      memoryBlock->size = s;
    }
    // block is now AT LEAST the right size, if not larger
    *(size_t*)memoryBlock = memoryBlock->size;
    allocated = (void*)((char*)memoryBlock + sizeof(size_t));
  } else {
    // Case 1 from A5
    // block is larger than a page, divide and find number of pages needed
    int splitPages = divide(s, PAGE_SIZE);

    // mmap call
    char* block = mmap(0, splitPages * PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    *((size_t*)block) = splitPages * PAGE_SIZE;
    allocated = (void*)(block + sizeof(size_t));
  }
  // returnv alue
  debug_printf("Malloc %zu bytes\n", s);
  return allocated;
}


void *mycalloc(size_t nmemb, size_t s) {
  // gets size of call
  size_t mallocSize = nmemb * s;

  //  pthread_mutex_lock(&list_lock);
  // gets memory
  void *p = mymalloc(mallocSize);
  //  pthread_mutex_unlock(&list_lock);
  
  // sets all memory from malloc to 0
  memset(p, 0, mallocSize);

  
  if(!p){
    debug_printf("Calloc failed, out of memory");
    // do something here, memory overflow
    return;
  }
  
  debug_printf("calloc %zu bytes\n", s);
  return p; 
}

// Frees memory without using free() (now uses munmap()) (now uses threads)
void myfree(void* ptr){
  // does the pointer exist?
  if(!ptr){
    debug_printf("Tried to free memory from a location that doesn't exist.");
    return;
  }

  free_list_elm* freeMe = (free_list_elm*)((char*)(ptr) - sizeof(size_t));
  // smaller than a page, use free_list (threads)
  if(freeMe->size < PAGE_SIZE) {
    pthread_mutex_lock(&list_lock);
    free_list = addFreeList(free_list, freeMe);
    pthread_mutex_unlock(&list_lock);
  } else {
    // larger than a page, use munmap
    munmap(ptr, freeMe->size);
  }

  debug_printf("Freed %zu bytes\n", freeMe->size);
}

