#include "global.h"
#include "bg.h"
#include "decompress.h"
#include "gpu_regs.h"
#include "main.h"
#include "palette.h"
#include "scanline_effect.h"
#include "sound.h"
#include "sprite.h"
#include "task.h"
#include "text.h"
#include "window.h"
#include "character_select.h"
#include "constants/rgb.h"
#include "constants/songs.h"

// ---------------------------------------------------------------------------
// Hero / Hina selection screen
//
//  - Black background with turquoise corner brackets and a thin line.
//  - Both protagonists are on screen. The selected one is drawn large and
//    bright (two stacked 64x64 sprites); the other is drawn small and dim
//    (one 32x64 sprite) and sits slightly higher, further "in the back".
//  - LEFT selects Hero, RIGHT selects Hina, A confirms.
// ---------------------------------------------------------------------------

#define SEL_HERO 0 // MALE
#define SEL_HINA 1 // FEMALE

#define TAG_HERO_TOP    0x2000
#define TAG_HERO_BOTTOM 0x2001
#define TAG_HERO_SMALL  0x2002
#define TAG_HINA_TOP    0x2003
#define TAG_HINA_BOTTOM 0x2004
#define TAG_HINA_SMALL  0x2005

#define TAG_PAL_HERO_BRIGHT 0x2010
#define TAG_PAL_HERO_DIM    0x2011
#define TAG_PAL_HINA_BRIGHT 0x2012
#define TAG_PAL_HINA_DIM    0x2013

// Sprite layout (sprite x/y are the centre of the sprite).
#define BIG_TOP_Y    38
#define BIG_BOTTOM_Y 102
#define SMALL_Y      98

// Horizontal centre of each character depending on who is selected.
#define HERO_X_WHEN_SELECTED   84
#define HERO_X_WHEN_UNSELECTED 52
#define HINA_X_WHEN_SELECTED   156
#define HINA_X_WHEN_UNSELECTED 188

// Centre of each name (pixels) and the matching overline tiles.
#define HERO_NAME_CENTER_X 64
#define HINA_NAME_CENTER_X 184
#define HERO_LINE_TILE_X   4
#define HINA_LINE_TILE_X   19
#define LINE_TILE_WIDTH    8
#define LINE_TILE_Y        17

// Frames during which input is ignored after switching character.
#define SWITCH_LOCK_FRAMES 8

// Decoration tile numbers (see sDecorTiles).
#define DECOR_BLANK        0
#define DECOR_HLINE        1
#define DECOR_VLINE        2
#define DECOR_CORNER_TL    3
#define DECOR_CORNER_TR    4
#define DECOR_CORNER_BL    5
#define DECOR_CORNER_BR    6
#define DECOR_DIM_LINE     7
#define DECOR_BRIGHT_LINE  8

#define tSelection data[0]
#define tLockTimer data[1]

// ---------------------------------------------------------------------------
// Graphics
// ---------------------------------------------------------------------------

static const u32 sHeroTop_Gfx[]    = INCGFX_U32("graphics/character_select/hero_top.png", ".4bpp.smol");
static const u32 sHeroBottom_Gfx[] = INCGFX_U32("graphics/character_select/hero_bottom.png", ".4bpp.smol");
static const u32 sHeroSmall_Gfx[]  = INCGFX_U32("graphics/character_select/hero_small.png", ".4bpp.smol");
static const u32 sHinaTop_Gfx[]    = INCGFX_U32("graphics/character_select/hina_top.png", ".4bpp.smol");
static const u32 sHinaBottom_Gfx[] = INCGFX_U32("graphics/character_select/hina_bottom.png", ".4bpp.smol");
static const u32 sHinaSmall_Gfx[]  = INCGFX_U32("graphics/character_select/hina_small.png", ".4bpp.smol");

// The big sprites carry the bright palette, the small sprites the dim one.
static const u16 sHeroBright_Pal[] = INCGFX_U16("graphics/character_select/hero_top.png", ".gbapal");
static const u16 sHeroDim_Pal[]    = INCGFX_U16("graphics/character_select/hero_small.png", ".gbapal");
static const u16 sHinaBright_Pal[] = INCGFX_U16("graphics/character_select/hina_top.png", ".gbapal");
static const u16 sHinaDim_Pal[]    = INCGFX_U16("graphics/character_select/hina_small.png", ".gbapal");

static const struct CompressedSpriteSheet sSpriteSheets[] =
{
    { .data = sHeroTop_Gfx,    .size = 0x800, .tag = TAG_HERO_TOP },
    { .data = sHeroBottom_Gfx, .size = 0x800, .tag = TAG_HERO_BOTTOM },
    { .data = sHeroSmall_Gfx,  .size = 0x400, .tag = TAG_HERO_SMALL },
    { .data = sHinaTop_Gfx,    .size = 0x800, .tag = TAG_HINA_TOP },
    { .data = sHinaBottom_Gfx, .size = 0x800, .tag = TAG_HINA_BOTTOM },
    { .data = sHinaSmall_Gfx,  .size = 0x400, .tag = TAG_HINA_SMALL },
};

static const struct SpritePalette sSpritePalettes[] =
{
    { .data = sHeroBright_Pal, .tag = TAG_PAL_HERO_BRIGHT },
    { .data = sHeroDim_Pal,    .tag = TAG_PAL_HERO_DIM },
    { .data = sHinaBright_Pal, .tag = TAG_PAL_HINA_BRIGHT },
    { .data = sHinaDim_Pal,    .tag = TAG_PAL_HINA_DIM },
    {}
};

static const struct OamData sOam_Big =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(64x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(64x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

static const struct OamData sOam_Small =
{
    .y = DISPLAY_HEIGHT,
    .affineMode = ST_OAM_AFFINE_OFF,
    .objMode = ST_OAM_OBJ_NORMAL,
    .mosaic = FALSE,
    .bpp = ST_OAM_4BPP,
    .shape = SPRITE_SHAPE(32x64),
    .x = 0,
    .matrixNum = 0,
    .size = SPRITE_SIZE(32x64),
    .tileNum = 0,
    .priority = 1,
    .paletteNum = 0,
    .affineParam = 0,
};

#define CHAR_SPRITE_TEMPLATE(tile, pal, oamData)    \
{                                                   \
    .tileTag = tile,                                \
    .paletteTag = pal,                              \
    .oam = oamData,                                 \
    .anims = gDummySpriteAnimTable,                 \
    .images = NULL,                                 \
    .affineAnims = gDummySpriteAffineAnimTable,     \
    .callback = SpriteCallbackDummy,                \
}

static const struct SpriteTemplate sSpriteTemplate_HeroTop    = CHAR_SPRITE_TEMPLATE(TAG_HERO_TOP, TAG_PAL_HERO_BRIGHT, &sOam_Big);
static const struct SpriteTemplate sSpriteTemplate_HeroBottom = CHAR_SPRITE_TEMPLATE(TAG_HERO_BOTTOM, TAG_PAL_HERO_BRIGHT, &sOam_Big);
static const struct SpriteTemplate sSpriteTemplate_HeroSmall  = CHAR_SPRITE_TEMPLATE(TAG_HERO_SMALL, TAG_PAL_HERO_DIM, &sOam_Small);
static const struct SpriteTemplate sSpriteTemplate_HinaTop    = CHAR_SPRITE_TEMPLATE(TAG_HINA_TOP, TAG_PAL_HINA_BRIGHT, &sOam_Big);
static const struct SpriteTemplate sSpriteTemplate_HinaBottom = CHAR_SPRITE_TEMPLATE(TAG_HINA_BOTTOM, TAG_PAL_HINA_BRIGHT, &sOam_Big);
static const struct SpriteTemplate sSpriteTemplate_HinaSmall  = CHAR_SPRITE_TEMPLATE(TAG_HINA_SMALL, TAG_PAL_HINA_DIM, &sOam_Small);

static u8 sHeroTopId;
static u8 sHeroBottomId;
static u8 sHeroSmallId;
static u8 sHinaTopId;
static u8 sHinaBottomId;
static u8 sHinaSmallId;

// ---------------------------------------------------------------------------
// Background: decoration tiles (4bpp, drawn here so no extra image is needed)
//   colour 1 = turquoise, colour 2 = dark teal
// ---------------------------------------------------------------------------

static const u32 sDecorTiles[] =
{
    // 0: blank
    0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // 1: horizontal line
    0x00000000, 0x00000000, 0x00000000, 0x11111111, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // 2: vertical line
    0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000, 0x00001000,
    // 3: corner top-left
    0x00000000, 0x00000000, 0x00000000, 0x11111000, 0x00001000, 0x00001000, 0x00001000, 0x00001000,
    // 4: corner top-right
    0x00000000, 0x00000000, 0x00000000, 0x00001111, 0x00001000, 0x00001000, 0x00001000, 0x00001000,
    // 5: corner bottom-left
    0x00001000, 0x00001000, 0x00001000, 0x11111000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // 6: corner bottom-right
    0x00001000, 0x00001000, 0x00001000, 0x00001111, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // 7: dim horizontal line
    0x00000000, 0x00000000, 0x00000000, 0x22222222, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
    // 8: bright, thicker horizontal line (selection marker)
    0x00000000, 0x00000000, 0x00000000, 0x11111111, 0x11111111, 0x00000000, 0x00000000, 0x00000000,
};

static const u16 sDecorPal[16] =
{
    RGB_BLACK,       // 0: transparent / backdrop
    RGB(0, 29, 26),  // 1: turquoise
    RGB(0, 11, 11),  // 2: dark teal
};

// Text uses palette bank 1:
//   1 = turquoise (selected name), 2 = dark teal (its shadow),
//   3 = grey (unselected name),    4 = dark grey (its shadow)
static const u16 sTextPal[16] =
{
    RGB_BLACK,
    RGB(0, 29, 26),
    RGB(0, 11, 11),
    RGB(14, 15, 16),
    RGB(4, 5, 6),
};

static const u8 sTextColors_Selected[] = {0, 1, 2};
static const u8 sTextColors_Unselected[] = {0, 3, 4};

static const u8 sText_Hero[] = _("Hero");
static const u8 sText_Hina[] = _("Hina");

static EWRAM_DATA u16 sDecorTilemapBuffer[0x400] = {0};

static const struct BgTemplate sBgTemplates[] =
{
    {
        .bg = 0,
        .charBaseIndex = 0,
        .mapBaseIndex = 31,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 0,
        .baseTile = 0
    },
    {
        .bg = 1,
        .charBaseIndex = 1,
        .mapBaseIndex = 30,
        .screenSize = 0,
        .paletteMode = 0,
        .priority = 1,
        .baseTile = 0
    },
};

static const struct WindowTemplate sWindowTemplates[] =
{
    {
        .bg = 0,
        .tilemapLeft = 0,
        .tilemapTop = 18,
        .width = 30,
        .height = 2,
        .paletteNum = 1,
        .baseBlock = 1
    },
    DUMMY_WIN_TEMPLATE,
};

// ---------------------------------------------------------------------------
// Functions
// ---------------------------------------------------------------------------

static void VBlankCB_CharacterSelect(void);
static void CB2_CharacterSelectMain(void);
static void Task_CharacterSelectInput(u8 taskId);
static void Task_CharacterSelectFadeOut(u8 taskId);
static void DrawStaticDecor(void);
static void DrawSelectionVisuals(u8 selection);
static void ApplySelection(u8 selection);

static void VBlankCB_CharacterSelect(void)
{
    LoadOam();
    ProcessSpriteCopyRequests();
    TransferPlttBuffer();
}

static void CB2_CharacterSelectMain(void)
{
    RunTasks();
    AnimateSprites();
    BuildOamBuffer();
    DoScheduledBgTilemapCopiesToVram();
    UpdatePaletteFade();
}

// Corner brackets and the thin dim line above the names.
static void DrawStaticDecor(void)
{
    FillBgTilemapBufferRect_Palette0(1, DECOR_BLANK, 0, 0, 32, 32);

    // top-left
    FillBgTilemapBufferRect_Palette0(1, DECOR_CORNER_TL, 1, 1, 1, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_HLINE, 2, 1, 4, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_VLINE, 1, 2, 1, 3);
    // top-right
    FillBgTilemapBufferRect_Palette0(1, DECOR_CORNER_TR, 28, 1, 1, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_HLINE, 24, 1, 4, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_VLINE, 28, 2, 1, 3);
    // bottom-left
    FillBgTilemapBufferRect_Palette0(1, DECOR_CORNER_BL, 1, 15, 1, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_HLINE, 2, 15, 4, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_VLINE, 1, 12, 1, 3);
    // bottom-right
    FillBgTilemapBufferRect_Palette0(1, DECOR_CORNER_BR, 28, 15, 1, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_HLINE, 24, 15, 4, 1);
    FillBgTilemapBufferRect_Palette0(1, DECOR_VLINE, 28, 12, 1, 3);
}

// Redraws the parts that depend on the selection: the bright line above the
// chosen name, and both names in turquoise (selected) or grey (not selected).
static void DrawSelectionVisuals(u8 selection)
{
    const u8 *heroColors = (selection == SEL_HERO) ? sTextColors_Selected : sTextColors_Unselected;
    const u8 *hinaColors = (selection == SEL_HINA) ? sTextColors_Selected : sTextColors_Unselected;
    s32 x;

    // line above the names
    FillBgTilemapBufferRect_Palette0(1, DECOR_DIM_LINE, 1, LINE_TILE_Y, 28, 1);
    if (selection == SEL_HERO)
        FillBgTilemapBufferRect_Palette0(1, DECOR_BRIGHT_LINE, HERO_LINE_TILE_X, LINE_TILE_Y, LINE_TILE_WIDTH, 1);
    else
        FillBgTilemapBufferRect_Palette0(1, DECOR_BRIGHT_LINE, HINA_LINE_TILE_X, LINE_TILE_Y, LINE_TILE_WIDTH, 1);
    ScheduleBgCopyTilemapToVram(1);

    // names
    FillWindowPixelBuffer(0, PIXEL_FILL(0));
    x = HERO_NAME_CENTER_X - GetStringWidth(FONT_NORMAL, sText_Hero, 0) / 2;
    AddTextPrinterParameterized3(0, FONT_NORMAL, x, 0, heroColors, 0, sText_Hero);
    x = HINA_NAME_CENTER_X - GetStringWidth(FONT_NORMAL, sText_Hina, 0) / 2;
    AddTextPrinterParameterized3(0, FONT_NORMAL, x, 0, hinaColors, 0, sText_Hina);
    PutWindowTilemap(0);
    CopyWindowToVram(0, COPYWIN_FULL);
}

static void ShowBig(u8 topId, u8 bottomId, u8 smallId, s16 x)
{
    gSprites[topId].x = x;
    gSprites[topId].y = BIG_TOP_Y;
    gSprites[topId].invisible = FALSE;
    gSprites[bottomId].x = x;
    gSprites[bottomId].y = BIG_BOTTOM_Y;
    gSprites[bottomId].invisible = FALSE;
    gSprites[smallId].invisible = TRUE;
}

static void ShowSmall(u8 topId, u8 bottomId, u8 smallId, s16 x)
{
    gSprites[topId].invisible = TRUE;
    gSprites[bottomId].invisible = TRUE;
    gSprites[smallId].x = x;
    gSprites[smallId].y = SMALL_Y;
    gSprites[smallId].invisible = FALSE;
}

static void ApplySelection(u8 selection)
{
    if (selection == SEL_HERO)
    {
        ShowBig(sHeroTopId, sHeroBottomId, sHeroSmallId, HERO_X_WHEN_SELECTED);
        ShowSmall(sHinaTopId, sHinaBottomId, sHinaSmallId, HINA_X_WHEN_UNSELECTED);
    }
    else
    {
        ShowSmall(sHeroTopId, sHeroBottomId, sHeroSmallId, HERO_X_WHEN_UNSELECTED);
        ShowBig(sHinaTopId, sHinaBottomId, sHinaSmallId, HINA_X_WHEN_SELECTED);
    }
    DrawSelectionVisuals(selection);
}

static void Task_CharacterSelectInput(u8 taskId)
{
    s16 *data = gTasks[taskId].data;

    // Ignore input while fading in/out and briefly after switching, so a
    // button press cannot confirm the wrong protagonist mid-transition.
    if (gPaletteFade.active)
        return;
    if (tLockTimer != 0)
    {
        tLockTimer--;
        return;
    }

    if (JOY_NEW(DPAD_LEFT) && tSelection != SEL_HERO)
    {
        PlaySE(SE_SELECT);
        tSelection = SEL_HERO;
        tLockTimer = SWITCH_LOCK_FRAMES;
        ApplySelection(tSelection);
    }
    else if (JOY_NEW(DPAD_RIGHT) && tSelection != SEL_HINA)
    {
        PlaySE(SE_SELECT);
        tSelection = SEL_HINA;
        tLockTimer = SWITCH_LOCK_FRAMES;
        ApplySelection(tSelection);
    }
    else if (JOY_NEW(A_BUTTON))
    {
        PlaySE(SE_SELECT);
        gSaveBlock2Ptr->playerGender = tSelection; // 0 = MALE (Hero), 1 = FEMALE (Hina)
        BeginNormalPaletteFade(PALETTES_ALL, 0, 0, 16, RGB_BLACK);
        gTasks[taskId].func = Task_CharacterSelectFadeOut;
    }
    // B does nothing: there is no earlier screen to go back to.
}

static void Task_CharacterSelectFadeOut(u8 taskId)
{
    if (!gPaletteFade.active)
    {
        FreeAllWindowBuffers();
        DestroyTask(taskId);
        MainMenu_StartNamingScreenAfterCharacterSelect();
    }
}

void CB2_CharacterSelect(void)
{
    u8 taskId;
    u8 i;

    SetVBlankCallback(NULL);

    SetGpuReg(REG_OFFSET_DISPCNT, 0);
    SetGpuReg(REG_OFFSET_BG3CNT, 0);
    SetGpuReg(REG_OFFSET_BG2CNT, 0);
    SetGpuReg(REG_OFFSET_BG1CNT, 0);
    SetGpuReg(REG_OFFSET_BG0CNT, 0);

    ChangeBgX(0, 0, BG_COORD_SET);
    ChangeBgY(0, 0, BG_COORD_SET);
    ChangeBgX(1, 0, BG_COORD_SET);
    ChangeBgY(1, 0, BG_COORD_SET);
    ChangeBgX(2, 0, BG_COORD_SET);
    ChangeBgY(2, 0, BG_COORD_SET);
    ChangeBgX(3, 0, BG_COORD_SET);
    ChangeBgY(3, 0, BG_COORD_SET);

    DmaFill16(3, 0, VRAM, VRAM_SIZE);
    DmaFill32(3, 0, OAM, OAM_SIZE);
    DmaFill16(3, 0, PLTT, PLTT_SIZE);

    ResetBgsAndClearDma3BusyFlags(0);
    InitBgsFromTemplates(0, sBgTemplates, ARRAY_COUNT(sBgTemplates));
    SetBgTilemapBuffer(1, sDecorTilemapBuffer);
    InitWindows(sWindowTemplates);

    DeactivateAllTextPrinters();
    ClearScheduledBgCopiesToVram();
    ScanlineEffect_Stop();
    ResetTasks();
    ResetSpriteData();
    ResetPaletteFade();
    FreeAllSpritePalettes();

    LoadPalette(sDecorPal, BG_PLTT_ID(0), PLTT_SIZE_4BPP);
    LoadPalette(sTextPal, BG_PLTT_ID(1), PLTT_SIZE_4BPP);
    CpuCopy32(sDecorTiles, (void *)BG_CHAR_ADDR(1), sizeof(sDecorTiles));
    DrawStaticDecor();

    for (i = 0; i < ARRAY_COUNT(sSpriteSheets); i++)
        LoadCompressedSpriteSheet(&sSpriteSheets[i]);
    LoadSpritePalettes(sSpritePalettes);

    sHeroTopId    = CreateSprite(&sSpriteTemplate_HeroTop, HERO_X_WHEN_SELECTED, BIG_TOP_Y, 0);
    sHeroBottomId = CreateSprite(&sSpriteTemplate_HeroBottom, HERO_X_WHEN_SELECTED, BIG_BOTTOM_Y, 0);
    sHeroSmallId  = CreateSprite(&sSpriteTemplate_HeroSmall, HERO_X_WHEN_UNSELECTED, SMALL_Y, 0);
    sHinaTopId    = CreateSprite(&sSpriteTemplate_HinaTop, HINA_X_WHEN_SELECTED, BIG_TOP_Y, 0);
    sHinaBottomId = CreateSprite(&sSpriteTemplate_HinaBottom, HINA_X_WHEN_SELECTED, BIG_BOTTOM_Y, 0);
    sHinaSmallId  = CreateSprite(&sSpriteTemplate_HinaSmall, HINA_X_WHEN_UNSELECTED, SMALL_Y, 0);

    taskId = CreateTask(Task_CharacterSelectInput, 0);
    gTasks[taskId].tSelection = SEL_HERO;
    gTasks[taskId].tLockTimer = 0;
    ApplySelection(SEL_HERO);

    BeginNormalPaletteFade(PALETTES_ALL, 0, 16, 0, RGB_BLACK);

    EnableInterrupts(DISPSTAT_VBLANK);
    SetVBlankCallback(VBlankCB_CharacterSelect);
    SetMainCallback2(CB2_CharacterSelectMain);

    SetGpuReg(REG_OFFSET_DISPCNT, DISPCNT_OBJ_ON | DISPCNT_OBJ_1D_MAP);
    ShowBg(0);
    ShowBg(1);
}

#undef tSelection
#undef tLockTimer
