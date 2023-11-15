#include "global.h"
#include "battle.h"
#include "main.h"
#include "m4a.h"
#include "palette.h"
#include "sound.h"
#include "constants/songs.h"
#include "string_util.h"
#include "window.h"
#include "text.h"
#include "blit.h"
#include "menu.h"
#include "dynamic_placeholder_text_util.h"
#include "fonts.h"

static u16 RenderText(struct TextPrinter *);
static u32 RenderFont(struct TextPrinter *);
static u16 FontFunc_Small(struct TextPrinter *);
static u16 FontFunc_Normal(struct TextPrinter *);
static u16 FontFunc_Short(struct TextPrinter *);
static u16 FontFunc_ShortCopy1(struct TextPrinter *);
static u16 FontFunc_ShortCopy2(struct TextPrinter *);
static u16 FontFunc_ShortCopy3(struct TextPrinter *);
static u16 FontFunc_Narrow(struct TextPrinter *);
static u16 FontFunc_SmallNarrow(struct TextPrinter *);
static void DecompressGlyph_Small(u16, bool32);
static void DecompressGlyph_Normal(u16, bool32);
static void DecompressGlyph_Short(u16, bool32);
static void DecompressGlyph_Narrow(u16, bool32);
static void DecompressGlyph_SmallNarrow(u16, bool32);
static void DecompressGlyph_Bold(u16);
static u32 GetGlyphWidth_Small(u16, bool32);
static u32 GetGlyphWidth_Normal(u16, bool32);
static u32 GetGlyphWidth_Short(u16, bool32);
static u32 GetGlyphWidth_Narrow(u16, bool32);
static u32 GetGlyphWidth_SmallNarrow(u16, bool32);

static EWRAM_DATA struct TextPrinter sTempTextPrinter = {0};
static EWRAM_DATA struct TextPrinter sTextPrinters[WINDOWS_MAX] = {0};

static u16 sFontHalfRowLookupTable[0x51];
static u16 sLastTextBgColor;
static u16 sLastTextFgColor;
static u16 sLastTextShadowColor;

bool8 gDisableTextPrinters;
struct TextGlyph gCurGlyph;
TextFlags gTextFlags;

static const u8 sFontHalfRowOffsets[] =
{
    0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x03, 0x06, 0x07, 0x08, 0x06, 0x00, 0x01, 0x02, 0x00,
    0x09, 0x0A, 0x0B, 0x09, 0x0C, 0x0D, 0x0E, 0x0C, 0x0F, 0x10, 0x11, 0x0F, 0x09, 0x0A, 0x0B, 0x09,
    0x12, 0x13, 0x14, 0x12, 0x15, 0x16, 0x17, 0x15, 0x18, 0x19, 0x1A, 0x18, 0x12, 0x13, 0x14, 0x12,
    0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x03, 0x06, 0x07, 0x08, 0x06, 0x00, 0x01, 0x02, 0x00,
    0x1B, 0x1C, 0x1D, 0x1B, 0x1E, 0x1F, 0x20, 0x1E, 0x21, 0x22, 0x23, 0x21, 0x1B, 0x1C, 0x1D, 0x1B,
    0x24, 0x25, 0x26, 0x24, 0x27, 0x28, 0x29, 0x27, 0x2A, 0x2B, 0x2C, 0x2A, 0x24, 0x25, 0x26, 0x24,
    0x2D, 0x2E, 0x2F, 0x2D, 0x30, 0x31, 0x32, 0x30, 0x33, 0x34, 0x35, 0x33, 0x2D, 0x2E, 0x2F, 0x2D,
    0x1B, 0x1C, 0x1D, 0x1B, 0x1E, 0x1F, 0x20, 0x1E, 0x21, 0x22, 0x23, 0x21, 0x1B, 0x1C, 0x1D, 0x1B,
    0x36, 0x37, 0x38, 0x36, 0x39, 0x3A, 0x3B, 0x39, 0x3C, 0x3D, 0x3E, 0x3C, 0x36, 0x37, 0x38, 0x36,
    0x3F, 0x40, 0x41, 0x3F, 0x42, 0x43, 0x44, 0x42, 0x45, 0x46, 0x47, 0x45, 0x3F, 0x40, 0x41, 0x3F,
    0x48, 0x49, 0x4A, 0x48, 0x4B, 0x4C, 0x4D, 0x4B, 0x4E, 0x4F, 0x50, 0x4E, 0x48, 0x49, 0x4A, 0x48,
    0x36, 0x37, 0x38, 0x36, 0x39, 0x3A, 0x3B, 0x39, 0x3C, 0x3D, 0x3E, 0x3C, 0x36, 0x37, 0x38, 0x36,
    0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x03, 0x06, 0x07, 0x08, 0x06, 0x00, 0x01, 0x02, 0x00,
    0x09, 0x0A, 0x0B, 0x09, 0x0C, 0x0D, 0x0E, 0x0C, 0x0F, 0x10, 0x11, 0x0F, 0x09, 0x0A, 0x0B, 0x09,
    0x12, 0x13, 0x14, 0x12, 0x15, 0x16, 0x17, 0x15, 0x18, 0x19, 0x1A, 0x18, 0x12, 0x13, 0x14, 0x12,
    0x00, 0x01, 0x02, 0x00, 0x03, 0x04, 0x05, 0x03, 0x06, 0x07, 0x08, 0x06, 0x00, 0x01, 0x02, 0x00
};

static const u8 sDownArrowTiles[] = INCBIN_U8("graphics/fonts/down_arrow.4bpp");
static const u8 sDarkDownArrowTiles[] = INCBIN_U8("graphics/fonts/down_arrow_alt.4bpp");
static const u8 sUnusedFRLGBlankedDownArrow[] = INCBIN_U8("graphics/fonts/unused_frlg_blanked_down_arrow.4bpp");
static const u8 sUnusedFRLGDownArrow[] = INCBIN_U8("graphics/fonts/unused_frlg_down_arrow.4bpp");
static const u8 sDownArrowYCoords[] = { 0, 1, 2, 1 };
static const u8 sWindowVerticalScrollSpeeds[] = {
    [OPTIONS_TEXT_SPEED_SLOW] = 1,
    [OPTIONS_TEXT_SPEED_MID] = 2,
    [OPTIONS_TEXT_SPEED_FAST] = 4,
};

static const struct GlyphWidthFunc sGlyphWidthFuncs[] =
{
    { FONT_SMALL,        GetGlyphWidth_Small },
    { FONT_NORMAL,       GetGlyphWidth_Normal },
    { FONT_SHORT,        GetGlyphWidth_Short },
    { FONT_SHORT_COPY_1, GetGlyphWidth_Short },
    { FONT_SHORT_COPY_2, GetGlyphWidth_Short },
    { FONT_SHORT_COPY_3, GetGlyphWidth_Short },
    { FONT_BRAILLE,      GetGlyphWidth_Braille },
    { FONT_NARROW,       GetGlyphWidth_Narrow },
    { FONT_SMALL_NARROW, GetGlyphWidth_SmallNarrow }
};

struct
{
    u16 tileOffset;
    u8 width;
    u8 height;
} static const sKeypadIcons[] =
{
    [CHAR_A_BUTTON]       = { 0x00,  8, 12 },
    [CHAR_B_BUTTON]       = { 0x01,  8, 12 },
    [CHAR_L_BUTTON]       = { 0x02, 16, 12 },
    [CHAR_R_BUTTON]       = { 0x04, 16, 12 },
    [CHAR_START_BUTTON]   = { 0x06, 24, 12 },
    [CHAR_SELECT_BUTTON]  = { 0x09, 24, 12 },
    [CHAR_DPAD_UP]        = { 0x0C,  8, 12 },
    [CHAR_DPAD_DOWN]      = { 0x0D,  8, 12 },
    [CHAR_DPAD_LEFT]      = { 0x0E,  8, 12 },
    [CHAR_DPAD_RIGHT]     = { 0x0F,  8, 12 },
    [CHAR_DPAD_UPDOWN]    = { 0x20,  8, 12 },
    [CHAR_DPAD_LEFTRIGHT] = { 0x21,  8, 12 },
    [CHAR_DPAD_NONE]      = { 0x22,  8, 12 }
};

static const u8 sKeypadIconTiles[] = INCBIN_U8("graphics/fonts/keypad_icons.4bpp");

const struct FontInfo gFonts[] =
{
    [FONT_SMALL] = {
        .fontFunction = FontFunc_Small,
        .maxLetterWidth = 5,
        .maxLetterHeight = 12,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_NORMAL] = {
        .fontFunction = FontFunc_Normal,
        .maxLetterWidth = 6,
        .maxLetterHeight = 16,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_SHORT] = {
        .fontFunction = FontFunc_Short,
        .maxLetterWidth = 6,
        .maxLetterHeight = 14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_SHORT_COPY_1] = {
        .fontFunction = FontFunc_ShortCopy1,
        .maxLetterWidth = 6,
        .maxLetterHeight =  14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_SHORT_COPY_2] = {
        .fontFunction = FontFunc_ShortCopy2,
        .maxLetterWidth = 6,
        .maxLetterHeight =  14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_SHORT_COPY_3] = {
        .fontFunction = FontFunc_ShortCopy3,
        .maxLetterWidth = 6,
        .maxLetterHeight =  14,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_BRAILLE] = {
        .fontFunction = FontFunc_Braille,
        .maxLetterWidth = 8,
        .maxLetterHeight = 16,
        .letterSpacing = 0,
        .lineSpacing = 8,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_NARROW] = {
        .fontFunction = FontFunc_Narrow,
        .maxLetterWidth = 5,
        .maxLetterHeight = 16,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_SMALL_NARROW] = {
        .fontFunction = FontFunc_SmallNarrow,
        .maxLetterWidth = 5,
        .maxLetterHeight = 8,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 2,
        .bgColor = 1,
        .shadowColor = 3,
    },
    [FONT_BOLD] = {
        .fontFunction = NULL,
        .maxLetterWidth = 8,
        .maxLetterHeight = 8,
        .letterSpacing = 0,
        .lineSpacing = 0,
        .fgColor = 1,
        .bgColor = 2,
        .shadowColor = 15,
    }
};

static const u8 sMenuCursorDimensions[][2] =
{
    [FONT_SMALL]        = { 8,  12 },
    [FONT_NORMAL]       = { 8,  15 },
    [FONT_SHORT]        = { 8,  14 },
    [FONT_SHORT_COPY_1] = { 8,  14 },
    [FONT_SHORT_COPY_2] = { 8,  14 },
    [FONT_SHORT_COPY_3] = { 8,  14 },
    [FONT_BRAILLE]      = { 8,  16 },
    [FONT_NARROW]       = { 8,  15 },
    [FONT_SMALL_NARROW] = { 8,   8 },
    [FONT_BOLD]         = {}
};

static const u16 sFontBoldJapaneseGlyphs[] = INCBIN_U16("graphics/fonts/bold.hwjpnfont");

void DeactivateAllTextPrinters(void)
{
    unsigned int printer;
    for (printer = 0; printer < WINDOWS_MAX; ++printer)
        sTextPrinters[printer].active = FALSE;
}

void AddTextPrinterParameterized(u8 windowId, u8 fontId, const u8 *str, u8 x, u8 y, u8 speed, void (*callback)(struct TextPrinterTemplate *, u16))
{
    sTempTextPrinter.printerTemplate.currentChar = str;
    sTempTextPrinter.printerTemplate.windowId = windowId;
    sTempTextPrinter.printerTemplate.fontId = fontId;
    sTempTextPrinter.printerTemplate.x = x;
    sTempTextPrinter.printerTemplate.y = y;
    sTempTextPrinter.printerTemplate.currentX = x;
    sTempTextPrinter.printerTemplate.currentY = y;

    // TODO: are these fields redundant when we have the fontID?
    sTempTextPrinter.printerTemplate.letterSpacing = gFonts[fontId].letterSpacing;
    sTempTextPrinter.printerTemplate.lineSpacing = gFonts[fontId].lineSpacing;
    sTempTextPrinter.printerTemplate.fgColor = gFonts[fontId].fgColor;
    sTempTextPrinter.printerTemplate.bgColor = gFonts[fontId].bgColor;
    sTempTextPrinter.printerTemplate.shadowColor = gFonts[fontId].shadowColor;

    sTempTextPrinter.active = TRUE;
    sTempTextPrinter.state = RENDER_STATE_HANDLE_CHAR;
    sTempTextPrinter.textSpeed = speed;
    sTempTextPrinter.delayCounter = 0;
    sTempTextPrinter.scrollDistance = 0;

    memset(&sTempTextPrinter.subStruct, 0, sizeof(sTempTextPrinter.subStruct));

    sTempTextPrinter.callback = callback;
    sTempTextPrinter.minLetterSpacing = 0;
    sTempTextPrinter.japanese = 0;

    GenerateFontHalfRowLookupTable(sTempTextPrinter.printerTemplate.fgColor, sTempTextPrinter.printerTemplate.bgColor, sTempTextPrinter.printerTemplate.shadowColor);
    if (speed != TEXT_SKIP_DRAW && speed != 0)
    {
        sTempTextPrinter.textSpeed--;
        sTextPrinters[windowId] = sTempTextPrinter;
    }
    else
    {
        sTempTextPrinter.textSpeed = 0;

        // Render all text (up to limit) at once
        for (u32 j = 0; j < 0x400; ++j)
        {
            if (RenderFont(&sTempTextPrinter) == RENDER_FINISH)
                break;
        }

        // All the text is rendered to the window but don't draw it yet.
        if (speed != TEXT_SKIP_DRAW)
            CopyWindowToVram(windowId, COPYWIN_GFX);
        sTextPrinters[windowId].active = FALSE;
    }
    gDisableTextPrinters = FALSE;
}

void AddTextPrinter(struct TextPrinterTemplate *printerTemplate, u8 speed, void (*callback)(struct TextPrinterTemplate *, u16))
{
    unsigned int i;
    u32 j;

    sTempTextPrinter.active = TRUE;
    sTempTextPrinter.state = RENDER_STATE_HANDLE_CHAR;
    sTempTextPrinter.textSpeed = speed;
    sTempTextPrinter.delayCounter = 0;
    sTempTextPrinter.scrollDistance = 0;

    memset(&sTempTextPrinter.subStruct, 0, sizeof(sTempTextPrinter.subStruct));

    sTempTextPrinter.printerTemplate = *printerTemplate;
    sTempTextPrinter.callback = callback;
    sTempTextPrinter.minLetterSpacing = 0;
    sTempTextPrinter.japanese = 0;

    GenerateFontHalfRowLookupTable(printerTemplate->fgColor, printerTemplate->bgColor, printerTemplate->shadowColor);
    if (speed != TEXT_SKIP_DRAW && speed != 0)
    {
        sTempTextPrinter.textSpeed--;
        sTextPrinters[printerTemplate->windowId] = sTempTextPrinter;
    }
    else
    {
        sTempTextPrinter.textSpeed = 0;

        // Render all text (up to limit) at once
        for (j = 0; j < 0x400; ++j)
        {
            if (RenderFont(&sTempTextPrinter) == RENDER_FINISH)
                break;
        }

        // All the text is rendered to the window but don't draw it yet.
        if (speed != TEXT_SKIP_DRAW)
            CopyWindowToVram(sTempTextPrinter.printerTemplate.windowId, COPYWIN_GFX);
        sTextPrinters[printerTemplate->windowId].active = FALSE;
    }
    gDisableTextPrinters = FALSE;
}

void RunTextPrinters(void)
{
    unsigned int i;

    if (!gDisableTextPrinters)
    {
        for (i = 0; i < WINDOWS_MAX; ++i)
        {
            if (sTextPrinters[i].active)
            {
                u16 renderCmd = RenderFont(&sTextPrinters[i]);
                switch (renderCmd)
                {
                case RENDER_PRINT:
                    CopyWindowToVram(sTextPrinters[i].printerTemplate.windowId, COPYWIN_GFX);
                case RENDER_UPDATE:
                    if (sTextPrinters[i].callback != NULL)
                        sTextPrinters[i].callback(&sTextPrinters[i].printerTemplate, renderCmd);
                    break;
                case RENDER_FINISH:
                    sTextPrinters[i].active = FALSE;
                    break;
                }
            }
        }
    }
}

bool16 IsTextPrinterActive(u8 id)
{
    return sTextPrinters[id].active;
}

static u32 RenderFont(struct TextPrinter *textPrinter)
{
    u32 ret;
    do
    {
        ret = gFonts[textPrinter->printerTemplate.fontId].fontFunction(textPrinter);
    } while (ret == RENDER_REPEAT);
    return ret;
}

void GenerateFontHalfRowLookupTable(u8 fgColor, u8 bgColor, u8 shadowColor)
{
    u32 fg12, bg12, shadow12;
    u32 temp;

    u16 *current = sFontHalfRowLookupTable;

    sLastTextBgColor = bgColor;
    sLastTextFgColor = fgColor;
    sLastTextShadowColor = shadowColor;

    bg12 = bgColor << 12;
    fg12 = fgColor << 12;
    shadow12 = shadowColor << 12;

    temp = (bgColor << 8) | (bgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (bgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (bgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (fgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (fgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (fgColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (shadowColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (shadowColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (shadowColor << 4) | bgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (bgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (bgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (bgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (fgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (fgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (fgColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (shadowColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (shadowColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (shadowColor << 4) | fgColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (bgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (bgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (bgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (fgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (fgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (fgColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (bgColor << 8) | (shadowColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (fgColor << 8) | (shadowColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;

    temp = (shadowColor << 8) | (shadowColor << 4) | shadowColor;
    *(current++) = (bg12) | temp;
    *(current++) = (fg12) | temp;
    *(current++) = (shadow12) | temp;
}

void SaveTextColors(u8 *fgColor, u8 *bgColor, u8 *shadowColor)
{
    *bgColor = sLastTextBgColor;
    *fgColor = sLastTextFgColor;
    *shadowColor = sLastTextShadowColor;
}

void RestoreTextColors(u8 *fgColor, u8 *bgColor, u8 *shadowColor)
{
    GenerateFontHalfRowLookupTable(*fgColor, *bgColor, *shadowColor);
}

void DecompressGlyphTile(const u16 *src, u32 *dest)
{
    u32 temp;

    temp = *(src++);
    *(dest)++ = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);

    temp = *(src++);
    *(dest++) = ((sFontHalfRowLookupTable[sFontHalfRowOffsets[temp & 0xFF]]) << 16) | (sFontHalfRowLookupTable[sFontHalfRowOffsets[temp >> 8]]);
}

static u8 UNUSED GetLastTextColor(u8 colorType)
{
    switch (colorType)
    {
    case 0:
        return sLastTextFgColor;
    case 2:
        return sLastTextBgColor;
    case 1:
        return sLastTextShadowColor;
    default:
        return 0;
    }
}

inline static void GLYPH_COPY(u8 *windowTiles, u32 widthOffset, u32 j, u32 i, u32 *glyphPixels, u32 width, u32 height)
{
    u32 xAdd, yAdd, pixelData, bits, toOrr, dummyX;
    u8 *dst;

    xAdd = j + width;
    yAdd = i + height;
    dummyX = j;
    for (; i < yAdd; i++)
    {
        pixelData = *glyphPixels++;
        for (j = dummyX; j < xAdd; j++)
        {
            if ((toOrr = pixelData & 0xF))
            {
                dst = windowTiles + (j / 8) * 32 + (j % 8) / 2 + (i / 8) * widthOffset + (i % 8) * 4;
                bits = ((j & 1) * 4);
                *dst = (toOrr << bits) | (*dst & (0xF0 >> bits));
            }
            pixelData >>= 4;
        }
    }
}

void CopyGlyphToWindow(struct TextPrinter *textPrinter)
{
    struct Window *window;
    struct WindowTemplate *template;
    u32 *glyphPixels;
    u32 currX, currY, widthOffset;
    u32 glyphWidth, glyphHeight;
    u8 *windowTiles;

    window = &gWindows[textPrinter->printerTemplate.windowId];
    template = &window->window;

    if ((glyphWidth = (template->width * 8) - textPrinter->printerTemplate.currentX) > gCurGlyph.width)
        glyphWidth = gCurGlyph.width;

    if ((glyphHeight = (template->height * 8) - textPrinter->printerTemplate.currentY) > gCurGlyph.height)
        glyphHeight = gCurGlyph.height;

    currX = textPrinter->printerTemplate.currentX;
    currY = textPrinter->printerTemplate.currentY;
    glyphPixels = gCurGlyph.gfxBufferTop;
    windowTiles = window->tileData;
    widthOffset = template->width * 32;

    if (glyphWidth <= 8)
    {
        if (glyphHeight <= 8)
        {
            GLYPH_COPY(windowTiles, widthOffset, currX, currY, glyphPixels, glyphWidth, glyphHeight);
        }
        else
        {
            GLYPH_COPY(windowTiles, widthOffset, currX, currY, glyphPixels, glyphWidth, 8);
            GLYPH_COPY(windowTiles, widthOffset, currX, currY + 8, glyphPixels + 16, glyphWidth, glyphHeight - 8);
        }
    }
    else
    {
        if (glyphHeight <= 8)
        {
            GLYPH_COPY(windowTiles, widthOffset, currX, currY, glyphPixels, 8, glyphHeight);
            GLYPH_COPY(windowTiles, widthOffset, currX + 8, currY, glyphPixels + 8, glyphWidth - 8, glyphHeight);
        }
        else
        {
            GLYPH_COPY(windowTiles, widthOffset, currX, currY, glyphPixels, 8, 8);
            GLYPH_COPY(windowTiles, widthOffset, currX + 8, currY, glyphPixels + 8, glyphWidth - 8, 8);
            GLYPH_COPY(windowTiles, widthOffset, currX, currY + 8, glyphPixels + 16, 8, glyphHeight - 8);
            GLYPH_COPY(windowTiles, widthOffset, currX + 8, currY + 8, glyphPixels + 24, glyphWidth - 8, glyphHeight - 8);
        }
    }
}

void ClearTextSpan(struct TextPrinter *textPrinter, u32 width)
{
    struct Window *window;
    struct Bitmap pixels_data;

    if (sLastTextBgColor == TEXT_COLOR_TRANSPARENT)
        return;
    {
        window = &gWindows[textPrinter->printerTemplate.windowId];
        pixels_data.pixels = window->tileData;
        pixels_data.width = window->window.width * 8;
        pixels_data.height = window->window.height * 8;

        FillBitmapRect4Bit(
            &pixels_data,
            textPrinter->printerTemplate.currentX,
            textPrinter->printerTemplate.currentY,
            width,
            gCurGlyph.height,
            sLastTextBgColor);
    }
}

static u16 FontFunc_Small(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SMALL;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_Normal(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_NORMAL;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_Short(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SHORT;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_ShortCopy1(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SHORT_COPY_1;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_ShortCopy2(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SHORT_COPY_2;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_ShortCopy3(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SHORT_COPY_3;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_Narrow(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_NARROW;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

static u16 FontFunc_SmallNarrow(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.hasFontIdBeenSet == FALSE)
    {
        textPrinter->subStruct.fontId = FONT_SMALL_NARROW;
        textPrinter->subStruct.hasFontIdBeenSet = TRUE;
    }
    return RenderText(textPrinter);
}

void TextPrinterInitDownArrowCounters(struct TextPrinter *textPrinter)
{
    if (gTextFlags.autoScroll)
    {
        textPrinter->subStruct.autoScrollDelay = 0;
    }
    else
    {
        textPrinter->subStruct.downArrowYPosIdx = 0;
        textPrinter->subStruct.downArrowDelay = 0;
    }
}

void TextPrinterDrawDownArrow(struct TextPrinter *textPrinter)
{
    const u8 *arrowTiles;

    if (gTextFlags.autoScroll)
        return;

    if (textPrinter->subStruct.downArrowDelay != 0)
    {
        textPrinter->subStruct.downArrowDelay--;
        return;
    }

    FillWindowPixelRect(
        textPrinter->printerTemplate.windowId,
        textPrinter->printerTemplate.bgColor << 4 | textPrinter->printerTemplate.bgColor,
        textPrinter->printerTemplate.currentX,
        textPrinter->printerTemplate.currentY,
        8,
        16);

    arrowTiles = gTextFlags.useAlternateDownArrow ? sDarkDownArrowTiles : sDownArrowTiles;

    BlitBitmapRectToWindow(
        textPrinter->printerTemplate.windowId,
        arrowTiles,
        0,
        sDownArrowYCoords[textPrinter->subStruct.downArrowYPosIdx],
        8,
        16,
        textPrinter->printerTemplate.currentX,
        textPrinter->printerTemplate.currentY,
        8,
        16);
    CopyWindowToVram(textPrinter->printerTemplate.windowId, COPYWIN_GFX);

    textPrinter->subStruct.downArrowDelay = 8;
    textPrinter->subStruct.downArrowYPosIdx++;
}

void TextPrinterClearDownArrow(struct TextPrinter *textPrinter)
{
    FillWindowPixelRect(
        textPrinter->printerTemplate.windowId,
        textPrinter->printerTemplate.bgColor << 4 | textPrinter->printerTemplate.bgColor,
        textPrinter->printerTemplate.currentX,
        textPrinter->printerTemplate.currentY,
        8,
        16);
    CopyWindowToVram(textPrinter->printerTemplate.windowId, COPYWIN_GFX);
}

bool16 TextPrinterWaitAutoMode(struct TextPrinter *textPrinter)
{
    if (textPrinter->subStruct.autoScrollDelay == 49)
    {
        return TRUE;
    }
    else
    {
        textPrinter->subStruct.autoScrollDelay++;
        return FALSE;
    }
}

bool16 TextPrinterWaitWithDownArrow(struct TextPrinter *textPrinter)
{
    bool16 result = FALSE;
    if (gTextFlags.autoScroll != 0)
    {
        result = TextPrinterWaitAutoMode(textPrinter);
    }
    else
    {
        TextPrinterDrawDownArrow(textPrinter);
        if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            result = TRUE;
            PlaySE(SE_SELECT);
        }
    }
    return result;
}

bool16 TextPrinterWait(struct TextPrinter *textPrinter)
{
    bool16 result = FALSE;
    if (gTextFlags.autoScroll != 0)
    {
        result = TextPrinterWaitAutoMode(textPrinter);
    }
    else
    {
        if (JOY_NEW(A_BUTTON | B_BUTTON))
        {
            result = TRUE;
            PlaySE(SE_SELECT);
        }
    }
    return result;
}

void DrawDownArrow(u8 windowId, u16 x, u16 y, u8 bgColor, bool8 drawArrow, u8 *counter, u8 *yCoordIndex)
{
    const u8 *arrowTiles;

    if (*counter != 0)
    {
        --*counter;
        return;
    }

    FillWindowPixelRect(windowId, (bgColor << 4) | bgColor, x, y, 0x8, 0x10);
    if (drawArrow == 0)
    {
        arrowTiles = gTextFlags.useAlternateDownArrow ? sDarkDownArrowTiles : sDownArrowTiles;

        BlitBitmapRectToWindow(windowId, arrowTiles, 0, sDownArrowYCoords[*yCoordIndex & 3], 8, 16, x, y - 2, 8, 16);
        CopyWindowToVram(windowId, COPYWIN_GFX);
        *counter = 8;
        ++*yCoordIndex;
    }
}

static u16 RenderText(struct TextPrinter *textPrinter)
{
    u16 currChar;
    u32 scrollSpeed, speed;

    switch (textPrinter->state)
    {
    case RENDER_STATE_HANDLE_CHAR:
        if (JOY_HELD(A_BUTTON | B_BUTTON) && textPrinter->subStruct.hasPrintBeenSpedUp)
            textPrinter->delayCounter = 0;
        else if (textPrinter->delayCounter && textPrinter->textSpeed)
        {
            textPrinter->delayCounter--;
            if (gTextFlags.canABSpeedUpPrint && (JOY_NEW(A_BUTTON | B_BUTTON)))
            {
                textPrinter->subStruct.hasPrintBeenSpedUp = TRUE;
                textPrinter->delayCounter = 0;
            }
            return RENDER_UPDATE;
        }

        if (!(gBattleTypeFlags & BATTLE_TYPE_RECORDED) && gTextFlags.autoScroll)
            textPrinter->delayCounter = 3;
        else
            textPrinter->delayCounter = textPrinter->textSpeed;

        currChar = *textPrinter->printerTemplate.currentChar++;

        switch (currChar)
        {
        case EOS:
            return RENDER_FINISH;
        case CHAR_NEWLINE:
            textPrinter->printerTemplate.currentX = textPrinter->printerTemplate.x;
            textPrinter->printerTemplate.currentY += (gFonts[textPrinter->printerTemplate.fontId].maxLetterHeight + textPrinter->printerTemplate.lineSpacing);
            return RENDER_REPEAT;
        case PLACEHOLDER_BEGIN:
            textPrinter->printerTemplate.currentChar++;
            return RENDER_REPEAT;
        case EXT_CTRL_CODE_BEGIN:
            currChar = *textPrinter->printerTemplate.currentChar++;
            switch (currChar)
            {
            case EXT_CTRL_CODE_COLOR:
                textPrinter->printerTemplate.fgColor = *textPrinter->printerTemplate.currentChar++;
                GenerateFontHalfRowLookupTable(textPrinter->printerTemplate.fgColor, textPrinter->printerTemplate.bgColor, textPrinter->printerTemplate.shadowColor);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_HIGHLIGHT:
                textPrinter->printerTemplate.bgColor = *textPrinter->printerTemplate.currentChar++;
                GenerateFontHalfRowLookupTable(textPrinter->printerTemplate.fgColor, textPrinter->printerTemplate.bgColor, textPrinter->printerTemplate.shadowColor);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_SHADOW:
                textPrinter->printerTemplate.shadowColor = *textPrinter->printerTemplate.currentChar++;
                GenerateFontHalfRowLookupTable(textPrinter->printerTemplate.fgColor, textPrinter->printerTemplate.bgColor, textPrinter->printerTemplate.shadowColor);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_COLOR_HIGHLIGHT_SHADOW:
                textPrinter->printerTemplate.fgColor = textPrinter->printerTemplate.currentChar[0];
                textPrinter->printerTemplate.bgColor = textPrinter->printerTemplate.currentChar[1];
                textPrinter->printerTemplate.shadowColor = textPrinter->printerTemplate.currentChar[2];
                textPrinter->printerTemplate.currentChar += 3;
                GenerateFontHalfRowLookupTable(textPrinter->printerTemplate.fgColor, textPrinter->printerTemplate.bgColor, textPrinter->printerTemplate.shadowColor);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_PALETTE:
                textPrinter->printerTemplate.currentChar++;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_FONT:
                textPrinter->subStruct.fontId = *textPrinter->printerTemplate.currentChar++;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_RESET_FONT:
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_PAUSE:
                textPrinter->delayCounter = *textPrinter->printerTemplate.currentChar++;
                textPrinter->state = RENDER_STATE_PAUSE;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_PAUSE_UNTIL_PRESS:
                textPrinter->state = RENDER_STATE_WAIT;
                if (gTextFlags.autoScroll)
                    textPrinter->subStruct.autoScrollDelay = 0;
                return RENDER_UPDATE;
            case EXT_CTRL_CODE_WAIT_SE:
                textPrinter->state = RENDER_STATE_WAIT_SE;
                return RENDER_UPDATE;
            case EXT_CTRL_CODE_PLAY_BGM:
                currChar = *textPrinter->printerTemplate.currentChar++;
                currChar |= *textPrinter->printerTemplate.currentChar++ << 8;
                PlayBGM(currChar);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_ESCAPE:
                currChar = *textPrinter->printerTemplate.currentChar++ + 0x100;
                break;
            case EXT_CTRL_CODE_PLAY_SE:
                currChar = *textPrinter->printerTemplate.currentChar++;
                currChar |= (*textPrinter->printerTemplate.currentChar++ << 8);
                PlaySE(currChar);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_SHIFT_RIGHT:
                textPrinter->printerTemplate.currentX = textPrinter->printerTemplate.x + *textPrinter->printerTemplate.currentChar;
                textPrinter->printerTemplate.currentChar++;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_SHIFT_DOWN:
                textPrinter->printerTemplate.currentY = textPrinter->printerTemplate.y + *textPrinter->printerTemplate.currentChar;
                textPrinter->printerTemplate.currentChar++;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_FILL_WINDOW:
                FillWindowPixelBuffer(textPrinter->printerTemplate.windowId, PIXEL_FILL(textPrinter->printerTemplate.bgColor));
                textPrinter->printerTemplate.currentX = textPrinter->printerTemplate.x;
                textPrinter->printerTemplate.currentY = textPrinter->printerTemplate.y;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_PAUSE_MUSIC:
                m4aMPlayStop(&gMPlayInfo_BGM);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_RESUME_MUSIC:
                m4aMPlayContinue(&gMPlayInfo_BGM);
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_CLEAR:
            {
                s32 spc = *textPrinter->printerTemplate.currentChar++;
                if (spc > 0)
                {
                    ClearTextSpan(textPrinter, spc);
                    textPrinter->printerTemplate.currentX += spc;
                    return RENDER_PRINT;
                }
                return RENDER_REPEAT;
            }
            case EXT_CTRL_CODE_SKIP:
                textPrinter->printerTemplate.currentX = *textPrinter->printerTemplate.currentChar++ + textPrinter->printerTemplate.x;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_CLEAR_TO:
                {
                    s32 x, spc;
                    x = *textPrinter->printerTemplate.currentChar++ + textPrinter->printerTemplate.x;
                    spc = x - textPrinter->printerTemplate.currentX;
                    if (spc > 0)
                    {
                        ClearTextSpan(textPrinter, spc);
                        textPrinter->printerTemplate.currentX += spc;
                        return RENDER_PRINT;
                    }
                }
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_MIN_LETTER_SPACING:
                textPrinter->minLetterSpacing = *textPrinter->printerTemplate.currentChar++;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_JPN:
                textPrinter->japanese = TRUE;
                return RENDER_REPEAT;
            case EXT_CTRL_CODE_ENG:
                textPrinter->japanese = FALSE;
                return RENDER_REPEAT;
            }
            break;
        case CHAR_PROMPT_CLEAR:
            textPrinter->state = RENDER_STATE_CLEAR;
            TextPrinterInitDownArrowCounters(textPrinter);
            return RENDER_UPDATE;
        case CHAR_PROMPT_SCROLL:
            textPrinter->state = RENDER_STATE_SCROLL_START;
            TextPrinterInitDownArrowCounters(textPrinter);
            return RENDER_UPDATE;
        case CHAR_EXTRA_SYMBOL:
            currChar = *textPrinter->printerTemplate.currentChar + 0x100;
            textPrinter->printerTemplate.currentChar++;
            break;
        case CHAR_KEYPAD_ICON:
            currChar = *textPrinter->printerTemplate.currentChar++;
            gCurGlyph.width = DrawKeypadIcon(textPrinter->printerTemplate.windowId, currChar, textPrinter->printerTemplate.currentX, textPrinter->printerTemplate.currentY);
            textPrinter->printerTemplate.currentX += gCurGlyph.width + textPrinter->printerTemplate.letterSpacing;
            return RENDER_PRINT;
        }

        switch (textPrinter->subStruct.fontId)
        {
        case FONT_SMALL:
            DecompressGlyph_Small(currChar, textPrinter->japanese);
            break;
        case FONT_NORMAL:
            DecompressGlyph_Normal(currChar, textPrinter->japanese);
            break;
        case FONT_SHORT:
        case FONT_SHORT_COPY_1:
        case FONT_SHORT_COPY_2:
        case FONT_SHORT_COPY_3:
            DecompressGlyph_Short(currChar, textPrinter->japanese);
            break;
        case FONT_NARROW:
            DecompressGlyph_Narrow(currChar, textPrinter->japanese);
            break;
        case FONT_SMALL_NARROW:
            DecompressGlyph_SmallNarrow(currChar, textPrinter->japanese);
            break;
        case FONT_BRAILLE:
            break;
        }

        CopyGlyphToWindow(textPrinter);

        if (textPrinter->minLetterSpacing > 0)
        {
            s32 spc;
            textPrinter->printerTemplate.currentX += gCurGlyph.width;
            spc = textPrinter->minLetterSpacing - gCurGlyph.width;
            if (spc > 0)
            {
                ClearTextSpan(textPrinter, spc);
                textPrinter->printerTemplate.currentX += spc;
            }
        }
        else
        {
            if (textPrinter->japanese)
                textPrinter->printerTemplate.currentX += (gCurGlyph.width + textPrinter->printerTemplate.letterSpacing);
            else
                textPrinter->printerTemplate.currentX += gCurGlyph.width;
        }
        return RENDER_PRINT;
    case RENDER_STATE_WAIT:
        if (TextPrinterWait(textPrinter))
            textPrinter->state = RENDER_STATE_HANDLE_CHAR;
        return RENDER_UPDATE;
    case RENDER_STATE_CLEAR:
        if (TextPrinterWaitWithDownArrow(textPrinter))
        {
            FillWindowPixelBuffer(textPrinter->printerTemplate.windowId, PIXEL_FILL(textPrinter->printerTemplate.bgColor));
            textPrinter->printerTemplate.currentX = textPrinter->printerTemplate.x;
            textPrinter->printerTemplate.currentY = textPrinter->printerTemplate.y;
            textPrinter->state = RENDER_STATE_HANDLE_CHAR;
        }
        return RENDER_UPDATE;
    case RENDER_STATE_SCROLL_START:
        if (TextPrinterWaitWithDownArrow(textPrinter))
        {
            TextPrinterClearDownArrow(textPrinter);
            textPrinter->scrollDistance = gFonts[textPrinter->printerTemplate.fontId].maxLetterHeight + textPrinter->printerTemplate.lineSpacing;
            textPrinter->printerTemplate.currentX = textPrinter->printerTemplate.x;
            textPrinter->state = RENDER_STATE_SCROLL;
        }
        return RENDER_UPDATE;
    case RENDER_STATE_SCROLL:
        if (textPrinter->scrollDistance)
        {
            scrollSpeed = GetPlayerTextSpeed();
            speed = sWindowVerticalScrollSpeeds[scrollSpeed];
            if (textPrinter->scrollDistance < speed)
            {
                ScrollWindow(textPrinter->printerTemplate.windowId, 0, textPrinter->scrollDistance, PIXEL_FILL(textPrinter->printerTemplate.bgColor));
                textPrinter->scrollDistance = 0;
            }
            else
            {
                ScrollWindow(textPrinter->printerTemplate.windowId, 0, speed, PIXEL_FILL(textPrinter->printerTemplate.bgColor));
                textPrinter->scrollDistance -= speed;
            }
            CopyWindowToVram(textPrinter->printerTemplate.windowId, COPYWIN_GFX);
        }
        else
        {
            textPrinter->state = RENDER_STATE_HANDLE_CHAR;
        }
        return RENDER_UPDATE;
    case RENDER_STATE_WAIT_SE:
        if (!IsSEPlaying())
            textPrinter->state = RENDER_STATE_HANDLE_CHAR;
        return RENDER_UPDATE;
    case RENDER_STATE_PAUSE:
        if (textPrinter->delayCounter != 0)
            textPrinter->delayCounter--;
        else
            textPrinter->state = RENDER_STATE_HANDLE_CHAR;
        return RENDER_UPDATE;
    }

    return RENDER_FINISH;
}

static u32 UNUSED GetStringWidthFixedWidthFont(const u8 *str, u8 fontId, u8 letterSpacing)
{
    int i;
    u8 width;
    int temp;
    int temp2;
    u8 line;
    int strPos;
    u8 lineWidths[8];
    const u8 *strLocal;

    for (i = 0; i < ARRAY_COUNT(lineWidths); i++)
        lineWidths[i] = 0;

    width = 0;
    line = 0;
    strLocal = str;
    strPos = 0;

    do
    {
        temp = strLocal[strPos++];
        switch (temp)
        {
        case CHAR_NEWLINE:
        case EOS:
            lineWidths[line] = width;
            width = 0;
            line++;
            break;
        case EXT_CTRL_CODE_BEGIN:
            temp2 = strLocal[strPos++];
            switch (temp2)
            {
            case EXT_CTRL_CODE_COLOR_HIGHLIGHT_SHADOW:
                ++strPos;
            case EXT_CTRL_CODE_PLAY_BGM:
            case EXT_CTRL_CODE_PLAY_SE:
                ++strPos;
            case EXT_CTRL_CODE_COLOR:
            case EXT_CTRL_CODE_HIGHLIGHT:
            case EXT_CTRL_CODE_SHADOW:
            case EXT_CTRL_CODE_PALETTE:
            case EXT_CTRL_CODE_FONT:
            case EXT_CTRL_CODE_PAUSE:
            case EXT_CTRL_CODE_ESCAPE:
            case EXT_CTRL_CODE_SHIFT_RIGHT:
            case EXT_CTRL_CODE_SHIFT_DOWN:
            case EXT_CTRL_CODE_CLEAR:
            case EXT_CTRL_CODE_SKIP:
            case EXT_CTRL_CODE_CLEAR_TO:
            case EXT_CTRL_CODE_MIN_LETTER_SPACING:
                ++strPos;
                break;
            case EXT_CTRL_CODE_RESET_FONT:
            case EXT_CTRL_CODE_PAUSE_UNTIL_PRESS:
            case EXT_CTRL_CODE_WAIT_SE:
            case EXT_CTRL_CODE_FILL_WINDOW:
            case EXT_CTRL_CODE_JPN:
            case EXT_CTRL_CODE_ENG:
            default:
                break;
            }
            break;
        case CHAR_DYNAMIC:
        case PLACEHOLDER_BEGIN:
            ++strPos;
            break;
        case CHAR_PROMPT_SCROLL:
        case CHAR_PROMPT_CLEAR:
            break;
        case CHAR_KEYPAD_ICON:
        case CHAR_EXTRA_SYMBOL:
            ++strPos;
        default:
            ++width;
            break;
        }
    } while (temp != EOS);

    for (width = 0, strPos = 0; strPos < 8; ++strPos)
    {
        if (width < lineWidths[strPos])
            width = lineWidths[strPos];
    }

    return (u8)(GetFontAttribute(fontId, FONTATTR_MAX_LETTER_WIDTH) + letterSpacing) * width;
}

static u32 (*GetFontWidthFunc(u8 fontId))(u16, bool32)
{
    u32 i;

    for (i = 0; i < ARRAY_COUNT(sGlyphWidthFuncs); ++i)
    {
        if (fontId == sGlyphWidthFuncs[i].fontId)
            return sGlyphWidthFuncs[i].func;
    }

    return NULL;
}

u32 GetStringWidth(u8 fontId, const u8 *str, s16 letterSpacing)
{
    bool32 isJapanese;
    u32 size, size_max, spc_x;
    u32 (*func)(u16 fontId, bool32 isJapanese);
    u32 width;
    const u8 *cp;

    isJapanese = 0;
    width = 0;

    func = GetFontWidthFunc(fontId);
    if (func == NULL)
        return 0;

    if (letterSpacing < 0)
        spc_x = gFonts[fontId].letterSpacing;
    else
        spc_x = letterSpacing;

    size = size_max = 0;
    cp = NULL;

    while (*str != EOS)
    {
        switch (*str)
        {
        case CHAR_NEWLINE:
			if(size > size_max)
				size_max = size;
            size = 0;
            break;
        case PLACEHOLDER_BEGIN:
            switch (*++str)
            {
            case PLACEHOLDER_ID_STRING_VAR_1:
                cp = gStringVar1;
                break;
            case PLACEHOLDER_ID_STRING_VAR_2:
                cp = gStringVar2;
                break;
            case PLACEHOLDER_ID_STRING_VAR_3:
                cp = gStringVar3;
                break;
            default:
                return 0;
            }
        case CHAR_DYNAMIC:
            if (cp == NULL)
                cp = DynamicPlaceholderTextUtil_GetPlaceholderPtr(*++str);
            while (*cp != EOS)
            {
                u32 w;
                w = func(*cp++, isJapanese);
                if (width > 0)
                {
                    size += width > w ? width : w;
                }
                else
                {
                    size += w;
                    if (isJapanese && str[1] != EOS)
                        size += spc_x;
                }
            }
            cp = NULL;
            break;
        case CHAR_PROMPT_SCROLL:
        case CHAR_PROMPT_CLEAR:
            break;
        case EXT_CTRL_CODE_BEGIN:
            switch (*++str)
            {
            case EXT_CTRL_CODE_COLOR_HIGHLIGHT_SHADOW:
                ++str;
            case EXT_CTRL_CODE_PLAY_BGM:
            case EXT_CTRL_CODE_PLAY_SE:
                ++str;
            case EXT_CTRL_CODE_COLOR:
            case EXT_CTRL_CODE_HIGHLIGHT:
            case EXT_CTRL_CODE_SHADOW:
            case EXT_CTRL_CODE_PALETTE:
            case EXT_CTRL_CODE_PAUSE:
            case EXT_CTRL_CODE_ESCAPE:
            case EXT_CTRL_CODE_SHIFT_RIGHT:
            case EXT_CTRL_CODE_SHIFT_DOWN:
                ++str;
            case EXT_CTRL_CODE_RESET_FONT:
            case EXT_CTRL_CODE_PAUSE_UNTIL_PRESS:
            case EXT_CTRL_CODE_WAIT_SE:
            case EXT_CTRL_CODE_FILL_WINDOW:
                break;
            case EXT_CTRL_CODE_FONT:
                func = GetFontWidthFunc(*++str);
                if (func == NULL)
                    return 0;
                if (letterSpacing < 0)
                    spc_x = GetFontAttribute(*str, FONTATTR_LETTER_SPACING);
                break;
            case EXT_CTRL_CODE_CLEAR:
                size += *++str;
                break;
            case EXT_CTRL_CODE_SKIP:
                size = *++str;
                break;
            case EXT_CTRL_CODE_CLEAR_TO:
            {
                u32 x = *++str;
                if (x > size)
                    size = x;
                break;
            }
            case EXT_CTRL_CODE_MIN_LETTER_SPACING:
                width = *++str;
                break;
            case EXT_CTRL_CODE_JPN:
                isJapanese = 1;
                break;
            case EXT_CTRL_CODE_ENG:
                isJapanese = 0;
                break;
            default:
                break;
            }
            break;
        case CHAR_KEYPAD_ICON:
        case CHAR_EXTRA_SYMBOL:
        {
            u32 w;
            if (*str == CHAR_EXTRA_SYMBOL)
                w = func(*++str + 0x100, isJapanese);
            else
                w = GetKeypadIconWidth(*++str);

            if (width > 0)
            {
                size += width > w ? width : w;
            }
            else
            {
                size += w;
                if (isJapanese && str[1] != EOS)
                    size += spc_x;
            }
            break;
        }
        default:
        {
            u32 w;
            w = func(*str, isJapanese);
            if (width > 0)
            {
                size += width > w ? width : w;
            }
            else
            {
                size += w;
                if (isJapanese && str[1] != EOS)
                    size += spc_x;
            }
            break;
        }
        }
        ++str;
    }

    if (size > size_max)
        return size;
    return size_max;
}

u16 RenderTextHandleBold(u8 *pixels, u8 fontId, u8 *str)
{
	u32	i;
	u16	fcode,fcodesub;
	u8*	strLocal;
	u8	fgColor,fcol_backup;
	u8	bgColor,bcol_backup;
	u8	shadowColor,scol_backup;

    SaveTextColors(&fcol_backup,&bcol_backup,&scol_backup);

    fgColor = TEXT_COLOR_WHITE;
    bgColor = TEXT_COLOR_TRANSPARENT;
    shadowColor = TEXT_COLOR_LIGHT_GRAY;

    GenerateFontHalfRowLookupTable(TEXT_COLOR_WHITE, TEXT_COLOR_TRANSPARENT, TEXT_COLOR_LIGHT_GRAY);
    strLocal = str;
    i = 0;

    do
    {
        fcode = strLocal[i++];
        switch (fcode)
        {
        case EOS:
        case CHAR_NEWLINE:
            break;
        case PLACEHOLDER_BEGIN:
        case CHAR_DYNAMIC:
            ++i;
        case CHAR_PROMPT_CLEAR:
        case CHAR_PROMPT_SCROLL:
            break;
        case EXT_CTRL_CODE_BEGIN:
            fcodesub = strLocal[i++];
            switch (fcodesub)
            {
            case EXT_CTRL_CODE_COLOR_HIGHLIGHT_SHADOW:
                fgColor = strLocal[i++];
                bgColor = strLocal[i++];
                shadowColor = strLocal[i++];
                GenerateFontHalfRowLookupTable(fgColor, bgColor, shadowColor);
                continue;
            case EXT_CTRL_CODE_COLOR:
                fgColor = strLocal[i++];
                GenerateFontHalfRowLookupTable(fgColor, bgColor, shadowColor);
                continue;
            case EXT_CTRL_CODE_HIGHLIGHT:
                bgColor = strLocal[i++];
                GenerateFontHalfRowLookupTable(fgColor, bgColor, shadowColor);
                continue;
            case EXT_CTRL_CODE_SHADOW:
                shadowColor = strLocal[i++];
                GenerateFontHalfRowLookupTable(fgColor, bgColor, shadowColor);
                continue;
            case EXT_CTRL_CODE_FONT:
                fontId = strLocal[i++];
                break;
            case EXT_CTRL_CODE_PLAY_BGM:
            case EXT_CTRL_CODE_PLAY_SE:
                ++i;
            case EXT_CTRL_CODE_PALETTE:
            case EXT_CTRL_CODE_PAUSE:
            case EXT_CTRL_CODE_ESCAPE:
            case EXT_CTRL_CODE_SHIFT_RIGHT:
            case EXT_CTRL_CODE_SHIFT_DOWN:
                ++i;
            case EXT_CTRL_CODE_RESET_FONT:
            case EXT_CTRL_CODE_PAUSE_UNTIL_PRESS:
            case EXT_CTRL_CODE_WAIT_SE:
            case EXT_CTRL_CODE_FILL_WINDOW:
                break;
            case EXT_CTRL_CODE_CLEAR:
            case EXT_CTRL_CODE_SKIP:
            case EXT_CTRL_CODE_CLEAR_TO:
            case EXT_CTRL_CODE_MIN_LETTER_SPACING:
                ++i;

            case EXT_CTRL_CODE_JPN:
            case EXT_CTRL_CODE_ENG:
            default:
                break;
            }
            break;
        case CHAR_KEYPAD_ICON:
        case CHAR_EXTRA_SYMBOL:
            break;
        default:
            switch (fontId)
            {
            case FONT_BOLD:
                DecompressGlyph_Bold(fcode);
                break;
            case FONT_NORMAL:
            default:
                DecompressGlyph_Normal(fcode, TRUE);
                break;
            }
            CpuCopy32(gCurGlyph.gfxBufferTop, pixels, 0x20);
            CpuCopy32(gCurGlyph.gfxBufferBottom, pixels + 0x20, 0x20);
            pixels += 0x40;
            break;
        }
    }
    while (fcode != EOS);

    RestoreTextColors(&fcol_backup,&bcol_backup,&scol_backup);
    return 1;
}

u8 DrawKeypadIcon(u8 windowId, u8 keypadIconId, u16 x, u16 y)
{
    BlitBitmapRectToWindow(
        windowId,
        sKeypadIconTiles + (sKeypadIcons[keypadIconId].tileOffset * 0x20),
        0,
        0,
        0x80,
        0x80,
        x,
        y,
        sKeypadIcons[keypadIconId].width,
        sKeypadIcons[keypadIconId].height);
    return sKeypadIcons[keypadIconId].width;
}

u8 GetKeypadIconTileOffset(u8 keypadIconId)
{
    return sKeypadIcons[keypadIconId].tileOffset;
}

u8 GetKeypadIconWidth(u8 keypadIconId)
{
    return sKeypadIcons[keypadIconId].width;
}

u8 GetKeypadIconHeight(u8 keypadIconId)
{
    return sKeypadIcons[keypadIconId].height;
}

u8 GetFontAttribute(u8 fontId, u8 attributeId)
{
    u8 result = 0;
    switch (attributeId)
    {
        case FONTATTR_MAX_LETTER_WIDTH:
            result = gFonts[fontId].maxLetterWidth;
            break;
        case FONTATTR_MAX_LETTER_HEIGHT:
            result = gFonts[fontId].maxLetterHeight;
            break;
        case FONTATTR_LETTER_SPACING:
            result = gFonts[fontId].letterSpacing;
            break;
        case FONTATTR_LINE_SPACING:
            result = gFonts[fontId].lineSpacing;
            break;
        case FONTATTR_COLOR_FOREGROUND:
            result = gFonts[fontId].fgColor;
            break;
        case FONTATTR_COLOR_BACKGROUND:
            result = gFonts[fontId].bgColor;
            break;
        case FONTATTR_COLOR_SHADOW:
            result = gFonts[fontId].shadowColor;
            break;
    }
    return result;
}

u8 GetMenuCursorDimensionByFont(u8 fontId, u8 whichDimension)
{
    return sMenuCursorDimensions[fontId][whichDimension];
}

static void DecompressGlyph_Small(u16 glyphId, bool32 isJapanese)
{
    const u16 *glyphs;

    if (isJapanese == 1)
    {
        glyphs = gFontSmallJapaneseGlyphs + (0x100 * (glyphId >> 0x4)) + (0x8 * (glyphId & 0xF));
        DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
        DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);
        gCurGlyph.width = 8;
        gCurGlyph.height = 12;
    }
    else
    {
        glyphs = gFontSmallLatinGlyphs + (0x20 * glyphId);
        gCurGlyph.width = gFontSmallLatinGlyphWidths[glyphId];

        if (gCurGlyph.width <= 8)
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
        }
        else
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
            DecompressGlyphTile(glyphs + 0x18, gCurGlyph.gfxBufferBottom + 8);
        }

        gCurGlyph.height = 13;
    }
}

static u32 GetGlyphWidth_Small(u16 glyphId, bool32 isJapanese)
{
    if (isJapanese)
        return 8;
    else
        return gFontSmallLatinGlyphWidths[glyphId];
}

static void DecompressGlyph_Narrow(u16 glyphId, bool32 isJapanese)
{
    const u16 *glyphs;

    if (isJapanese)
    {
        glyphs = gFontNormalJapaneseGlyphs + (0x100 * (glyphId >> 0x4)) + (0x8 * (glyphId % 0x10));
        DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
        DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);
        gCurGlyph.width = 8;
        gCurGlyph.height = 15;
    }
    else
    {
        glyphs = gFontNarrowLatinGlyphs + (0x20 * glyphId);
        gCurGlyph.width = gFontNarrowLatinGlyphWidths[glyphId];

        if (gCurGlyph.width <= 8)
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
        }
        else
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
            DecompressGlyphTile(glyphs + 0x18, gCurGlyph.gfxBufferBottom + 8);
        }

        gCurGlyph.height = 15;
    }
}

static u32 GetGlyphWidth_Narrow(u16 glyphId, bool32 isJapanese)
{
    if (isJapanese)
        return 8;
    else
        return gFontNarrowLatinGlyphWidths[glyphId];
}

static void DecompressGlyph_SmallNarrow(u16 glyphId, bool32 isJapanese)
{
    const u16 *glyphs;

    if (isJapanese)
    {
        glyphs = gFontSmallJapaneseGlyphs + (0x100 * (glyphId >> 0x4)) + (0x8 * (glyphId & 0xF));
        DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
        DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);
        gCurGlyph.width = 8;
        gCurGlyph.height = 12;
    }
    else
    {
        glyphs = gFontSmallNarrowLatinGlyphs + (0x20 * glyphId);
        gCurGlyph.width = gFontSmallNarrowLatinGlyphWidths[glyphId];

        if (gCurGlyph.width <= 8)
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
        }
        else
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
            DecompressGlyphTile(glyphs + 0x18, gCurGlyph.gfxBufferBottom + 8);
        }

        gCurGlyph.height = 12;
    }
}

static u32 GetGlyphWidth_SmallNarrow(u16 glyphId, bool32 isJapanese)
{
    if (isJapanese)
        return 8;
    else
        return gFontSmallNarrowLatinGlyphWidths[glyphId];
}

static void DecompressGlyph_Short(u16 glyphId, bool32 isJapanese)
{
    const u16 *glyphs;

    if (isJapanese)
    {
        glyphs = gFontShortJapaneseGlyphs + (0x100 * (glyphId >> 0x3)) + (0x10 * (glyphId & 0x7));
        DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
        DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
        DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);    // gCurGlyph + 0x20
        DecompressGlyphTile(glyphs + 0x88, gCurGlyph.gfxBufferBottom + 8);    // gCurGlyph + 0x60
        gCurGlyph.width = gFontShortJapaneseGlyphWidths[glyphId];
        gCurGlyph.height = 14;
    }
    else
    {
        glyphs = gFontShortLatinGlyphs + (0x20 * glyphId);
        gCurGlyph.width = gFontShortLatinGlyphWidths[glyphId];

        if (gCurGlyph.width <= 8)
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
        }
        else
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
            DecompressGlyphTile(glyphs + 0x18, gCurGlyph.gfxBufferBottom + 8);
        }

        gCurGlyph.height = 14;
    }
}

static u32 GetGlyphWidth_Short(u16 glyphId, bool32 isJapanese)
{
    if (isJapanese)
        return gFontShortJapaneseGlyphWidths[glyphId];
    else
        return gFontShortLatinGlyphWidths[glyphId];
}

static void DecompressGlyph_Normal(u16 glyphId, bool32 isJapanese)
{
    const u16 *glyphs;

    if (isJapanese)
    {
        glyphs = gFontNormalJapaneseGlyphs + (0x100 * (glyphId >> 0x4)) + (0x8 * (glyphId % 0x10));
        DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
        DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);
        gCurGlyph.width = 8;
        gCurGlyph.height = 15;
    }
    else
    {
        glyphs = gFontNormalLatinGlyphs + (0x20 * glyphId);
        gCurGlyph.width = gFontNormalLatinGlyphWidths[glyphId];

        if (gCurGlyph.width <= 8)
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
        }
        else
        {
            DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
            DecompressGlyphTile(glyphs + 0x8, gCurGlyph.gfxBufferTop + 8);
            DecompressGlyphTile(glyphs + 0x10, gCurGlyph.gfxBufferBottom);
            DecompressGlyphTile(glyphs + 0x18, gCurGlyph.gfxBufferBottom + 8);
        }

        gCurGlyph.height = 15;
    }
}

static u32 GetGlyphWidth_Normal(u16 glyphId, bool32 isJapanese)
{
    if (isJapanese)
        return 8;
    else
        return gFontNormalLatinGlyphWidths[glyphId];
}

static void DecompressGlyph_Bold(u16 glyphId)
{
    const u16 *glyphs;

    glyphs = sFontBoldJapaneseGlyphs + (0x100 * (glyphId >> 4)) + (0x8 * (glyphId & 0xF));
    DecompressGlyphTile(glyphs, gCurGlyph.gfxBufferTop);
    DecompressGlyphTile(glyphs + 0x80, gCurGlyph.gfxBufferBottom);
    gCurGlyph.width = 8;
    gCurGlyph.height = 12;
}
