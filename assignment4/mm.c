/*-------------------------------------------------------------------                             
 * Lab 5 Starter code:                                                                            
 *        single doubly-linked free block list with LIFO policy                                   
 *        with support for coalescing adjacent free blocks  
 *
 * Terminology:
 * o We will implement an explicit free list allocator
 * o We use "next" and "previous" to refer to blocks as ordered in
 *   the free list.
 * o We use "following" and "preceding" to refer to adjacent blocks
 *   in memory.
 *-------------------------------------------------------------------- */


/*
   Group Members:
   Bradley Zylstra
   Ben Short
   
   We have implemented mm_realloc()
*/

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <stdint.h>
#include "memlib.h"
#include "mm.h"


/* Macros for unscaled pointer arithmetic to keep other code cleaner.  
   Casting to a char* has the effect that pointer arithmetic happens at
   the byte granularity (i.e. POINTER_ADD(0x1, 1) would be 0x2).  (By
   default, incrementing a pointer in C has the effect of incrementing
   it by the size of the type to which it points (e.g. BlockInfo).)
   We cast the result to void* to force you to cast back to the 
   appropriate type and ensure you don't accidentally use the resulting
   pointer as a char* implicitly.  You are welcome to implement your
   own pointer arithmetic instead of using these macros.
*/
#define UNSCALED_POINTER_ADD(p,x) ((void*)((char*)(p) + (x)))
#define UNSCALED_POINTER_SUB(p,x) ((void*)((char*)(p) - (x)))


/******** FREE LIST IMPLEMENTATION ***********************************/


/* A BlockInfo contains information about a block, including the size
   and usage tags, as well as pointers to the next and previous blocks
   in the free list.  This is exactly the "explicit free list" structure
   illustrated in the lecture slides.
   
   Note that the next and prev pointers and the boundary tag are only
   needed when the block is free.  To achieve better utilization, mm_malloc
   should use the space for next and prev as part of the space it returns.

   +--------------+
   | sizeAndTags  |  <-  BlockInfo pointers in free list point here
   |  (header)    |
   +--------------+
   |     next     |  <-  Pointers returned by mm_malloc point here
   +--------------+
   |     prev     |
   +--------------+
   |  space and   |
   |   padding    |
   |     ...      |
   |     ...      |
   +--------------+
   | boundary tag |
   |  (footer)    |
   +--------------+
*/
struct BlockInfo {
  // Size of the block (in the high bits) and tags for whether the
  // block and its predecessor in memory are in use.  See the SIZE()
  // and TAG macros, below, for more details.
  size_t sizeAndTags;
  // Pointer to the next block in the free list.
  struct BlockInfo* next;
  // Pointer to the previous block in the free list.
  struct BlockInfo* prev;
};
typedef struct BlockInfo BlockInfo;


// Global Variable to keep track of when coalescing has occured for deferred scheme
int coal = 0;

// Global Variable to set whether or not realloc is being used, in which case switch
// to best fit free block searching
int mall = 0;

// Unused global pointer for nextFit approach. Was going to convert to void* but set to BlockInfo* for debug
BlockInfo* nextSearch;


/* Pointer to the first BlockInfo in the free list, the list's head. 
   
   A pointer to the head of the free list in this implementation is
   always stored in the first word in the heap.  mem_heap_lo() returns
   a pointer to the first word in the heap, so we cast the result of
   mem_heap_lo() to a BlockInfo** (a pointer to a pointer to
   BlockInfo) and dereference this to get a pointer to the first
   BlockInfo in the free list. */
#define FREE_LIST_HEAD *((BlockInfo **)mem_heap_lo())



/* Size of a word on this architecture. */
#define WORD_SIZE sizeof(void*)

/* Minimum block size (to account for size header, next ptr, prev ptr,
   and boundary tag) */
#define MIN_BLOCK_SIZE (sizeof(BlockInfo) + WORD_SIZE)

/* Alignment of blocks returned by mm_malloc. */
#define ALIGNMENT 8

/* SIZE(blockInfo->sizeAndTags) extracts the size of a 'sizeAndTags' field.
   Also, calling SIZE(size) selects just the higher bits of 'size' to ensure
   that 'size' is properly aligned.  We align 'size' so we can use the low
   bits of the sizeAndTags field to tag a block as free/used, etc, like this:
   
      sizeAndTags:
      +-------------------------------------------+
      | 31 | 30 | 29 | 28 |  . . . .  | 2 | 1 | 0 |
      +-------------------------------------------+
        ^                                       ^
      high bit                               low bit

   Since ALIGNMENT == 8, we reserve the low 3 bits of sizeAndTags for tag
   bits, and we use bits 3-63 to store the size.

   Bit 0 (2^0 == 1): TAG_USED
   Bit 1 (2^1 == 2): TAG_PRECEDING_USED
*/
#define SIZE(x) ((x) & ~(ALIGNMENT - 1))

/* TAG_USED is the bit mask used in sizeAndTags to mark a block as used. */
#define TAG_USED 1 

/* TAG_PRECEDING_USED is the bit mask used in sizeAndTags to indicate
   that the block preceding it in memory is used. (used in turn for
   coalescing).  If the previous block is not used, we can learn the size
   of the previous block from its boundary tag */
#define TAG_PRECEDING_USED 2


/* Find a free block of the requested size in the free list.  Returns
   NULL if no free block is large enough. */
static void * searchFreeList(size_t reqSize) {   
  BlockInfo* freeBlock;

  freeBlock = FREE_LIST_HEAD;
  while (freeBlock != NULL){
    if (SIZE(freeBlock->sizeAndTags) >= reqSize) {
      return freeBlock;
    } else {
      freeBlock = freeBlock->next;
    }
  }
  return NULL;
}

static void * searchFreeListContinue(size_t reqSize) {

  /*
  NOT USED
  This implements a next fit algorithm. It continues the search where the old search left off.
  The next search approach has many problems with it because if the left off block is coalesced 
  the algorithm will break. This could be fixed by adding code into coalesce block, but first fit
  is already max efficient for our test cases so this approach will not offer any benefit
  */
  BlockInfo* freeBlock;
  BlockInfo* startBlock;
  if(nextSearch==NULL){
    freeBlock = FREE_LIST_HEAD;
    startBlock = NULL;
  }
  else
    freeBlock = nextSearch;
    startBlock = nextSearch->prev;
  while (freeBlock != startBlock){
    if(freeBlock==NULL)
      if(startBlock==FREE_LIST_HEAD)
         return NULL;
      freeBlock = FREE_LIST_HEAD;
    nextSearch = freeBlock;
    if (SIZE(freeBlock->sizeAndTags) >= reqSize) {
      return freeBlock;
    } else {
      freeBlock = freeBlock->next;
    }
  }
  nextSearch = FREE_LIST_HEAD;
  return NULL;
}

static void* searchFreeListBestFit(size_t reqSize) {
	/*
	USED FOR REALLOC CASE
	Searches the free list for best fit block. Simply keeps track of the best size value and returns
	the block of the best size found, and if a block of exact size is found then return immediately
	Some work was done with expanding the leeway for what constitutes an exact match for performance
	concerns, but this had marginal improvements while causing increased fragmentation
	*/
	BlockInfo* freeBlock;
	BlockInfo* bestBlock = NULL;
	size_t best_size = SIZE_MAX;

	freeBlock = FREE_LIST_HEAD;
	while (freeBlock != NULL) {
		size_t current_size = SIZE(freeBlock->sizeAndTags);
		if (current_size >= reqSize && current_size<best_size) {
			bestBlock = freeBlock;
			best_size = current_size;
			if (current_size == reqSize)
				return bestBlock;

		}
		freeBlock = freeBlock->next;
	}
	return bestBlock;
}

/* Insert freeBlock at the head of the list.  (LIFO) */
static void insertFreeBlock(BlockInfo* freeBlock) {
  
  BlockInfo* oldHead = FREE_LIST_HEAD;
  freeBlock->next = oldHead;
  if (oldHead != NULL) {
    oldHead->prev = freeBlock;
  }
  //freeBlock->prev = NULL;
  FREE_LIST_HEAD = freeBlock;
  
    /*
    This code inserts into the free list based on memory size
    The end result is similar to the best fit block selector
    but with worse performance because insertFreeBlock is called
    more often than searchFreeBlock.
    */
    // Code for inserting based on memory size
	/*
	freeBlock->next = NULL;
	freeBlock->prev = NULL;
	// Places elements based on their memory size
	BlockInfo* oldHead = FREE_LIST_HEAD;
	// Case where there is no freeblocks
	if (!oldHead) {
		freeBlock->prev = NULL;
		FREE_LIST_HEAD = freeBlock;
		return;
	}


	size_t free_size = SIZE(freeBlock->sizeAndTags);
	int i = 0;
	while (oldHead->next) {
		if(free_size <= SIZE(oldHead->sizeAndTags))
			break;
		i++;
		oldHead = oldHead->next;
	}
	BlockInfo* prior = oldHead->prev;
	
	if (prior) {
		prior->next = freeBlock;
		freeBlock->prev = prior;
	}
	// Case where we are inserting at beginning of list
	else {
		freeBlock->prev = NULL;
		FREE_LIST_HEAD = freeBlock;
	}
	// Case where we are inserting at end of list
	oldHead->prev = freeBlock;
	freeBlock->next = oldHead;
	*/

	

}      

/* Remove a free block from the free list. */
static void removeFreeBlock(BlockInfo* freeBlock) {

  BlockInfo *nextFree, *prevFree;
  
  nextFree = freeBlock->next;
  prevFree = freeBlock->prev;

  // If the next block is not null, patch its prev pointer.
  if (nextFree != NULL) {
    nextFree->prev = prevFree;
  }

  // If we're removing the head of the free list, set the head to be
  // the next block, otherwise patch the previous block's next pointer.
  if (freeBlock == FREE_LIST_HEAD) {
    FREE_LIST_HEAD = nextFree;
  } else {
    prevFree->next = nextFree;
  }
  if(freeBlock == nextSearch)
    nextSearch = freeBlock->next;
}

/* Coalesce 'oldBlock' with any preceeding or following free blocks. */
static void coalesceFreeBlock(BlockInfo* oldBlock) {
  BlockInfo *blockCursor;
  BlockInfo *newBlock;
  BlockInfo *freeBlock;
  // size of old block
  size_t oldSize = SIZE(oldBlock->sizeAndTags);
  // running sum to be size of final coalesced block
  size_t newSize = oldSize;

  // Coalesce with any preceding free block
  blockCursor = oldBlock;
  while ((blockCursor->sizeAndTags & TAG_PRECEDING_USED)==0) { 
    // While the block preceding this one in memory (not the
    // prev. block in the free list) is free:

    // Get the size of the previous block from its boundary tag.
    size_t size = SIZE(*((size_t*)UNSCALED_POINTER_SUB(blockCursor, WORD_SIZE)));
    // Use this size to find the block info for that block.
    freeBlock = (BlockInfo*)UNSCALED_POINTER_SUB(blockCursor, size);
    // Remove that block from free list.
    removeFreeBlock(freeBlock);

    // Count that block's size and update the current block pointer.
    newSize += size;
    blockCursor = freeBlock;
  }
  newBlock = blockCursor;

  // Coalesce with any following free block.
  // Start with the block following this one in memory
  blockCursor = (BlockInfo*)UNSCALED_POINTER_ADD(oldBlock, oldSize);
  while ((blockCursor->sizeAndTags & TAG_USED)==0) {
    // While the block is free:

    size_t size = SIZE(blockCursor->sizeAndTags);
    // Remove it from the free list.
    removeFreeBlock(blockCursor);
    // Count its size and step to the following block.
    newSize += size;
    blockCursor = (BlockInfo*)UNSCALED_POINTER_ADD(blockCursor, size);
  }
  
  // If the block actually grew, remove the old entry from the free
  // list and add the new entry.
  if (newSize != oldSize) {
    // Remove the original block from the free list
    removeFreeBlock(oldBlock);

    // Set coal = 1 for deferred coalescing scheme
    coal = 1;

    // Save the new size in the block info and in the boundary tag
    // and tag it to show the preceding block is used (otherwise, it
    // would have become part of this one!).
    newBlock->sizeAndTags = newSize | TAG_PRECEDING_USED;
    // The boundary tag of the preceding block is the word immediately
    // preceding block in memory where we left off advancing blockCursor.
    *(size_t*)UNSCALED_POINTER_SUB(blockCursor, WORD_SIZE) = newSize | TAG_PRECEDING_USED;  

    // Put the new block in the free list.
    insertFreeBlock(newBlock);
  }
  return;
}

void* coalesce_all() {
	/*
	NOT USED
	This implements a deferred coalesing scheme. Works by iterating through every 
	block and coalesing. If coalescing did occur, since there is no way to determine the
	next block, start from the beginning of the free list. Performance could be improved by
	having coalesceFreeBlock return a pointer to the new block after coalescing.
	*/
	BlockInfo* oldHead = FREE_LIST_HEAD;
	if (!oldHead) {
		return;
	}

	BlockInfo* nextBlock = oldHead->next;
	while (nextBlock) {
		coalesceFreeBlock(oldHead);
		if (coal) {
			coal = 0;
			coalesce_all();
			return;
		}
		oldHead = nextBlock;
		nextBlock = oldHead->next;
	}

}

/* Get more heap space of size at least reqSize. */
static void requestMoreSpace(size_t reqSize) {
  size_t pagesize = mem_pagesize();
  size_t numPages = (reqSize + pagesize - 1) / pagesize;
  BlockInfo *newBlock;
  size_t totalSize = numPages * pagesize;
  size_t prevLastWordMask;

  void* mem_sbrk_result = mem_sbrk(totalSize);
  if ((size_t)mem_sbrk_result == -1) {
    printf("ERROR: mem_sbrk failed in requestMoreSpace\n");
    exit(0);
  }
  newBlock = (BlockInfo*)UNSCALED_POINTER_SUB(mem_sbrk_result, WORD_SIZE);

  /* initialize header, inherit TAG_PRECEDING_USED status from the
     previously useless last word however, reset the fake TAG_USED
     bit */
  prevLastWordMask = newBlock->sizeAndTags & TAG_PRECEDING_USED;
  newBlock->sizeAndTags = totalSize | prevLastWordMask;
  // Initialize boundary tag.
  ((BlockInfo*)UNSCALED_POINTER_ADD(newBlock, totalSize - WORD_SIZE))->sizeAndTags = 
    totalSize | prevLastWordMask;

  /* initialize "new" useless last word
     the previous block is free at this moment
     but this word is useless, so its use bit is set
     This trick lets us do the "normal" check even at the end of
     the heap and avoid a special check to see if the following
     block is the end of the heap... */
  *((size_t*)UNSCALED_POINTER_ADD(newBlock, totalSize)) = TAG_USED;

  // Add the new block to the free list and immediately coalesce newly
  // allocated memory space
  insertFreeBlock(newBlock);
  coalesceFreeBlock(newBlock);
}

/* Print the heap by iterating through it as an implicit free list. */
static void examine_heap() {
  BlockInfo *block;

  /* print to stderr so output isn't buffered and not output if we crash */
  fprintf(stderr, "FREE_LIST_HEAD: %p\n", (void *)FREE_LIST_HEAD);

  for (block = (BlockInfo *)UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE); /* first block on hea\
									       p */
       SIZE(block->sizeAndTags) != 0 && (void*)block < (void*)mem_heap_hi();
       block = (BlockInfo *)UNSCALED_POINTER_ADD(block, SIZE(block->sizeAndTags))) {

    /* print out common block attributes */
    fprintf(stderr, "%p: %ld %ld %ld\t",
            (void *)block,
            SIZE(block->sizeAndTags),
            block->sizeAndTags & TAG_PRECEDING_USED,
            block->sizeAndTags & TAG_USED);

    /* and allocated/free specific data */
    if (block->sizeAndTags & TAG_USED) {
      fprintf(stderr, "ALLOCATED\n");
    } else {
      fprintf(stderr, "FREE\tnext: %p, prev: %p\n",
              (void *)block->next,
              (void *)block->prev);
    }
  }
  fprintf(stderr, "END OF HEAP\n\n");
}

/* Initialize the allocator. */
int mm_init () {
  // Head of the free list.
  BlockInfo *firstFreeBlock;

  // Initial heap size: WORD_SIZE byte heap-header (stores pointer to head
  // of free list), MIN_BLOCK_SIZE bytes of space, WORD_SIZE byte heap-footer.
  size_t initSize = WORD_SIZE+MIN_BLOCK_SIZE+WORD_SIZE;
  size_t totalSize;

  void* mem_sbrk_result = mem_sbrk(initSize);
  //  printf("mem_sbrk returned %p\n", mem_sbrk_result);
  if ((size_t)mem_sbrk_result == -1) {
    printf("ERROR: mem_sbrk failed in mm_init, returning %p\n", 
           mem_sbrk_result);
    exit(1);
  }

  firstFreeBlock = (BlockInfo*)UNSCALED_POINTER_ADD(mem_heap_lo(), WORD_SIZE);

  // Total usable size is full size minus heap-header and heap-footer words
  // NOTE: These are different than the "header" and "footer" of a block!
  // The heap-header is a pointer to the first free block in the free list.
  // The heap-footer is used to keep the data structures consistent (see
  // requestMoreSpace() for more info, but you should be able to ignore it).
  totalSize = initSize - WORD_SIZE - WORD_SIZE;

  // The heap starts with one free block, which we initialize now.
  firstFreeBlock->sizeAndTags = totalSize | TAG_PRECEDING_USED;
  firstFreeBlock->next = NULL;
  firstFreeBlock->prev = NULL;
  // boundary tag
  *((size_t*)UNSCALED_POINTER_ADD(firstFreeBlock, totalSize - WORD_SIZE)) = totalSize | TAG_PRECEDING_USED;
  
  // Tag "useless" word at end of heap as used.
  // This is the is the heap-footer.
  *((size_t*)UNSCALED_POINTER_SUB(mem_heap_hi(), WORD_SIZE - 1)) = TAG_USED;

  // set the head of the free list to this new free block.
  FREE_LIST_HEAD = firstFreeBlock;
  return 0;
}



// TOP-LEVEL ALLOCATOR INTERFACE ------------------------------------
void* mm_malloc (size_t size) {
  /*
  Create a free block of given size and return a pointer to it. 
  After much testing, we decided to use the default allocation scheme which is:
  LIFO free list insertion, Immediate Coalescing, and first fit block selection
  The next best case was best fit block selection, but it only marginally improved utilization
  while suffering massively in performance
  */
  size_t reqSize;
  BlockInfo * ptrFreeBlock = NULL;
  size_t blockSize;
  size_t precedingBlockUseTag;

  // Zero-size requests get NULL.
  if (size == 0) {
    return NULL;
  }

  // Add one word for the initial size header.
  // Note that we don't need to boundary tag when the block is used!
  size += WORD_SIZE;
  if (size <= MIN_BLOCK_SIZE) {
    // Make sure we allocate enough space for a blockInfo in case we
    // free this block (when we free this block, we'll need to use the
    // next pointer, the prev pointer, and the boundary tag).
    reqSize = MIN_BLOCK_SIZE;
  } else {
    // Round up for correct alignment
    reqSize = ALIGNMENT * ((size + ALIGNMENT - 1) / ALIGNMENT);
  }
  
  //search the list for a free block. If using realloc, then switch to best-fit scheme
  void* freeBlock;
  if (mall) {
	  freeBlock = searchFreeListBestFit(reqSize);
  }
  else {
	  
	  freeBlock = searchFreeList(reqSize);

  }
	
  // If free block is not found in current free list
  if(freeBlock == NULL){
	  // if using deferrred scheme, coalesce all blocks if none are found 
	  //coalesce_all();
	  
	  // If no free block is found, increase the heap size by the required size and then get that new block
	  requestMoreSpace(reqSize);
	  freeBlock = searchFreeList(reqSize);

	  // This code will only run during deferred coalescing scheme if not enough space was freed up during coalescing
	  // Since deferred coalescing is not used, this code will never run
	  if (freeBlock == NULL) {
		  requestMoreSpace(reqSize);
		  freeBlock = searchFreeList(reqSize);
	  }
	  
  }

  //cast to a BlockInfo pointer
  ptrFreeBlock = (BlockInfo*) freeBlock; 

  // Save the preceding block use tag in case we need to remake the free block
  precedingBlockUseTag = ptrFreeBlock->sizeAndTags & TAG_PRECEDING_USED;
  
  //remove the block from the list and check its size
  //if the block has extra space greater than or equal to the minimum block size, split into two blocks
  
  removeFreeBlock(ptrFreeBlock);
  if(SIZE(ptrFreeBlock->sizeAndTags) >= (reqSize + MIN_BLOCK_SIZE)){

	  //split remaining space into new free block
	  BlockInfo* newFreeBlock = (BlockInfo*) UNSCALED_POINTER_ADD(ptrFreeBlock, reqSize);
	  newFreeBlock->next = NULL;
	  newFreeBlock->prev = NULL;


	  //add size, tags, footer
	  size_t newBlockSize = SIZE(ptrFreeBlock->sizeAndTags) - reqSize;
	  newFreeBlock->sizeAndTags = newBlockSize | TAG_PRECEDING_USED;
	  *((size_t*) (UNSCALED_POINTER_ADD(newFreeBlock, SIZE(newFreeBlock->sizeAndTags)-WORD_SIZE))) = newBlockSize | TAG_PRECEDING_USED;
	  
	  //add the new, smaller free block back to the list
	  insertFreeBlock(newFreeBlock);
	  coalesceFreeBlock(newFreeBlock);
	  // Set the preceding tag that was previously saved and the new reqSize for the block
	  ptrFreeBlock->sizeAndTags = reqSize | precedingBlockUseTag;
  }
  else {
	  // If the reqSize fits without need for splitting, then just set the preceding tag info for the next block to filled
	  // We dont need to set any header information because the found block is the perfect size
	  BlockInfo* nextBlock = (BlockInfo*)UNSCALED_POINTER_ADD(ptrFreeBlock, SIZE(ptrFreeBlock->sizeAndTags));
	  nextBlock->sizeAndTags = nextBlock->sizeAndTags | TAG_PRECEDING_USED;

  }
  //mark ptrFreeBlock as used
  ptrFreeBlock->sizeAndTags |= TAG_USED;
  //return a pointer to the part of the allocated block after the size and tags
  return UNSCALED_POINTER_ADD(ptrFreeBlock, WORD_SIZE);
}

/* Free the block referenced by ptr. */
void mm_free (void *ptr) {
	/*
	Implements mm_free. Finds the block referred to by PTR(ptr-word_size), sets its header and boundary tags properly and
	the following blocks tag correctly and then adds the block back to the free list.
	
	We ran several algorithms to check if deferred coalescing would increase speeds, but in the test cases provided it is faster
	to coalesce right when a block is freed, so after the block is added to the list coalescing is performed.

	*/
	
	if (!ptr) {
		return NULL;
	}
  	size_t payloadSize;
  	// Get the BlockInfo pointers to this block and the next block
	BlockInfo * blockInfo = (BlockInfo*) UNSCALED_POINTER_SUB(ptr, WORD_SIZE);
  	BlockInfo * followingBlock = (BlockInfo*) UNSCALED_POINTER_ADD(blockInfo, SIZE(blockInfo->sizeAndTags));

  	// Set following block and current block tags to show a free block without changing the other tags
  	blockInfo->sizeAndTags = blockInfo->sizeAndTags & (~TAG_USED);
  	followingBlock->sizeAndTags = followingBlock->sizeAndTags & (~TAG_PRECEDING_USED);
	
	// If following block is free, also update its boundary tag
	// This is only useful for deferred coalescing. If instant, this block will be coalesced later in the function
	if(!(followingBlock->sizeAndTags && TAG_USED)){
		*((size_t*)UNSCALED_POINTER_ADD(followingBlock, SIZE(followingBlock->sizeAndTags) - WORD_SIZE)) = followingBlock->sizeAndTags;
	}
	
	// Set the boundary tag for this block equal to the header
  	*((size_t*)UNSCALED_POINTER_ADD(blockInfo, SIZE(blockInfo->sizeAndTags) - WORD_SIZE)) = blockInfo->sizeAndTags;
	
	// Insert block back into the free list
  	insertFreeBlock(blockInfo);
  	// Either Coalesce Here or when there are no more free spaces
  	coalesceFreeBlock(blockInfo);
}


// Implement a heap consistency checker as needed.
int mm_check() {
  return 0;
}

// Extra credit.
void* mm_realloc(void* ptr, size_t size) {
  /*
	   The realloc() function changes the size of the memory block pointed to by
	   ptr to size bytes.  The contents will be unchanged in the range from the
	   start of the region up to the minimum of the old and new sizes.  If the
	   new size is larger than the old size, the added memory will not be
	   initialized.  If ptr is NULL, then the call is equivalent to
	   malloc(size), for all values of size; if size is equal to zero, and ptr
	   is not NULL, then the call is equivalent to free(ptr).  Unless ptr is
	   NULL, it must have been returned by an earlier call to malloc(), calloc()
	   or realloc().  If the area pointed to was moved, a free(ptr) is done.
	   
	   This implementation checks for 4 different use cases. If size < currentSize
	   that means the block is shrinking. If the requested size leaves no room for a new block,
	   do nothing and just return the old pointer. Otherwise, shrink the currentSize and create a new
	   block using the leftover space
	   
	   If size > current size, then expand the block. If the next block is either not free or not large enough
	   to fit the requested size, then ask for a new block large enough and copy the data over to there. If the
	   next block is large enough to fit the new data, then expand the current block and either split off the remainder 
	   into a new free block or swallow the rest of the space.
  */
  // Set the global variable for malloc to use best-fit, because best-fit has increased performance over first-fit 
  // while using realloc (nearly 5x!)
  mall = 1;
  // if the ptr is null and the size is zero, do nothing
  if (ptr == NULL && size == 0)
	return NULL;
  // If the pointer is null, just call malloc on the size
  if(ptr==NULL)
    return mm_malloc(size);
  // If the size is free, then just call free on the pointer and return nothing
  if(size==0){
    mm_free(ptr);
    return NULL;
  }
	
  // Set the correct size to expand the block by. Must be 8byte aligned!
  size_t reqSize;
  if(size < MIN_BLOCK_SIZE)
    reqSize = MIN_BLOCK_SIZE;
  else
    reqSize = ALIGNMENT * ((size + WORD_SIZE + ALIGNMENT - 1) / ALIGNMENT);

  // Get the block ptr points to and its size
  BlockInfo* blockInfo = (BlockInfo*)UNSCALED_POINTER_SUB(ptr, WORD_SIZE);
  size_t currentSize = SIZE(blockInfo->sizeAndTags);

  // If the current size is equal to the requested size, do nothing
  if(reqSize == currentSize)
    return ptr;

  // Get the pointer for the block following the requested block, and its size as well
  BlockInfo* nextBlock = (BlockInfo*)UNSCALED_POINTER_ADD(blockInfo, currentSize);
  size_t nextSize = SIZE(nextBlock->sizeAndTags);

  // If the next block is used, set nextSize to zero to show there is no available space
  if(nextBlock->sizeAndTags & TAG_USED)
    nextSize = 0;

  // store preceding tag incase we need to change block size
  size_t previousTagInfo = blockInfo->sizeAndTags & TAG_PRECEDING_USED;
	
  // If we are reducing the block size
  if(reqSize < currentSize){
    // If the proposed size is less than the minimum block size, do nothing
    if((currentSize - reqSize) < MIN_BLOCK_SIZE)
      return ptr;

    // Go to next block that will be freed
    BlockInfo* newBlock = (BlockInfo*)UNSCALED_POINTER_ADD(blockInfo, reqSize);
    
    // Set the new block's header and footer info and insert into free block list and then coalesce
    newBlock->sizeAndTags = (currentSize - reqSize) | TAG_PRECEDING_USED;
	newBlock->next = NULL;
	newBlock->prev = NULL;
    *(size_t*)UNSCALED_POINTER_ADD(newBlock,(currentSize-reqSize)-WORD_SIZE) = newBlock->sizeAndTags;
    insertFreeBlock(newBlock);
    coalesceFreeBlock(newBlock);
    
    // set the new Tag info
    blockInfo->sizeAndTags = reqSize | previousTagInfo;
	
    blockInfo->sizeAndTags |= TAG_USED;
    // Just return the original pointer since we didnt move it around
    return ptr;
    

  }
  // Now handle heap expansion case
  else{
    // Case where the size isnt large enough for both
    if(reqSize > (currentSize+nextSize)){
      // Malloc a new block and then copy over the old data to the new block
		
      void* newData = mm_malloc(size);
      size_t* loopData = (size_t*)newData;
      size_t* finalData = (size_t*)UNSCALED_POINTER_ADD(blockInfo, currentSize);
      size_t* oldData = (size_t*)UNSCALED_POINTER_ADD(blockInfo, WORD_SIZE);
	  
      while(oldData != finalData){
        *loopData = *oldData;
        oldData++;
        loopData++;
      }
      // Free the old block of data and return the new pointer
      mm_free(UNSCALED_POINTER_ADD(blockInfo, WORD_SIZE));
      return newData;


      }
     // Case where there is enough space in the next block
    else{
      // Case where size cuts into the next block such that there isnt space for a new block
      if((nextSize - reqSize + currentSize) <  MIN_BLOCK_SIZE){
	// Remove the nextBlock from the free list since we are using it, and change the current blocks's size
	// to account for the new block. Return ptr because we didnt move anything in memory
        removeFreeBlock(nextBlock);
	blockInfo->sizeAndTags = reqSize | TAG_USED;
	blockInfo->sizeAndTags|= previousTagInfo;
        return ptr;
      }
      // Case where next block is big enough and can split into new block
      else{
	// Remove the next block from memory, and expand the current block to the required size.
        removeFreeBlock(nextBlock);
	blockInfo->sizeAndTags = reqSize | TAG_USED;
	blockInfo->sizeAndTags |= previousTagInfo;
        BlockInfo* newBlock = (BlockInfo*)UNSCALED_POINTER_ADD(blockInfo, reqSize);
	// The new block's size is the current and next block sizes minus the required size. Set this new blocks size and 
	// boundary tag
        newBlock->sizeAndTags = (nextSize + currentSize - reqSize) | TAG_PRECEDING_USED;
	newBlock->next = NULL;
	newBlock->prev = NULL;
        *(size_t*)UNSCALED_POINTER_ADD(newBlock, SIZE(newBlock->sizeAndTags) - WORD_SIZE) = newBlock->sizeAndTags;
	// Insert the new block, coalesce, and then return the original pointer because we didnt move it in memory
        insertFreeBlock(newBlock);
	coalesceFreeBlock(newBlock);
        return ptr;
     }
    }

  }
  // This will never be reached
  return NULL;
}
