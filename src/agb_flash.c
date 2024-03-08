#include "gba/gba.h"
#include "gba/flash_internal.h"

static u8 sTimerNum;
static u16 sTimerCount;
static vu16 *sTimerReg;
static u16 sSavedIme;

u8 gFlashTimeoutFlag;
u8 (*PollFlashStatus)(vu8 *);
u16 (*WaitForFlashWrite)(u8 phase, vu8 *addr, u8 lastData);
u16 (*ProgramFlashSector)(u16 sectorNum, vu8 *src);
const struct FlashType *gFlash;
u16 (*ProgramFlashByte)(u16 sectorNum, u32 offset, u8 data);
u16 gFlashNumRemainingBytes;
u16 (*EraseFlashChip)();
u16 (*EraseFlashSector)(u16 sectorNum);
const u16 *gFlashMaxTime;

void SetReadFlash1(vu16 *dest);

void SwitchFlashBank(u8 bankNum)
{
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xB0);
    FLASH_WRITE(0x0000, bankNum);
}

#define DELAY()                  \
do {                             \
    vu32 i;                      \
    for (i = 20000; i != 0; i--) \
        ;                        \
} while (0)

u16 ReadFlashId(void)
{
    u16 flashId;
    u16 readFlash1Buffer[0x20];
    u8 (*readFlash1)(vu8 *);

    SetReadFlash1(readFlash1Buffer);
    readFlash1 = (u8 (*)(vu8 *))((u32)readFlash1Buffer + 1);

    // Enter ID mode.
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0x90);
    DELAY();

    flashId = readFlash1(FLASH_BASE + 1) << 8 | readFlash1(FLASH_BASE);

    // Leave ID mode.
    FLASH_WRITE(0x5555, 0xAA);
    FLASH_WRITE(0x2AAA, 0x55);
    FLASH_WRITE(0x5555, 0xF0);
    FLASH_WRITE(0x5555, 0xF0);
    DELAY();

    return flashId;
}

void FlashTimerIntr(void)
{
    if (sTimerCount != 0 && --sTimerCount == 0)
        gFlashTimeoutFlag = 1;
}

u16 SetFlashTimerIntr(u8 timerNum, void (**intrFunc)(void))
{
    if (timerNum >= 4)
        return 1;

    sTimerNum = timerNum;
    sTimerReg = &REG_TMCNT_L(sTimerNum);
    *intrFunc = FlashTimerIntr;
    return 0;
}

void StartFlashTimer(u8 phase)
{
    const u16 *maxTime = &gFlashMaxTime[phase * 3];
    sSavedIme = REG_IME;
    REG_IME = 0;
    sTimerReg[1] = 0;
    REG_IE |= (INTR_FLAG_TIMER0 << sTimerNum);
    gFlashTimeoutFlag = 0;
    sTimerCount = *maxTime++;
    *sTimerReg++ = *maxTime++;
    *sTimerReg-- = *maxTime++;
    REG_IF = (INTR_FLAG_TIMER0 << sTimerNum);
    REG_IME = 1;
}

void StopFlashTimer(void)
{
    REG_IME = 0;
    *sTimerReg++ = 0;
    *sTimerReg-- = 0;
    REG_IE &= ~(INTR_FLAG_TIMER0 << sTimerNum);
    REG_IME = sSavedIme;
}

u8 ReadFlash1(vu8 *addr)
{
    return *addr;
}

void SetReadFlash1(vu16 *dest)
{
    vu16 *src;
    u32 i;

    PollFlashStatus = (u8 (*)(vu8 *))((u32)dest + 1);

    src = (u16 *)ReadFlash1;
    src = (u16 *)((u32)src ^ 1);

    i = ((u32)SetReadFlash1 - (u32)ReadFlash1) >> 1;

    while (i != 0)
    {
        *dest++ = *src++;
        i--;
    }
}

// Using volatile here to make sure the flash memory will ONLY be read as bytes, to prevent any compiler optimizations.
void ReadFlash_Core(vu8 *src, vu8 *dest, u32 size)
{
   for (; size; size--)
    {
        *dest++ = *src++;
    }
}

void ReadFlash(u16 sectorNum, u32 offset, vu8 *dest, u32 size)
{
    vu8 *src;
    u32 i;
    vu16 readFlash_Core_Buffer[0x40];
    vu16 *funcSrc;
    vu16 *funcDest;
    void (*readFlash_Core)(vu8 *, vu8 *, u32);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    if (gFlash->romSize == FLASH_ROM_SIZE_1M)
    {
        SwitchFlashBank(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;
    }

    funcSrc = (vu16 *)ReadFlash_Core;
    funcSrc = (vu16 *)((u32)funcSrc ^ 1);
    funcDest = readFlash_Core_Buffer;

    i = ((u32)ReadFlash - (u32)ReadFlash_Core) >> 1;

    while (i != 0)
    {
        *funcDest++ = *funcSrc++;
        i--;
    }

    readFlash_Core = (void (*)(vu8 *, vu8 *, u32))((u32)readFlash_Core_Buffer + 1);

    src = FLASH_BASE + (sectorNum << gFlash->sector.shift) + offset;

    readFlash_Core(src, dest, size);
}

u32 VerifyFlashSector_Core(vu8 *src, vu8 *tgt, u32 size)
{
    while (size-- != 0)
    {
        if (*tgt++ != *src++)
            return (u32)(tgt - 1);
    }

    return 0;
}

u32 VerifyFlashSector(u16 sectorNum, vu8 *src)
{
    u32 i;
    vu16 verifyFlashSector_Core_Buffer[0x80];
    vu16 *funcSrc;
    vu16 *funcDest;
    vu8 *tgt;
    u16 size;
    u32 (*verifyFlashSector_Core)(vu8 *, vu8 *, u32);

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    if (gFlash->romSize == FLASH_ROM_SIZE_1M)
    {
        SwitchFlashBank(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;
    }

    funcSrc = (vu16 *)VerifyFlashSector_Core;
    funcSrc = (vu16 *)((u32)funcSrc ^ 1);
    funcDest = verifyFlashSector_Core_Buffer;

    i = ((u32)VerifyFlashSector - (u32)VerifyFlashSector_Core) >> 1;

    while (i != 0)
    {
        *funcDest++ = *funcSrc++;
        i--;
    }

    verifyFlashSector_Core = (u32 (*)(vu8 *, vu8 *, u32))((u32)verifyFlashSector_Core_Buffer + 1);

    tgt = FLASH_BASE + (sectorNum << gFlash->sector.shift);
    size = gFlash->sector.size;

    return verifyFlashSector_Core(src, tgt, size);
}

u32 VerifyFlashSectorNBytes(u16 sectorNum, vu8 *src, u32 n)
{
    u32 i;
    vu16 verifyFlashSector_Core_Buffer[0x80];
    vu16 *funcSrc;
    vu16 *funcDest;
    vu8 *tgt;
    u32 (*verifyFlashSector_Core)(vu8 *, vu8 *, u32);

    if (gFlash->romSize == FLASH_ROM_SIZE_1M)
    {
        SwitchFlashBank(sectorNum / SECTORS_PER_BANK);
        sectorNum %= SECTORS_PER_BANK;
    }

    REG_WAITCNT = (REG_WAITCNT & ~WAITCNT_SRAM_MASK) | WAITCNT_SRAM_8;

    funcSrc = (vu16 *)VerifyFlashSector_Core;
    funcSrc = (vu16 *)((u32)funcSrc ^ 1);
    funcDest = verifyFlashSector_Core_Buffer;

    i = ((u32)VerifyFlashSector - (u32)VerifyFlashSector_Core) >> 1;

    while (i != 0)
    {
        *funcDest++ = *funcSrc++;
        i--;
    }

    verifyFlashSector_Core = (u32 (*)(vu8 *, vu8 *, u32))((u32)verifyFlashSector_Core_Buffer + 1);

    tgt = FLASH_BASE + (sectorNum << gFlash->sector.shift);

    return verifyFlashSector_Core(src, tgt, n);
}

u32 ProgramFlashSectorAndVerify(u16 sectorNum, vu8 *src)
{
    u32 i;
    u32 result;

    for (i = 0; i < 3; i++)
    {
        result = ProgramFlashSector(sectorNum, src);
        if (result != 0)
            continue;

        result = VerifyFlashSector(sectorNum, src);
        if (result == 0)
            break;
    }

    return result;
}

u32 ProgramFlashSectorAndVerifyNBytes(u16 sectorNum, vu8 *src, u32 n)
{
    u32 i;
    u32 result;

    for (i = 0; i < 3; i++)
    {
        result = ProgramFlashSector(sectorNum, src);
        if (result != 0)
            continue;

        result = VerifyFlashSectorNBytes(sectorNum, src, n);
        if (result == 0)
            break;
    }

    return result;
}
