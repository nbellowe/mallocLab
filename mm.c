/*
 * mm-implicit.c -  Simple allocator based on implicit free lists,
 *                  first fit placement, and boundary tag coalescing.
 *
 * Each block has header and footer of the form:
 *
 *      31                     3  2  1  0
 *      -----------------------------------
 *     | s  s  s  s  ... s  s  s  0  0  a/f
 *      -----------------------------------
 *
 * where s are the meaningful size bits and a/f is set
 * iff the block is allocated. The list has the following form:
 *
 * begin                                                          end
 * heap                                                           heap
 *  -----------------------------------------------------------------
 * |  pad   | hdr(8:a) | ftr(8:a) | zero or more usr blks | hdr(8:a) |
 *  -----------------------------------------------------------------
 *          |       prologue      |                       | epilogue |
 *          |         block       |                       | block    |
 *
 * The allocated prologue and epilogue blocks are overhead that
 * eliminate edge conditions during coalescing.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory.h>
#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
  /* Team name */
  "Team Awesome",
  /* First member's full name */
  "Nathan Bellowe",
  /* First member's email address */
  "Nathan.Bellowe@colorado.edu",
  /* Second member's full name (leave blank if none) */
  "Sarah Niemeyer",
  /* Second member's email address (leave blank if none) */
  "Sarah.Niemeyer@colorado.edu"
};

/////////////////////////////////////////////////////////////////////////////
// Constants and macros
//
// These correspond to the material in Figure 9.43 of the text
// The macros have been turned into C++ inline functions to
// make debugging code easier.
//
/////////////////////////////////////////////////////////////////////////////
#define WSIZE       4       /* word size (bytes) */
#define DSIZE       8       /* doubleword size (bytes) */
#define CHUNKSIZE  (1<<12)  /* initial heap size (bytes) */
#define OVERHEAD    8       /* overhead of header and footer (bytes) */

static inline int MAX(int x, int y) {
  return x > y ? x : y;
}

//
// Pack a size and allocated bit into a word
// We mask of the "alloc" field to insure only
// the lower bit is used
//
static inline size_t PACK(size_t size, int alloc) {
  return ((size) | (alloc & 0x1));
}

//
// Read and write a word at address p
//
static inline size_t GET(void *p) { return  *(size_t *)p; }
static inline void PUT( void *p, size_t val)
{
  *((size_t *)p) = val;
}

//
// Read the size and allocated fields from address p
//
static inline size_t GET_SIZE( void *p )  {
  return GET(p) & ~0x7;
}

static inline int GET_ALLOC( void *p  ) {
  return GET(p) & 0x1;
}

//
// Given block ptr bp, compute address of its header and footer
//
static inline void *HDRP(void *bp) {

  return ( (char *)bp) - WSIZE;
}
static inline void *FTRP(void *bp) {
  return ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE);
}

//
// Given block ptr bp, compute address of next and previous blocks
//
static inline void *NEXT_BLKP(void *bp) {
  return  ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)));
}

static inline void* PREV_BLKP(void *bp){
  return  ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)));
}

/////////////////////////////////////////////////////////////////////////////
//
// Global Variables
//

static char *heap_listp;  /* pointer to first block */

//
// function prototypes for internal helper routines
//
static void *extend_heap(size_t words);
static void place(void *bp, size_t asize);
static void *find_fit(size_t asize);
static void *coalesce(void *bp);
static void printblock(void *bp);
static void checkblock(void *bp);

//
// mm_init - Initialize the memory manager
//
int mm_init(void)
{
  //
  // You need to provide this
  //
  //initialize heap
  heap_listp = mem_sbrk(4*WSIZE);
  //error handling
  if(heap_listp == (void*)-1)
  {
    printf("MSBRK threw an error \n");
    return -1;
  }
  //padding block
  PUT(heap_listp, 0);
  //Prologue Header
  //8 = size of header + size of footer
  //0 = not allocated
  PUT(heap_listp + (1*WSIZE), PACK(8, 1));
  //Prologue footer
  //8 = size of header + size of footer
  //0 = not allocated
  PUT(heap_listp + (2*WSIZE), PACK(8, 1));
  //Epilogue header
  PUT(heap_listp + (3*WSIZE), PACK(0, 1));

  heap_listp += (2*WSIZE);

  //extend the heap!
  extend_heap(CHUNKSIZE/WSIZE);
  return 0;
}


//
// extend_heap - Extend heap with free block and return its block pointer
//
static void *extend_heap(size_t words)
{
  //
  // You need to provide this
  //
  //Extend heap by words (check first to make sure multiple of two)
  if(words%2 == 1)
  {
    words++;
  }
  words = words * WSIZE;

  //Use mem_sbrk(desired_size) to get pointer to something.
  char* bptr = mem_sbrk(words);

  //make block header
  PUT(HDRP(bptr), PACK(words, 0));
  //make block footer
  PUT(FTRP(bptr), PACK(words, 0));
  //make epilogue
  PUT(HDRP(NEXT_BLKP(bptr)), PACK(0, 1));
  //coalesce
  return coalesce(bptr);
}


//
// Practice problem 9.8
//
// find_fit - Find a fit for a block with asize bytes
//
static void *find_fit(size_t asize)
{
  char* roverptr;
  //go through from start of heap to end of heap, end when find
  //unallocated block of size >= asize
  for(roverptr = heap_listp; GET_SIZE(HDRP(roverptr)) > 0; roverptr = NEXT_BLKP(roverptr))
  {
    if( (GET_SIZE(HDRP(roverptr)) >= asize) && !GET_ALLOC(HDRP(roverptr)))
      return roverptr;
  }
  return NULL; /* no fit */

}

//
// mm_free - Free a block
//
void mm_free(void *bp)
{
  //
  // You need to provide this
  //
  //first check if there is any memory free up
  if(heap_listp == 0)
  {
    printf("OH SHIT! NOT INITIALIZED!\n");
    mm_init();
  }
  //put a new footer and ptr without allocated bits at hdrp and ftrp
  PUT(HDRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
  PUT(FTRP(bp), PACK(GET_SIZE(HDRP(bp)), 0));
  coalesce(bp);
}

//
// coalesce - boundary tag coalescing. Return ptr to coalesced block
//
static void *coalesce(void *bp)
{      //get the last previous and next blocks.
  int pb = GET_ALLOC(FTRP(PREV_BLKP(bp)));
  int nb = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
  int currsize = GET_SIZE(HDRP(bp));
  int nextsize = GET_SIZE(HDRP(NEXT_BLKP(bp)));
  int prevsize = GET_SIZE(FTRP(PREV_BLKP(bp)));
  //If both are allocated do nothing.
  if(pb && nb);
  //Iff next is unallocated, change header of current and footer of next,
  else if(pb && !nb)
  {
    PUT(FTRP(NEXT_BLKP(bp)), PACK((currsize + nextsize), 0));
    PUT(HDRP(bp), PACK((currsize + nextsize), 0));
  }
  //Iff next is unallocated change header of prev and footer of current to size of current block + prev block
  else if(!pb && nb)
  {
    PUT(HDRP(PREV_BLKP(bp)), PACK((currsize + prevsize), 0));
    PUT(FTRP(bp), PACK((currsize + prevsize), 0));
  }
  //Iff both, change header of prev, footer of next
  else
  {
    PUT(FTRP(NEXT_BLKP(bp)), PACK((currsize + nextsize + prevsize), 0));
    PUT(HDRP(PREV_BLKP(bp)), PACK((currsize + nextsize + prevsize), 0));
  }
  return bp;

}


//
// mm_malloc - Allocate a block with at least bytes of payload
//
void *mm_malloc(size_t size)
{
  if (size <= DSIZE) size = 2*DSIZE; //minimum size
  else {
      size = DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
      /*int two = (size % 4) != 0 ? size + DSIZE : DSIZE * ((size + (DSIZE) + (DSIZE-1)) / DSIZE);
      if( one != two) {
          printf("not equal: size %d, first %d, second %d\n", size, one, two);
      }
      else{
          printf(" equal size %d\n", size);
      }
      size = one;*/
  }
  char* location = find_fit(size);
  if(location == NULL)
  {
    extend_heap(size/WSIZE);
    location = find_fit(size);
    if(location == NULL)
      printf("Can't find location even after extending");
  }
  place(location, size);
  return location;
  //adjust size to be aligned plus + size of the header/ftr (SIZE_T_SIZE)
  //find fit for size, placing it if found.
  //If not found, extendheap to be bigger, then place.
}

//
//
// Practice problem 9.9
//
// place - Place block of asize bytes at start of free block bp
//         and split if remainder would be at least minimum block size
//
static void place(void *bp, size_t asize)
{
  //if the block at bp is 'too big' split it into two blocks, assign the extra to be unallocated.
  //otherwise, just adjust bp's header and footer
  int initialsize = GET_SIZE(HDRP(bp));
  if(initialsize > asize + 2*DSIZE)
  {
    PUT(HDRP(bp), PACK(asize, 1));
    PUT(FTRP(bp), PACK(asize, 1));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(initialsize - asize, 0));
    PUT(FTRP(NEXT_BLKP(bp)), PACK(initialsize - asize, 0));
  }
  else
  {
    PUT(HDRP(bp), PACK(initialsize, 1));
    PUT(FTRP(bp), PACK(initialsize, 1));
  }
}


//
// mm_realloc -- implemented for you
//
void *mm_realloc(void *ptr, size_t size)
{
  void *newp;
  size_t copySize;

  newp = mm_malloc(size);
  if (newp == NULL) {
    printf("ERROR: mm_malloc failed in mm_realloc\n");
    exit(1);
  }
  copySize = GET_SIZE(HDRP(ptr));
  if (size < copySize) {
    copySize = size;
  }
  memcpy(newp, ptr, copySize);
  mm_free(ptr);
  return newp;
}

//
// mm_checkheap - Check the heap for consistency
//
void mm_checkheap(int verbose)
{
  //
  // This provided implementation assumes you're using the structure
  // of the sample solution in the text. If not, omit this code
  // and provide your own mm_checkheap
  //
  void *bp = heap_listp;

  if (verbose) {
    printf("Heap (%p):\n", heap_listp);
  }

  if ((GET_SIZE(HDRP(heap_listp)) != DSIZE) || !GET_ALLOC(HDRP(heap_listp))) {
    printf("Bad prologue header\n");
  }
  checkblock(heap_listp);

  for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
    if (verbose)  {
      printblock(bp);
    }
    checkblock(bp);
  }

  if (verbose) {
    printblock(bp);
  }

  if ((GET_SIZE(HDRP(bp)) != 0) || !(GET_ALLOC(HDRP(bp)))) {
    printf("Bad epilogue header\n");
  }
}

static void printblock(void *bp)
{
  size_t hsize, halloc, fsize, falloc;

  hsize = GET_SIZE(HDRP(bp));
  halloc = GET_ALLOC(HDRP(bp));
  fsize = GET_SIZE(FTRP(bp));
  falloc = GET_ALLOC(FTRP(bp));

  if (hsize == 0) {
    printf("%p: EOL\n", bp);
    return;
  }

  printf("%p: header: [%d:%c] footer: [%d:%c]\n",
    bp,
    (int) hsize, (halloc ? 'a' : 'f'),
    (int) fsize, (falloc ? 'a' : 'f'));
}

static void checkblock(void *bp)
{
  if ((size_t)bp % 8) {
    printf("Error: %p is not doubleword aligned\n", bp);
  }
  if (GET(HDRP(bp)) != GET(FTRP(bp))) {
    printf("Error: header does not match footer\n");
  }
}
