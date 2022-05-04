/**
 * @file mm.c
 * @brief A 64-bit struct-based implicit free list memory allocator
 *
 * 15-213: Introduction to Computer Systems
 *
 * TODO: insert your documentation here. :)
 *
 *************************************************************************
 *
 * ADVICE FOR STUDENTS.
 * - Step 0: Please read the writeup!
 * - Step 1: Write your heap checker.
 * - Step 2: Write contracts / debugging assert statements.
 * - Good luck, and have fun!
 *
 *************************************************************************
 *
 * @author Shaofeng Qin shaofenq@andrew.cmu.edu
 */

#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "memlib.h"
#include "mm.h"

/* Do not change the following! */

#ifdef DRIVER
/* create aliases for driver tests */
#define malloc mm_malloc
#define free mm_free
#define realloc mm_realloc
#define calloc mm_calloc
#define memset mem_memset
#define memcpy mem_memcpy
#endif /* def DRIVER */

/* You can change anything from here onward */

/*
 *****************************************************************************
 * If DEBUG is defined (such as when running mdriver-dbg), these macros      *
 * are enabled. You can use them to print debugging output and to check      *
 * contracts only in debug mode.                                             *
 *                                                                           *
 * Only debugging macros with names beginning "dbg_" are allowed.            *
 * You may not define any other macros having arguments.                     *
 *****************************************************************************
 */
#ifdef DEBUG
/* When DEBUG is defined, these form aliases to useful functions */
#define dbg_printf(...) printf(__VA_ARGS__)
#define dbg_requires(expr) assert(expr)
#define dbg_assert(expr) assert(expr)
#define dbg_ensures(expr) assert(expr)
#define dbg_printheap(...) print_heap(__VA_ARGS__)
#else
/* When DEBUG is not defined, no code gets generated for these */
/* The sizeof() hack is used to avoid "unused variable" warnings */
#define dbg_printf(...) (sizeof(__VA_ARGS__), -1)
#define dbg_requires(expr) (sizeof(expr), 1)
#define dbg_assert(expr) (sizeof(expr), 1)
#define dbg_ensures(expr) (sizeof(expr), 1)
#define dbg_printheap(...) ((void)sizeof(__VA_ARGS__))
#endif

/* Basic constants */

typedef uint64_t word_t;

/** @brief Word and header size (bytes) */
static const size_t wsize = sizeof(word_t);

/** @brief Double word size (bytes) */
static const size_t dsize = 2 * wsize;

/** @brief Minimum block size (bytes) */
static const size_t min_block_size = 2 * dsize;

/**
 * 
 * (Must be divisible by dsize)
 */
static const size_t chunksize = (1 << 12);

/**
 * TODO: explain what alloc_mask is
 */
static const word_t alloc_mask = 0x1;

/**
 * TODO: explain what size_mask is
 */
static const word_t size_mask = ~(word_t)0xF;

/** @brief Represents the header and payload of one block in the heap */


typedef struct block {
    /** @brief Header contains size + allocation flag */
    word_t header;

    union {
        struct{
            struct block *next;
            struct block *prev;
        };
        char payload[0];
    };
    

    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Why can't we declare the block footer here as part of the struct? we don't know the size of the payload
     * Why do we even have footers -- will the code work fine without them?
     * which functions actually use the data contained in footers? 
     */
} block_t;

/* Global variables */

/** @brief Pointer to first block in the heap */
static block_t *heap_start = NULL;

// pointer to first free block (start of the free list)
static block_t *free_list_start = NULL;

//
word_t *start;
block_t *ptr;
block_t *block;

/*
 *****************************************************************************
 * The functions below are short wrapper functions to perform                *
 * bit manipulation, pointer arithmetic, and other helper operations.        *
 *                                                                           *
 * We've given you the function header comments for the functions below      *
 * to help you understand how this baseline code works.                      *
 *                                                                           *
 * Note that these function header comments are short since the functions    *
 * they are describing are short as well; you will need to provide           *
 * adequate details for the functions that you write yourself!               *
 *****************************************************************************
 */

/*
 * ---------------------------------------------------------------------------
 *                        BEGIN SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/**
 * @brief Returns the maximum of two integers.
 * @param[in] x
 * @param[in] y
 * @return `x` if `x > y`, and `y` otherwise.
 */
static size_t max(size_t x, size_t y) {
    return (x > y) ? x : y;
}

/**
 * @brief Rounds `size` up to next multiple of n
 * @param[in] size
 * @param[in] n
 * @return The size after rounding up
 */
static size_t round_up(size_t size, size_t n) {
    return n * ((size + (n - 1)) / n);
}

/**
 * @brief Packs the `size` and `alloc` of a block into a word suitable for
 *        use as a packed value.
 *
 * Packed values are used for both headers and footers.
 *
 * The allocation status is packed into the lowest bit of the word.
 *
 * @param[in] size The size of the block being represented
 * @param[in] alloc True if the block is allocated
 * @return The packed value
 */
static word_t pack(size_t size, bool alloc) {
    word_t word = size;
    if (alloc) {
        word |= alloc_mask;
    }
    return word;
}

/**
 * @brief Extracts the size represented in a packed word.
 *
 * This function simply clears the lowest 4 bits of the word, as the heap
 * is 16-byte aligned.
 *
 * @param[in] word
 * @return The size of the block represented by the word
 */
static size_t extract_size(word_t word) {
    return (word & size_mask);
}

/**
 * @brief Extracts the size of a block from its header.
 * @param[in] block
 * @return The size of the block
 */
static size_t get_size(block_t *block) {
    return extract_size(block->header);
}

/**
 * @brief Given a payload pointer, returns a pointer to the corresponding
 *        block.
 * @param[in] bp A pointer to a block's payload
 * @return The corresponding block
 */
static block_t *payload_to_header(void *bp) {
    return (block_t *)((char *)bp - offsetof(block_t, payload));
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        payload.
 * @param[in] block
 * @return A pointer to the block's payload
 * @pre The block must be a valid block, not a boundary tag.
 */
static void *header_to_payload(block_t *block) {
    dbg_requires(get_size(block) != 0);
    return (void *)(block->payload);
}

/**
 * @brief Given a block pointer, returns a pointer to the corresponding
 *        footer.
 * @param[in] block
 * @return A pointer to the block's footer
 * @pre The block must be a valid block, not a boundary tag.
 */
static word_t *header_to_footer(block_t *block) {
    dbg_requires(get_size(block) != 0 &&
                 "Called header_to_footer on the epilogue block");
    return (word_t *)(block->payload + get_size(block) - dsize);
}

/**
 * @brief Given a block footer, returns a pointer to the corresponding
 *        header.
 * @param[in] footer A pointer to the block's footer
 * @return A pointer to the start of the block
 * @pre The footer must be the footer of a valid block, not a boundary tag.
 */
static block_t *footer_to_header(word_t *footer) {
    size_t size = extract_size(*footer);
    dbg_assert(size != 0 && "Called footer_to_header on the prologue block");
    return (block_t *)((char *)footer + wsize - size);
}

/**
 * @brief Returns the payload size of a given block.
 *
 * The payload size is equal to the entire block size minus the sizes of the
 * block's header and footer.
 *
 * @param[in] block
 * @return The size of the block's payload
 */
static size_t get_payload_size(block_t *block) {
    size_t asize = get_size(block);
    return asize - dsize;
}

/**
 * @brief Returns the allocation status of a given header value.
 *
 * This is based on the lowest bit of the header value.
 *
 * @param[in] word
 * @return The allocation status correpsonding to the word
 */
static bool extract_alloc(word_t word) {
    return (bool)(word & alloc_mask);
}

/**
 * @brief Returns the allocation status of a block, based on its header.
 * @param[in] block
 require  block is not null
 * @return The allocation status of the block
 */
static bool get_alloc(block_t *block) {
    return extract_alloc(block->header);
}

/**
 * @brief Writes an epilogue header at the given address.
 *
 * The epilogue header has size 0, and is marked as allocated.
 *
 * @param[out] block The location to write the epilogue header
 */
static void write_epilogue(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires((char *)block == mem_heap_hi() - 7);
    block->header = pack(0, true);
}

/**
 * @brief Writes a block starting at the given address.
 *
 * This function writes both a header and footer, where the location of the
 * footer is computed in relation to the header.
 *
 * TODO: Are there any preconditions or postconditions?
 *
 * @param[out] block The location to begin writing the block header
 * @param[in] size The size of the new block
 * @param[in] alloc The allocation status of the new block
 */
static void write_block(block_t *block, size_t size, bool alloc) {
    dbg_requires(block != NULL);
    dbg_requires(size > 0);
    block->header = pack(size, alloc);
    word_t *footerp = header_to_footer(block);
    *footerp = pack(size, alloc);
}

/**
 * @brief Finds the next consecutive block on the heap.
 *
 * This function accesses the next block in the "implicit list" of the heap
 * by adding the size of the block.
 *
 * @param[in] block A block in the heap
 * @return The next consecutive block on the heap
 * @pre The block is not the epilogue
 */
static block_t *find_next(block_t *block) {
    dbg_requires(block != NULL);
    dbg_requires(get_size(block) != 0 &&
                 "Called find_next on the last block in the heap");
    return (block_t *)((char *)block + get_size(block));
}

/**
 * @brief Finds the footer of the previous block on the heap.
 * @param[in] block A block in the heap
 * @return The location of the previous block's footer
 */
static word_t *find_prev_footer(block_t *block) {
    // Compute previous footer position as one word before the header
    return &(block->header) - 1;
}

/**
 * @brief Finds the previous consecutive block on the heap.
 *
 * This is the previous block in the "implicit list" of the heap.
 *
 * If the function is called on the first block in the heap, NULL will be
 * returned, since the first block in the heap has no previous block!
 *
 * The position of the previous block is found by reading the previous
 * block's footer to determine its size, then calculating the start of the
 * previous block based on its size.
 *
 * @param[in] block A block in the heap
 * @return The previous consecutive block in the heap.
 */
static block_t *find_prev(block_t *block) {
    dbg_requires(block != NULL);
    word_t *footerp = find_prev_footer(block);

    // Return NULL if called on first block in the heap
    //if block is the first block, return null
    if (extract_size(*footerp) == 0) {
        return NULL;
    }

    return footer_to_header(footerp);
}

/*
 * ---------------------------------------------------------------------------
 *                        END SHORT HELPER FUNCTIONS
 * ---------------------------------------------------------------------------
 */

/******** The remaining content below are helper and debug routines ********/

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] block
 * @return
 */



// function that adds free block to the beginning of the free list
//pre: a free block pointer
//post: add it to the beiginning of the free list(free_list_start)

static void add_node_LIFO(block_t * block)
{
    
    if (block == NULL)
    {
        return;
    }
    
    else if (free_list_start != NULL)
    {
        block->next = free_list_start;
        block->prev = NULL;
        free_list_start->prev= block;
        free_list_start = block;
    }
    /*
    when the free block list is empty, as we add node when do coalscing, we need to set the pointers(prev/next to be NULL)
    otherwise, the deleting condition for the only one block wll not hold(block->next/prev != NULL) -> cannot successfully delete by our precondition
    */
    else if (free_list_start == NULL)
    {
        free_list_start = block;
        block->next = NULL;
        block->prev = NULL;
    }
    

}

// function that adds free block to the beginning of the free list
//pre: block pointer
//post: delete the block from the free list(once it is allocated)
static void delete_node(block_t* block)
{
    
    //if block is NULL
    if (block == NULL)
    {
        return;
    }
    // if the block is the only block in the free list
    else if ((block->next == NULL)&&(block->prev == NULL))
    {
        free_list_start = NULL;
    }
    // if the block is the first block
    else if ((block->next != NULL)&&(block->prev == NULL))
    {
        free_list_start = block->next;
        block->next->prev = block->prev;
    }
    // if the block is the last block
    else if ((block->prev != NULL)&& (block->next == NULL))
    {
        block->prev->next = NULL;
        block->prev = NULL;
    }
    else
    {
        block->prev->next = block->next;
        block->next->prev = block->prev;
    }

}


//update coalesce funtion for adding/deleting node when free is called 
static block_t *coalesce_block(block_t *block) {
    /*
     * for explicit free lists, we need do make some modification
     on manipulation on nodes(delete and insert)
     */
    bool prev_flag = false;
    if (find_prev(block) != NULL)
    {
        prev_flag = get_alloc(find_prev(block));
    }
    else if (find_prev(block) == NULL)
    {
        prev_flag = true;
    }
    bool next_flag = get_alloc(find_next(block));
    size_t old_size = get_size(block);
    size_t new_size;
    block_t *next_block = find_next(block);
    block_t *prev_block = find_prev(block);
    //case 1: alloc - free -alloc
    if ((prev_flag == true)&&(next_flag== true))
    {
        add_node_LIFO(block);
        return block;
    }
    //case 2: free-free-alloc
    else if ((prev_flag == false)&&(next_flag== true))
    {
        new_size = old_size + get_size(find_prev(block));
        delete_node(prev_block);
        prev_block->header = pack(new_size, false);
        *header_to_footer(block) = pack(new_size, false);
        block = prev_block;
        add_node_LIFO(block);
    }
    //case 3: alloc-free-free
    else if ((prev_flag == true)&&(next_flag== false))
    {
        new_size = old_size + get_size(find_next(block));
        delete_node(next_block);
        block->header = pack(new_size, false);
        *header_to_footer(next_block) = pack(new_size, false);
        add_node_LIFO(block);
        
    }
    //case4: free-free-free
    else
    {
        new_size = old_size + get_size(find_next(block)) + get_size(find_prev(block));
        delete_node(next_block);
        delete_node(prev_block);
        prev_block->header = pack(new_size, false);
        *header_to_footer(next_block) = pack(new_size, false);
        block = prev_block;
        add_node_LIFO(block);
    }
    return block;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] size
 * @return
 */
static block_t *extend_heap(size_t size) {
    void *bp;

    // Allocate an even number of words to maintain alignment
    size = round_up(size, dsize);
    if ((bp = mem_sbrk(size)) == (void *)-1) {
        return NULL;
    }

    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about what bp represents. Why do we write the new block
     * starting one word BEFORE bp, but with the same size that we
     * originally requested?
     */

    // Initialize free block header/footer
    block_t *block = payload_to_header(bp);
    write_block(block, size, false);

    // Create new epilogue header
    block_t *block_next = find_next(block);
    write_epilogue(block_next);

    // Coalesce in case the previous block was free
    block = coalesce_block(block);

    return block;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] block
 * @param[in] asize
 */
static void split_block(block_t *block, size_t asize) {
    dbg_requires(get_alloc(block));
    /* TODO: Can you write a precondition about the value of asize? */
    //asize need less than block size and satisfy alignment(round)
    size_t block_size = get_size(block);

    if ((block_size - asize) >= min_block_size) {
        block_t *block_next;
        delete_node(block);
        write_block(block, asize, true);
        block_next = find_next(block);
        write_block(block_next, block_size - asize, false);
        coalesce_block(block_next);
        // we need to modify the six pointers to update our free list
        /*block->prev->next= block_next;
        block_next->prev = block->prev;
        block_next->next = block->next;
        block->next->prev = block_next;*/
        //prev(block) = NULL;
        //next(block) = NULL;


    }
    else {
        delete_node(block);
    }

    dbg_ensures(get_alloc(block));
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] asize
 * @return
 */

//modify the find_hit function such that only going through free list
//first fit
static block_t *find_fit(size_t asize) {
    block_t *block;
    if (free_list_start == NULL)
    {
        return NULL;
    }

    for (block = free_list_start; block != NULL; block = block->next) {

        if (asize <= get_size(block)) 
        {
            return block;
        }
    }
    return NULL; // no fit found
}




//best fit

/*static block_t *best_fit(size_t asize) {
    block_t *block;

    for (block = free_list_start; (get_size(block) > 0)&&(get_alloc(block) == false); block = find_next(block)) {

        if (asize <= get_size(block)) 
        {
            return block;
        }
    }
    return NULL; // no fit found
}*/






/**
 * @brief
 *
 */
/*
Checking the heap (implicit list, explicit list, segregated list):
– Check for epilogue and prologue blocks.
– Check each block’s address alignment.
– Check blocks lie within heap boundaries.
– Check each block’s header and footer: size (minimum size), previous/next allocate/free
bit consistency, header and footer matching each other.
– Check coalescing: no consecutive free blocks in the heap.


• Checking the free list (explicit list, segregated list):
– All next/previous pointers are consistent (if A’s next pointer points to B, B’s previous
pointer should point to A).
All free list pointers are between mem heap lo() and mem heap high().
– Count free blocks by iterating through every block and traversing free list by pointers
and see if they match.
– All blocks in each list bucket fall within bucket size range (segregated list).
*/
/* input: line number
output: error message and return false if any of the above conditions failed
return true and no any output if all conditions work fine
 *
 * @param[in] line
 * @return
 */
bool mm_checkheap(int line) {
    // first check the heap：
    //check prologue(start)'s size and alloc (start -> word_t *)
    if ((extract_size(start[0]) != 0)||(extract_alloc(start[0]) != true))
    {
        printf("prologue is wrong setting\n");
        return false;
    }
    if (heap_start == NULL)
    {
        printf("Heap not initialized\n");
        return false;
    }
    // check for allignment
    for (block= heap_start; get_size(block) != 0;find_next(block)) //traverse the heap
    {
        //check if payload is divisible by dsize(block is header, need to transform to payload)
        if ((unsigned long)(header_to_payload(block))%dsize != 0) //type casting for address
        {
            printf("Alignment failed\n");
            return false;
        }
        //check if header is same as footer
        if ((get_size(block) != extract_size(*header_to_footer(block))|| (get_alloc(block)!= extract_alloc(*header_to_footer(block)))))
        {
            printf("header and footer not consistent\n");
            return false;

        }
        //check if each block lies in the boundaries of the heap ?
        if (((unsigned long)header_to_footer(block) > (unsigned long)mem_heap_hi()-7L-(unsigned long)wsize) || ((unsigned long)block <(unsigned long)mem_heap_lo()+7L))
        {
            printf("block out of boundaries of the heap\n");
            return false;
        }
        //check coalscing
        // need to classify the condtions that block is the first block
        // and the condition that block is not
        if (block == heap_start)
        {
            if ((get_alloc(block) == false)&& (get_alloc(find_next(block))==false))
            {
                printf("first block at address %lu has consecutive free blocks\n", (unsigned long)block);
                return false;
            }
        }
        else{
            if (((get_alloc(block) == false)&&(get_alloc(find_prev(block))== false))||((get_alloc(block) == false)&&(get_alloc(find_next(block))== false)))
            {
                printf("consecutive free blocks at address %lu\n", (unsigned long)block);
                return false;
            }
        }
        
        
    }
    //check epilogue's allocation status
    if (get_alloc(block) != true)
        {
            printf("wrong epilogue, not allocated\n");
            return false;
        }
    
    //check the free list (explict and segregated)
    
    ptr = free_list_start;
    while((ptr!= NULL)&&(ptr->next!= NULL)) //traverse the free list
    {
        //check if the next/prev pointers are consistent
        if (ptr->next != ptr->next->prev->next)
        {
            printf("free list block is not consistent\n");
            return false;
        }
        //check if all blocks in free list are free
        if ((get_alloc(ptr) == true)|| (extract_alloc(*header_to_footer(ptr)) == true ))
        {
            printf("block at address %lu is not free\n", (unsigned long)ptr);
            return false;
        }
        if ((((ptr->next)>(block_t*)mem_heap_hi())|| ((ptr->next)<(block_t*)mem_heap_lo()))||(((ptr->prev)>(block_t*)mem_heap_hi())|| ((ptr->prev)<(block_t*)mem_heap_lo())))
        {
            printf("free pointers at address %lu are out of boundary of the heap\n", (unsigned long)ptr);
            return false;
        }
        // All blocks in each list bucket fall within bucket size range (segregated list).
        //not yet imported

    }
    return true;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @return
 */
bool mm_init(void) {
    // Create the initial empty heap
    start = (word_t *)(mem_sbrk(2 * wsize));

    if (start == (void *)-1) {
        return false;
    }

    /*
     * TODO: delete or replace this comment once you've thought about it.
     * Think about why we need a heap prologue and epilogue. Why do
     * they correspond to a block footer and header respectively?
     */

    start[0] = pack(0, true); // Heap prologue (block footer)
    start[1] = pack(0, true); // Heap epilogue (block header)

    // Heap starts with first "block header", currently the epilogue
    heap_start = (block_t *)&(start[1]);

    //initialize start of free_list
    free_list_start = heap_start;
    

    // Extend the empty heap with a free block of chunksize bytes
    if (extend_heap(chunksize) == NULL) {
        return false;
    }
    free_list_start->prev = NULL;
    free_list_start->next= NULL;
    return true;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] size
 * @return
 */
void *malloc(size_t size) {
    dbg_requires(mm_checkheap(__LINE__));

    size_t asize;      // Adjusted block size
    size_t extendsize; // Amount to extend heap if no fit is found
    block_t *block;
    void *bp = NULL;

    // Initialize heap if it isn't initialized
    if (heap_start == NULL) {
        mm_init();
    }

    // Ignore spurious request
    if (size == 0) {
        dbg_ensures(mm_checkheap(__LINE__));
        return bp;
    }

    // Adjust block size to include overhead and to meet alignment requirements
    asize = round_up(size + dsize, dsize);

    // Search the free list for a fit
    block = find_fit(asize);

    // If no fit is found, request more memory, and then and place the block
    if (block == NULL) {
        // Always request at least chunksize
        extendsize = max(asize, chunksize);
        block = extend_heap(extendsize);
        // extend_heap returns an error
        if (block == NULL) {
            return bp;
        }
    }

    // The block should be marked as free
    dbg_assert(!get_alloc(block));

    // Mark block as allocated
    size_t block_size = get_size(block);
    write_block(block, block_size, true);

    // Try to split the block if too large
    split_block(block, asize);

    bp = header_to_payload(block);

    dbg_ensures(mm_checkheap(__LINE__));
    return bp;
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] bp
 */
void free(void *bp) {
    dbg_requires(mm_checkheap(__LINE__));

    if (bp == NULL) {
        return;
    }

    block_t *block = payload_to_header(bp);
    size_t size = get_size(block);

    // The block should be marked as allocated
    dbg_assert(get_alloc(block));

    // Mark the block as free
    write_block(block, size, false);

    // Try to coalesce the block with its neighbors
    block = coalesce_block(block);

    dbg_ensures(mm_checkheap(__LINE__));
}

/**
 * @brief
 *
 * <What does this function do?>
 * <What are the function's arguments?>
 * <What is the function's return value?>
 * <Are there any preconditions or postconditions?>
 *
 * @param[in] ptr
 * @param[in] size
 * @return
 */
void *realloc(void *ptr, size_t size) {
    block_t *block = payload_to_header(ptr);
    size_t copysize;
    void *newptr;

    // If size == 0, then free block and return NULL
    if (size == 0) {
        free(ptr);
        return NULL;
    }

    // If ptr is NULL, then equivalent to malloc
    if (ptr == NULL) {
        return malloc(size);
    }

    // Otherwise, proceed with reallocation
    newptr = malloc(size);

    // If malloc fails, the original block is left untouched
    if (newptr == NULL) {
        return NULL;
    }

    // Copy the old data
    copysize = get_payload_size(block); // gets size of old payload
    if (size < copysize) {
        copysize = size;
    }
    memcpy(newptr, ptr, copysize);

    // Free the old block
    free(ptr);

    return newptr;
}

/**
 * @brief
 *
 *pre: size of elements, number of elements
 *post: a pointer
 * @param[in] elements
 * @param[in] size
 * @return
 */

void *calloc(size_t elements, size_t size) {
    void *bp;
    size_t asize = elements * size;

    if (elements == 0) {
        return NULL;
    }
    if (asize / elements != size) {
        // Multiplication overflowed
        return NULL;
    }

    bp = malloc(asize);
    if (bp == NULL) {
        return NULL;
    }

    // Initialize all bits to 0
    memset(bp, 0, asize); //?

    return bp;
}

/*
 *****************************************************************************
 * Do not delete the following super-secret(tm) lines!                       *
 *                                                                           *
 * 53 6f 20 79 6f 75 27 72 65 20 74 72 79 69 6e 67 20 74 6f 20               *
 *                                                                           *
 * 66 69 67 75 72 65 20 6f 75 74 20 77 68 61 74 20 74 68 65 20               *
 * 68 65 78 61 64 65 63 69 6d 61 6c 20 64 69 67 69 74 73 20 64               *
 * 6f 2e 2e 2e 20 68 61 68 61 68 61 21 20 41 53 43 49 49 20 69               *
 *                                                                           *
 * 73 6e 27 74 20 74 68 65 20 72 69 67 68 74 20 65 6e 63 6f 64               *
 * 69 6e 67 21 20 4e 69 63 65 20 74 72 79 2c 20 74 68 6f 75 67               *
 * 68 21 20 2d 44 72 2e 20 45 76 69 6c 0a c5 7c fc 80 6e 57 0a               *
 *                                                                           *
 *****************************************************************************
 */