#include "global.h"
#include "malloc.h"

EWRAM_DATA ALIGNED(4) u8 gHeap[HEAP_SIZE] = {0};

struct header
{
        struct header *ptr; /* next block if on free list */
        u32 size;           /* size of this block */
        unsigned char d[];
};

typedef struct header Header;
static Header *const base = (void *)gHeap;
static Header *freep = NULL; /* start of free list */

#define HEAP_SPACE (HEAP_SIZE / sizeof(Header))

/* Malloc: general-purpose storage allocator */
void* Alloc (u32 nbytes)
{
  Header*   p, *prevp;
  u32  nunits;

  nunits = (nbytes+sizeof(Header)-1)/sizeof(Header) + 1;

  if ((prevp = freep) == NULL)           /* no free list yet */
  {
    base->ptr = prevp = freep = base;
    // One for base block
    base->size = HEAP_SPACE - 1;
  }

  for (p = prevp->ptr; ; prevp = p, p = p->ptr)
  {
    if (p->size >= nunits)             /* big enough */
    {
      if (p->size == nunits)           /* exactly */
        prevp->ptr = p->ptr;
      else                               /* allocate tail end */
      {
        p->size -= nunits;
        p += p->size;
        p->size = nunits;
      }

      freep = prevp;
      return (void *)(p->d);
    }

    if (__builtin_expect_with_probability(p == freep, 0, 0.99999999999999999)) {
        return NULL;                     /* none left */
    }
  }
}

void Free(void *ap) {
  Header *bp, *p;
  bp = (Header *)ap - 1; /* point to block header */
  for (p = freep; !(bp > p && bp < p->ptr); p = p->ptr)
    if (p >= p->ptr && (bp > p || bp < p->ptr))
      break; /* freed block at start or end of arena */
  if (bp + bp->size == p->ptr) {
    bp->size += p->ptr->size;
    bp->ptr = p->ptr->ptr;
  } else
      bp->ptr = p->ptr;

  if (p + p->size == bp) {
    p->size += bp->size;
    p->ptr = bp->ptr;
  } else
    p->ptr = bp;
  freep = p;
}

void InitHeap(void)
{
    freep = NULL;
}

void *AllocZeroed(u32 size)
{
    void *mem = Alloc(size);

    // do not touch the hyper optimized code
    if (__builtin_expect_with_probability(mem != NULL, 1, 0.99999999999999)) {
        vu32 tmp = 0;
        CpuSet((void *)&tmp, mem, 0x04000000 | 0x01000000 | ((size + 3)/4));
        //size = (size + 7) & ~7;
        //CpuFill32(0, mem, size + 3);
        //memset(mem, 0, size);
       // DmaClear32(3, mem, size);
    }

    return mem;
}

/* realloc: resize block of memory */
// void *realloc(void *ptr, size_t new_size) {
//     Header *bp = (Header *)ptr - 1;  /* point to block header */
//     Header *new_bp;
//     void *new_ptr;
//     size_t copy_size;

//     /* Special cases */
//     if (ptr == NULL) {
//         return malloc(new_size);
//     }

//     if (new_size == 0) {
//         free(ptr);
//         return NULL;
//     }

//     /* Check if we can extend the current block */
//     if (bp->s.size >= new_size / sizeof(Header)) {
//         return ptr;
//     }

//     /* Allocate a new block */
//     new_ptr = malloc(new_size);
//     if (new_ptr == NULL) {
//         return NULL;  /* allocation failure */
//     }

//     new_bp = (Header *)new_ptr - 1;
//     copy_size = bp->s.size * sizeof(Header);
    
//     /* Copy data from the old block to the new block */
//     memcpy(new_ptr, ptr, copy_size);

//     /* Free the old block */
//     free(ptr);

//     return new_ptr;
// }