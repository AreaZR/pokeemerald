#include "global.h"
#include "malloc.h"
#include "data.h"
#include "decompress.h"
#include "pokemon.h"
#include "text.h"

EWRAM_DATA ALIGNED(4) u8 gDecompressionBuffer[0x4000] = {0};

static void DuplicateDeoxysTiles(void *pointer);

u16 LoadCompressedSpriteSheet(const struct SpriteSheet *src)
{
    struct SpriteSheet dest;

    LZ77UnCompWram(src->data, gDecompressionBuffer);
    dest.data = gDecompressionBuffer;
    dest.size = src->size;
    dest.tag = src->tag;
    return LoadSpriteSheet(&dest);
}

void LoadCompressedSpriteSheetOverrideBuffer(const struct SpriteSheet *src, void *buffer)
{
    struct SpriteSheet dest;

    LZ77UnCompWram(src->data, buffer);
    dest.data = buffer;
    dest.size = src->size;
    dest.tag = src->tag;
    LoadSpriteSheet(&dest);
}

void LoadCompressedSpritePalette(const struct CompressedSpritePalette *src)
{
    struct SpritePalette dest;

    LZ77UnCompWram(src->data, gDecompressionBuffer);
    dest.data = (void *) gDecompressionBuffer;
    dest.tag = src->tag;
    LoadSpritePalette(&dest);
}

void LoadCompressedSpritePaletteOverrideBuffer(const struct CompressedSpritePalette *src, void *buffer)
{
    struct SpritePalette dest;

    LZ77UnCompWram(src->data, buffer);
    dest.data = buffer;
    dest.tag = src->tag;
    LoadSpritePalette(&dest);
}

void DecompressPicFromTable(const struct SpriteSheet *src, void *buffer, s32 species)
{
    if (species > NUM_SPECIES) {
        LZ77UnCompWram(gMonFrontPicTable[0].data, buffer);
        return;
    }

    LZ77UnCompWram(src->data, buffer);
    if (species == SPECIES_DEOXYS)
        DuplicateDeoxysTiles(buffer);
}

void HandleLoadSpecialPokePic(const struct SpriteSheet *src, void *dest, s32 species, u32 personality)
{
    bool8 isFrontPic;

    if (src == &gMonFrontPicTable[species])
        isFrontPic = TRUE; // frontPic
    else
        isFrontPic = FALSE; // backPic

    LoadSpecialPokePic_2(src, dest, species, personality, isFrontPic);
}

void LoadSpecialPokePic(const struct SpriteSheet *src, void *dest, s32 species, u32 personality, bool8 isFrontPic)
{
    if (species == SPECIES_UNOWN)
    {
        u16 i = GET_UNOWN_LETTER(personality);

        // The other Unowns are separate from Unown A.
        if (i == 0)
            i = SPECIES_UNOWN;
        else
            i += SPECIES_UNOWN_B - 1;

        if (!isFrontPic)
            LZ77UnCompWram(gMonBackPicTable[i].data, dest);
        else
            LZ77UnCompWram(gMonFrontPicTable[i].data, dest);
        return;
    }
    if (species > NUM_SPECIES) { // is species unknown? draw the ? icon 
        LZ77UnCompWram(gMonFrontPicTable[0].data, dest);
        return;
    }
    
    LZ77UnCompWram(src->data, dest);

    if (species == SPECIES_SPINDA) {
        if (isFrontPic)
            DrawSpindaSpots(personality, dest);
        return;
    }

    if (species == SPECIES_DEOXYS)
        DuplicateDeoxysTiles(dest);
}

u32 GetDecompressedDataSize(const void *ptr)
{
    const u8 *ptr8 = (const u8 *)ptr;
    return (ptr8[3] << 16) | (ptr8[2] << 8) | (ptr8[1]);
}

bool8 LoadCompressedSpriteSheetUsingHeap(const struct SpriteSheet *src)
{
    struct SpriteSheet dest;
    void *buffer;

    const u32 *size = src->data;

    buffer = AllocZeroed(*size >> 8);
    LZ77UnCompWram(src->data, buffer);

    dest.data = buffer;
    dest.size = src->size;
    dest.tag = src->tag;

    LoadSpriteSheet(&dest);
    Free(buffer);
    return FALSE;
}

bool8 LoadCompressedSpritePaletteUsingHeap(const struct CompressedSpritePalette *src)
{
    struct SpritePalette dest;
    void *buffer;

    buffer = AllocZeroed(src->data[0] >> 8);
    LZ77UnCompWram(src->data, buffer);
    dest.data = buffer;
    dest.tag = src->tag;

    LoadSpritePalette(&dest);
    Free(buffer);
    return FALSE;
}

void DecompressPicFromTable_2(const struct SpriteSheet *src, void *buffer, s32 species) // a copy of DecompressPicFromTable
{
    if (species > NUM_SPECIES) {
        LZ77UnCompWram(gMonFrontPicTable[0].data, buffer);
        return;
    }
    
    LZ77UnCompWram(src->data, buffer);
    
    if (species == SPECIES_DEOXYS)
        DuplicateDeoxysTiles(buffer);
}

void LoadSpecialPokePic_2(const struct SpriteSheet *src, void *dest, s32 species, u32 personality, bool8 isFrontPic) // a copy of LoadSpecialPokePic
{
    if (species == SPECIES_UNOWN)
    {
        u16 i = GET_UNOWN_LETTER(personality);

        // The other Unowns are separate from Unown A.
        if (i == 0)
            i = SPECIES_UNOWN;
        else
            i += SPECIES_UNOWN_B - 1;

        if (!isFrontPic)
            LZ77UnCompWram(gMonBackPicTable[i].data, dest);
        else
            LZ77UnCompWram(gMonFrontPicTable[i].data, dest);
    }
    else if (species > NUM_SPECIES) // is species unknown? draw the ? icon
        LZ77UnCompWram(gMonFrontPicTable[0].data, dest);
    else
        LZ77UnCompWram(src->data, dest);

    if (species == SPECIES_SPINDA) {
        if (isFrontPic)
            DrawSpindaSpots(personality, dest);
        return;
    }
    if (species == SPECIES_DEOXYS) {
        DuplicateDeoxysTiles(dest);
    }
}

void HandleLoadSpecialPokePic_2(const struct SpriteSheet *src, void *dest, s32 species, u32 personality) // a copy of HandleLoadSpecialPokePic
{
    bool8 isFrontPic;

    if (src == &gMonFrontPicTable[species])
        isFrontPic = TRUE; // frontPic
    else
        isFrontPic = FALSE; // backPic

    LoadSpecialPokePic_2(src, dest, species, personality, isFrontPic);
}

void DecompressPicFromTable_DontHandleDeoxys(const struct SpriteSheet *src, void *buffer, s32 species)
{
    if (species > NUM_SPECIES)
        LZ77UnCompWram(gMonFrontPicTable[0].data, buffer);
    else
        LZ77UnCompWram(src->data, buffer);
}

void HandleLoadSpecialPokePic_DontHandleDeoxys(const struct SpriteSheet *src, void *dest, s32 species, u32 personality)
{
    bool8 isFrontPic;

    if (src == &gMonFrontPicTable[species])
        isFrontPic = TRUE; // frontPic
    else
        isFrontPic = FALSE; // backPic

    LoadSpecialPokePic_DontHandleDeoxys(src, dest, species, personality, isFrontPic);
}

void LoadSpecialPokePic_DontHandleDeoxys(const struct SpriteSheet *src, void *dest, s32 species, u32 personality, bool8 isFrontPic)
{
    if (species == SPECIES_UNOWN)
    {
        u16 i = GET_UNOWN_LETTER(personality);

        // The other Unowns are separate from Unown A.
        if (i == 0)
            i = SPECIES_UNOWN;
        else
            i += SPECIES_UNOWN_B - 1;

        if (!isFrontPic)
            LZ77UnCompWram(gMonBackPicTable[i].data, dest);
        else
            LZ77UnCompWram(gMonFrontPicTable[i].data, dest);
        return;
    }
    
    if (species > NUM_SPECIES) {// is species unknown? draw the ? icon
        LZ77UnCompWram(gMonFrontPicTable[0].data, dest);
        return;
    }

    LZ77UnCompWram(src->data, dest);

    if (species == SPECIES_SPINDA && isFrontPic) {
        DrawSpindaSpots(personality, dest);
    }
}

static void DuplicateDeoxysTiles(void *pointer)
{
    CpuCopy32(pointer + MON_PIC_SIZE, pointer, MON_PIC_SIZE);
}
