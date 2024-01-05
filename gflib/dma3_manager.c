#include "global.h"
#include "dma3.h"

#define MAX_DMA_REQUESTS 128

#define DMA_REQUEST_COPY32 1
#define DMA_REQUEST_FILL32 2
#define DMA_REQUEST_COPY16 3
#define DMA_REQUEST_FILL16 4

struct Dma3Request
{
    const void *src;
    void *dest;
    u16 size;
    u16 mode;
    u32 value;
};

static struct Dma3Request sDma3Requests[MAX_DMA_REQUESTS];

// Okay this is somehow more performant by preventing a mov when volatile when it doesn't have to be
// TODO: what about on clang?
static volatile int sDma3ManagerLocked;
static s8 sDma3RequestCursor;

void ClearDma3Requests(void)
{
    // sDma3ManagerLocked = TRUE;
    // asm volatile ("" : : : "memory");

    // DMA should prevent interrupts
    DmaFill32(3, 0, sDma3Requests, sizeof(sDma3Requests));
    asm volatile ("" : : : "memory");
    // Hence I put this here so that the compiler cannot try any funny stuff.
    sDma3RequestCursor = 0;

    // for (i = 0; i < MAX_DMA_REQUESTS; i++)
    // {
    //     sDma3Requests[i].size = 0;
    // }
    // asm volatile ("" : : : "memory");
    // sDma3ManagerLocked = FALSE;
}

void ProcessDma3Requests(void)
{
    u32 bytesTransferred;

    if (sDma3ManagerLocked)
        return;

    bytesTransferred = 0;

    // as long as there are DMA requests to process (unless size or vblank is an issue), do not exit
    while (sDma3Requests[sDma3RequestCursor].size != 0)
    {
        bytesTransferred += sDma3Requests[sDma3RequestCursor].size;

        if (bytesTransferred > 40 * 1024)
            return; // don't transfer more than 40 KiB
        if (*(vu8 *)REG_ADDR_VCOUNT > 224)
            return; // we're about to leave vblank, stop

        switch (sDma3Requests[sDma3RequestCursor].mode)
        {
        case DMA_REQUEST_COPY32: // regular 32-bit copy
            Dma3CopyLarge32_(sDma3Requests[sDma3RequestCursor].src,
                             sDma3Requests[sDma3RequestCursor].dest,
                             sDma3Requests[sDma3RequestCursor].size);
            break;
        case DMA_REQUEST_FILL32: // repeat a single 32-bit value across RAM
            Dma3FillLarge32_(sDma3Requests[sDma3RequestCursor].value,
                             sDma3Requests[sDma3RequestCursor].dest,
                             sDma3Requests[sDma3RequestCursor].size);
            break;
        case DMA_REQUEST_COPY16:    // regular 16-bit copy
            Dma3CopyLarge16_(sDma3Requests[sDma3RequestCursor].src,
                             sDma3Requests[sDma3RequestCursor].dest,
                             sDma3Requests[sDma3RequestCursor].size);
            break;
        case DMA_REQUEST_FILL16: // repeat a single 16-bit value across RAM
            Dma3FillLarge16_(sDma3Requests[sDma3RequestCursor].value,
                             sDma3Requests[sDma3RequestCursor].dest,
                             sDma3Requests[sDma3RequestCursor].size);
            break;
        }

        // Free the request
        sDma3Requests[sDma3RequestCursor].src = NULL;
        sDma3Requests[sDma3RequestCursor].dest = NULL;
        sDma3Requests[sDma3RequestCursor].size = 0;
        sDma3Requests[sDma3RequestCursor].mode = 0;
        sDma3Requests[sDma3RequestCursor].value = 0;
        sDma3RequestCursor = (sDma3RequestCursor + 1) & 127;
    }
}

s8 RequestDma3Copy(const void *src, void *dest, u16 size, u8 mode)
{
    s8 cursor;
    u32 i;

    sDma3ManagerLocked = TRUE;
    asm volatile ("" : : : "memory");
    cursor = sDma3RequestCursor;

    for (i = 0; i < MAX_DMA_REQUESTS; i++)
    {
        if (sDma3Requests[cursor].size == 0) // an empty request was found.
        {
            sDma3Requests[cursor].src = src;
            sDma3Requests[cursor].dest = dest;
            sDma3Requests[cursor].size = size;

            if (mode)
                sDma3Requests[cursor].mode = DMA_REQUEST_COPY32;
            else
                sDma3Requests[cursor].mode = DMA_REQUEST_COPY16;

            asm volatile ("" : : : "memory");
            sDma3ManagerLocked = FALSE;
            return cursor;
        }
        // loop back to start.

        cursor = (cursor + 1) & 127;
    }
    asm volatile ("" : : : "memory");
    sDma3ManagerLocked = FALSE;
    return -1;  // no free DMA request was found
}

s8 RequestDma3Fill(u32 value, void *dest, u16 size, u8 mode)
{
    s8 cursor;
    u32 i;

    sDma3ManagerLocked = TRUE;
    asm volatile ("" : : : "memory");
    cursor = sDma3RequestCursor;


    for (i = 0; i < MAX_DMA_REQUESTS; i++)
    {
        if (sDma3Requests[cursor].size == 0) // an empty request was found.
        {
            sDma3Requests[cursor].dest = dest;
            sDma3Requests[cursor].size = size;
            sDma3Requests[cursor].mode = mode;
            sDma3Requests[cursor].value = value;

            if(mode)
                sDma3Requests[cursor].mode = DMA_REQUEST_FILL32;
            else
                sDma3Requests[cursor].mode = DMA_REQUEST_FILL16;

            asm volatile ("" : : : "memory");
            sDma3ManagerLocked = FALSE;
            return cursor;
        }

        cursor = (cursor + 1) & 127;
    }
    asm volatile ("" : : : "memory");
    sDma3ManagerLocked = FALSE;
    return -1;  // no free DMA request was found
}

s8 CheckForSpaceForDma3Request(s8 index)
{
    u32 i;

    if (index == -1)  // check if all requests are free
    {
        for (i = 0; i < MAX_DMA_REQUESTS; i++)
        {
            if (sDma3Requests[i].size != 0)
                return -1;
        }
        return 0;
    }
    else  // check the specified request
    {
        if (sDma3Requests[index].size != 0)
            return -1;
        return 0;
    }
}
