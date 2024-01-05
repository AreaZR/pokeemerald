#include "global.h"
#include "battle.h"
#include "battle_transition.h"
#include "battle_transition_frontier.h"
#include "bg.h"
#include "decompress.h"
#include "event_object_movement.h"
#include "field_camera.h"
#include "field_effect.h"
#include "field_weather.h"
#include "gpu_regs.h"
#include "main.h"
#include "malloc.h"
#include "overworld.h"
#include "palette.h"
#include "random.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "trig.h"
#include "util.h"
#include "constants/field_effects.h"
#include "constants/songs.h"
#include "constants/trainers.h"
#include "constants/rgb.h"

#define PALTAG_UNUSED_MUGSHOT 0x100A

#define B_TRANS_DMA_FLAGS (1 | ((DMA_SRC_INC | DMA_DEST_FIXED | DMA_REPEAT | DMA_16BIT | DMA_START_HBLANK | DMA_ENABLE) << 16))

// Used by each transition task to determine which of its functions to call
#define tState          data[0]

// Below are data defines for InitBlackWipe and UpdateBlackWipe, for the TransitionData data array.
// These will be re-used by any transitions that use these functions.
#define tWipeStartX line.init_x
#define tWipeStartY line.init_y
#define tWipeCurrX  line.x0
#define tWipeCurrY  line.y0
#define tWipeEndX   line.x1
#define tWipeEndY   line.y1
#define tWipeXMove  line.mx
#define tWipeYMove  line.my
#define tWipeXDist  line.xdiff
#define tWipeYDist  line.ydiff
#define tWipeTemp   line.work

#define SET_TILE(ptr, posY, posX, tile) \
    ptr[((posY) << 5) + (posX)] = tile | (15 << 12)

struct LINEWORK
{
	s16 init_x;
	s16 init_y;
	s16 x0;
	s16 y0;
	s16 x1;
	s16 y1;
	s16 mx;
	s16 my;
	s16 xdiff;
	s16 ydiff;
	s16 work;
};

struct TransitionData
{
    vu8 VBlank_DMA;
    u16 WININ;
    u16 WINOUT;
    u16 WIN0H;
    u16 WIN0V;
    u16 unused1;
    u16 unused2;
    u16 BLDCNT;
    u16 BLDALPHA;
    u16 BLDY;
    s16 cameraX;
    s16 cameraY;
    s16 BG0HOFS_Lower;
    s16 BG0HOFS_Upper;
    s16 BG0VOFS; // used but not set
    s16 unused3;
    s16 counter;
    struct LINEWORK  line;
};

struct RectangularSpiralLine
{
    u8 state;
    s16 position;
    u8 moveIdx;
    s16 reboundPosition;
    bool8 outward;
};

typedef bool8 (*TransitionStateFunc)(struct Task *task);
typedef bool8 (*TransitionSpriteCallback)(struct Sprite *sprite);

static bool8 Transition_StartIntro(struct Task *);
static bool8 Transition_WaitForIntro(struct Task *);
static bool8 Transition_StartMain(struct Task *);
static bool8 Transition_WaitForMain(struct Task *);

static void LaunchBattleTransitionTask(u8);
static void Task_BattleTransition(u8);
static void Task_Intro(u8);
static void Task_Blur(u8);
static void Task_Swirl(u8);
static void Task_Shuffle(u8);
static void Task_BigPokeball(u8);
static void Task_PokeballsTrail(u8);
static void Task_ClockwiseWipe(u8);
static void Task_Ripple(u8);
static void Task_Wave(u8);
static void Task_Slice(u8);
static void Task_WhiteBarsFade(u8);
static void Task_GridSquares(u8);
static void Task_AngledWipes(u8);
static void Task_Sidney(u8);
static void Task_Phoebe(u8);
static void Task_Glacia(u8);
static void Task_Drake(u8);
static void Task_Champion(u8);
static void Task_Aqua(u8);
static void Task_Magma(u8);
static void Task_Regice(u8);
static void Task_Registeel(u8);
static void Task_Regirock(u8);
static void Task_Kyogre(u8);
static void Task_Groudon(u8);
static void Task_Rayquaza(u8);
static void Task_ShredSplit(u8);
static void Task_Blackhole(u8);
static void Task_BlackholePulsate(u8);
static void Task_RectangularSpiral(u8);
static void Task_FrontierLogoWiggle(u8);
static void Task_FrontierLogoWave(u8);
static void Task_FrontierSquares(u8);
static void Task_FrontierSquaresScroll(u8);
static void Task_FrontierSquaresSpiral(u8);
static void VBlankCB_BattleTransition(void);
static void VBlankCB_Swirl(void);
static void HBlankCB_Swirl(void);
static void VBlankCB_Shuffle(void);
static void HBlankCB_Shuffle(void);
static void VBlankCB_PatternWeave(void);
static void VBlankCB_CircularMask(void);
static void VBlankCB_ClockwiseWipe(void);
static void VBlankCB_Ripple(void);
static void HBlankCB_Ripple(void);
static void VBlankCB_FrontierLogoWave(void);
static void HBlankCB_FrontierLogoWave(void);
static void VBlankCB_Wave(void);
static void VBlankCB_Slice(void);
static void HBlankCB_Slice(void);
static void VBlankCB_WhiteBarsFade(void);
static void VBlankCB_WhiteBarsFade_Blend(void);
static void HBlankCB_WhiteBarsFade(void);
static void VBlankCB_AngledWipes(void);
static void VBlankCB_Rayquaza(void);
static bool8 Blur_Init(struct Task *);
static bool8 Blur_Main(struct Task *);
static bool8 Blur_End(struct Task *);
static bool8 Swirl_Init(struct Task *);
static bool8 Swirl_End(struct Task *);
static bool8 Shuffle_Init(struct Task *);
static bool8 Shuffle_End(struct Task *);
static bool8 Aqua_Init(struct Task *);
static bool8 Aqua_SetGfx(struct Task *);
static bool8 Magma_Init(struct Task *);
static bool8 Magma_SetGfx(struct Task *);
static bool8 FramesCountdown(struct Task *);
static bool8 Regi_Init(struct Task *);
static bool8 Regice_SetGfx(struct Task *);
static bool8 Registeel_SetGfx(struct Task *);
static bool8 Regirock_SetGfx(struct Task *);
static bool8 WeatherTrio_BgFadeBlack(struct Task *);
static bool8 WeatherTrio_WaitFade(struct Task *);
static bool8 Kyogre_Init(struct Task *);
static bool8 Kyogre_PaletteFlash(struct Task *);
static bool8 Kyogre_PaletteBrighten(struct Task *);
static bool8 Groudon_Init(struct Task *);
static bool8 Groudon_PaletteFlash(struct Task *);
static bool8 Groudon_PaletteBrighten(struct Task *);
static bool8 WeatherDuo_FadeOut(struct Task *);
static bool8 WeatherDuo_End(struct Task *);
static bool8 BigPokeball_Init(struct Task *);
static bool8 BigPokeball_SetGfx(struct Task *);
static bool8 PatternWeave_Blend1(struct Task *);
static bool8 PatternWeave_Blend2(struct Task *);
static bool8 PatternWeave_FinishAppear(struct Task *);
static bool8 PatternWeave_CircularMask(struct Task *);
static bool8 PokeballsTrail_Init(struct Task *);
static bool8 PokeballsTrail_Main(struct Task *);
static bool8 PokeballsTrail_End(struct Task *);
static bool8 ClockwiseWipe_Init(struct Task *);
static bool8 ClockwiseWipe_TopRight(struct Task *);
static bool8 ClockwiseWipe_Right(struct Task *);
static bool8 ClockwiseWipe_Bottom(struct Task *);
static bool8 ClockwiseWipe_Left(struct Task *);
static bool8 ClockwiseWipe_TopLeft(struct Task *);
static bool8 ClockwiseWipe_End(struct Task *);
static bool8 Ripple_Init(struct Task *);
static bool8 Ripple_Main(struct Task *);
static bool8 Wave_Init(struct Task *);
static bool8 Wave_Main(struct Task *);
static bool8 Wave_End(struct Task *);
static bool8 Slice_Init(struct Task *);
static bool8 Slice_Main(struct Task *);
static bool8 Slice_End(struct Task *);
static bool8 WhiteBarsFade_Init(struct Task *);
static bool8 WhiteBarsFade_StartBars(struct Task *);
static bool8 WhiteBarsFade_WaitBars(struct Task *);
static bool8 WhiteBarsFade_BlendToBlack(struct Task *);
static bool8 WhiteBarsFade_End(struct Task *);
static bool8 GridSquares_Init(struct Task *);
static bool8 GridSquares_Main(struct Task *);
static bool8 GridSquares_End(struct Task *);
static bool8 AngledWipes_Init(struct Task *);
static bool8 AngledWipes_SetWipeData(struct Task *);
static bool8 AngledWipes_DoWipe(struct Task *);
static bool8 AngledWipes_TryEnd(struct Task *);
static bool8 AngledWipes_StartNext(struct Task *);
static bool8 ShredSplit_Init(struct Task *);
static bool8 ShredSplit_Main(struct Task *);
static bool8 ShredSplit_BrokenCheck(struct Task *);
static bool8 ShredSplit_End(struct Task *);
static bool8 Blackhole_Init(struct Task *);
static bool8 Blackhole_Vibrate(struct Task *);
static bool8 Blackhole_GrowEnd(struct Task *);
static bool8 BlackholePulsate_Main(struct Task *);
static bool8 FrontierLogoWiggle_Init(struct Task *);
static bool8 FrontierLogoWiggle_SetGfx(struct Task *);
static bool8 FrontierLogoWave_Init(struct Task *);
static bool8 FrontierLogoWave_SetGfx(struct Task *);
static bool8 FrontierLogoWave_InitScanline(struct Task *);
static bool8 FrontierLogoWave_Main(struct Task *);
static bool8 Rayquaza_Init(struct Task *);
static bool8 Rayquaza_SetGfx(struct Task *);
static bool8 Rayquaza_PaletteFlash(struct Task *);
static bool8 Rayquaza_FadeToBlack(struct Task *);
static bool8 Rayquaza_WaitFade(struct Task *);
static bool8 Rayquaza_SetBlack(struct Task *);
static bool8 Rayquaza_TriRing(struct Task *);
static bool8 FrontierSquares_Init(struct Task *);
static bool8 FrontierSquares_Draw(struct Task *);
static bool8 FrontierSquares_Shrink(struct Task *);
static bool8 FrontierSquares_End(struct Task *);
static bool8 FrontierSquaresSpiral_Init(struct Task *);
static bool8 FrontierSquaresSpiral_Outward(struct Task *);
static bool8 FrontierSquaresSpiral_SetBlack(struct Task *);
static bool8 FrontierSquaresSpiral_Inward(struct Task *);
static bool8 FrontierSquaresScroll_Init(struct Task *);
static bool8 FrontierSquaresScroll_Draw(struct Task *);
static bool8 FrontierSquaresScroll_SetBlack(struct Task *);
static bool8 FrontierSquaresScroll_Erase(struct Task *);
static bool8 FrontierSquaresScroll_End(struct Task *);
static bool8 Mugshot_Init(struct Task *);
static bool8 Mugshot_SetGfx(struct Task *);
static bool8 Mugshot_ShowBanner(struct Task *);
static bool8 Mugshot_StartOpponentSlide(struct Task *);
static bool8 Mugshot_WaitStartPlayerSlide(struct Task *);
static bool8 Mugshot_WaitPlayerSlide(struct Task *);
static bool8 Mugshot_GradualWhiteFade(struct Task *);
static bool8 Mugshot_InitFadeWhiteToBlack(struct Task *);
static bool8 Mugshot_FadeToBlack(struct Task *);
static bool8 Mugshot_End(struct Task *);
static void DoMugshotTransition(u8);
static void Mugshots_CreateTrainerPics(struct Task *);
static void VBlankCB_Mugshots(void);
static void VBlankCB_MugshotsFadeOut(void);
static void HBlankCB_Mugshots(void);
static void InitTransitionData(void);
static void FadeScreenBlack(void);
static void CreateIntroTask(s16, s16, s16, s16, s16);
static void SetCircularMask(u16 *, s16, s16, s16);
static void SetSinWave(u16 *, s16, s16, s16, s16, s16);
static void GetBg0TilemapDst(u16 **);
static void InitBlackWipe(struct LINEWORK *, s16, s16, s16, s16, s16, s16);
static bool8 UpdateBlackWipe(struct LINEWORK *, bool8, bool8);
static void SetTrainerPicSlideDirection(s16, s16);
static void IncrementTrainerPicState(s16);
static s16 IsTrainerPicSlideDone(s16);
static bool8 TransitionIntro_FadeToGray(struct Task *);
static bool8 TransitionIntro_FadeFromGray(struct Task *);
static bool8 IsIntroTaskDone(void);
static void SpriteCB_FldEffPokeballTrail(struct Sprite *);
static void SpriteCB_MugshotTrainerPic(struct Sprite *);
static void SpriteCB_WhiteBarFade(struct Sprite *);
static bool8 MugshotTrainerPic_Pause(struct Sprite *);
static bool8 MugshotTrainerPic_Init(struct Sprite *);
static bool8 MugshotTrainerPic_Slide(struct Sprite *);
static bool8 MugshotTrainerPic_SlideSlow(struct Sprite *);
static bool8 MugshotTrainerPic_SlideOffscreen(struct Sprite *);

static u8 sTestingTransitionId;
static u8 sTestingTransitionState;

EWRAM_DATA static struct TransitionData *sTransitionData = NULL;

static const u32 sBigPokeball_Tileset[] = INCBIN_U32("graphics/battle_transitions/big_pokeball.4bpp");
static const u32 sPokeballTrail_Tileset[] = INCBIN_U32("graphics/battle_transitions/pokeball_trail.4bpp");
static const u8 sPokeball_Gfx[] = INCBIN_U8("graphics/battle_transitions/pokeball.4bpp");
static const u32 sEliteFour_Tileset[] = INCBIN_U32("graphics/battle_transitions/elite_four_bg.4bpp");
static const u8 sUnusedBrendan_Gfx[] = INCBIN_U8("graphics/battle_transitions/unused_brendan.4bpp");
static const u8 sUnusedLass_Gfx[] = INCBIN_U8("graphics/battle_transitions/unused_lass.4bpp");
static const u32 sShrinkingBoxTileset[] = INCBIN_U32("graphics/battle_transitions/shrinking_box.4bpp");
static const u16 sEvilTeam_Palette[] = INCBIN_U16("graphics/battle_transitions/evil_team.gbapal");
static const u32 sTeamAqua_Tileset[] = INCBIN_U32("graphics/battle_transitions/team_aqua.4bpp.lz");
static const u32 sTeamAqua_Tilemap[] = INCBIN_U32("graphics/battle_transitions/team_aqua.bin.lz");
static const u32 sTeamMagma_Tileset[] = INCBIN_U32("graphics/battle_transitions/team_magma.4bpp.lz");
static const u32 sTeamMagma_Tilemap[] = INCBIN_U32("graphics/battle_transitions/team_magma.bin.lz");
static const u32 sRegis_Tileset[] = INCBIN_U32("graphics/battle_transitions/regis.4bpp");
static const u16 sRegice_Palette[] = INCBIN_U16("graphics/battle_transitions/regice.gbapal");
static const u16 sRegisteel_Palette[] = INCBIN_U16("graphics/battle_transitions/registeel.gbapal");
static const u16 sRegirock_Palette[] = INCBIN_U16("graphics/battle_transitions/regirock.gbapal");
static const u32 sRegice_Tilemap[] = INCBIN_U32("graphics/battle_transitions/regice.bin");
static const u32 sRegisteel_Tilemap[] = INCBIN_U32("graphics/battle_transitions/registeel.bin");
static const u32 sRegirock_Tilemap[] = INCBIN_U32("graphics/battle_transitions/regirock.bin");
static const u16 sUnused_Palette[] = INCBIN_U16("graphics/battle_transitions/unused.gbapal");
static const u32 sKyogre_Tileset[] = INCBIN_U32("graphics/battle_transitions/kyogre.4bpp.lz");
static const u32 sKyogre_Tilemap[] = INCBIN_U32("graphics/battle_transitions/kyogre.bin.lz");
static const u32 sGroudon_Tileset[] = INCBIN_U32("graphics/battle_transitions/groudon.4bpp.lz");
static const u32 sGroudon_Tilemap[] = INCBIN_U32("graphics/battle_transitions/groudon.bin.lz");
static const u16 sKyogre1_Palette[] = INCBIN_U16("graphics/battle_transitions/kyogre_pt1.gbapal");
static const u16 sKyogre2_Palette[] = INCBIN_U16("graphics/battle_transitions/kyogre_pt2.gbapal");
static const u16 sGroudon1_Palette[] = INCBIN_U16("graphics/battle_transitions/groudon_pt1.gbapal");
static const u16 sGroudon2_Palette[] = INCBIN_U16("graphics/battle_transitions/groudon_pt2.gbapal");
static const u16 sRayquaza_Palette[] = INCBIN_U16("graphics/battle_transitions/rayquaza.gbapal");
static const u32 sRayquaza_Tileset[] = INCBIN_U32("graphics/battle_transitions/rayquaza.4bpp");
static const u32 sRayquaza_Tilemap[] = INCBIN_U32("graphics/battle_transitions/rayquaza.bin");
static const u16 sFrontierLogo_Palette[] = INCBIN_U16("graphics/battle_transitions/frontier_logo.gbapal");
static const u32 sFrontierLogo_Tileset[] = INCBIN_U32("graphics/battle_transitions/frontier_logo.4bpp.lz");
static const u32 sFrontierLogo_Tilemap[] = INCBIN_U32("graphics/battle_transitions/frontier_logo.bin.lz");
static const u16 sFrontierSquares_Palette[] = INCBIN_U16("graphics/battle_transitions/frontier_squares_blanktiles.gbapal");
static const u32 sFrontierSquares_FilledBg_Tileset[] = INCBIN_U32("graphics/battle_transitions/frontier_square_1.4bpp.lz");
static const u32 sFrontierSquares_EmptyBg_Tileset[] = INCBIN_U32("graphics/battle_transitions/frontier_square_2.4bpp.lz");
static const u32 sFrontierSquares_Shrink1_Tileset[] = INCBIN_U32("graphics/battle_transitions/frontier_square_3.4bpp.lz");
static const u32 sFrontierSquares_Shrink2_Tileset[] = INCBIN_U32("graphics/battle_transitions/frontier_square_4.4bpp.lz");
static const u32 sFrontierSquares_Tilemap[] = INCBIN_U32("graphics/battle_transitions/frontier_squares.bin");

// After the intro each transition has a unique main task.
// This task will call the functions that do the transition effects.
static const TaskFunc sTasks_Main[B_TRANSITION_COUNT] =
{
    [B_TRANSITION_BLUR] = Task_Blur,
    [B_TRANSITION_SWIRL] = Task_Swirl,
    [B_TRANSITION_SHUFFLE] = Task_Shuffle,
    [B_TRANSITION_BIG_POKEBALL] = Task_BigPokeball,
    [B_TRANSITION_POKEBALLS_TRAIL] = Task_PokeballsTrail,
    [B_TRANSITION_CLOCKWISE_WIPE] = Task_ClockwiseWipe,
    [B_TRANSITION_RIPPLE] = Task_Ripple,
    [B_TRANSITION_WAVE] = Task_Wave,
    [B_TRANSITION_SLICE] = Task_Slice,
    [B_TRANSITION_WHITE_BARS_FADE] = Task_WhiteBarsFade,
    [B_TRANSITION_GRID_SQUARES] = Task_GridSquares,
    [B_TRANSITION_ANGLED_WIPES] = Task_AngledWipes,
    [B_TRANSITION_SIDNEY] = Task_Sidney,
    [B_TRANSITION_PHOEBE] = Task_Phoebe,
    [B_TRANSITION_GLACIA] = Task_Glacia,
    [B_TRANSITION_DRAKE] = Task_Drake,
    [B_TRANSITION_CHAMPION] = Task_Champion,
    [B_TRANSITION_AQUA] = Task_Aqua,
    [B_TRANSITION_MAGMA] = Task_Magma,
    [B_TRANSITION_REGICE] = Task_Regice,
    [B_TRANSITION_REGISTEEL] = Task_Registeel,
    [B_TRANSITION_REGIROCK] = Task_Regirock,
    [B_TRANSITION_KYOGRE] = Task_Kyogre,
    [B_TRANSITION_GROUDON] = Task_Groudon,
    [B_TRANSITION_RAYQUAZA] = Task_Rayquaza,
    [B_TRANSITION_BLACKHOLE] = Task_Blackhole,
    [B_TRANSITION_BLACKHOLE_PULSATE] = Task_BlackholePulsate,
    [B_TRANSITION_FRONTIER_LOGO_WIGGLE] = Task_FrontierLogoWiggle,
    [B_TRANSITION_FRONTIER_LOGO_WAVE] = Task_FrontierLogoWave,
    [B_TRANSITION_FRONTIER_SQUARES] = Task_FrontierSquares,
    [B_TRANSITION_FRONTIER_SQUARES_SCROLL] = Task_FrontierSquaresScroll,
    [B_TRANSITION_FRONTIER_SQUARES_SPIRAL] = Task_FrontierSquaresSpiral,
    [B_TRANSITION_FRONTIER_CIRCLES_MEET] = Task_FrontierCirclesMeet,
    [B_TRANSITION_FRONTIER_CIRCLES_CROSS] = Task_FrontierCirclesCross,
    [B_TRANSITION_FRONTIER_CIRCLES_ASYMMETRIC_SPIRAL] = Task_FrontierCirclesAsymmetricSpiral,
    [B_TRANSITION_FRONTIER_CIRCLES_SYMMETRIC_SPIRAL] = Task_FrontierCirclesSymmetricSpiral,
    [B_TRANSITION_FRONTIER_CIRCLES_MEET_IN_SEQ] = Task_FrontierCirclesMeetInSeq,
    [B_TRANSITION_FRONTIER_CIRCLES_CROSS_IN_SEQ] = Task_FrontierCirclesCrossInSeq,
    [B_TRANSITION_FRONTIER_CIRCLES_ASYMMETRIC_SPIRAL_IN_SEQ] = Task_FrontierCirclesAsymmetricSpiralInSeq,
    [B_TRANSITION_FRONTIER_CIRCLES_SYMMETRIC_SPIRAL_IN_SEQ] = Task_FrontierCirclesSymmetricSpiralInSeq,
};

static const TransitionStateFunc sTaskHandlers[] =
{
    &Transition_StartIntro,
    &Transition_WaitForIntro,
    &Transition_StartMain,
    &Transition_WaitForMain
};

static const TransitionStateFunc sBlur_Funcs[] =
{
    Blur_Init,
    Blur_Main,
    Blur_End
};

static const TransitionStateFunc sSwirl_Funcs[] =
{
    Swirl_Init,
    Swirl_End,
};

static const TransitionStateFunc sShuffle_Funcs[] =
{
    Shuffle_Init,
    Shuffle_End,
};

static const TransitionStateFunc sAqua_Funcs[] =
{
    Aqua_Init,
    Aqua_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    FramesCountdown,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sMagma_Funcs[] =
{
    Magma_Init,
    Magma_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    FramesCountdown,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sBigPokeball_Funcs[] =
{
    BigPokeball_Init,
    BigPokeball_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sRegice_Funcs[] =
{
    Regi_Init,
    Regice_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sRegisteel_Funcs[] =
{
    Regi_Init,
    Registeel_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sRegirock_Funcs[] =
{
    Regi_Init,
    Regirock_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sKyogre_Funcs[] =
{
    WeatherTrio_BgFadeBlack,
    WeatherTrio_WaitFade,
    Kyogre_Init,
    Kyogre_PaletteFlash,
    Kyogre_PaletteBrighten,
    FramesCountdown,
    WeatherDuo_FadeOut,
    WeatherDuo_End
};

static const TransitionStateFunc sPokeballsTrail_Funcs[] =
{
    PokeballsTrail_Init,
    PokeballsTrail_Main,
    PokeballsTrail_End
};

#define NUM_POKEBALL_TRAILS 5
static const s16 sPokeballsTrail_StartXCoords[2] = { -16, DISPLAY_WIDTH + 16 };
static const s16 sPokeballsTrail_Delays[NUM_POKEBALL_TRAILS] = {0, 32, 64, 18, 48};
static const s16 sPokeballsTrail_Speeds[2] = {8, -8};

static const TransitionStateFunc sClockwiseWipe_Funcs[] =
{
    ClockwiseWipe_Init,
    ClockwiseWipe_TopRight,
    ClockwiseWipe_Right,
    ClockwiseWipe_Bottom,
    ClockwiseWipe_Left,
    ClockwiseWipe_TopLeft,
    ClockwiseWipe_End
};

static const TransitionStateFunc sRipple_Funcs[] =
{
    Ripple_Init,
    Ripple_Main
};

static const TransitionStateFunc sWave_Funcs[] =
{
    Wave_Init,
    Wave_Main,
    Wave_End
};

static const TransitionStateFunc sMugshot_Funcs[] =
{
    Mugshot_Init,
    Mugshot_SetGfx,
    Mugshot_ShowBanner,
    Mugshot_StartOpponentSlide,
    Mugshot_WaitStartPlayerSlide,
    Mugshot_WaitPlayerSlide,
    Mugshot_GradualWhiteFade,
    Mugshot_InitFadeWhiteToBlack,
    Mugshot_FadeToBlack,
    Mugshot_End
};

static const u8 sMugshotsTrainerPicIDsTable[MUGSHOTS_COUNT] =
{
    [MUGSHOT_SIDNEY]   = TRAINER_PIC_ELITE_FOUR_SIDNEY,
    [MUGSHOT_PHOEBE]   = TRAINER_PIC_ELITE_FOUR_PHOEBE,
    [MUGSHOT_GLACIA]   = TRAINER_PIC_ELITE_FOUR_GLACIA,
    [MUGSHOT_DRAKE]    = TRAINER_PIC_ELITE_FOUR_DRAKE,
    [MUGSHOT_CHAMPION] = TRAINER_PIC_CHAMPION_WALLACE,
};
static const s16 sMugshotsOpponentRotationScales[MUGSHOTS_COUNT][2] =
{
    [MUGSHOT_SIDNEY] =   {0x200, 0x200},
    [MUGSHOT_PHOEBE] =   {0x200, 0x200},
    [MUGSHOT_GLACIA] =   {0x1B0, 0x1B0},
    [MUGSHOT_DRAKE] =    {0x1A0, 0x1A0},
    [MUGSHOT_CHAMPION] = {0x188, 0x188},
};
static const s16 sMugshotsOpponentCoords[MUGSHOTS_COUNT][2] =
{
    [MUGSHOT_SIDNEY] =   { 0,  0},
    [MUGSHOT_PHOEBE] =   { 0,  0},
    [MUGSHOT_GLACIA] =   {-4,  4},
    [MUGSHOT_DRAKE] =    { 0,  5},
    [MUGSHOT_CHAMPION] = {-8,  7},
};

static const TransitionSpriteCallback sMugshotTrainerPicFuncs[] =
{
    MugshotTrainerPic_Pause,
    MugshotTrainerPic_Init,
    MugshotTrainerPic_Slide,
    MugshotTrainerPic_SlideSlow,
    MugshotTrainerPic_Pause,
    MugshotTrainerPic_SlideOffscreen,
    MugshotTrainerPic_Pause
};

// One element per slide direction.
// Sign of acceleration is opposite speed, so slide decelerates.
static const s16 sTrainerPicSlideSpeeds[2] = {12, -12};
static const s16 sTrainerPicSlideAccels[2] = {-1,   1};

static const TransitionStateFunc sSlice_Funcs[] =
{
    Slice_Init,
    Slice_Main,
    Slice_End
};

static const TransitionStateFunc sBlackhole_Funcs[] =
{
    Blackhole_Init,
    Blackhole_Vibrate,
    Blackhole_GrowEnd
};

static const TransitionStateFunc sBlackholePulsate_Funcs[] =
{
    Blackhole_Init,
    BlackholePulsate_Main
};

// Blackhole rapidly alternates adding these values to the radius,
// resulting in a vibrating shrink/grow effect.
static const s16 sBlackhole_Vibrations[] = {-6, 4};

static const TransitionStateFunc sGroudon_Funcs[] =
{
    WeatherTrio_BgFadeBlack,
    WeatherTrio_WaitFade,
    Groudon_Init,
    Groudon_PaletteFlash,
    Groudon_PaletteBrighten,
    FramesCountdown,
    WeatherDuo_FadeOut,
    WeatherDuo_End
};

static const TransitionStateFunc sRayquaza_Funcs[] =
{
    WeatherTrio_BgFadeBlack,
    WeatherTrio_WaitFade,
    Rayquaza_Init,
    Rayquaza_SetGfx,
    Rayquaza_PaletteFlash,
    Rayquaza_FadeToBlack,
    Rayquaza_WaitFade,
    Rayquaza_SetBlack,
    Rayquaza_TriRing,
    Blackhole_Vibrate,
    Blackhole_GrowEnd
};

static const TransitionStateFunc sWhiteBarsFade_Funcs[] =
{
    WhiteBarsFade_Init,
    WhiteBarsFade_StartBars,
    WhiteBarsFade_WaitBars,
    WhiteBarsFade_BlendToBlack,
    WhiteBarsFade_End
};

#define NUM_WHITE_BARS 8
static const s16 sWhiteBarsFade_StartDelays[NUM_WHITE_BARS] = {0, 20, 15, 40, 10, 25, 35, 5};

static const TransitionStateFunc sGridSquares_Funcs[] =
{
    GridSquares_Init,
    GridSquares_Main,
    GridSquares_End
};

static const TransitionStateFunc sAngledWipes_Funcs[] =
{
    AngledWipes_Init,
    AngledWipes_SetWipeData,
    AngledWipes_DoWipe,
    AngledWipes_TryEnd,
    AngledWipes_StartNext
};

#define NUM_ANGLED_WIPES 7

static const s16 sAngledWipes_MoveData[NUM_ANGLED_WIPES][5] =
{
// startX          startY          endX            endY            yDirection
    {56,            0,              0,              DISPLAY_HEIGHT, 0},
    {104,           DISPLAY_HEIGHT, DISPLAY_WIDTH,  88,             1},
    {DISPLAY_WIDTH, 72,             56,             0,              1},
    {0,             32,             144,            DISPLAY_HEIGHT, 0},
    {144,           DISPLAY_HEIGHT, 184,            0,              1},
    {56,            0,              168,            DISPLAY_HEIGHT, 0},
    {168,           DISPLAY_HEIGHT, 48,             0,              1},
};

static const s16 sAngledWipes_EndDelays[NUM_ANGLED_WIPES] = {8, 4, 2, 1, 1, 1, 0};

static const TransitionStateFunc sTransitionIntroFuncs[] =
{
    TransitionIntro_FadeToGray,
    TransitionIntro_FadeFromGray
};

static const struct SpriteFrameImage sSpriteImage_Pokeball[] =
{
    {sPokeball_Gfx, sizeof(sPokeball_Gfx)}
};

static const union AnimCmd sSpriteAnim_Pokeball[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_Pokeball[] =
{
    sSpriteAnim_Pokeball
};

static const union AffineAnimCmd sSpriteAffineAnim_Pokeball1[] =
{
    AFFINEANIMCMD_FRAME(0, 0, -4, 1),
    AFFINEANIMCMD_JUMP(0)
};

static const union AffineAnimCmd sSpriteAffineAnim_Pokeball2[] =
{
    AFFINEANIMCMD_FRAME(0, 0, 4, 1),
    AFFINEANIMCMD_JUMP(0)
};

static const union AffineAnimCmd *const sSpriteAffineAnimTable_Pokeball[] =
{
    sSpriteAffineAnim_Pokeball1,
    sSpriteAffineAnim_Pokeball2
};

static const struct SpriteTemplate sSpriteTemplate_Pokeball =
{
    .tileTag = TAG_NONE,
    .paletteTag = FLDEFF_PAL_TAG_POKEBALL_TRAIL,
    .oam = &gObjectEventBaseOam_32x32,
    .anims = sSpriteAnimTable_Pokeball,
    .images = sSpriteImage_Pokeball,
    .affineAnims = sSpriteAffineAnimTable_Pokeball,
    .callback = SpriteCB_FldEffPokeballTrail
};

static const struct OamData sOam_UnusedBrendanLass =
{
    .y = 0,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 0,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct SpriteFrameImage sImageTable_UnusedBrendan[] =
{
    {sUnusedBrendan_Gfx, sizeof(sUnusedBrendan_Gfx)}
};

static const struct SpriteFrameImage sImageTable_UnusedLass[] =
{
    {sUnusedLass_Gfx, sizeof(sUnusedLass_Gfx)}
};

static const union AnimCmd sSpriteAnim_UnusedBrendanLass[] =
{
    ANIMCMD_FRAME(0, 1),
    ANIMCMD_END
};

static const union AnimCmd *const sSpriteAnimTable_UnusedBrendanLass[] =
{
    sSpriteAnim_UnusedBrendanLass
};

static const struct SpriteTemplate sSpriteTemplate_UnusedBrendan =
{
    .tileTag = TAG_NONE,
    .paletteTag = PALTAG_UNUSED_MUGSHOT,
    .oam = &sOam_UnusedBrendanLass,
    .anims = sSpriteAnimTable_UnusedBrendanLass,
    .images = sImageTable_UnusedBrendan,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_MugshotTrainerPic
};

static const struct SpriteTemplate sSpriteTemplate_UnusedLass =
{
    .tileTag = TAG_NONE,
    .paletteTag = PALTAG_UNUSED_MUGSHOT,
    .oam = &sOam_UnusedBrendanLass,
    .anims = sSpriteAnimTable_UnusedBrendanLass,
    .images = sImageTable_UnusedLass,
    .affineAnims = gDummySpriteAffineAnimTable,
    .callback = SpriteCB_MugshotTrainerPic
};

static const u16 sFieldEffectPal_Pokeball[] = INCBIN_U16("graphics/field_effects/palettes/pokeball.gbapal");

const struct SpritePalette gSpritePalette_Pokeball = {sFieldEffectPal_Pokeball, FLDEFF_PAL_TAG_POKEBALL_TRAIL};

static const u16 sMugshotPal_Sidney[] = INCBIN_U16("graphics/battle_transitions/sidney_bg.gbapal");
static const u16 sMugshotPal_Phoebe[] = INCBIN_U16("graphics/battle_transitions/phoebe_bg.gbapal");
static const u16 sMugshotPal_Glacia[] = INCBIN_U16("graphics/battle_transitions/glacia_bg.gbapal");
static const u16 sMugshotPal_Drake[] = INCBIN_U16("graphics/battle_transitions/drake_bg.gbapal");
static const u16 sMugshotPal_Champion[] = INCBIN_U16("graphics/battle_transitions/wallace_bg.gbapal");
static const u16 sMugshotPal_Brendan[] = INCBIN_U16("graphics/battle_transitions/brendan_bg.gbapal");
static const u16 sMugshotPal_May[] = INCBIN_U16("graphics/battle_transitions/may_bg.gbapal");

static const u16 *const sOpponentMugshotsPals[MUGSHOTS_COUNT] =
{
    [MUGSHOT_SIDNEY] = sMugshotPal_Sidney,
    [MUGSHOT_PHOEBE] = sMugshotPal_Phoebe,
    [MUGSHOT_GLACIA] = sMugshotPal_Glacia,
    [MUGSHOT_DRAKE] = sMugshotPal_Drake,
    [MUGSHOT_CHAMPION] = sMugshotPal_Champion
};

static const u16 *const sPlayerMugshotsPals[GENDER_COUNT] =
{
    [MALE] = sMugshotPal_Brendan,
    [FEMALE] = sMugshotPal_May
};

static const u16 sUnusedTrainerPalette[] = INCBIN_U16("graphics/battle_transitions/unused_trainer.gbapal");
static const struct SpritePalette sSpritePalette_UnusedTrainer = {sUnusedTrainerPalette, PALTAG_UNUSED_MUGSHOT};

static const u16 sBigPokeball_Tilemap[] = INCBIN_U16("graphics/battle_transitions/big_pokeball_map.bin");
static const u16 sMugshotsTilemap[] = INCBIN_U16("graphics/battle_transitions/elite_four_bg_map.bin");

static const TransitionStateFunc sFrontierLogoWiggle_Funcs[] =
{
    FrontierLogoWiggle_Init,
    FrontierLogoWiggle_SetGfx,
    PatternWeave_Blend1,
    PatternWeave_Blend2,
    PatternWeave_FinishAppear,
    PatternWeave_CircularMask
};

static const TransitionStateFunc sFrontierLogoWave_Funcs[] =
{
    FrontierLogoWave_Init,
    FrontierLogoWave_SetGfx,
    FrontierLogoWave_InitScanline,
    FrontierLogoWave_Main
};

static const TransitionStateFunc sFrontierSquares_Funcs[] =
{
    FrontierSquares_Init,
    FrontierSquares_Draw,
    FrontierSquares_Shrink,
    FrontierSquares_End
};

static const TransitionStateFunc sFrontierSquaresSpiral_Funcs[] =
{
    FrontierSquaresSpiral_Init,
    FrontierSquaresSpiral_Outward,
    FrontierSquaresSpiral_SetBlack,
    FrontierSquaresSpiral_Inward,
    FrontierSquares_End
};

static const TransitionStateFunc sFrontierSquaresScroll_Funcs[] =
{
    FrontierSquaresScroll_Init,
    FrontierSquaresScroll_Draw,
    FrontierSquaresScroll_SetBlack,
    FrontierSquaresScroll_Erase,
    FrontierSquaresScroll_End
};

#define SQUARE_SIZE 4
#define MARGIN_SIZE 1 // Squares do not fit evenly across the width, so there is a margin on either side.
#define NUM_SQUARES_PER_ROW ((DISPLAY_WIDTH - (MARGIN_SIZE * 8 * 2)) / (SQUARE_SIZE * 8))
#define NUM_SQUARES_PER_COL (DISPLAY_HEIGHT / (SQUARE_SIZE * 8))
#define NUM_SQUARES         (NUM_SQUARES_PER_ROW * NUM_SQUARES_PER_COL)

// The order in which the squares should appear/disappear to create
// the spiral effect. Spiraling inward starts with the first element,
// and spiraling outward starts with the last. The positions are the
// squares numbered left-to-right top-to-bottom.
static const u8 sFrontierSquaresSpiral_Positions[NUM_SQUARES] = {
    28, 29, 30, 31, 32, 33, 34,
    27, 20, 13,  6,  5,  4,  3,
     2,  1,  0,  7, 14, 21, 22,
    23, 24, 25, 26, 19, 12, 11,
    10,  9,  8, 15, 16, 17, 18
};

// In the scrolling version the squares appear/disappear in a "random" order
// dictated by the list below.
static const u8 sFrontierSquaresScroll_Positions[] = {
     0, 16, 41, 22, 44,  2, 43, 21,
    46, 27,  9, 48, 38,  5, 57, 59,
    12, 63, 35, 28, 10, 53,  7, 49,
    39, 23, 55,  1, 62, 17, 61, 30,
     6, 34, 15, 51, 32, 58, 13, 45,
    37, 52, 11, 24, 60, 19, 56, 33,
    29, 50, 40, 54, 14,  3, 47, 20,
    18, 25,  4, 36, 26, 42, 31,  8
};

//---------------------------
// Main transition functions
//---------------------------

static void CB2_TestBattleTransition(void)
{
    switch (sTestingTransitionState)
    {
    case 0:
        LaunchBattleTransitionTask(sTestingTransitionId);
        sTestingTransitionState++;
        break;
    case 1:
        if (IsBattleTransitionDone())
        {
            sTestingTransitionState = 0;
            SetMainCallback2(CB2_ReturnToField);
        }
        break;
    }

    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    UpdatePaletteFade();
}

static void UNUSED TestBattleTransition(u8 transitionId)
{
    sTestingTransitionId = transitionId;
    SetMainCallback2(CB2_TestBattleTransition);
}

void BattleTransition_StartOnField(u8 transitionId)
{
    gMain.callback2 = CB2_OverworldBasic;
    LaunchBattleTransitionTask(transitionId);
}

void BattleTransition_Start(u8 transitionId)
{
    LaunchBattleTransitionTask(transitionId);
}

// main task that launches sub-tasks for phase1 and phase2
#define tTransitionId   data[1]
#define tTransitionDone data[15]

bool8 IsBattleTransitionDone(void)
{
    u8 taskId = FindTaskIdByFunc(Task_BattleTransition);
    if (gTasks[taskId].tTransitionDone)
    {
        DestroyTask(taskId);
        FREE_AND_SET_NULL(sTransitionData);
        return TRUE;
    }

    return FALSE;
}

static void LaunchBattleTransitionTask(u8 transitionId)
{
    u8 taskId = CreateTask(Task_BattleTransition, 2);
    gTasks[taskId].tTransitionId = transitionId;
    sTransitionData = AllocZeroed(sizeof(*sTransitionData));
}

static void Task_BattleTransition(u8 taskId)
{
    while (sTaskHandlers[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Transition_StartIntro(struct Task *task)
{
    SetWeatherScreenFadeOut();
    CpuFastCopy(gPlttBufferFaded, gPlttBufferUnfaded, PLTT_SIZE);

    CreateTask(Task_Intro, 4);
    task->tState++;
    return FALSE;
}

static bool8 Transition_WaitForIntro(struct Task *task)
{
    if (FindTaskIdByFunc(Task_Intro) == TASK_NONE)
    {
        task->tState++;
        return TRUE;
    }

    return FALSE;
}

static bool8 Transition_StartMain(struct Task *task)
{
    CreateTask(sTasks_Main[task->tTransitionId], 0);
    task->tState++;
    return FALSE;
}

static bool8 Transition_WaitForMain(struct Task *task)
{
    task->tTransitionDone = FALSE;
    if (FindTaskIdByFunc(sTasks_Main[task->tTransitionId]) == TASK_NONE)
        task->tTransitionDone = TRUE;
    return FALSE;
}

#undef tTransitionId
#undef tTransitionDone

static void Task_Intro(u8 taskId)
{
    if (gTasks[taskId].tState == 0)
    {
        gTasks[taskId].tState++;
        CreateIntroTask(0, 0, 3, 2, 2);
    }
    else if (IsIntroTaskDone())
    {
        DestroyTask(taskId);
    }
}

//--------------------
// B_TRANSITION_BLUR
//--------------------

#define tDelay   data[1]
#define tCounter data[2]

static void Task_Blur(u8 taskId)
{
    while (sBlur_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Blur_Init(struct Task *task)
{
    SetGpuReg(REG_OFFSET_MOSAIC, 0);
    SetGpuRegBits(REG_OFFSET_BG1CNT, BGCNT_MOSAIC);
    SetGpuRegBits(REG_OFFSET_BG2CNT, BGCNT_MOSAIC);
    SetGpuRegBits(REG_OFFSET_BG3CNT, BGCNT_MOSAIC);
    task->tState++;
    return TRUE;
}

static bool8 Blur_Main(struct Task *task)
{
    if (task->tDelay != 0)
    {
        task->tDelay--;
        return FALSE;
    }

    task->tDelay = 4;
    if (++task->tCounter == 10)
        BeginNormalPaletteFade(PALETTES_ALL, -1, 0, 16, RGB_BLACK);
    SetGpuReg(REG_OFFSET_MOSAIC, (task->tCounter & 0xF) * 17);
    if (task->tCounter >= 15)
        task->tState++;
    return FALSE;
}

static bool8 Blur_End(struct Task *task)
{
    if (!gPaletteFade.active)
    {
        DestroyTask(FindTaskIdByFunc(Task_Blur));
    }
    return FALSE;
}

#undef tDelay
#undef tCounter

//--------------------
// B_TRANSITION_SWIRL
//--------------------

#define tSinIndex  data[1]
#define tAmplitude data[2]

static void Task_Swirl(u8 taskId)
{
    while (sSwirl_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Swirl_Init(struct Task *task)
{
    InitTransitionData();
    ScanlineEffect_Clear();
    BeginNormalPaletteFade(PALETTES_ALL, 4, 0, 16, RGB_BLACK);
    SetSinWave(gScanlineEffectRegBuffers[1], sTransitionData->cameraX, 0, 2, 0, DISPLAY_HEIGHT);

    SetVBlankCallback(VBlankCB_Swirl);
    SetHBlankCallback(HBlankCB_Swirl);

    EnableInterrupts(INTR_FLAG_VBLANK | INTR_FLAG_HBLANK);

    task->tState++;
    return FALSE;
}

static bool8 Swirl_End(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;
    task->tSinIndex += 4;
    task->tAmplitude += 8;

    SetSinWave(gScanlineEffectRegBuffers[0], sTransitionData->cameraX, task->tSinIndex, 2, task->tAmplitude, DISPLAY_HEIGHT);

    if (!gPaletteFade.active)
    {
        DestroyTask(FindTaskIdByFunc(Task_Swirl));
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static void VBlankCB_Swirl(void)
{
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
}

static void HBlankCB_Swirl(void)
{
    u16 var = gScanlineEffectRegBuffers[1][REG_VCOUNT];
    REG_BG1HOFS = var;
    REG_BG2HOFS = var;
    REG_BG3HOFS = var;
}

#undef tSinIndex
#undef tAmplitude

//----------------------
// B_TRANSITION_SHUFFLE
//----------------------

#define tSinVal    data[1]
#define tAmplitude data[2]

static void Task_Shuffle(u8 taskId)
{
    while (sShuffle_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Shuffle_Init(struct Task *task)
{
    InitTransitionData();
    ScanlineEffect_Clear();

    BeginNormalPaletteFade(PALETTES_ALL, 4, 0, 16, RGB_BLACK);
    memset(gScanlineEffectRegBuffers[1], sTransitionData->cameraY, DISPLAY_HEIGHT * 2);

    SetVBlankCallback(VBlankCB_Shuffle);
    SetHBlankCallback(HBlankCB_Shuffle);

    EnableInterrupts(INTR_FLAG_VBLANK | INTR_FLAG_HBLANK);

    task->tState++;
    return FALSE;
}

static bool8 Shuffle_End(struct Task *task)
{
    u32 i;
    s16 amplitude, sinVal;

    sinVal = task->tSinVal;
    amplitude = (task->tAmplitude & 0xFFFF) >> 8;
    task->tSinVal += 4224;
    task->tAmplitude += 384;

    sTransitionData->VBlank_DMA = FALSE;
    for (i = 0; i < DISPLAY_HEIGHT; i++, sinVal += 4224)
    {
        s16 sinIndex = (sinVal & 0xFFFF) >> 8;;
        gScanlineEffectRegBuffers[0][i] = sTransitionData->cameraY + Sin(sinIndex, amplitude);
    }

    sTransitionData->VBlank_DMA = TRUE;

    if (!gPaletteFade.active)
        DestroyTask(FindTaskIdByFunc(Task_Shuffle));

    return FALSE;
}

static void VBlankCB_Shuffle(void)
{
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
}

static void HBlankCB_Shuffle(void)
{
    u16 var = gScanlineEffectRegBuffers[1][REG_VCOUNT];
    REG_BG1VOFS = var;
    REG_BG2VOFS = var;
    REG_BG3VOFS = var;
}

#undef tSinVal
#undef tAmplitude

//------------------------------------------------------------------------
// B_TRANSITION_BIG_POKEBALL, B_TRANSITION_AQUA, B_TRANSITION_MAGMA,
// B_TRANSITION_REGICE, B_TRANSITION_REGISTEEL, B_TRANSITION_REGIROCK
// and B_TRANSITION_KYOGRE.
//
// With the exception of B_TRANSITION_KYOGRE, all of the above transitions
// use the same weave effect (see the PatternWeave functions).
// Unclear why Kyogre's was grouped here and not with Groudon/Rayquaza's.
//------------------------------------------------------------------------

#define tBlendTarget1 data[1]
#define tBlendTarget2 data[2]
#define tBlendDelay   data[3]

// Data 1-3 change purpose for PatternWeave_CircularMask
#define tRadius      data[1]
#define tRadiusDelta data[2]
#define tVBlankSet   data[3]

#define tSinIndex     data[4]
#define tAmplitude    data[5]
#define tEndDelay     data[8]

static void Task_BigPokeball(u8 taskId)
{
    while (sBigPokeball_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Aqua(u8 taskId)
{
    while (sAqua_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Magma(u8 taskId)
{
    while (sMagma_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Regice(u8 taskId)
{
    while (sRegice_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Registeel(u8 taskId)
{
    while (sRegisteel_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Regirock(u8 taskId)
{
    while (sRegirock_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_Kyogre(u8 taskId)
{
    while (sKyogre_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void InitPatternWeaveTransition(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    task->tBlendTarget1 = 16;
    task->tBlendTarget2 = 0;
    task->tSinIndex = 0;
    task->tAmplitude = 0x4000;
    sTransitionData->WININ = WININ_WIN0_ALL;
    sTransitionData->WINOUT = 0;
    sTransitionData->WIN0H = DISPLAY_WIDTH;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;
    sTransitionData->BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_ALL;
    sTransitionData->BLDALPHA = BLDALPHA_BLEND(task->tBlendTarget2, task->tBlendTarget1);

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = DISPLAY_WIDTH;

    SetVBlankCallback(VBlankCB_PatternWeave);
}

static bool8 Aqua_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    task->tEndDelay = 60;
    InitPatternWeaveTransition(task);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sTeamAqua_Tileset, tileset);
    LoadPalette(sEvilTeam_Palette, BG_PLTT_ID(15), sizeof(sEvilTeam_Palette));

    task->tState++;
    return FALSE;
}

static bool8 Magma_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    task->tEndDelay = 60;
    InitPatternWeaveTransition(task);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sTeamMagma_Tileset, tileset);
    LoadPalette(sEvilTeam_Palette, BG_PLTT_ID(15), sizeof(sEvilTeam_Palette));

    task->tState++;
    return FALSE;
}

static bool8 Regi_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    task->tEndDelay = 60;
    InitPatternWeaveTransition(task);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    CpuCopy16(sRegis_Tileset, tileset, 0x2000);

    task->tState++;
    return FALSE;
}

static bool8 BigPokeball_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    InitPatternWeaveTransition(task);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    CpuCopy16(sBigPokeball_Tileset, tileset, sizeof(sBigPokeball_Tileset));
    LoadPalette(sFieldEffectPal_Pokeball, BG_PLTT_ID(15), sizeof(sFieldEffectPal_Pokeball));

    task->tState++;
    return FALSE;
}

static bool8 BigPokeball_SetGfx(struct Task *task)
{
    u32 i, j;
    u16 *tilemap, *tileset;
    const u16 *bigPokeballMap;

    GetBg0TilesDst(&tilemap, &tileset);
    bigPokeballMap = sBigPokeball_Tilemap;
    for (i = 0; i < 20; i++)
    {
        for (j = 0; j < 30; j++, bigPokeballMap++)
            SET_TILE(tilemap, i, j, *bigPokeballMap);
    }

    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return TRUE;
}

static bool8 Aqua_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sTeamAqua_Tilemap, tilemap);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return FALSE;
}

static bool8 Magma_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sTeamMagma_Tilemap, tilemap);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return FALSE;
}

static bool8 Regice_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LoadPalette(sRegice_Palette, BG_PLTT_ID(15), sizeof(sRegice_Palette));
    CpuCopy16(sRegice_Tilemap, tilemap, 0x500);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return FALSE;
}

static bool8 Registeel_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LoadPalette(sRegisteel_Palette, BG_PLTT_ID(15), sizeof(sRegisteel_Palette));
    CpuCopy16(sRegisteel_Tilemap, tilemap, 0x500);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return FALSE;
}

static bool8 Regirock_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LoadPalette(sRegirock_Palette, BG_PLTT_ID(15), sizeof(sRegirock_Palette));
    CpuCopy16(sRegirock_Tilemap, tilemap, 0x500);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return FALSE;
}

#define tTimer data[1]

static bool8 Kyogre_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sKyogre_Tileset, tileset);
    LZ77UnCompVram(sKyogre_Tilemap, tilemap);

    task->tState++;
    return FALSE;
}

static bool8 Kyogre_PaletteFlash(struct Task *task)
{
    if (task->tTimer % 3 == 0)
    {
        u16 offset = task->tTimer % 30;
        LoadPalette(&sKyogre1_Palette[offset/3 * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer > 58)
    {
        task->tState++;
        task->tTimer = 0;
    }

    return FALSE;
}

static bool8 Kyogre_PaletteBrighten(struct Task *task)
{
    if (task->tTimer % 5 == 0)
    {
        LoadPalette(&sKyogre2_Palette[(task->tTimer / 5) * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer > 68)
    {
        task->tState++;
        task->tTimer = 0;
        task->tEndDelay = 30;
    }

    return FALSE;
}

static bool8 WeatherDuo_FadeOut(struct Task *task)
{
    BeginNormalPaletteFade(PALETTES_OBJECTS | (1 << 15), 1, 0, 16, RGB_BLACK);
    task->tState++;
    return FALSE;
}

static bool8 WeatherDuo_End(struct Task *task)
{
    if (!gPaletteFade.active)
    {
        DmaStop(0);
        FadeScreenBlack();
        DestroyTask(FindTaskIdByFunc(task->func));
    }
    return FALSE;
}

#undef tTimer

// The PatternWeave_ functions are used by several different transitions.
// They create an effect where a pattern/image (such as the Magma emblem) is
// formed by a shimmering weave effect.
static bool8 PatternWeave_Blend1(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;
    if (task->tBlendDelay == 0 || --task->tBlendDelay == 0)
    {
        task->tBlendTarget2++;
        task->tBlendDelay = 2;
    }
    sTransitionData->BLDALPHA = BLDALPHA_BLEND(task->tBlendTarget2, task->tBlendTarget1);
    if (task->tBlendTarget2 > 15)
        task->tState++;

    task->tSinIndex += 8;
    task->tAmplitude -= 256;

    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude >> 8, DISPLAY_HEIGHT);

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 PatternWeave_Blend2(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;
    if (task->tBlendDelay == 0 || --task->tBlendDelay == 0)
    {
        task->tBlendTarget1--;
        task->tBlendDelay = 2;
    }
    sTransitionData->BLDALPHA = BLDALPHA_BLEND(task->tBlendTarget2, task->tBlendTarget1);
    if (task->tBlendTarget1 <= 0)
        task->tState++;
    task->tSinIndex += 8;
    task->tAmplitude -= 256;

    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude >> 8, DISPLAY_HEIGHT);

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 PatternWeave_FinishAppear(struct Task *task)
{
        sTransitionData->VBlank_DMA = FALSE;
    task->tSinIndex += 8;
    task->tAmplitude -= 256;

    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude >> 8, DISPLAY_HEIGHT);

    if (task->tAmplitude <= 0)
    {
        task->tState++;
        task->tRadius = DISPLAY_HEIGHT;
        task->tRadiusDelta = 1 << 8;
        task->tVBlankSet = FALSE;
    }
    sTransitionData->VBlank_DMA = TRUE;

    return FALSE;
}

static bool8 FramesCountdown(struct Task *task)
{
    if (--task->tEndDelay == 0)
        task->tState++;
    return FALSE;
}

static bool8 WeatherTrio_BgFadeBlack(struct Task *task)
{
    BeginNormalPaletteFade(PALETTES_BG, 1, 0, 16, RGB_BLACK);
    task->tState++;
    return FALSE;
}

static bool8 WeatherTrio_WaitFade(struct Task *task)
{
    if (!gPaletteFade.active)
        task->tState++;
    return FALSE;
}

// Do a shrinking circular mask to go to a black screen after the pattern appears.
static bool8 PatternWeave_CircularMask(struct Task *task)
{

    if (task->tRadiusDelta < (4 << 8))
        task->tRadiusDelta += 128; // 256 is 1 unit of speed. Speed up every other frame (128 / 256)
    if (task->tRadius != 0)
    {
        task->tRadius -= task->tRadiusDelta >> 8;
        if (task->tRadius < 0)
            task->tRadius = 0;
    }
    
    if (task->tRadius == 0)
    {
        SetVBlankCallback(NULL);
        DmaStop(0);
        FadeScreenBlack();
        DestroyTask(FindTaskIdByFunc(task->func));
        return FALSE;
    }

    sTransitionData->VBlank_DMA = FALSE;
    SetCircularMask(gScanlineEffectRegBuffers[0], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, task->tRadius);
    sTransitionData->VBlank_DMA = TRUE;
    if (!task->tVBlankSet)
    {
        task->tVBlankSet = TRUE;
        SetVBlankCallback(VBlankCB_CircularMask);
    }


    return FALSE;
}

static void VBlankCB_SetWinAndBlend(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    REG_BLDCNT = sTransitionData->BLDCNT;
    REG_BLDALPHA = sTransitionData->BLDALPHA;
}

static void VBlankCB_PatternWeave(void)
{
    VBlankCB_SetWinAndBlend();
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_BG0HOFS, B_TRANS_DMA_FLAGS);
}

static void VBlankCB_CircularMask(void)
{
    VBlankCB_SetWinAndBlend();
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

#undef tAmplitude
#undef tSinIndex
#undef tBlendTarget1
#undef tBlendTarget2
#undef tRadius
#undef tRadiusDelta
#undef tVBlankSet

//------------------------------
// B_TRANSITION_POKEBALLS_TRAIL
//------------------------------

#define sSide  data[0]
#define sDelay data[1]
#define sPrevX data[2]

static void Task_PokeballsTrail(u8 taskId)
{
    while (sPokeballsTrail_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 PokeballsTrail_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    CpuCopy16(sPokeballTrail_Tileset, tileset, 0x20 * 2);
    CpuFill32(0, tilemap, BG_SCREEN_SIZE);
    LoadPalette(sFieldEffectPal_Pokeball, BG_PLTT_ID(15), sizeof(sFieldEffectPal_Pokeball));

    task->tState++;
    return FALSE;
}

static bool8 PokeballsTrail_Main(struct Task *task)
{
    int i;
    int side;

    // Randomly pick which side the first ball should start on.
    // The side is then flipped for each subsequent ball.
    side = Random() & 1;
    for (i = 0; i < NUM_POKEBALL_TRAILS; i++, side ^= 1)
    {
        gFieldEffectArguments[0] = sPokeballsTrail_StartXCoords[side];   // x
        gFieldEffectArguments[1] = (i << 5) + 16;  // y
        gFieldEffectArguments[2] = side;
        gFieldEffectArguments[3] = sPokeballsTrail_Delays[i];
        FieldEffectStart(FLDEFF_POKEBALL_TRAIL);
    }

    task->tState++;
    return FALSE;
}

static bool8 PokeballsTrail_End(struct Task *task)
{
    if (!FieldEffectActiveListContains(FLDEFF_POKEBALL_TRAIL))
    {
        FadeScreenBlack();
        DestroyTask(FindTaskIdByFunc(Task_PokeballsTrail));
    }
    return FALSE;
}

bool8 FldEff_PokeballTrail(void)
{
    u8 spriteId = CreateSpriteAtEnd(&sSpriteTemplate_Pokeball, gFieldEffectArguments[0], gFieldEffectArguments[1], 0);
    gSprites[spriteId].oam.priority = 0;
    gSprites[spriteId].oam.affineMode = ST_OAM_AFFINE_NORMAL;
    gSprites[spriteId].sSide = gFieldEffectArguments[2];
    gSprites[spriteId].sDelay = gFieldEffectArguments[3];
    gSprites[spriteId].sPrevX = -1;
    InitSpriteAffineAnim(&gSprites[spriteId]);
    StartSpriteAffineAnim(&gSprites[spriteId], gFieldEffectArguments[2]);
    return FALSE;
}

static void SpriteCB_FldEffPokeballTrail(struct Sprite *sprite)
{
    if (sprite->sDelay != 0)
    {
        sprite->sDelay--;
        return;
    }

    if (sprite->x >= 0 && sprite->x <= DISPLAY_WIDTH)
    {
        // Set Pokéball position
        s16 posX = sprite->x >> 3;
        s16 posY = sprite->y >> 3;

        // If Pokéball moved forward clear trail behind it
        if (posX != sprite->sPrevX)
        {
            u16 var;
            vu16 *ptr;

            sprite->sPrevX = posX;
            var = ((REG_BG0CNT >> 8) & 0x1F);
            ptr = (vu16 *)(BG_VRAM + (var << 11));

            SET_TILE(ptr, posY - 2, posX, 1);
            SET_TILE(ptr, posY - 1, posX, 1);
            SET_TILE(ptr, posY, posX, 1);
            SET_TILE(ptr, posY + 1, posX, 1);
        }
    }
    sprite->x += sPokeballsTrail_Speeds[sprite->sSide];
    if (sprite->x <= -16 || sprite->x >= DISPLAY_WIDTH + 16)
        FieldEffectStop(sprite, FLDEFF_POKEBALL_TRAIL);
}

#undef sSide
#undef sDelay
#undef sPrevX

//-----------------------------
// B_TRANSITION_CLOCKWISE_WIPE
//-----------------------------

static void Task_ClockwiseWipe(u8 taskId)
{
    while (sClockwiseWipe_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 ClockwiseWipe_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    sTransitionData->WININ = 0;
    sTransitionData->WINOUT = WINOUT_WIN01_ALL;
    sTransitionData->WIN0H = WIN_RANGE(DISPLAY_WIDTH, DISPLAY_WIDTH + 1);
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = ((DISPLAY_WIDTH + 3) << 8) | (DISPLAY_WIDTH + 4);

    SetVBlankCallback(VBlankCB_ClockwiseWipe);
    sTransitionData->tWipeEndX = DISPLAY_WIDTH / 2;

    task->tState++;
    return TRUE;
}

static bool8 ClockwiseWipe_TopRight(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;

    InitBlackWipe(&sTransitionData->line, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, sTransitionData->tWipeEndX, -1, 1, 1);
    do
    {
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = (sTransitionData->tWipeCurrX + 1) | ((DISPLAY_WIDTH / 2) << 8);
    } while (!UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE));

    sTransitionData->tWipeEndX += 16;
    if (sTransitionData->tWipeEndX >= DISPLAY_WIDTH)
    {
        sTransitionData->tWipeEndY = 0;
        task->tState++;
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 ClockwiseWipe_Right(struct Task *task)
{
    s16 start, end;

    sTransitionData->VBlank_DMA = FALSE;

    InitBlackWipe(&sTransitionData->line, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, DISPLAY_WIDTH, sTransitionData->tWipeEndY, 1, 1);

    do {
        start = DISPLAY_WIDTH / 2, end = sTransitionData->tWipeCurrX + 1;
        if (sTransitionData->tWipeEndY >= DISPLAY_HEIGHT / 2)
            start = sTransitionData->tWipeCurrX, end = DISPLAY_WIDTH;
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = end | (start << 8);
        
    } while (!UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE));

    sTransitionData->tWipeEndY += 8;
    if (sTransitionData->tWipeEndY >= DISPLAY_HEIGHT)
    {
        sTransitionData->tWipeEndX = DISPLAY_WIDTH;
        task->tState++;
    }
    else
    {
        while (sTransitionData->tWipeCurrY < sTransitionData->tWipeEndY)
            gScanlineEffectRegBuffers[0][++sTransitionData->tWipeCurrY] = end | (start << 8);
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 ClockwiseWipe_Bottom(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;

    InitBlackWipe(&sTransitionData->line, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, sTransitionData->tWipeEndX, DISPLAY_HEIGHT, 1, 1);
    do
    {
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = (sTransitionData->tWipeCurrX << 8) | DISPLAY_WIDTH;
    } while (!UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE));

    sTransitionData->tWipeEndX -= 16;
    if (sTransitionData->tWipeEndX <= 0)
    {
        sTransitionData->tWipeEndY = DISPLAY_HEIGHT;
        task->tState++;
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 ClockwiseWipe_Left(struct Task *task)
{
    s16 end, start;

    sTransitionData->VBlank_DMA = FALSE;

    InitBlackWipe(&sTransitionData->line, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, 0, sTransitionData->tWipeEndY, 1, 1);

    do{
        if (sTransitionData->tWipeEndY <= DISPLAY_HEIGHT / 2)
            start = DISPLAY_WIDTH / 2, end = sTransitionData->tWipeCurrX;
        else
        {
            start = (gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY]);
            end = start & 0xFF;
            start = sTransitionData->tWipeCurrX;
        }
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = end | (start << 8);
    } while (!UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE));

    sTransitionData->tWipeEndY -= 8;
    if (sTransitionData->tWipeEndY <= 0)
    {
        sTransitionData->tWipeEndX = 0;
        task->tState++;
    }
    else
    {
        while (sTransitionData->tWipeCurrY > sTransitionData->tWipeEndY)
            gScanlineEffectRegBuffers[0][--sTransitionData->tWipeCurrY] = end | (start << 8);
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 ClockwiseWipe_TopLeft(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;

    InitBlackWipe(&sTransitionData->line, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, sTransitionData->tWipeEndX, 0, 1, 1);
    do
    {
        s16 start, end;
        start = DISPLAY_WIDTH / 2, end = sTransitionData->tWipeCurrX;
        if (end >= DISPLAY_WIDTH / 2)
            start = 0, end = DISPLAY_WIDTH;
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = end | (start << 8);
    } while (!UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE));

    sTransitionData->tWipeEndX += 16;
    if (sTransitionData->tWipeCurrX > DISPLAY_WIDTH / 2)
        task->tState++;

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 ClockwiseWipe_End(struct Task *task)
{
    DmaStop(0);
    FadeScreenBlack();
    DestroyTask(FindTaskIdByFunc(Task_ClockwiseWipe));
    return FALSE;
}

static void VBlankCB_ClockwiseWipe(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA != 0)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    REG_WIN0H = gScanlineEffectRegBuffers[1][0];
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

//---------------------
// B_TRANSITION_RIPPLE
//---------------------

#define tSinVal       data[1]
#define tAmplitudeVal data[2]
#define tTimer        data[3]
#define tFadeStarted  data[4]

static void Task_Ripple(u8 taskId)
{
    while (sRipple_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Ripple_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = sTransitionData->cameraY;

    SetVBlankCallback(VBlankCB_Ripple);
    SetHBlankCallback(HBlankCB_Ripple);

    EnableInterrupts(INTR_FLAG_HBLANK);

    task->tState++;
    return TRUE;
}

static bool8 Ripple_Main(struct Task *task)
{
    u32 i;
    u16 amplitude;
    s16 sinVal, speed;

    sTransitionData->VBlank_DMA = FALSE;

    amplitude = task->tAmplitudeVal >> 8;
    sinVal = task->tSinVal;
    speed = 0x180;
    task->tSinVal += 0x400;
    if (task->tAmplitudeVal < 0x2000)
        task->tAmplitudeVal += 0x180;

    for (i = 0; i < DISPLAY_HEIGHT; i++, sinVal += speed)
    {
        s16 sinIndex = (sinVal & 0xFFFF) >> 8;
        gScanlineEffectRegBuffers[0][i] = sTransitionData->cameraY + Sin(sinIndex, amplitude);
    }

    if (++task->tTimer == 81)
    {
        task->tFadeStarted++;
        BeginNormalPaletteFade(PALETTES_ALL, -2, 0, 16, RGB_BLACK);
    }

    if (task->tFadeStarted && !gPaletteFade.active)
        DestroyTask(FindTaskIdByFunc(Task_Ripple));

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static void VBlankCB_Ripple(void)
{
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
}

static void HBlankCB_Ripple(void)
{
    u16 var = gScanlineEffectRegBuffers[1][REG_VCOUNT];
    REG_BG1VOFS = var;
    REG_BG2VOFS = var;
    REG_BG3VOFS = var;
}

#undef tSinVal
#undef tAmplitudeVal
#undef tTimer
#undef tFadeStarted

//-------------------
// B_TRANSITION_WAVE
//-------------------

#define tX        data[1]
#define tSinIndex data[2]

static void Task_Wave(u8 taskId)
{
    while (sWave_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Wave_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    sTransitionData->WININ = WININ_WIN0_ALL;
    sTransitionData->WINOUT = 0;
    sTransitionData->WIN0H = DISPLAY_WIDTH;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = DISPLAY_WIDTH + 2;

    SetVBlankCallback(VBlankCB_Wave);

    task->tState++;
    return TRUE;
}

static bool8 Wave_Main(struct Task *task)
{
    u32 i;
    u8 sinIndex;
    u16 *toStore;
    bool8 finished;

    sTransitionData->VBlank_DMA = FALSE;
    toStore = gScanlineEffectRegBuffers[0];
    sinIndex = task->tSinIndex;
    task->tSinIndex += 16;
    task->tX += 8;

    for (i = 0, finished = TRUE; i < DISPLAY_HEIGHT; i++, sinIndex += 4, toStore++)
    {
        s16 x = task->tX + Sin(sinIndex, 40);
        if (x < 0)
            x = 0;
        else if (x > DISPLAY_WIDTH)
            x = DISPLAY_WIDTH;
        *toStore = (x << 8) | (DISPLAY_WIDTH + 1);
        if (x < DISPLAY_WIDTH)
            finished = FALSE;
    }
    if (finished)
        task->tState++;

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 Wave_End(struct Task *task)
{
    DmaStop(0);
    FadeScreenBlack();
    DestroyTask(FindTaskIdByFunc(Task_Wave));
    return FALSE;
}

static void VBlankCB_Wave(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA != 0)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

#undef tX
#undef tSinIndex

//----------------------------------------------------------------
// B_TRANSITION_SIDNEY, B_TRANSITION_PHOEBE, B_TRANSITION_GLACIA,
// B_TRANSITION_DRAKE, and B_TRANSITION_CHAMPION
//
// These are all the "mugshot" transitions, where a banner shows
// the trainer pic of the player and their opponent.
//----------------------------------------------------------------

#define tSinIndex           data[1]
#define tTopBannerX         data[2]
#define tBottomBannerX      data[3]
#define tTimer              data[3] // Re-used
#define tFadeSpread         data[4]
#define tOpponentSpriteId   data[13]
#define tPlayerSpriteId     data[14]
#define tMugshotId          data[15]

// Sprite data for trainer sprites in mugshots
#define sState       data[0]
#define sSlideSpeed  data[1]
#define sSlideAccel  data[2]
#define sDone        data[6]
#define sSlideDir    data[7]

static void Task_Sidney(u8 taskId)
{
    gTasks[taskId].tMugshotId = MUGSHOT_SIDNEY;
    DoMugshotTransition(taskId);
}

static void Task_Phoebe(u8 taskId)
{
    gTasks[taskId].tMugshotId = MUGSHOT_PHOEBE;
    DoMugshotTransition(taskId);
}

static void Task_Glacia(u8 taskId)
{
    gTasks[taskId].tMugshotId = MUGSHOT_GLACIA;
    DoMugshotTransition(taskId);
}

static void Task_Drake(u8 taskId)
{
    gTasks[taskId].tMugshotId = MUGSHOT_DRAKE;
    DoMugshotTransition(taskId);
}

static void Task_Champion(u8 taskId)
{
    gTasks[taskId].tMugshotId = MUGSHOT_CHAMPION;
    DoMugshotTransition(taskId);
}

static void DoMugshotTransition(u8 taskId)
{
    while (sMugshot_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Mugshot_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();
    Mugshots_CreateTrainerPics(task);

    task->tSinIndex = 0;
    task->tTopBannerX = 1;
    task->tBottomBannerX = DISPLAY_WIDTH - 1;
    sTransitionData->WININ = WININ_WIN0_ALL;
    sTransitionData->WINOUT = WINOUT_WIN01_BG1 | WINOUT_WIN01_BG2 | WINOUT_WIN01_BG3 | WINOUT_WIN01_OBJ | WINOUT_WIN01_CLR;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = (DISPLAY_WIDTH << 8) | (DISPLAY_WIDTH + 1);

    SetVBlankCallback(VBlankCB_Mugshots);

    task->tState++;
    return FALSE;
}

static bool8 Mugshot_SetGfx(struct Task *task)
{
    u32 i, j;
    u16 *tilemap, *tileset;
    const u16 *mugshotsMap;

    mugshotsMap = sMugshotsTilemap;
    GetBg0TilesDst(&tilemap, &tileset);
    CpuSet(sEliteFour_Tileset, tileset, 0xF0);
    LoadPalette(sOpponentMugshotsPals[task->tMugshotId], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    LoadPalette(sPlayerMugshotsPals[gSaveBlock2.playerGender], BG_PLTT_ID(15) + 10, PLTT_SIZEOF(6));

    for (i = 0; i < 20; i++)
    {
        for (j = 0; j < 32; j++, mugshotsMap++)
            SET_TILE(tilemap, i, j, *mugshotsMap);
    }

    EnableInterrupts(INTR_FLAG_HBLANK);
    SetHBlankCallback(HBlankCB_Mugshots);

    task->tState++;
    return FALSE;
}

static bool8 Mugshot_ShowBanner(struct Task *task)
{
    u8 sinIndex;
    u32 i;
    u16 *toStore;
    s16 x;

    sTransitionData->VBlank_DMA = FALSE;

    toStore = gScanlineEffectRegBuffers[0];
    sinIndex = task->tSinIndex;
    task->tSinIndex += 16;

    // Update top banner
    for (i = 0; i < DISPLAY_HEIGHT / 2; i++, toStore++, sinIndex += 16)
    {
        x = task->tTopBannerX + Sin(sinIndex, 16);
        if (x < 0)
            x = 1;
        else if (x > DISPLAY_WIDTH)
            x = DISPLAY_WIDTH;
        *toStore = x;
    }

    // Update bottom banner
    for (; i < DISPLAY_HEIGHT; i++, toStore++, sinIndex += 16)
    {
        x = task->tBottomBannerX - Sin(sinIndex, 16);
        if (x < 0)
            x = 0;
        if (x > DISPLAY_WIDTH - 1)
            x = DISPLAY_WIDTH - 1;
        *toStore = (x << 8) | DISPLAY_WIDTH;
    }

    // Slide banners across screen
    task->tTopBannerX += 8;
    task->tBottomBannerX -= 8;

    if (task->tTopBannerX > DISPLAY_WIDTH)
        task->tTopBannerX = DISPLAY_WIDTH;
    if (task->tBottomBannerX < 0)
        task->tBottomBannerX = 0;

    if (task->tTopBannerX == DISPLAY_WIDTH && task->tBottomBannerX == 0 )
        task->tState++;

    sTransitionData->BG0HOFS_Lower -= 8;
    sTransitionData->BG0HOFS_Upper += 8;
    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 Mugshot_StartOpponentSlide(struct Task *task)
{
    u32 i;
    u16 *toStore;

    sTransitionData->VBlank_DMA = FALSE;

    for (i = 0, toStore = gScanlineEffectRegBuffers[0]; i < DISPLAY_HEIGHT; i++, toStore++)
        *toStore = DISPLAY_WIDTH;

    task->tState++;

    // Clear old data
    task->tSinIndex = 0;
    task->tTopBannerX = 0;
    task->tBottomBannerX = 0;

    sTransitionData->BG0HOFS_Lower -= 8;
    sTransitionData->BG0HOFS_Upper += 8;

    SetTrainerPicSlideDirection(task->tOpponentSpriteId, 0);
    SetTrainerPicSlideDirection(task->tPlayerSpriteId, 1);

    // Start opponent slide
    IncrementTrainerPicState(task->tOpponentSpriteId);

    PlaySE(SE_MUGSHOT);

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 Mugshot_WaitStartPlayerSlide(struct Task *task)
{
    sTransitionData->BG0HOFS_Lower -= 8;
    sTransitionData->BG0HOFS_Upper += 8;

    // Start player's slide in once the opponent is finished
    if (IsTrainerPicSlideDone(task->tOpponentSpriteId))
    {
        task->tState++;
        IncrementTrainerPicState(task->tPlayerSpriteId);
    }
    return FALSE;
}

static bool8 Mugshot_WaitPlayerSlide(struct Task *task)
{
    sTransitionData->BG0HOFS_Lower -= 8;
    sTransitionData->BG0HOFS_Upper += 8;

    if (IsTrainerPicSlideDone(task->tPlayerSpriteId))
    {
        sTransitionData->VBlank_DMA = FALSE;
        SetVBlankCallback(NULL);
        DmaStop(0);
        memset(gScanlineEffectRegBuffers[0], 0, DISPLAY_HEIGHT * 2);
        memset(gScanlineEffectRegBuffers[1], 0, DISPLAY_HEIGHT * 2);
        SetGpuReg(REG_OFFSET_WIN0H, DISPLAY_WIDTH);
        SetGpuReg(REG_OFFSET_BLDY, 0);
        task->tState++;
        task->tTimer = 0;
        task->tFadeSpread = 0;
        sTransitionData->BLDCNT = BLDCNT_TGT1_ALL | BLDCNT_EFFECT_LIGHTEN;
        SetVBlankCallback(VBlankCB_MugshotsFadeOut);
    }
    return FALSE;
}

static bool8 Mugshot_GradualWhiteFade(struct Task *task)
{
    bool32 active;

    sTransitionData->VBlank_DMA = FALSE;
    active = TRUE;
    sTransitionData->BG0HOFS_Lower -= 8;
    sTransitionData->BG0HOFS_Upper += 8;

    if (task->tFadeSpread < DISPLAY_HEIGHT / 2)
        task->tFadeSpread += 2;
    if (task->tFadeSpread > DISPLAY_HEIGHT / 2)
        task->tFadeSpread = DISPLAY_HEIGHT / 2;

    if (++task->tTimer & 1)
    {
        s16 i;
        for (i = 0, active = FALSE; i <= task->tFadeSpread; i++)
        {
            // Fade starts in middle of screen and
            // spreads outwards in both directions.
            s16 index1 = DISPLAY_HEIGHT / 2 - i;
            s16 index2 = DISPLAY_HEIGHT / 2 + i;
            if (gScanlineEffectRegBuffers[0][index1] <= 15)
            {
                active = TRUE;
                gScanlineEffectRegBuffers[0][index1]++;
            }
            if (gScanlineEffectRegBuffers[0][index2] <= 15)
            {
                active = TRUE;
                gScanlineEffectRegBuffers[0][index2]++;
            }
        }
    }

    if (task->tFadeSpread == DISPLAY_HEIGHT / 2 && !active)
        task->tState++;

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

// Set palette to white to replace the scanline white fade
// before the screen fades to black.
static bool8 Mugshot_InitFadeWhiteToBlack(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;
    BlendPalettes(PALETTES_ALL, 16, RGB_WHITE);
    sTransitionData->BLDCNT = 0xFF;
    task->tTimer = 0;

    task->tState++;
    return TRUE;
}

static bool8 Mugshot_FadeToBlack(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;

    task->tTimer++;
    memset(gScanlineEffectRegBuffers[0], task->tTimer, DISPLAY_HEIGHT * 2);
    if (task->tTimer > 15)
        task->tState++;

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 Mugshot_End(struct Task *task)
{
    DmaStop(0);
    FadeScreenBlack();
    DestroyTask(FindTaskIdByFunc(task->func));
    return FALSE;
}

static void VBlankCB_Mugshots(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA != 0)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_BG0VOFS = sTransitionData->BG0VOFS;
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

static void VBlankCB_MugshotsFadeOut(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA != 0)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_BLDCNT = sTransitionData->BLDCNT;
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_BLDY, B_TRANS_DMA_FLAGS);
}

static void HBlankCB_Mugshots(void)
{
    if (REG_VCOUNT < DISPLAY_HEIGHT / 2)
        REG_BG0HOFS = sTransitionData->BG0HOFS_Lower;
    else
        REG_BG0HOFS = sTransitionData->BG0HOFS_Upper;
}

static void Mugshots_CreateTrainerPics(struct Task *task)
{
    struct Sprite *opponentSprite, *playerSprite;

    s16 mugshotId = task->tMugshotId;
    task->tOpponentSpriteId = CreateTrainerSprite(sMugshotsTrainerPicIDsTable[mugshotId],
                                                  sMugshotsOpponentCoords[mugshotId][0] - 32,
                                                  sMugshotsOpponentCoords[mugshotId][1] + 42,
                                                  0, gDecompressionBuffer);
    task->tPlayerSpriteId = CreateTrainerSprite(PlayerGenderToFrontTrainerPicId(gSaveBlock2.playerGender),
                                                DISPLAY_WIDTH + 32,
                                                106,
                                                0, gDecompressionBuffer);

    opponentSprite = &gSprites[task->tOpponentSpriteId];
    playerSprite = &gSprites[task->tPlayerSpriteId];

    opponentSprite->callback = SpriteCB_MugshotTrainerPic;
    playerSprite->callback = SpriteCB_MugshotTrainerPic;

    opponentSprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;
    playerSprite->oam.affineMode = ST_OAM_AFFINE_DOUBLE;

    opponentSprite->oam.matrixNum = AllocOamMatrix();
    playerSprite->oam.matrixNum = AllocOamMatrix();

    opponentSprite->oam.shape = SPRITE_SHAPE(64x32);
    playerSprite->oam.shape = SPRITE_SHAPE(64x32);

    opponentSprite->oam.size = SPRITE_SIZE(64x32);
    playerSprite->oam.size = SPRITE_SIZE(64x32);

    CalcCenterToCornerVec(opponentSprite, SPRITE_SHAPE(64x32), SPRITE_SIZE(64x32), ST_OAM_AFFINE_DOUBLE);
    CalcCenterToCornerVec(playerSprite, SPRITE_SHAPE(64x32), SPRITE_SIZE(64x32), ST_OAM_AFFINE_DOUBLE);

    SetOamMatrixRotationScaling(opponentSprite->oam.matrixNum, sMugshotsOpponentRotationScales[mugshotId][0], sMugshotsOpponentRotationScales[mugshotId][1], 0);
    SetOamMatrixRotationScaling(playerSprite->oam.matrixNum, -512, 512, 0);
}

static void SpriteCB_MugshotTrainerPic(struct Sprite *sprite)
{
    while (sMugshotTrainerPicFuncs[sprite->sState](sprite));
}

// Wait until IncrementTrainerPicState is called
static bool8 MugshotTrainerPic_Pause(struct Sprite *sprite)
{
    return FALSE;
}

static bool8 MugshotTrainerPic_Init(struct Sprite *sprite)
{
    sprite->sState++;
    sprite->sSlideSpeed = sTrainerPicSlideSpeeds[sprite->sSlideDir];
    sprite->sSlideAccel = sTrainerPicSlideAccels[sprite->sSlideDir];
    return TRUE;
}

static bool8 MugshotTrainerPic_Slide(struct Sprite *sprite)
{
    sprite->x += sprite->sSlideSpeed;

    // Advance state when pic passes ~40% of screen
    if (sprite->sSlideDir && sprite->x < DISPLAY_WIDTH - 107)
        sprite->sState++;
    else if (!sprite->sSlideDir && sprite->x > 103)
        sprite->sState++;
    return FALSE;
}

static bool8 MugshotTrainerPic_SlideSlow(struct Sprite *sprite)
{
    // Add acceleration value to speed, then add speed.
    // For both sides acceleration is opposite speed, so slide slows down.
    sprite->sSlideSpeed += sprite->sSlideAccel;
    sprite->x += sprite->sSlideSpeed;

    // Advance state when slide comes to a stop
    if (sprite->sSlideSpeed == 0)
    {
        sprite->sState++;
        sprite->sSlideAccel = -sprite->sSlideAccel;
        sprite->sDone = TRUE;
    }
    return FALSE;
}

// Slides trainer pic offscreen. This is never reached, because it's preceded
// by a second MugshotTrainerPic_Pause, and IncrementTrainerPicState is
// only called once per trainer pic.
static bool8 MugshotTrainerPic_SlideOffscreen(struct Sprite *sprite)
{
    sprite->sSlideSpeed += sprite->sSlideAccel;
    sprite->x += sprite->sSlideSpeed;
    if (sprite->x < -31 || sprite->x > DISPLAY_WIDTH + 31)
        sprite->sState++;
    return FALSE;
}

static void SetTrainerPicSlideDirection(s16 spriteId, s16 dirId)
{
    gSprites[spriteId].sSlideDir = dirId;
}

static void IncrementTrainerPicState(s16 spriteId)
{
    gSprites[spriteId].sState++;
}

static s16 IsTrainerPicSlideDone(s16 spriteId)
{
    return gSprites[spriteId].sDone;
}

#undef sState
#undef sSlideSpeed
#undef sSlideAccel
#undef sDone
#undef sSlideDir
#undef tSinIndex
#undef tTopBannerX
#undef tBottomBannerX
#undef tTimer
#undef tFadeSpread
#undef tOpponentSpriteId
#undef tPlayerSpriteId
#undef tMugshotId

//--------------------
// B_TRANSITION_SLICE
//--------------------

#define tEffectX data[1]
#define tSpeed   data[2]
#define tAccel   data[3]

static void Task_Slice(u8 taskId)
{
    while (sSlice_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Slice_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    task->tSpeed = 1 << 8;
    task->tAccel = 1;
    sTransitionData->WININ = WININ_WIN0_ALL;
    sTransitionData->WINOUT = 0;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;
    sTransitionData->VBlank_DMA = FALSE;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
    {
        gScanlineEffectRegBuffers[1][i] = sTransitionData->cameraX;
        gScanlineEffectRegBuffers[1][DISPLAY_HEIGHT + i] = DISPLAY_WIDTH;
    }

    EnableInterrupts(INTR_FLAG_HBLANK);
    SetGpuRegBits(REG_OFFSET_DISPSTAT, DISPSTAT_HBLANK_INTR);

    SetVBlankCallback(VBlankCB_Slice);
    SetHBlankCallback(HBlankCB_Slice);

    task->tState++;
    return TRUE;
}

static bool8 Slice_Main(struct Task *task)
{
    u32 i;

    sTransitionData->VBlank_DMA = FALSE;

    task->tEffectX += (task->tSpeed >> 8);
    if (task->tEffectX > DISPLAY_WIDTH)
        task->tEffectX = DISPLAY_WIDTH;
    if (task->tSpeed <= 0xFFF)
        task->tSpeed += task->tAccel;
    if (task->tAccel < 128)
        task->tAccel <<= 1; // multiplying by two

    for (i = 0; i < DISPLAY_HEIGHT; i++)
    {
        u16 *storeLoc1 = &gScanlineEffectRegBuffers[0][i];
        u16 *storeLoc2 = &gScanlineEffectRegBuffers[0][DISPLAY_HEIGHT + i];

        // Alternate rows
        if (i & 1)
        {
            *storeLoc1 = sTransitionData->cameraX + task->tEffectX;
            *storeLoc2 = DISPLAY_WIDTH - task->tEffectX;
        }
        else
        {
            *storeLoc1 = sTransitionData->cameraX - task->tEffectX;
            *storeLoc2 = (task->tEffectX << 8) | (DISPLAY_WIDTH + 1);
        }
    }

    if (task->tEffectX >= DISPLAY_WIDTH)
        task->tState++;

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 Slice_End(struct Task *task)
{
    DmaStop(0);
    FadeScreenBlack();
    DestroyTask(FindTaskIdByFunc(Task_Slice));
    return FALSE;
}

static void VBlankCB_Slice(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 4);
    DmaSet(0, &gScanlineEffectRegBuffers[1][DISPLAY_HEIGHT], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

static void HBlankCB_Slice(void)
{
    u8 vcount = REG_VCOUNT;
    if (vcount < DISPLAY_HEIGHT)
    {
        u16 var = gScanlineEffectRegBuffers[1][vcount];
        REG_BG1HOFS = var;
        REG_BG2HOFS = var;
        REG_BG3HOFS = var;
    }
}

#undef tEffectX
#undef tSpeed
#undef tAccel

//-----------------------------------------------------------
// B_TRANSITION_BLACKHOLE and B_TRANSITION_BLACKHOLE_PULSATE
//-----------------------------------------------------------

#define tRadius    data[1]
#define tGrowSpeed data[2]
#define tSinIndex  data[5]
#define tVibrateId data[6]
#define tAmplitude data[6] // Used differently by the two transitions
#define tFlag      data[7] // Used generally to indicate an action has taken place.

static void Task_Blackhole(u8 taskId)
{
    while (sBlackhole_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_BlackholePulsate(u8 taskId)
{
    while (sBlackholePulsate_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

// Init is shared by both transitions
static bool8 Blackhole_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    sTransitionData->WININ = 0;
    sTransitionData->WINOUT = WINOUT_WIN01_ALL;
    sTransitionData->WIN0H = DISPLAY_WIDTH;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = 0;

    SetVBlankCallback(VBlankCB_CircularMask);

    task->tState++;
    task->tRadius = 1;
    task->tGrowSpeed = 1 << 8;
    task->tFlag = FALSE;

    return FALSE;
}

static bool8 Blackhole_GrowEnd(struct Task *task)
{
    if (task->tFlag == TRUE)
    {
        DmaStop(0);
        SetVBlankCallback(NULL);
        DestroyTask(FindTaskIdByFunc(task->func));
        return FALSE;
    }

    sTransitionData->VBlank_DMA = FALSE;
    if (task->tGrowSpeed < 1024)
        task->tGrowSpeed += 128;
    if (task->tRadius < DISPLAY_HEIGHT)
        task->tRadius += task->tGrowSpeed >> 8;
    if (task->tRadius > DISPLAY_HEIGHT)
        task->tRadius = DISPLAY_HEIGHT;
    SetCircularMask(gScanlineEffectRegBuffers[0], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, task->tRadius);
    if (task->tRadius == DISPLAY_HEIGHT)
    {
        task->tFlag = TRUE;
        FadeScreenBlack();
    }
    else
    {
        sTransitionData->VBlank_DMA = TRUE;
    }

    return FALSE;
}

static bool8 Blackhole_Vibrate(struct Task *task)
{
    sTransitionData->VBlank_DMA = FALSE;
    if (task->tFlag == FALSE)
    {
        task->tFlag = TRUE;
        task->tRadius = 48;
        task->tVibrateId = 0;
    }
    task->tRadius += sBlackhole_Vibrations[task->tVibrateId];
    // task->tVibrateId = (task->tVibrateId + 1) % ARRAY_COUNT(sBlackhole_Vibrations);
    // array count is 2, so it alternates between 1 and 0, so we can do this
    task->tVibrateId ^= 1;
    SetCircularMask(gScanlineEffectRegBuffers[0], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, task->tRadius);
    if (task->tRadius < 9)
    {
        task->tState++;
        task->tFlag = FALSE;
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 BlackholePulsate_Main(struct Task *task)
{
    s16 index; // should be s16 I think
    s16 amplitude;
    
    if (task->tRadius >= DISPLAY_HEIGHT)
    {
        DmaStop(0);
        FadeScreenBlack();
        DestroyTask(FindTaskIdByFunc(task->func));
        return FALSE;
    }

    sTransitionData->VBlank_DMA = FALSE;
    if (task->tFlag == FALSE)
    {
        task->tFlag = TRUE;
        task->tSinIndex = 2;
        task->tAmplitude = 2;
    }

    SetCircularMask(gScanlineEffectRegBuffers[0], DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2, task->tRadius);

    index = task->tSinIndex;
    if ((index & 0xFF) <= 128)
    {
        amplitude = task->tAmplitude;
        task->tSinIndex += 8;
    }
    else
    {
        amplitude = task->tAmplitude - 1;
        task->tSinIndex += 16;
    }
    task->tRadius += Sin(index & 0xFF, amplitude);

    if (task->tRadius <= 0)
        task->tRadius = 1;

    if (task->tSinIndex >= 0xFF)
    {
        task->tSinIndex >>= 8;
        task->tAmplitude++;
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

#undef tRadius
#undef tGrowSpeed
#undef tSinIndex
#undef tVibrateId
#undef tAmplitude
#undef tFlag

//----------------------
// B_TRANSITION_GROUDON
//----------------------

#define tTimer data[1]

static void Task_Groudon(u8 taskId)
{
    while (sGroudon_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Groudon_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sGroudon_Tileset, tileset);
    LZ77UnCompVram(sGroudon_Tilemap, tilemap);

    task->tState++;
    task->tTimer = 0;
    return FALSE;
}

static bool8 Groudon_PaletteFlash(struct Task *task)
{
    if (task->tTimer % 3 == 0)
    {
        u16 offset = (task->tTimer % 30) / 3;
        LoadPalette(&sGroudon1_Palette[offset * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer > 58)
    {
        task->tState++;
        task->tTimer = 0;
    }

    return FALSE;
}

static bool8 Groudon_PaletteBrighten(struct Task *task)
{
    if (task->tTimer % 5 == 0)
    {
        LoadPalette(&sGroudon2_Palette[(task->tTimer / 5) * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer > 68)
    {
        task->tState++;
        task->tTimer = 0;
        task->tEndDelay = 30;
    }

    return FALSE;
}

#undef tTimer
#undef tEndDelay

//-----------------------
// B_TRANSITION_RAYQUAZA
//-----------------------

#define tTimer     data[1]
#define tGrowSpeed data[2] // Shared from B_TRANSITION_BLACKHOLE
#define tFlag      data[7] // Shared from B_TRANSITION_BLACKHOLE

static void Task_Rayquaza(u8 taskId)
{
    while (sRayquaza_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 Rayquaza_Init(struct Task *task)
{
    u16 *tilemap, *tileset;
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    SetGpuReg(REG_OFFSET_BG0CNT, BGCNT_CHARBASE(2) | BGCNT_SCREENBASE(26) | BGCNT_TXT256x512);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    CpuCopy16(sRayquaza_Tileset, tileset, 0x2000);

    sTransitionData->counter = 0;
    task->tState++;
    LoadPalette(&sRayquaza_Palette[80], BG_PLTT_ID(15), PLTT_SIZE_4BPP);

    for (i = 0; i < DISPLAY_HEIGHT; i++)
    {
        gScanlineEffectRegBuffers[0][i] = 0;
        gScanlineEffectRegBuffers[1][i] = 0x100;
    }

    SetVBlankCallback(VBlankCB_Rayquaza);
    return FALSE;
}

static bool8 Rayquaza_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    CpuCopy16(sRayquaza_Tilemap, tilemap, sizeof(sRayquaza_Tilemap));
    task->tState++;
    return FALSE;
}

static bool8 Rayquaza_PaletteFlash(struct Task *task)
{
    if ((task->tTimer % 4) == 0)
    {
        u16 value = task->tTimer / 4;
        LoadPalette(&sRayquaza_Palette[(value + 5) * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer > 40)
    {
        task->tState++;
        task->tTimer = 0;
    }

    return FALSE;
}

static bool8 Rayquaza_FadeToBlack(struct Task *task)
{
    if (++task->tTimer > 20)
    {
        task->tState++;
        task->tTimer = 0;
        BeginNormalPaletteFade(PALETTES_OBJECTS | (1 << 15), 2, 0, 16, RGB_BLACK);
    }

    return FALSE;
}

static bool8 Rayquaza_WaitFade(struct Task *task)
{
    if (!gPaletteFade.active)
    {
        sTransitionData->counter = 1;
        task->tState++;
    }
    return FALSE;
}

static bool8 Rayquaza_SetBlack(struct Task *task)
{
    BlendPalettes(PALETTES_BG & ~(1 << 15), 8, RGB_BLACK);
    BlendPalettes(PALETTES_OBJECTS | (1 << 15), 0, RGB_BLACK);

    task->tState++;
    return FALSE;
}

static bool8 Rayquaza_TriRing(struct Task *task)
{
    if ((task->tTimer % 3) == 0)
    {
        u16 value = task->tTimer / 3;
        LoadPalette(&sRayquaza_Palette[value * 16], BG_PLTT_ID(15), PLTT_SIZE_4BPP);
    }
    if (++task->tTimer >= 40)
    {
        u32 i;

        sTransitionData->WININ = 0;
        sTransitionData->WINOUT = WINOUT_WIN01_ALL;
        sTransitionData->WIN0H = DISPLAY_WIDTH;
        sTransitionData->WIN0V = DISPLAY_HEIGHT;

        for (i = 0; i < DISPLAY_HEIGHT; i++)
            gScanlineEffectRegBuffers[1][i] = 0;

        SetVBlankCallback(VBlankCB_CircularMask);
        task->tState++;
        task->tGrowSpeed = 1 << 8;
        task->tFlag = FALSE;
        ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_BG0_ON);
    }

    return FALSE;
}

static void VBlankCB_Rayquaza(void)
{
    void *dmaSrc;

    DmaStop(0);
    VBlankCB_BattleTransition();

    if (sTransitionData->counter == 0)
        dmaSrc = gScanlineEffectRegBuffers[0];
    else if (sTransitionData->counter == 1)
        dmaSrc = gScanlineEffectRegBuffers[1];
    else
        dmaSrc = gScanlineEffectRegBuffers[0];

    DmaSet(0, dmaSrc, &REG_BG0VOFS, B_TRANS_DMA_FLAGS);
}

#undef tTimer
#undef tGrowSpeed
#undef tFlag

//------------------------------
// B_TRANSITION_WHITE_BARS_FADE
//------------------------------

#define sFade            data[0]
#define sFinished        data[1]
#define sDestroyAttempts data[2]
#define sDelay           data[5]
#define sIsMainSprite    data[6]

#define FADE_TARGET (16 << 8)

static void Task_WhiteBarsFade(u8 taskId)
{
    while (sWhiteBarsFade_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 WhiteBarsFade_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    sTransitionData->BLDCNT = BLDCNT_TGT1_ALL | BLDCNT_EFFECT_LIGHTEN;
    sTransitionData->BLDY = 0;
    sTransitionData->WININ = WININ_WIN0_BG1 | WININ_WIN0_BG2 | WININ_WIN0_BG3 | WININ_WIN0_OBJ;
    sTransitionData->WINOUT = WINOUT_WIN01_ALL;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
    {
        gScanlineEffectRegBuffers[1][i] = 0;
        gScanlineEffectRegBuffers[1][i + DISPLAY_HEIGHT] = DISPLAY_WIDTH;
    }

    EnableInterrupts(INTR_FLAG_VBLANK | INTR_FLAG_HBLANK);
    SetHBlankCallback(HBlankCB_WhiteBarsFade);
    SetVBlankCallback(VBlankCB_WhiteBarsFade);

    task->tState++;
    return FALSE;
}

static bool8 WhiteBarsFade_StartBars(struct Task *task)
{
    s16 posY;
    u32 i;
    struct Sprite *sprite;

    for (i = 0, posY = 0; i < NUM_WHITE_BARS; i++, posY += DISPLAY_HEIGHT / NUM_WHITE_BARS)
    {
        sprite = &gSprites[CreateInvisibleSprite(SpriteCB_WhiteBarFade)];
        sprite->x = DISPLAY_WIDTH;
        sprite->y = posY;
        sprite->sDelay = sWhiteBarsFade_StartDelays[i];
    }

    // Set on one sprite only. This one will enable the DMA
    // copy in VBlank and wait for the others to destroy.
    sprite->sIsMainSprite++;

    task->tState++;
    return FALSE;
}

static bool8 WhiteBarsFade_WaitBars(struct Task *task)
{
    sTransitionData->VBlank_DMA = 0;
    if (sTransitionData->counter >= NUM_WHITE_BARS)
    {
        BlendPalettes(PALETTES_ALL, 16, RGB_WHITE);
        task->tState++;
    }
    return FALSE;
}

static bool8 WhiteBarsFade_BlendToBlack(struct Task *task)
{
    sTransitionData->VBlank_DMA = 0;

    DmaStop(0);
    SetVBlankCallback(0);
    SetHBlankCallback(0);

    sTransitionData->WIN0H = DISPLAY_WIDTH;
    sTransitionData->BLDY = 0;
    sTransitionData->BLDCNT = 0xFF;
    sTransitionData->WININ = WININ_WIN0_ALL;

    SetVBlankCallback(VBlankCB_WhiteBarsFade_Blend);

    task->tState++;
    return FALSE;
}

static bool8 WhiteBarsFade_End(struct Task *task)
{
   if (++sTransitionData->BLDY > 16)
   {
       FadeScreenBlack();
       DestroyTask(FindTaskIdByFunc(Task_WhiteBarsFade));
   }
   return FALSE;
}

static void VBlankCB_WhiteBarsFade(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    REG_BLDCNT = sTransitionData->BLDCNT;
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 4);
    DmaSet(0, &gScanlineEffectRegBuffers[1][DISPLAY_HEIGHT], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

static void VBlankCB_WhiteBarsFade_Blend(void)
{
    VBlankCB_BattleTransition();
    REG_BLDY = sTransitionData->BLDY;
    REG_BLDCNT = sTransitionData->BLDCNT;
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0H = sTransitionData->WIN0H;
    REG_WIN0V = sTransitionData->WIN0V;
}

static void HBlankCB_WhiteBarsFade(void)
{
    REG_BLDY = gScanlineEffectRegBuffers[1][REG_VCOUNT];
}

static void SpriteCB_WhiteBarFade(struct Sprite *sprite)
{
    if (sprite->sDelay)
    {
        sprite->sDelay--;
        if (sprite->sIsMainSprite)
            sTransitionData->VBlank_DMA = 1;
        return;
    }
    else
    {
        u16 i;
        u16 *ptr1 = &gScanlineEffectRegBuffers[0][sprite->y];
        u16 *ptr2 = &gScanlineEffectRegBuffers[0][sprite->y + DISPLAY_HEIGHT];
        for (i = 0; i < DISPLAY_HEIGHT / NUM_WHITE_BARS; i++)
        {
            ptr1[i] = sprite->sFade >> 8;
            ptr2[i] = (u8)sprite->x;
        }
        if (sprite->x == 0 && sprite->sFade == FADE_TARGET)
            sprite->sFinished = TRUE;

        sprite->x -= 16;
        sprite->sFade += FADE_TARGET / 32;

        if (sprite->x < 0)
            sprite->x = 0;
        if (sprite->sFade > FADE_TARGET)
            sprite->sFade = FADE_TARGET;

        if (sprite->sIsMainSprite)
            sTransitionData->VBlank_DMA = 1;

        if (sprite->sFinished)
        {
            // If not the main sprite, destroy self. Otherwise, wait until the
            // others have destroyed themselves, or until enough time has elapsed.
            if (!sprite->sIsMainSprite || (sTransitionData->counter >= NUM_WHITE_BARS - 1 && sprite->sDestroyAttempts++ > 7))
            {
                sTransitionData->counter++;
                DestroySprite(sprite);
            }
        }
    }
}

#undef sFade
#undef sFinished
#undef sDestroyAttempts
#undef sDelay
#undef sIsMainSprite

//---------------------------
// B_TRANSITION_GRID_SQUARES
//---------------------------

#define tDelay       data[1]
#define tShrinkStage data[2]

static void Task_GridSquares(u8 taskId)
{
    while (sGridSquares_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 GridSquares_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    CpuSet(sShrinkingBoxTileset, tileset, 16);
    CpuFill16(0xF0 << 8, tilemap, BG_SCREEN_SIZE);
    LoadPalette(sFieldEffectPal_Pokeball, BG_PLTT_ID(15), sizeof(sFieldEffectPal_Pokeball));

    task->tState++;
    return FALSE;
}

static bool8 GridSquares_Main(struct Task *task)
{
    u16 *tileset;

    if (task->tDelay == 0)
    {
        GetBg0TilemapDst(&tileset);
        task->tDelay = 3;
        task->tShrinkStage++;
        CpuSet(&sShrinkingBoxTileset[task->tShrinkStage * 8], tileset, 16);
        if (task->tShrinkStage > 13)
        {
            task->tState++;
            task->tDelay = 16;
        }
    }

    task->tDelay--;
    return FALSE;
}

static bool8 GridSquares_End(struct Task *task)
{
    if (--task->tDelay == 0)
    {
        FadeScreenBlack();
        DestroyTask(FindTaskIdByFunc(Task_GridSquares));
    }
    return FALSE;
}

#undef tDelay
#undef tShrinkStage

//---------------------------
// B_TRANSITION_ANGLED_WIPES
//---------------------------

#define tWipeId data[1]
#define tDir    data[2]
#define tDelay  data[3]

static void Task_AngledWipes(u8 taskId)
{
    while (sAngledWipes_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 AngledWipes_Init(struct Task *task)
{
    u32 i;

    InitTransitionData();
    ScanlineEffect_Clear();

    sTransitionData->WININ = WININ_WIN0_ALL;
    sTransitionData->WINOUT = 0;
    sTransitionData->WIN0V = DISPLAY_HEIGHT;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[0][i] = DISPLAY_WIDTH;

    CpuSet(gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT);
    SetVBlankCallback(VBlankCB_AngledWipes);

    task->tState++;
    return TRUE;
}

static bool8 AngledWipes_SetWipeData(struct Task *task)
{
    InitBlackWipe(&sTransitionData->line,
                  sAngledWipes_MoveData[task->tWipeId][0],
                  sAngledWipes_MoveData[task->tWipeId][1],
                  sAngledWipes_MoveData[task->tWipeId][2],
                  sAngledWipes_MoveData[task->tWipeId][3],
                  1, 1);
    task->tDir = sAngledWipes_MoveData[task->tWipeId][4];
    task->tState++;
    return TRUE;
}

static bool8 AngledWipes_DoWipe(struct Task *task)
{
    bool8 finished;

    sTransitionData->VBlank_DMA = 0;
    u32 i;

    for (i = 0, finished = FALSE; i < 16; i++)
    {
        s16 r3 = gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] >> 8;
        s16 r4 = gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] & 0xFF;
        if (task->tDir == 0)
        {
            // Moving down
            if (r3 < sTransitionData->tWipeCurrX)
                r3 = sTransitionData->tWipeCurrX;
            if (r3 > r4)
                r3 = r4;
        }
        else
        {
            // Moving up
            if (r4 > sTransitionData->tWipeCurrX)
                r4 = sTransitionData->tWipeCurrX;
            if (r4 <= r3)
                r4 = r3;
        }
        gScanlineEffectRegBuffers[0][sTransitionData->tWipeCurrY] = (r4) | (r3 << 8);
        if (finished)
        {
            task->tState++;
            break;
        }
        finished = UpdateBlackWipe(&sTransitionData->line, TRUE, TRUE);
    }

    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static bool8 AngledWipes_TryEnd(struct Task *task)
{
    if (++task->tWipeId < NUM_ANGLED_WIPES)
    {
        // Continue with next wipe
        task->tState++;
        task->tDelay = sAngledWipes_EndDelays[task->tWipeId - 1];
        return TRUE;
    }

    // End transition
    DmaStop(0);
    FadeScreenBlack();
    DestroyTask(FindTaskIdByFunc(Task_AngledWipes));
    return FALSE;
}

static bool8 AngledWipes_StartNext(struct Task *task)
{
    if (--task->tDelay == 0)
    {
        // Return to AngledWipes_SetWipeData
        task->tState = 1;
        return TRUE;
    }

    return FALSE;
}

static void VBlankCB_AngledWipes(void)
{
    DmaStop(0);
    VBlankCB_BattleTransition();
    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
    REG_WININ = sTransitionData->WININ;
    REG_WINOUT = sTransitionData->WINOUT;
    REG_WIN0V = sTransitionData->WIN0V;
    REG_WIN0H = gScanlineEffectRegBuffers[1][0];
    DmaSet(0, gScanlineEffectRegBuffers[1], &REG_WIN0H, B_TRANS_DMA_FLAGS);
}

#undef tWipeId
#undef tDir
#undef tDelay

//-----------------------------------
// Transition intro
//-----------------------------------

#define tFadeToGrayDelay       data[1]
#define tFadeFromGrayDelay     data[2]
#define tNumFades              data[3]
#define tFadeToGrayIncrement   data[4]
#define tFadeFromGrayIncrement data[5]
#define tDelayTimer            data[6]
#define tBlend                 data[7]

static void CreateIntroTask(s16 fadeToGrayDelay, s16 fadeFromGrayDelay, s16 numFades, s16 fadeToGrayIncrement, s16 fadeFromGrayIncrement)
{
    u8 taskId = CreateTask(Task_BattleTransition_Intro, 3);
    gTasks[taskId].tFadeToGrayDelay = fadeToGrayDelay;
    gTasks[taskId].tFadeFromGrayDelay = fadeFromGrayDelay;
    gTasks[taskId].tNumFades = numFades;
    gTasks[taskId].tFadeToGrayIncrement = fadeToGrayIncrement;
    gTasks[taskId].tFadeFromGrayIncrement = fadeFromGrayIncrement;
    gTasks[taskId].tDelayTimer = fadeToGrayDelay;
}

static bool8 IsIntroTaskDone(void)
{
    if (FindTaskIdByFunc(Task_BattleTransition_Intro) == TASK_NONE)
        return TRUE;
    else
        return FALSE;
}

void Task_BattleTransition_Intro(u8 taskId)
{
    while (sTransitionIntroFuncs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 TransitionIntro_FadeToGray(struct Task *task)
{
    if (task->tDelayTimer == 0 || --task->tDelayTimer == 0)
    {
        task->tDelayTimer = task->tFadeToGrayDelay;
        task->tBlend += task->tFadeToGrayIncrement;
        if (task->tBlend > 16)
            task->tBlend = 16;
        BlendPalettes(PALETTES_ALL, task->tBlend, RGB(11, 11, 11));
    }
    if (task->tBlend >= 16)
    {
        // Fade to gray complete, start fade back
        task->tState++;
        task->tDelayTimer = task->tFadeFromGrayDelay;
    }
    return FALSE;
}

static bool8 TransitionIntro_FadeFromGray(struct Task *task)
{
    if (task->tDelayTimer == 0 || --task->tDelayTimer == 0)
    {
        task->tDelayTimer = task->tFadeFromGrayDelay;
        task->tBlend -= task->tFadeFromGrayIncrement;
        if (task->tBlend < 0)
            task->tBlend = 0;
        BlendPalettes(PALETTES_ALL, task->tBlend, RGB(11, 11, 11));
    }
    if (task->tBlend == 0)
    {
        if (--task->tNumFades == 0)
        {
            // All fades done, end intro
            DestroyTask(FindTaskIdByFunc(Task_BattleTransition_Intro));
        }
        else
        {
            // Fade from gray complete, start new fade
            task->tDelayTimer = task->tFadeToGrayDelay;
            task->tState = 0;
        }
    }
    return FALSE;
}

#undef tFadeToGrayDelay
#undef tFadeFromGrayDelay
#undef tNumFades
#undef tFadeToGrayIncrement
#undef tFadeFromGrayIncrement
#undef tDelayTimer
#undef tBlend

//-----------------------------------
// General transition functions
//-----------------------------------

static void InitTransitionData(void)
{
    memset(sTransitionData, 0, sizeof(*sTransitionData));
    GetCameraOffsetWithPan(&sTransitionData->cameraX, &sTransitionData->cameraY);
}

static void VBlankCB_BattleTransition(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void GetBg0TilemapDst(u16 **tileset)
{
    u16 charBase = REG_BG0CNT >> 2;
    charBase <<= 14;
    *tileset = (u16 *)(BG_VRAM + charBase);
}

void GetBg0TilesDst(u16 **tilemap, u16 **tileset)
{
    u16 screenBase = REG_BG0CNT >> 8;
    u16 charBase = REG_BG0CNT >> 2;

    screenBase <<= 11;
    charBase <<= 14;

    *tilemap = (u16 *)(BG_VRAM + screenBase);
    *tileset = (u16 *)(BG_VRAM + charBase);
}

static void FadeScreenBlack(void)
{
    BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
}

static void SetSinWave(u16 *array, s16 sinAdd, s16 index, s16 indexIncrementer, s16 amplitude, s16 arrSize)
{
    u32 i;
    for (i = 0; arrSize > 0; arrSize--, i++, index += indexIncrementer)
        array[i] = sinAdd + Sin(index & 0xFF, amplitude);
}

static void SetCircularMask(u16 *buffer, s16 centerX, s16 centerY, s16 radius)
{
    s16 i;

    memset(buffer, 10, DISPLAY_HEIGHT * sizeof(u16));
    for (i = 0; i < 64; i++)
    {
        s16 sinResult, cosResult;
        s16 drawXLeft, drawYBottNext, drawYTopNext, drawX, drawYTop, drawYBott;

        sinResult = Sin(i, radius);
        cosResult = Cos(i, radius);

        drawXLeft = centerX - sinResult;
        drawX = centerX + sinResult;
        drawYTop = centerY - cosResult;
        drawYBott = centerY + cosResult;

        if (drawXLeft < 0)
            drawXLeft = 0;
        if (drawX > DISPLAY_WIDTH)
            drawX = DISPLAY_WIDTH;
        if (drawYTop < 0)
            drawYTop = 0;
        if (drawYBott > DISPLAY_HEIGHT - 1)
            drawYBott = DISPLAY_HEIGHT - 1;

        drawX |= (drawXLeft << 8);

        buffer[drawYTop] = drawX;
        buffer[drawYBott] = drawX;

        cosResult = Cos(i + 1, radius);

        drawYTopNext = centerY - cosResult;
        drawYBottNext = centerY + cosResult;

        if (drawYTopNext < 0)
            drawYTopNext = 0;
        if (drawYBottNext > DISPLAY_HEIGHT - 1)
            drawYBottNext = DISPLAY_HEIGHT - 1;

        while (drawYTop > drawYTopNext)
            buffer[--drawYTop] = drawX;
        while (drawYTop < drawYTopNext)
            buffer[++drawYTop] = drawX;

        while (drawYBott > drawYBottNext)
            buffer[--drawYBott] = drawX;
        while (drawYBott < drawYBottNext)
            buffer[++drawYBott] = drawX;
    }
}

static void InitBlackWipe(struct LINEWORK *line, s16 startX, s16 startY, s16 endX, s16 endY, s16 xMove, s16 yMove)
{
	line->init_x = startX;
	line->init_y = startY;
	line->x0 = startX;
	line->y0 = startY;
	line->x1 = endX;
	line->y1 = endY;
	line->mx = xMove;
	line->my = yMove;

	line->xdiff = endX - startX;

	if( line->xdiff < 0 )
	{
		line->xdiff = -(line->xdiff);
		line->mx = -xMove;
	}

	line->ydiff = endY - startY;

	if( line->ydiff < 0 )
	{
		line->ydiff = -(line->ydiff);
		line->my = -yMove;
	}
	
	line->work = 0;
}

static bool8 UpdateBlackWipe(struct LINEWORK *line, bool8 xExact, bool8 yExact)
{
	u8 end;

	if( line->xdiff > line->ydiff )
	{
		line->x0 += line->mx;
		line->work += line->ydiff;

		if( line->work > line->xdiff )
		{
			line->y0 += line->my;
			line->work -= line->xdiff;
		}
	}
	else
	{
		line->y0 += line->my;
		line->work += line->xdiff;

		if( line->work > line->ydiff )
		{
			line->x0 += line->mx;
			line->work -= line->ydiff;
		}
	}

	end = 0;

	if( (line->mx > 0 && line->x0 >= line->x1) ||
		(line->mx < 0 && line->x0 <= line->x1) )
	{
		end++;
		if( xExact ) line->x0 = line->x1;
	}

	if( (line->my > 0 && line->y0 >= line->y1) ||
		(line->my < 0 && line->y0 <= line->y1) )
	{
		end++;
		if( yExact ) line->y0 = line->y1;
	}

	if( end == 2 ) return( TRUE );

	return( FALSE );
}

//-----------------------------------
// B_TRANSITION_FRONTIER_LOGO_WIGGLE
//-----------------------------------

#define tSinIndex  data[4]
#define tAmplitude data[5]

static bool8 FrontierLogoWiggle_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    InitPatternWeaveTransition(task);
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sFrontierLogo_Tileset, tileset);
    LoadPalette(sFrontierLogo_Palette, BG_PLTT_ID(15), sizeof(sFrontierLogo_Palette));

    task->tState++;
    return FALSE;
}

static bool8 FrontierLogoWiggle_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sFrontierLogo_Tilemap, tilemap);
    SetSinWave(gScanlineEffectRegBuffers[0], 0, task->tSinIndex, 132, task->tAmplitude, DISPLAY_HEIGHT);

    task->tState++;
    return TRUE;
}

static void Task_FrontierLogoWiggle(u8 taskId)
{
    while (sFrontierLogoWiggle_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

#undef tSinIndex
#undef tAmplitude

//---------------------------------
// B_TRANSITION_FRONTIER_LOGO_WAVE
//---------------------------------

#define tSinVal       data[1]
#define tAmplitudeVal data[2]
#define tTimer        data[3]
#define tStartedFade  data[4]
#define tBlendTarget2 data[5]
#define tBlendTarget1 data[6]
#define tSinDecrement data[7]

static void Task_FrontierLogoWave(u8 taskId)
{
    while (sFrontierLogoWave_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 FrontierLogoWave_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    InitTransitionData();
    ScanlineEffect_Clear();
    ClearGpuRegBits(REG_OFFSET_DISPCNT, DISPCNT_WIN0_ON | DISPCNT_WIN1_ON);
    task->tAmplitudeVal = 32 << 8;
    task->tSinVal = 0x7FFF;
    task->tBlendTarget2 = 0;
    task->tBlendTarget1 = 16;
    task->tSinDecrement = 2560;
    sTransitionData->BLDCNT = BLDCNT_TGT1_BG0 | BLDCNT_EFFECT_BLEND | BLDCNT_TGT2_ALL;
    sTransitionData->BLDALPHA = BLDALPHA_BLEND(task->tBlendTarget2, task->tBlendTarget1);
    REG_BLDCNT = sTransitionData->BLDCNT;
    REG_BLDALPHA = sTransitionData->BLDALPHA;
    GetBg0TilesDst(&tilemap, &tileset);
    CpuFill16(0, tilemap, BG_SCREEN_SIZE);
    LZ77UnCompVram(sFrontierLogo_Tileset, tileset);
    LoadPalette(sFrontierLogo_Palette, BG_PLTT_ID(15), sizeof(sFrontierLogo_Palette));
    sTransitionData->cameraY = 0;

    task->tState++;
    return FALSE;
}

static bool8 FrontierLogoWave_SetGfx(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sFrontierLogo_Tilemap, tilemap);

    task->tState++;
    return TRUE;
}

static bool8 FrontierLogoWave_InitScanline(struct Task *task)
{
    u32 i;

    for (i = 0; i < DISPLAY_HEIGHT; i++)
        gScanlineEffectRegBuffers[1][i] = sTransitionData->cameraY;

    SetVBlankCallback(VBlankCB_FrontierLogoWave);
    SetHBlankCallback(HBlankCB_FrontierLogoWave);
    EnableInterrupts(INTR_FLAG_HBLANK);

    task->tState++;
    return TRUE;
}

static bool8 FrontierLogoWave_Main(struct Task *task)
{
    u8 i;
    u16 sinVal, amplitude, sinSpread;

    sTransitionData->VBlank_DMA = FALSE;

    amplitude = task->tAmplitudeVal >> 8;
    sinVal = task->tSinVal;
    sinSpread = 384;

    task->tSinVal -= task->tSinDecrement;

    if (task->tTimer >= 70)
    {
        // Decrease amount of logo movement and distortion
        // until it rests normally in the middle of the screen.
        if (task->tAmplitudeVal >= 384)
            task->tAmplitudeVal -= 384;
        else
            task->tAmplitudeVal = 0;
    }

    if (task->tTimer >= 0 && task->tTimer % 3 == 0)
    {
        // Blend logo into view
        if (task->tBlendTarget2 < 16)
            task->tBlendTarget2++;
        else if (task->tBlendTarget1 > 0)
            task->tBlendTarget1--;

        sTransitionData->BLDALPHA = BLDALPHA_BLEND(task->tBlendTarget2, task->tBlendTarget1);
    }

    // Move logo up and down and distort it
    for (i = 0; i < DISPLAY_HEIGHT; i++, sinVal += sinSpread)
    {
        u16 index = (sinVal >> 8);
        gScanlineEffectRegBuffers[0][i] = sTransitionData->cameraY + Sin(index & 0xff, amplitude);
    }

    if (++task->tTimer == 101)
    {
        task->tStartedFade++;
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
    }

    if (task->tStartedFade && !gPaletteFade.active)
        DestroyTask(FindTaskIdByFunc(Task_FrontierLogoWave));

    task->tSinDecrement -= 17;
    sTransitionData->VBlank_DMA = TRUE;
    return FALSE;
}

static void VBlankCB_FrontierLogoWave(void)
{
    VBlankCB_BattleTransition();
    REG_BLDCNT = sTransitionData->BLDCNT;
    REG_BLDALPHA = sTransitionData->BLDALPHA;

    if (sTransitionData->VBlank_DMA)
        DmaCopy16(3, gScanlineEffectRegBuffers[0], gScanlineEffectRegBuffers[1], DISPLAY_HEIGHT * 2);
}

static void HBlankCB_FrontierLogoWave(void)
{
    REG_BG0VOFS = gScanlineEffectRegBuffers[1][REG_VCOUNT];
}

#undef tSinVal
#undef tAmplitudeVal
#undef tTimer
#undef tStartedFade
#undef tBlendTarget2
#undef tBlendTarget1
#undef tSinDecrement

//----------------------------------------------------------------------
// B_TRANSITION_FRONTIER_SQUARES, B_TRANSITION_FRONTIER_SQUARES_SCROLL,
// and B_TRANSITION_FRONTIER_SQUARES_SPIRAL
//----------------------------------------------------------------------

#define tPosX             data[2]
#define tPosY             data[3]
#define tRowPos           data[4]
#define tShrinkState      data[5]
#define tShrinkDelayTimer data[6]
#define tShrinkDelay      data[7]

static void Task_FrontierSquares(u8 taskId)
{
    while (sFrontierSquares_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_FrontierSquaresSpiral(u8 taskId)
{
    while (sFrontierSquaresSpiral_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static void Task_FrontierSquaresScroll(u8 taskId)
{
    while (sFrontierSquaresScroll_Funcs[gTasks[taskId].tState](&gTasks[taskId]));
}

static bool8 FrontierSquares_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sFrontierSquares_FilledBg_Tileset, tileset);

    FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 32, 32);
    FillBgTilemapBufferRect(0, 1, 0, 0, MARGIN_SIZE, 32, 15);
    FillBgTilemapBufferRect(0, 1, 30 - MARGIN_SIZE, 0, MARGIN_SIZE, 32, 15);
    CopyBgTilemapBufferToVram(0);
    LoadPalette(sFrontierSquares_Palette, BG_PLTT_ID(15), sizeof(sFrontierSquares_Palette));

    task->tPosX = MARGIN_SIZE;
    task->tPosY = 0;
    task->tRowPos = 0;
    task->tShrinkDelay = 10;

    task->tState++;
    return FALSE;
}

static bool8 FrontierSquares_Draw(struct Task *task)
{
    CopyRectToBgTilemapBufferRect(0, sFrontierSquares_Tilemap, 0, 0,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  task->tPosX, task->tPosY,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  15, 0, 0);
    CopyBgTilemapBufferToVram(0);

    task->tPosX += SQUARE_SIZE;
    if (++task->tRowPos == NUM_SQUARES_PER_ROW)
    {
        task->tPosX = MARGIN_SIZE;
        task->tPosY += SQUARE_SIZE;
        task->tRowPos = 0;
        if (task->tPosY >= NUM_SQUARES_PER_COL * SQUARE_SIZE)
            task->tState++;
    }

    return FALSE;
}

static bool8 FrontierSquares_Shrink(struct Task *task)
{
    u32 i;
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    if (task->tShrinkDelayTimer++ >= task->tShrinkDelay)
    {
        switch (task->tShrinkState)
        {
        case 0:
            for (i = BG_PLTT_ID(15) + 10; i < BG_PLTT_ID(15) + 15; i++)
            {
                gPlttBufferUnfaded[i] = RGB_BLACK;
                gPlttBufferFaded[i] = RGB_BLACK;
            }
            break;
        case 1:
            BlendPalettes(PALETTES_ALL & ~(1 << 15), 16, RGB_BLACK);
            LZ77UnCompVram(sFrontierSquares_EmptyBg_Tileset, tileset);
            break;
        case 2:
            LZ77UnCompVram(sFrontierSquares_Shrink1_Tileset, tileset);
            break;
        case 3:
            LZ77UnCompVram(sFrontierSquares_Shrink2_Tileset, tileset);
            break;
        default:
            FillBgTilemapBufferRect_Palette0(0, 1, 0, 0, 32, 32);
            CopyBgTilemapBufferToVram(0);
            task->tState++;
            return FALSE;
        }

        task->tShrinkDelayTimer = 0;
        task->tShrinkState++;
    }

    return FALSE;
}

#undef tPosX
#undef tPosY
#undef tRowPos
#undef tShrinkState
#undef tShrinkDelayTimer
#undef tShrinkDelay

#define tSquareNum data[2]
#define tFadeFlag  data[3]

static bool8 FrontierSquaresSpiral_Init(struct Task *task)
{
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sFrontierSquares_FilledBg_Tileset, tileset);

    FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 32, 32);
    FillBgTilemapBufferRect(0, 1, 0, 0, MARGIN_SIZE, 32, 15);
    FillBgTilemapBufferRect(0, 1, 30 - MARGIN_SIZE, 0, MARGIN_SIZE, 32, 15);
    CopyBgTilemapBufferToVram(0);
    LoadPalette(sFrontierSquares_Palette, BG_PLTT_ID(14), sizeof(sFrontierSquares_Palette));
    LoadPalette(sFrontierSquares_Palette, BG_PLTT_ID(15), sizeof(sFrontierSquares_Palette));
    BlendPalette(BG_PLTT_ID(14), 16, 8, RGB_BLACK);

    task->tSquareNum = NUM_SQUARES - 1;
    task->tFadeFlag = 0;

    task->tState++;
    return FALSE;
}

static bool8 FrontierSquaresSpiral_Outward(struct Task *task)
{
    u8 pos = sFrontierSquaresSpiral_Positions[task->tSquareNum];
    u8 x = pos % NUM_SQUARES_PER_ROW;
    u8 y = pos / NUM_SQUARES_PER_ROW;
    CopyRectToBgTilemapBufferRect(0, sFrontierSquares_Tilemap, 0, 0,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  SQUARE_SIZE * x + MARGIN_SIZE, SQUARE_SIZE * y,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  15, 0, 0);
    CopyBgTilemapBufferToVram(0);

    if (--task->tSquareNum < 0)
        task->tState++;
    return FALSE;
}

// Now that the overworld is completely covered by the squares,
// set it to black so it's not revealed when the squares are removed.
static bool8 FrontierSquaresSpiral_SetBlack(struct Task *task)
{
    BlendPalette(BG_PLTT_ID(14), 16, 3, RGB_BLACK);
    BlendPalettes(PALETTES_ALL & ~(1 << 15 | 1 << 14), 16, RGB_BLACK);

    task->tSquareNum = 0;
    task->tFadeFlag = 0;

    task->tState++;
    return FALSE;
}

// Spiral inward erasing the squares
static bool8 FrontierSquaresSpiral_Inward(struct Task *task)
{
    // Each square is faded first, then the one that was faded last move is erased.
    if (task->tFadeFlag ^= 1)
    {
        // Shade square
        CopyRectToBgTilemapBufferRect(0, sFrontierSquares_Tilemap, 0, 0,
                                      SQUARE_SIZE, SQUARE_SIZE,
                                      SQUARE_SIZE * (sFrontierSquaresSpiral_Positions[task->tSquareNum] % NUM_SQUARES_PER_ROW) + MARGIN_SIZE,
                                      SQUARE_SIZE * (sFrontierSquaresSpiral_Positions[task->tSquareNum] / NUM_SQUARES_PER_ROW),
                                      SQUARE_SIZE, SQUARE_SIZE,
                                      14, 0, 0);
    }
    else
    {
        if (task->tSquareNum > 0)
        {
            // Erase square
            FillBgTilemapBufferRect(0, 1,
                                    SQUARE_SIZE * (sFrontierSquaresSpiral_Positions[task->tSquareNum - 1] % NUM_SQUARES_PER_ROW) + MARGIN_SIZE,
                                    SQUARE_SIZE * (sFrontierSquaresSpiral_Positions[task->tSquareNum - 1] / NUM_SQUARES_PER_ROW),
                                    SQUARE_SIZE, SQUARE_SIZE,
                                    15);
        }
        task->tSquareNum++;
    }

    if (task->tSquareNum >= NUM_SQUARES)
        task->tState++;

    CopyBgTilemapBufferToVram(0);
    return FALSE;
}

static bool8 FrontierSquares_End(struct Task *task)
{
    FillBgTilemapBufferRect_Palette0(0, 1, 0, 0, 32, 32);
    CopyBgTilemapBufferToVram(0);
    BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);
    DestroyTask(FindTaskIdByFunc(task->func));
    return FALSE;
}

#undef tSquareNum
#undef tFadeFlag

#define tScrollXDir       data[0]
#define tScrollYDir       data[1]
#define tScrollUpdateFlag data[2]

#define tSquareNum        data[2]

static void Task_ScrollBg(u8 taskId)
{
    if (!(gTasks[taskId].tScrollUpdateFlag ^= 1))
    {
        SetGpuReg(REG_OFFSET_BG0VOFS, gBattle_BG0_X);
        SetGpuReg(REG_OFFSET_BG0HOFS, gBattle_BG0_Y);
        gBattle_BG0_X += gTasks[taskId].tScrollXDir;
        gBattle_BG0_Y += gTasks[taskId].tScrollYDir;
    }
}

static bool8 FrontierSquaresScroll_Init(struct Task *task)
{
    u8 taskId = 0;
    u16 *tilemap, *tileset;

    GetBg0TilesDst(&tilemap, &tileset);
    LZ77UnCompVram(sFrontierSquares_FilledBg_Tileset, tileset);
    FillBgTilemapBufferRect_Palette0(0, 0, 0, 0, 32, 32);
    CopyBgTilemapBufferToVram(0);
    LoadPalette(sFrontierSquares_Palette, BG_PLTT_ID(15), sizeof(sFrontierSquares_Palette));

    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    SetGpuReg(REG_OFFSET_BG0VOFS, gBattle_BG0_X);
    SetGpuReg(REG_OFFSET_BG0HOFS, gBattle_BG0_Y);

    task->tSquareNum = 0;

    // Start scrolling bg in a random direction.
    taskId = CreateTask(Task_ScrollBg, 1);
    switch (Random() % 4)
    {
    case 0: // Down/right
        gTasks[taskId].tScrollXDir = 1;
        gTasks[taskId].tScrollYDir = 1;
        break;
    case 1: // Up/left
        gTasks[taskId].tScrollXDir = -1;
        gTasks[taskId].tScrollYDir = -1;
        break;
    case 2: // Up/right
        gTasks[taskId].tScrollXDir = 1;
        gTasks[taskId].tScrollYDir = -1;
        break;
    default: // Down/left
        gTasks[taskId].tScrollXDir = -1;
        gTasks[taskId].tScrollYDir = 1;
        break;
    }

    task->tState++;
    return FALSE;
}

static bool8 FrontierSquaresScroll_Draw(struct Task *task)
{
    u8 pos = sFrontierSquaresScroll_Positions[task->tSquareNum];
    u8 x = pos / (NUM_SQUARES_PER_ROW + 1); // +1 because during scroll an additional column covers the margin.
    u8 y = pos % (NUM_SQUARES_PER_ROW + 1);

    CopyRectToBgTilemapBufferRect(0, &sFrontierSquares_Tilemap, 0, 0,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  SQUARE_SIZE * x + MARGIN_SIZE, SQUARE_SIZE * y,
                                  SQUARE_SIZE, SQUARE_SIZE,
                                  15, 0, 0);
    CopyBgTilemapBufferToVram(0);

    if (++task->tSquareNum >= (int)ARRAY_COUNT(sFrontierSquaresScroll_Positions))
        task->tState++;
    return 0;
}

// Now that the overworld is completely covered by the squares,
// set it to black so it's not revealed when the squares are removed.
static bool8 FrontierSquaresScroll_SetBlack(struct Task *task)
{
    BlendPalettes(PALETTES_ALL & ~(1 << 15), 16, RGB_BLACK);

    task->tSquareNum = 0;

    task->tState++;
    return FALSE;
}

static bool8 FrontierSquaresScroll_Erase(struct Task *task)
{
    u8 pos = sFrontierSquaresScroll_Positions[task->tSquareNum];
    u8 x = pos / (NUM_SQUARES_PER_ROW + 1);
    u8 y = pos % (NUM_SQUARES_PER_ROW + 1);

    FillBgTilemapBufferRect(0, 1,
                            SQUARE_SIZE * x + MARGIN_SIZE, SQUARE_SIZE * y,
                            SQUARE_SIZE, SQUARE_SIZE,
                            15);
    CopyBgTilemapBufferToVram(0);

    if (++task->tSquareNum >= ARRAY_COUNT(sFrontierSquaresScroll_Positions))
    {
        DestroyTask(FindTaskIdByFunc(Task_ScrollBg));
        task->tState++;
    }

    return FALSE;
}

static bool8 FrontierSquaresScroll_End(struct Task *task)
{
    gBattle_BG0_X = 0;
    gBattle_BG0_Y = 0;
    SetGpuReg(REG_OFFSET_BG0VOFS, 0);
    SetGpuReg(REG_OFFSET_BG0HOFS, gBattle_BG0_Y);

    FillBgTilemapBufferRect_Palette0(0, 1, 0, 0, 32, 32);
    CopyBgTilemapBufferToVram(0);
    BlendPalettes(PALETTES_ALL, 16, RGB_BLACK);

    DestroyTask(FindTaskIdByFunc(task->func));
    // task->tState++; // Changing value of a destroyed task

    return FALSE;
}

#undef tScrollXDir
#undef tScrollYDir
#undef tScrollUpdateFlag
#undef tSquareNum
