#include "global.h"
#include "palette.h"
#include "util.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "task.h"
#include "constants/rgb.h"

enum
{
    NORMAL_FADE,
    FAST_FADE,
    HARDWARE_FADE,
};

// These are structs for some unused palette system.
// The full functionality of this system is unknown.

#define NUM_PALETTE_STRUCTS 16

struct PaletteWork
{
    u16 tDelay;
    u16 tCoeffDelta;
    u16 tCoeffTarget;
};

struct PaletteStructTemplate
{
    u16 id;
    u16 *src;
    bool16 pst_field_8_0:1;
    u16 unused:9;
    u16 size:5;
    u8 time1;
    u8 srcCount:5;
    u8 state:3;
    u8 time2;
};

struct PaletteStruct
{
    const struct PaletteStructTemplate *template;
    bool32 active:1;
    bool32 flag:1;
    u32 baseDestOffset:9;
    u32 destOffset:10;
    u32 srcIndex:7;
    u8 countdown1;
    u8 countdown2;
};

static void PaletteStruct_Reset(u8);
static u8 PaletteStruct_GetPalNum(u16);
static u8 UpdateNormalPaletteFade(void);
static void BeginFastPaletteFadeInternal(u8);
static u8 UpdateFastPaletteFade(void);
static u8 UpdateHardwarePaletteFade(void);
static void UpdateBlendRegisters(void);
static bool8 IsSoftwarePaletteFadeFinishing(void);
static void Task_BlendPalettesGradually(u8 taskId);

// palette buffers require alignment with agbcc because
// unaligned word reads are issued in BlendPalette otherwise
ALIGNED(4) EWRAM_DATA u16 gPlttBufferUnfaded[PLTT_BUFFER_SIZE] = {0};
ALIGNED(4) EWRAM_DATA u16 gPlttBufferFaded[PLTT_BUFFER_SIZE] = {0};
static EWRAM_DATA struct PaletteStruct sPaletteStructs[NUM_PALETTE_STRUCTS] = {0};
EWRAM_DATA struct PaletteFadeControl gPaletteFade = {0};
static EWRAM_DATA u32 sFiller = 0;
static EWRAM_DATA vu32 sPlttBufferTransferPending = 0;
EWRAM_DATA u8 ALIGNED(2) gPaletteDecompressionBuffer[PLTT_SIZE] = {0};

static const struct PaletteStructTemplate sDummyPaletteStructTemplate = {
    .id = 0xFFFF,
    .state = 1
};

static const u8 sRoundedDownGrayscaleMap[] = {
     0,  0,  0,  0,  0,
     5,  5,  5,  5,  5,
    11, 11, 11, 11, 11,
    16, 16, 16, 16, 16,
    21, 21, 21, 21, 21,
    27, 27, 27, 27, 27,
    31, 31
};

void LoadCompressedPalette(const void *src, u16 offset, u16 size)
{
    LZ77UnCompWram(src, gPaletteDecompressionBuffer);
    CpuCopy16(gPaletteDecompressionBuffer, &gPlttBufferUnfaded[offset], size);
    CpuCopy16(gPaletteDecompressionBuffer, &gPlttBufferFaded[offset], size);
}

void LoadPalette(const void *src, u16 offset, u16 size)
{
    CpuCopy16(src, &gPlttBufferUnfaded[offset], size);
    CpuCopy16(src, &gPlttBufferFaded[offset], size);
}

void FillPalette(u16 value, u16 offset, u16 size)
{
    CpuFill16(value, &gPlttBufferUnfaded[offset], size);
    CpuFill16(value, &gPlttBufferFaded[offset], size);
}

void TransferPlttBuffer(void)
{
    if (gPaletteFade.bufferTransferDisabled)
        return;

    DmaCopy16(3, gPlttBufferFaded, (void *)PLTT, PLTT_SIZE);
    sPlttBufferTransferPending = FALSE;
    if (gPaletteFade.mode == HARDWARE_FADE && gPaletteFade.active)
        UpdateBlendRegisters();
}

u8 UpdatePaletteFade(void)
{
    u8 result;

    if (sPlttBufferTransferPending)
        return PALETTE_FADE_STATUS_LOADING;

    if (gPaletteFade.mode == NORMAL_FADE)
        result = UpdateNormalPaletteFade();
    else if (gPaletteFade.mode == FAST_FADE)
        result = UpdateFastPaletteFade();
    else
        result = UpdateHardwarePaletteFade();

    sPlttBufferTransferPending = gPaletteFade.multipurpose1;

    return result;
}

void ResetPaletteFade(void)
{
    u8 i;

    for (i = 0; i < NUM_PALETTE_STRUCTS; i++)
        PaletteStruct_Reset(i);

    ResetPaletteFadeControl();
}

bool8 BeginNormalPaletteFade(u32 selectedPalettes, s8 delay, u8 startY, u8 targetY, u16 blendColor)
{
    u8 temp;
    u16 color = blendColor;

    if (gPaletteFade.active)
    {
        return FALSE;
    }
    gPaletteFade.deltaY = 2;

    if (delay < 0)
    {
        gPaletteFade.deltaY -= delay;
        delay = 0;
    }

    gPaletteFade_selectedPalettes = selectedPalettes;
    gPaletteFade.delayCounter = delay;
    gPaletteFade_delay = delay;
    gPaletteFade.y = startY;
    gPaletteFade.targetY = targetY;
    gPaletteFade.blendColor = color;
    gPaletteFade.active = TRUE;
    gPaletteFade.mode = NORMAL_FADE;

    if (startY < targetY)
        gPaletteFade.yDec = 0;
    else
        gPaletteFade.yDec = 1;

    UpdatePaletteFade();

    temp = gPaletteFade.bufferTransferDisabled;
    gPaletteFade.bufferTransferDisabled = FALSE;

    CpuFastCopy(gPlttBufferFaded, (void *)PLTT, PLTT_SIZE);
    sPlttBufferTransferPending = FALSE;
    if (gPaletteFade.mode == HARDWARE_FADE && gPaletteFade.active)
        UpdateBlendRegisters();
    gPaletteFade.bufferTransferDisabled = temp;
    return TRUE;
}

void PaletteStruct_ResetById(u16 id)
{
    u8 paletteNum = PaletteStruct_GetPalNum(id);
    if (paletteNum != NUM_PALETTE_STRUCTS)
        PaletteStruct_Reset(paletteNum);
}

static void PaletteStruct_Reset(u8 paletteNum)
{
    sPaletteStructs[paletteNum].template = &sDummyPaletteStructTemplate;
    sPaletteStructs[paletteNum].active = FALSE;
    sPaletteStructs[paletteNum].baseDestOffset = 0;
    sPaletteStructs[paletteNum].destOffset = 0;
    sPaletteStructs[paletteNum].srcIndex = 0;
    sPaletteStructs[paletteNum].flag = 0;
    sPaletteStructs[paletteNum].countdown1 = 0;
    sPaletteStructs[paletteNum].countdown2 = 0;
}

void ResetPaletteFadeControl(void)
{
    gPaletteFade.multipurpose1 = 0;
    gPaletteFade.multipurpose2 = 0;
    gPaletteFade.delayCounter = 0;
    gPaletteFade.y = 0;
    gPaletteFade.targetY = 0;
    gPaletteFade.blendColor = 0;
    gPaletteFade.active = FALSE;
    gPaletteFade.yDec = 0;
    gPaletteFade.bufferTransferDisabled = FALSE;
    gPaletteFade.shouldResetBlendRegisters = FALSE;
    gPaletteFade.hardwareFadeFinishing = FALSE;
    gPaletteFade.softwareFadeFinishing = FALSE;
    gPaletteFade.softwareFadeFinishingCounter = 0;
    gPaletteFade.objPaletteToggle = 0;
    gPaletteFade.deltaY = 2;
}

static void UNUSED PaletteStruct_SetUnusedFlag(u16 id)
{
    u8 paletteNum = PaletteStruct_GetPalNum(id);
    if (paletteNum != NUM_PALETTE_STRUCTS)
        sPaletteStructs[paletteNum].flag = TRUE;
}

static void UNUSED PaletteStruct_ClearUnusedFlag(u16 id)
{
    u8 paletteNum = PaletteStruct_GetPalNum(id);
    if (paletteNum != NUM_PALETTE_STRUCTS)
        sPaletteStructs[paletteNum].flag = FALSE;
}

static u8 PaletteStruct_GetPalNum(u16 id)
{
    u8 i;

    for (i = 0; i < NUM_PALETTE_STRUCTS; i++)
        if (sPaletteStructs[i].template->id == id)
            return i;

    return NUM_PALETTE_STRUCTS;
}

static u8 UpdateNormalPaletteFade(void)
{
    u16 paletteOffset;
    u16 selectedPalettes;

    if (!gPaletteFade.active)
        return PALETTE_FADE_STATUS_DONE;

    if (IsSoftwarePaletteFadeFinishing())
    {
        return gPaletteFade.active;
    }

    if (!gPaletteFade.objPaletteToggle)
    {
        if (gPaletteFade.delayCounter < gPaletteFade_delay)
        {
            gPaletteFade.delayCounter++;
            return 2;
        }
        gPaletteFade.delayCounter = 0;
    }

    paletteOffset = 0;

    if (!gPaletteFade.objPaletteToggle)
    {
        selectedPalettes = gPaletteFade_selectedPalettes;
    }
    else
    {
        selectedPalettes = gPaletteFade_selectedPalettes >> 16;
        paletteOffset = OBJ_PLTT_OFFSET;
    }

    while (selectedPalettes)
    {
        if (selectedPalettes & 1)
            BlendPalette(
                paletteOffset,
                16,
                gPaletteFade.y,
                gPaletteFade.blendColor);
        selectedPalettes >>= 1;
        paletteOffset += 16;
    }

    gPaletteFade.objPaletteToggle ^= 1;

    if (!gPaletteFade.objPaletteToggle)
    {
        if (gPaletteFade.y == gPaletteFade.targetY)
        {
            gPaletteFade_selectedPalettes = 0;
            gPaletteFade.softwareFadeFinishing = TRUE;
        }
        else
        {
            s8 val;

            if (!gPaletteFade.yDec)
            {
                val = gPaletteFade.y;
                val += gPaletteFade.deltaY;
                if (val > gPaletteFade.targetY)
                    val = gPaletteFade.targetY;
                gPaletteFade.y = val;
            }
            else
            {
                val = gPaletteFade.y;
                val -= gPaletteFade.deltaY;
                if (val < gPaletteFade.targetY)
                    val = gPaletteFade.targetY;
                gPaletteFade.y = val;
            }
        }
    }

    // gPaletteFade.active cannot change since the last time it was checked. So this
    // is equivalent to `return PALETTE_FADE_STATUS_ACTIVE;`
    return PALETTE_FADE_STATUS_ACTIVE;
}

void InvertPlttBuffer(u32 selectedPalettes)
{
    u32 paletteOffset = 0;

    while (selectedPalettes)
    {
        if (selectedPalettes & 1)
        {
            u32 i;
            for (i = 0; i < 16; i++)
                gPlttBufferFaded[paletteOffset + i] = ~gPlttBufferFaded[paletteOffset + i];
        }
        selectedPalettes >>= 1;
        paletteOffset += 16;
    }
}

void TintPlttBuffer(u32 selectedPalettes, s8 r, s8 g, s8 b)
{
    u32 paletteOffset = 0;

    while (selectedPalettes)
    {
        if (selectedPalettes & 1)
        {
            u32 i;
            for (i = 0; i < 16; i++)
            {
                union colorWork data1;
                data1.raw = gPlttBufferUnfaded[i];
                data1.data.r += r;
                data1.data.g += g;
                data1.data.b += b;
                gPlttBufferUnfaded[i] = data1.raw;
            }
        }
        selectedPalettes >>= 1;
        paletteOffset += 16;
    }
}

void UnfadePlttBuffer(u32 selectedPalettes)
{
    u32 paletteOffset = 0;

    while (selectedPalettes)
    {
        if (selectedPalettes & 1)
        {
            u32 i;
            for (i = 0; i < 16; i++)
                gPlttBufferFaded[paletteOffset + i] = gPlttBufferUnfaded[paletteOffset + i];
        }
        selectedPalettes >>= 1;
        paletteOffset += 16;
    }
}

void BlendPalette(u16 palOffset, u16 numEntries, u8 coeff, u16 blendColor)
{
    u32 i, index;
    for (i = 0; i < numEntries; i++)
    {
        index = i + palOffset;
        union colorWork data1;
        data1.raw = gPlttBufferUnfaded[index];
        s8 r = data1.data.r;
        s8 g = data1.data.g;
        s8 b = data1.data.b;
        union colorWork data2;
        data2.raw = blendColor;
        gPlttBufferFaded[index] = RGB(r + (((data2.data.r - r) * coeff) >> 4),
                                      g + (((data2.data.g - g) * coeff) >> 4),
                                      b + (((data2.data.b - b) * coeff) >> 4));
    }
}

void BeginFastPaletteFade(u8 submode)
{
    gPaletteFade.deltaY = 2;
    BeginFastPaletteFadeInternal(submode);
}

static void BeginFastPaletteFadeInternal(u8 submode)
{
    gPaletteFade.y = 31;
    gPaletteFade_submode = submode & 0x3F;
    gPaletteFade.active = TRUE;
    gPaletteFade.mode = FAST_FADE;

    if (submode == FAST_FADE_IN_FROM_WHITE) {
        CpuFastFill16(RGB_WHITE, gPlttBufferFaded, PLTT_SIZE);
    }
    else if (submode == FAST_FADE_IN_FROM_BLACK) {
        CpuFastFill16(RGB_BLACK, gPlttBufferFaded, PLTT_SIZE);
    }


    UpdatePaletteFade();
}

static u8 UpdateFastPaletteFade(void)
{
    u16 i;
    u16 paletteOffsetStart;
    u16 paletteOffsetEnd;
    s8 r0;
    s8 g0;
    s8 b0;
    s8 r;
    s8 g;
    s8 b;

    if (!gPaletteFade.active)
        return PALETTE_FADE_STATUS_DONE;

    if (IsSoftwarePaletteFadeFinishing())
        return gPaletteFade.active ? PALETTE_FADE_STATUS_ACTIVE : PALETTE_FADE_STATUS_DONE;


    if (gPaletteFade.objPaletteToggle)
    {
        paletteOffsetStart = OBJ_PLTT_OFFSET;
        paletteOffsetEnd = PLTT_BUFFER_SIZE;
    }
    else
    {
        paletteOffsetStart = 0;
        paletteOffsetEnd = OBJ_PLTT_OFFSET;
    }

    switch (gPaletteFade_submode)
    {
    case FAST_FADE_IN_FROM_WHITE:
        for (i = paletteOffsetStart; i < paletteOffsetEnd; i++)
        {
            union colorWork unfaded;
            union colorWork faded;

            unfaded.raw = gPlttBufferUnfaded[i];
            r0 = unfaded.data.r;
            g0 = unfaded.data.g;
            b0 = unfaded.data.g;

            faded.raw = gPlttBufferFaded[i];
            r = faded.data.r - 2;
            g = faded.data.g - 2;
            b = faded.data.b - 2;

            if (r < r0)
                r = r0;
            if (g < g0)
                g = g0;
            if (b < b0)
                b = b0;

            gPlttBufferFaded[i] = RGB(r, g, b);
        }
        break;
    case FAST_FADE_OUT_TO_WHITE:
        for (i = paletteOffsetStart; i < paletteOffsetEnd; i++)
        {
            union colorWork faded;

            faded.raw = gPlttBufferFaded[i];
            r = faded.data.r + 2;
            g = faded.data.g + 2;
            b = faded.data.b + 2;

            if (r > 31)
                r = 31;
            if (g > 31)
                g = 31;
            if (b > 31)
                b = 31;

            gPlttBufferFaded[i] = RGB(r, g, b);
        }
        break;
    case FAST_FADE_IN_FROM_BLACK:
        for (i = paletteOffsetStart; i < paletteOffsetEnd; i++)
        {
                        union colorWork unfaded;
            union colorWork faded;

            unfaded.raw = gPlttBufferUnfaded[i];
            r0 = unfaded.data.r;
            g0 = unfaded.data.g;
            b0 = unfaded.data.g;

            faded.raw = gPlttBufferFaded[i];
            r = faded.data.r + 2;
            g = faded.data.g + 2;
            b = faded.data.b + 2;

            if (r > r0)
                r = r0;
            if (g > g0)
                g = g0;
            if (b > b0)
                b = b0;

            gPlttBufferFaded[i] = RGB(r, g, b);
        }
        break;
    case FAST_FADE_OUT_TO_BLACK:
        for (i = paletteOffsetStart; i < paletteOffsetEnd; i++)
        {
                        union colorWork faded;

            faded.raw = gPlttBufferFaded[i];
            r = faded.data.r - 2;
            g = faded.data.g - 2;
            b = faded.data.b - 2;

            if (r < 0)
                r = 0;
            if (g < 0)
                g = 0;
            if (b < 0)
                b = 0;

            gPlttBufferFaded[i] = RGB(r, g, b);
        }
    }

    gPaletteFade.objPaletteToggle ^= 1;

    if (gPaletteFade.objPaletteToggle)
        // gPaletteFade.active cannot change since the last time it was checked. So this
        // is equivalent to `return PALETTE_FADE_STATUS_ACTIVE;`
        return PALETTE_FADE_STATUS_ACTIVE;

    if (gPaletteFade.y - gPaletteFade.deltaY < 0)
        gPaletteFade.y = 0;
    else
        gPaletteFade.y -= gPaletteFade.deltaY;

    if (gPaletteFade.y == 0)
    {
        switch (gPaletteFade_submode)
        {
        case FAST_FADE_IN_FROM_WHITE:
        case FAST_FADE_IN_FROM_BLACK:
            CpuFastCopy(gPlttBufferUnfaded, gPlttBufferFaded, PLTT_SIZE);
            break;
        case FAST_FADE_OUT_TO_WHITE:
            CpuFastFill16(RGB_WHITE, gPlttBufferFaded, PLTT_SIZE);
            break;
        case FAST_FADE_OUT_TO_BLACK:
            CpuFastFill16(RGB_BLACK, gPlttBufferFaded, PLTT_SIZE);
            break;
        }

        gPaletteFade.mode = NORMAL_FADE;
        gPaletteFade.softwareFadeFinishing = TRUE;
    }

    // gPaletteFade.active cannot change since the last time it was checked. So this
    // is equivalent to `return PALETTE_FADE_STATUS_ACTIVE;`
    return PALETTE_FADE_STATUS_ACTIVE;
}

void BeginHardwarePaletteFade(u8 blendCnt, u8 delay, u8 y, u8 targetY, u8 shouldResetBlendRegisters)
{
    gPaletteFade_blendCnt = blendCnt;
    gPaletteFade.delayCounter = delay;
    gPaletteFade_delay = delay;
    gPaletteFade.y = y;
    gPaletteFade.targetY = targetY;
    gPaletteFade.active = TRUE;
    gPaletteFade.mode = HARDWARE_FADE;
    gPaletteFade.shouldResetBlendRegisters = shouldResetBlendRegisters & 1;
    gPaletteFade.hardwareFadeFinishing = FALSE;

    if (y < targetY)
        gPaletteFade.yDec = 0;
    else
        gPaletteFade.yDec = 1;
}

static u8 UpdateHardwarePaletteFade(void)
{
    if (!gPaletteFade.active)
        return PALETTE_FADE_STATUS_DONE;

    if (gPaletteFade.delayCounter < gPaletteFade_delay)
    {
        gPaletteFade.delayCounter++;
        return PALETTE_FADE_STATUS_DELAY;
    }

    gPaletteFade.delayCounter = 0;

    if (!gPaletteFade.yDec)
    {
        gPaletteFade.y++;
        if (gPaletteFade.y > gPaletteFade.targetY)
        {
            gPaletteFade.hardwareFadeFinishing++;
            gPaletteFade.y--;
        }
    }
    else
    {
        s32 y = gPaletteFade.y--;
        if (y - 1 < gPaletteFade.targetY)
        {
            gPaletteFade.hardwareFadeFinishing++;
            gPaletteFade.y++;
        }
    }

    if (gPaletteFade.hardwareFadeFinishing)
    {
        if (gPaletteFade.shouldResetBlendRegisters)
        {
            gPaletteFade_blendCnt = 0;
            gPaletteFade.y = 0;
        }
        gPaletteFade.shouldResetBlendRegisters = FALSE;
    }

    // gPaletteFade.active cannot change since the last time it was checked. So this
    // is equivalent to `return PALETTE_FADE_STATUS_ACTIVE;`
    return PALETTE_FADE_STATUS_ACTIVE;
}

static void UpdateBlendRegisters(void)
{
    SetGpuReg(REG_OFFSET_BLDCNT, (u16)gPaletteFade_blendCnt);
    SetGpuReg(REG_OFFSET_BLDY, gPaletteFade.y);
    if (gPaletteFade.hardwareFadeFinishing)
    {
        gPaletteFade.hardwareFadeFinishing = FALSE;
        gPaletteFade.mode = 0;
        gPaletteFade_blendCnt = 0;
        gPaletteFade.y = 0;
        gPaletteFade.active = FALSE;
    }
}

static bool8 IsSoftwarePaletteFadeFinishing(void)
{
    if (gPaletteFade.softwareFadeFinishing)
    {
        if (gPaletteFade.softwareFadeFinishingCounter == 4)
        {
            gPaletteFade.active = FALSE;
            gPaletteFade.softwareFadeFinishing = FALSE;
            gPaletteFade.softwareFadeFinishingCounter = 0;
        }
        else
        {
            gPaletteFade.softwareFadeFinishingCounter++;
        }

        return TRUE;
    }
    else
    {
        return FALSE;
    }
}

void BlendPalettes(u32 selectedPalettes, u8 coeff, u16 color)
{
    u16 paletteOffset;

    for (paletteOffset = 0; selectedPalettes; paletteOffset += 16)
    {
        if (selectedPalettes & 1)
            BlendPalette(paletteOffset, 16, coeff, color);
        selectedPalettes >>= 1;
    }
}

void BlendPalettesUnfaded(u32 selectedPalettes, u8 coeff, u16 color)
{
    DmaCopy32(3, gPlttBufferUnfaded, gPlttBufferFaded, PLTT_SIZE);
    BlendPalettes(selectedPalettes, coeff, color);
}

void TintPalette_GrayScale(u16 *palette, u16 count)
{
    s32 r, g, b, i;
    u32 gray;

    for (i = 0; i < count; i++)
    {
        r = GET_R(*palette);
        g = GET_G(*palette);
        b = GET_B(*palette);

        gray = (r * 76 + g * 151 + b * 29) >> 8;

        *palette++ = RGB(gray, gray, gray);
    }
}

void TintPalette_GrayScale2(u16 *palette, u16 count)
{
    s32 r, g, b, i;
    u32 gray;

    for (i = 0; i < count; i++)
    {
        r = GET_R(*palette);
        g = GET_G(*palette);
        b = GET_B(*palette);

        gray = (r * 76 + g * 151 + b * 29) >> 8;

        if (gray > 31)
            gray = 31;

        gray = sRoundedDownGrayscaleMap[gray];

        *palette++ = RGB(gray, gray, gray);
    }
}

void TintPalette_SepiaTone(u16 *palette, u16 count)
{
    #define R_FIL(x)	( ((u16)((x)*307)>>8) )
    #define G_FIL(x)	( ((u16)((x)*256)>>8) )
    #define B_FIL(x)	( ((u16)((x)*240)>>8) )
    s32 r, g, b, i;
    s32 gray;

    for (i = 0; i < count; i++)
    {
        r = GET_R(*palette);
        g = GET_G(*palette);
        b = GET_B(*palette);

        gray = (r * 76 + g * 151 + b * 29) >> 8;

		r = R_FIL(gray);
		g = G_FIL(gray);
		b = B_FIL(gray);

        if (r > 31)
            r = 31;

        *palette++ = RGB(r, g, b);
    }
}

void TintPalette_CustomTone(u16 *palette, u16 count, u16 rTone, u16 gTone, u16 bTone)
{
    s32 r, g, b, i;
    s32 gray;

    for (i = 0; i < count; i++)
    {
        r = GET_R(*palette);
        g = GET_G(*palette);
        b = GET_B(*palette);

        gray = (r * 76 + g * 151 + b * 29) >> 8;

        r = (u16)((rTone * gray)) >> 8;
        g = (u16)((gTone * gray)) >> 8;
        b = (u16)((bTone * gray)) >> 8;

        if (r > 31)
            r = 31;
        if (g > 31)
            g = 31;
        if (b > 31)
            b = 31;

        *palette++ = RGB(r, g, b);
    }
}

#define tCoeff       data[0]
#define tCoeffTarget data[1]
#define tCoeffDelta  data[2]
#define tDelay       data[3]
#define tDelayTimer  data[4]
#define tPalettes    5 // data[5] and data[6], set/get via Set/GetWordTaskArg
#define tColor       data[7]
#define tId          data[8]

// Blend the selected palettes in a series of steps toward or away from the color.
// Only used by the Groudon/Kyogre fight scene to flash the screen for lightning.
// One call is used to fade the bg from white, while another fades the duo from black
void BlendPalettesGradually(u32 selectedPalettes, s8 delay, u8 coeff, u8 coeffTarget, u16 color, u8 priority, u8 id)
{
    u8 taskId;

    taskId = CreateTask((void *)Task_BlendPalettesGradually, priority);
    gTasks[taskId].tCoeff = coeff;
    gTasks[taskId].tCoeffTarget = coeffTarget;

    if (delay >= 0)
    {
        gTasks[taskId].tDelay = delay;
        gTasks[taskId].tCoeffDelta = 1;
    }
    else
    {
        gTasks[taskId].tDelay = 0;
        gTasks[taskId].tCoeffDelta = -delay + 1;
    }

    if (coeffTarget < coeff)
        gTasks[taskId].tCoeffDelta *= -1;

    SetWordTaskArg(taskId, tPalettes, selectedPalettes);
    gTasks[taskId].tColor = color;
    gTasks[taskId].tId = id;
    gTasks[taskId].func(taskId);
}

static bool32 UNUSED IsBlendPalettesGraduallyTaskActive(u8 id)
{
    u32 i;

    for (i = 0; i < NUM_TASKS; i++)
        if ((gTasks[i].isActive == TRUE)
         && (gTasks[i].func == Task_BlendPalettesGradually)
         && (gTasks[i].tId == id))
            return TRUE;

    return FALSE;
}

static void UNUSED DestroyBlendPalettesGraduallyTask(void)
{
    u8 taskId;

    while (1)
    {
        taskId = FindTaskIdByFunc(Task_BlendPalettesGradually);
        if (taskId == TASK_NONE)
            break;
        DestroyTask(taskId);
    }
}

static void Task_BlendPalettesGradually(u8 taskId)
{
    u32 palettes;
    s16 *data;

    data = gTasks[taskId].data;
    palettes = GetWordTaskArg(taskId, tPalettes);

    if (++tDelayTimer > tDelay)
    {
        tDelayTimer = 0;
        BlendPalettes(palettes, tCoeff, tColor);
        if (tCoeff == tCoeffTarget)
        {
            DestroyTask(taskId);
            return;
        }

        tCoeff += tCoeffDelta;
        if (tCoeffDelta >= 0)
        {
            if (tCoeff > tCoeffTarget)
                tCoeff = tCoeffTarget;
        }
        else if (tCoeff < tCoeffTarget)
        {
            tCoeff = tCoeffTarget;
        }
    }
}
