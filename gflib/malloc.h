#ifndef GUARD_ALLOC_H
#define GUARD_ALLOC_H

#define HEAP_SIZE 0x1C000
#define malloc Alloc
#define calloc(ct, sz) AllocZeroed((ct) * (sz))
#define free Free

#define FREE_AND_SET_NULL(ptr)          \
{                                       \
    free(ptr);                          \
    ptr = NULL;                         \
}

#define TRY_FREE_AND_SET_NULL(ptr) if (ptr != NULL) FREE_AND_SET_NULL(ptr)

extern u8 gHeap[HEAP_SIZE];

void *Alloc(u32 size);
void *AllocZeroed(u32 size);
void Free(void *pointer);
void InitHeap(void);

#endif // GUARD_ALLOC_H
