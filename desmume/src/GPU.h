/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2009-2012 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GPU_H
#define GPU_H

#include <stdio.h>
#include "mem.h"
#include "common.h"
#include "registers.h"
#include "FIFO.h"
#include "MMU.h"
#include <iosfwd>

//#undef FORCEINLINE
//#define FORCEINLINE

void gpu_savestate(EMUFILE* os);
bool gpu_loadstate(EMUFILE* is, int size);

/*******************************************************************************
    this structure is for display control,
    it holds flags for general display
*******************************************************************************/

#ifdef WORDS_BIGENDIAN
struct _DISPCNT
{
/* 7*/  u8 ForceBlank:1;      // A+B:
/* 6*/  u8 OBJ_BMP_mapping:1; // A+B: 0=2D (128KB), 1=1D (128..256KB)
/* 5*/  u8 OBJ_BMP_2D_dim:1;  // A+B: 0=128x512,    1=256x256 pixels
/* 4*/  u8 OBJ_Tile_mapping:1;// A+B: 0=2D (32KB),  1=1D (32..256KB)
/* 3*/  u8 BG0_3D:1;          // A  : 0=2D,         1=3D
/* 0*/  u8 BG_Mode:3;         // A+B:
/*15*/  u8 WinOBJ_Enable:1;   // A+B: 0=disable, 1=Enable
/*14*/  u8 Win1_Enable:1;     // A+B: 0=disable, 1=Enable
/*13*/  u8 Win0_Enable:1;     // A+B: 0=disable, 1=Enable
/*12*/  u8 OBJ_Enable:1;      // A+B: 0=disable, 1=Enable
/*11*/  u8 BG3_Enable:1;      // A+B: 0=disable, 1=Enable
/*10*/  u8 BG2_Enable:1;      // A+B: 0=disable, 1=Enable
/* 9*/  u8 BG1_Enable:1;      // A+B: 0=disable, 1=Enable
/* 8*/  u8 BG0_Enable:1;        // A+B: 0=disable, 1=Enable
/*23*/  u8 OBJ_HBlank_process:1;    // A+B: OBJ processed during HBlank (GBA bit5)
/*22*/  u8 OBJ_BMP_1D_Bound:1;      // A  :
/*20*/  u8 OBJ_Tile_1D_Bound:2;     // A+B:
/*18*/  u8 VRAM_Block:2;            // A  : VRAM block (0..3=A..D)

/*16*/  u8 DisplayMode:2;     // A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
                                    // 0=off (white screen)
                                    // 1=on (normal BG & OBJ layers)
                                    // 2=VRAM display (coreA only)
                                    // 3=RAM display (coreA only, DMA transfers)

/*31*/  u8 ExOBJPalette_Enable:1;   // A+B: 0=disable, 1=Enable OBJ extended Palette
/*30*/  u8 ExBGxPalette_Enable:1;   // A+B: 0=disable, 1=Enable BG extended Palette
/*27*/  u8 ScreenBase_Block:3;      // A  : Screen Base (64K step)
/*24*/  u8 CharacBase_Block:3;      // A  : Character Base (64K step)
};
#else
struct _DISPCNT
{
/* 0*/  u8 BG_Mode:3;         // A+B:
/* 3*/  u8 BG0_3D:1;          // A  : 0=2D,         1=3D
/* 4*/  u8 OBJ_Tile_mapping:1;     // A+B: 0=2D (32KB),  1=1D (32..256KB)
/* 5*/  u8 OBJ_BMP_2D_dim:1;  // A+B: 0=128x512,    1=256x256 pixels
/* 6*/  u8 OBJ_BMP_mapping:1; // A+B: 0=2D (128KB), 1=1D (128..256KB)

                                    // 7-15 same as GBA
/* 7*/  u8 ForceBlank:1;      // A+B:
/* 8*/  u8 BG0_Enable:1;        // A+B: 0=disable, 1=Enable
/* 9*/  u8 BG1_Enable:1;      // A+B: 0=disable, 1=Enable
/*10*/  u8 BG2_Enable:1;      // A+B: 0=disable, 1=Enable
/*11*/  u8 BG3_Enable:1;      // A+B: 0=disable, 1=Enable
/*12*/  u8 OBJ_Enable:1;      // A+B: 0=disable, 1=Enable
/*13*/  u8 Win0_Enable:1;     // A+B: 0=disable, 1=Enable
/*14*/  u8 Win1_Enable:1;     // A+B: 0=disable, 1=Enable
/*15*/  u8 WinOBJ_Enable:1;   // A+B: 0=disable, 1=Enable

/*16*/  u8 DisplayMode:2;     // A+B: coreA(0..3) coreB(0..1) GBA(Green Swap)
                                    // 0=off (white screen)
                                    // 1=on (normal BG & OBJ layers)
                                    // 2=VRAM display (coreA only)
                                    // 3=RAM display (coreA only, DMA transfers)

/*18*/  u8 VRAM_Block:2;            // A  : VRAM block (0..3=A..D)
/*20*/  u8 OBJ_Tile_1D_Bound:2;     // A+B:
/*22*/  u8 OBJ_BMP_1D_Bound:1;      // A  :
/*23*/  u8 OBJ_HBlank_process:1;    // A+B: OBJ processed during HBlank (GBA bit5)
/*24*/  u8 CharacBase_Block:3;      // A  : Character Base (64K step)
/*27*/  u8 ScreenBase_Block:3;      // A  : Screen Base (64K step)
/*30*/  u8 ExBGxPalette_Enable:1;   // A+B: 0=disable, 1=Enable BG extended Palette
/*31*/  u8 ExOBJPalette_Enable:1;   // A+B: 0=disable, 1=Enable OBJ extended Palette
};
#endif

typedef union
{
    struct _DISPCNT bits;
    u32 val;
} DISPCNT;
#define BGxENABLED(cnt,num)    ((num<8)? ((cnt.val>>8) & num):0)


enum BlendFunc
{
	NoBlend, Blend, Increase, Decrease
};


/*******************************************************************************
    this structure is for display control of a specific layer,
    there are 4 background layers
    their priority indicate which one to draw on top of the other
    some flags indicate special drawing mode, size, FX
*******************************************************************************/

#ifdef WORDS_BIGENDIAN
struct _BGxCNT
{
/* 7*/ u8 Palette_256:1;         // 0=16x16, 1=1*256 palette
/* 6*/ u8 Mosaic_Enable:1;       // 0=disable, 1=Enable mosaic
/* 2*/ u8 CharacBase_Block:4;    // individual character base offset (n*16KB)
/* 0*/ u8 Priority:2;            // 0..3=high..low
/*14*/ u8 ScreenSize:2;          // text    : 256x256 512x256 256x512 512x512
                                       // x/rot/s : 128x128 256x256 512x512 1024x1024
                                       // bmp     : 128x128 256x256 512x256 512x512
                                       // large   : 512x1024 1024x512 - -
/*13*/ u8 PaletteSet_Wrap:1;     // BG0 extended palette set 0=set0, 1=set2
                                       // BG1 extended palette set 0=set1, 1=set3
                                       // BG2 overflow area wraparound 0=off, 1=wrap
                                       // BG3 overflow area wraparound 0=off, 1=wrap
/* 8*/ u8 ScreenBase_Block:5;    // individual screen base offset (text n*2KB, BMP n*16KB)
};
#else
struct _BGxCNT
{
/* 0*/ u8 Priority:2;            // 0..3=high..low
/* 2*/ u8 CharacBase_Block:4;    // individual character base offset (n*16KB)
/* 6*/ u8 Mosaic_Enable:1;       // 0=disable, 1=Enable mosaic
/* 7*/ u8 Palette_256:1;         // 0=16x16, 1=1*256 palette
/* 8*/ u8 ScreenBase_Block:5;    // individual screen base offset (text n*2KB, BMP n*16KB)
/*13*/ u8 PaletteSet_Wrap:1;     // BG0 extended palette set 0=set0, 1=set2
                                       // BG1 extended palette set 0=set1, 1=set3
                                       // BG2 overflow area wraparound 0=off, 1=wrap
                                       // BG3 overflow area wraparound 0=off, 1=wrap
/*14*/ u8 ScreenSize:2;          // text    : 256x256 512x256 256x512 512x512
                                       // x/rot/s : 128x128 256x256 512x512 1024x1024
                                       // bmp     : 128x128 256x256 512x256 512x512
                                       // large   : 512x1024 1024x512 - -
};
#endif


typedef union
{
    struct _BGxCNT bits;
    u16 val;
} BGxCNT;

/*******************************************************************************
    this structure is for background offset
*******************************************************************************/

typedef struct {
    u16 BGxHOFS;
    u16 BGxVOFS;
} BGxOFS;

/*******************************************************************************
    this structure is for rotoscale parameters
*******************************************************************************/

typedef struct {
    s16 BGxPA;
    s16 BGxPB;
    s16 BGxPC;
    s16 BGxPD;
    s32 BGxX;
    s32 BGxY;
} BGxPARMS;


/*******************************************************************************
	these structures are for window description,
	windows are square regions and can "subclass"
	background layers or object layers (i.e window controls the layers)

	screen
	|
	+-- Window0/Window1/OBJwindow/OutOfWindows
		|
		+-- BG0/BG1/BG2/BG3/OBJ
*******************************************************************************/

typedef union {
	struct	{
		u8 end:8;
		u8 start:8;
	} bits ;
	u16 val;
} WINxDIM;

#ifdef WORDS_BIGENDIAN
typedef struct {
/* 6*/  u8 :2;
/* 5*/  u8 WINx_Effect_Enable:1;
/* 4*/  u8 WINx_OBJ_Enable:1;
/* 3*/  u8 WINx_BG3_Enable:1;
/* 2*/  u8 WINx_BG2_Enable:1;
/* 1*/  u8 WINx_BG1_Enable:1;
/* 0*/  u8 WINx_BG0_Enable:1;
} WINxBIT;
#else
typedef struct {
/* 0*/  u8 WINx_BG0_Enable:1;
/* 1*/  u8 WINx_BG1_Enable:1;
/* 2*/  u8 WINx_BG2_Enable:1;
/* 3*/  u8 WINx_BG3_Enable:1;
/* 4*/  u8 WINx_OBJ_Enable:1;
/* 5*/  u8 WINx_Effect_Enable:1;
/* 6*/  u8 :2;
} WINxBIT;
#endif

#ifdef WORDS_BIGENDIAN
typedef union {
	struct {
		WINxBIT win0;
		WINxBIT win1;
	} bits;
	struct {
		u8 :3;
		u8 win0_en:5;
		u8 :3;
		u8 win1_en:5;
	} packed_bits;
	struct {
		u8 low;
		u8 high;
	} bytes;
	u16 val ;
} WINxCNT ;
#else
typedef union {
	struct {
		WINxBIT win0;
		WINxBIT win1;
	} bits;
	struct {
		u8 win0_en:5;
		u8 :3;
		u8 win1_en:5;
		u8 :3;
	} packed_bits;
	struct {
		u8 low;
		u8 high;
	} bytes;
	u16 val ;
} WINxCNT ;
#endif

/*
typedef struct {
    WINxDIM WIN0H;
    WINxDIM WIN1H;
    WINxDIM WIN0V;
    WINxDIM WIN1V;
    WINxCNT WININ;
    WINxCNT WINOUT;
} WINCNT;
*/

/*******************************************************************************
    this structure is for miscellanous settings
    //TODO: needs further description
*******************************************************************************/

typedef struct {
    u16 MOSAIC;
    u16 unused1;
    u16 unused2;//BLDCNT;
    u16 unused3;//BLDALPHA;
    u16 unused4;//BLDY;
    u16 unused5;
	/*
    u16 unused6;
    u16 unused7;
    u16 unused8;
    u16 unused9;
	*/
} MISCCNT;


/*******************************************************************************
    this structure is for 3D settings
*******************************************************************************/

struct _DISP3DCNT
{
/* 0*/ u8 EnableTexMapping:1;    //
/* 1*/ u8 PolygonShading:1;      // 0=Toon Shading, 1=Highlight Shading
/* 2*/ u8 EnableAlphaTest:1;     // see ALPHA_TEST_REF
/* 3*/ u8 EnableAlphaBlending:1; // see various Alpha values
/* 4*/ u8 EnableAntiAliasing:1;  //
/* 5*/ u8 EnableEdgeMarking:1;   // see EDGE_COLOR
/* 6*/ u8 FogOnlyAlpha:1;        // 0=Alpha and Color, 1=Only Alpha (see FOG_COLOR)
/* 7*/ u8 EnableFog:1;           // Fog Master Enable
/* 8*/ u8 FogShiftSHR:4;         // 0..10 SHR-Divider (see FOG_OFFSET)
/*12*/ u8 AckColorBufferUnderflow:1; // Color Buffer RDLINES Underflow (0=None, 1=Underflow/Acknowledge)
/*13*/ u8 AckVertexRAMOverflow:1;    // Polygon/Vertex RAM Overflow    (0=None, 1=Overflow/Acknowledge)
/*14*/ u8 RearPlaneMode:1;       // 0=Blank, 1=Bitmap
/*15*/ u8 :1;
/*16*/ u16 :16;
};

typedef union
{
    struct _DISP3DCNT bits;
    u32 val;
} DISP3DCNT;

/*******************************************************************************
    this structure is for capture control (core A only)

    source:
    http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
*******************************************************************************/
struct DISPCAPCNT
{
	enum CAPX {
		_128, _256
	} capx;
    u32 val;
	BOOL enabled;
	u8 EVA;
	u8 EVB;
	u8 writeBlock;
	u8 writeOffset;
	u16 capy;
	u8 srcA;
	u8 srcB;
	u8 readBlock;
	u8 readOffset;
	u8 capSrc;
} ;

/*******************************************************************************
    this structure holds everything and should be mapped to
    * core A : 0x04000000
    * core B : 0x04001000
*******************************************************************************/

typedef struct _reg_dispx {
    DISPCNT dispx_DISPCNT;            // 0x0400x000
    u16 dispA_DISPSTAT;               // 0x04000004
    u16 dispx_VCOUNT;                 // 0x0400x006
    BGxCNT dispx_BGxCNT[4];           // 0x0400x008
    BGxOFS dispx_BGxOFS[4];           // 0x0400x010
    BGxPARMS dispx_BG2PARMS;          // 0x0400x020
    BGxPARMS dispx_BG3PARMS;          // 0x0400x030
    u8			filler[12];           // 0x0400x040
    MISCCNT dispx_MISC;               // 0x0400x04C
    DISP3DCNT dispA_DISP3DCNT;        // 0x04000060
    DISPCAPCNT dispA_DISPCAPCNT;      // 0x04000064
    u32 dispA_DISPMMEMFIFO;           // 0x04000068
} REG_DISPx ;


typedef BOOL (*fun_gl_Begin) (int screen);
typedef void (*fun_gl_End) (int screen);
// the GUI should use this function prior to all gl calls
// if call to beg succeeds opengl draw
void register_gl_fun(fun_gl_Begin beg,fun_gl_End end);

#define GPU_MAIN	0
#define GPU_SUB		1

/* human readable bitmask names */
#define ADDRESS_STEP_512B	   0x00200
#define ADDRESS_STEP_1KB		0x00400
#define ADDRESS_STEP_2KB		0x00800
#define ADDRESS_STEP_4KB		0x01000
#define ADDRESS_STEP_8KB		0x02000
#define ADDRESS_STEP_16KB	   0x04000
#define ADDRESS_STEP_32KB	   0x08000
#define ADDRESS_STEP_64KB	   0x10000
#define ADDRESS_STEP_128KB	   0x20000
#define ADDRESS_STEP_256KB	   0x40000
#define ADDRESS_STEP_512KB	   0x80000
#define ADDRESS_MASK_256KB	   (ADDRESS_STEP_256KB-1)

#ifdef WORDS_BIGENDIAN
struct _TILEENTRY
{
/*14*/	unsigned Palette:4;
/*13*/	unsigned VFlip:1;	// VERTICAL FLIP (top<-->bottom)
/*12*/	unsigned HFlip:1;	// HORIZONTAL FLIP (left<-->right)
/* 0*/	unsigned TileNum:10;
};
#else
struct _TILEENTRY
{
/* 0*/	unsigned TileNum:10;
/*12*/	unsigned HFlip:1;	// HORIZONTAL FLIP (left<-->right)
/*13*/	unsigned VFlip:1;	// VERTICAL FLIP (top<-->bottom)
/*14*/	unsigned Palette:4;
};
#endif
typedef union
{
	struct _TILEENTRY bits;
	u16 val;
} TILEENTRY;

struct _ROTOCOORD
{
	u32 Fraction:8;
	s32 Integer:20;
	u32 pad:4;
};
typedef union
{
	struct _ROTOCOORD bits;
	s32 val;
} ROTOCOORD;


/*
	this structure is for color representation,
	it holds 5 meaningful bits per color channel (red,green,blue)
	and	  1 meaningful bit for alpha representation
	this bit can be unused or used for special FX
*/

struct _COLOR { // abgr x555
#ifdef WORDS_BIGENDIAN
	unsigned alpha:1;    // sometimes it is unused (pad)
	unsigned blue:5;
	unsigned green:5;
	unsigned red:5;
#else
     unsigned red:5;
     unsigned green:5;
     unsigned blue:5;
     unsigned alpha:1;    // sometimes it is unused (pad)
#endif
};
struct _COLORx { // abgr x555
	unsigned bgr:15;
	unsigned alpha:1;	// sometimes it is unused (pad)
};

typedef union
{
	struct _COLOR bits;
	struct _COLORx bitx;
	u16 val;
} COLOR;

struct _COLOR32 { // ARGB
	unsigned :3;
	unsigned blue:5;
	unsigned :3;
	unsigned green:5;
	unsigned :3;
	unsigned red:5;
	unsigned :7;
	unsigned alpha:1;	// sometimes it is unused (pad)
};

typedef union
{
	struct _COLOR32 bits;
	u32 val;
} COLOR32;

#define COLOR_16_32(w,i)	\
	/* doesnt matter who's 16bit who's 32bit */ \
	i.bits.red   = w.bits.red; \
	i.bits.green = w.bits.green; \
	i.bits.blue  = w.bits.blue; \
	i.bits.alpha = w.bits.alpha;



 // (00: Normal, 01: Transparent, 10: Object window, 11: Bitmap)
enum GPU_OBJ_MODE
{
	GPU_OBJ_MODE_Normal = 0,
	GPU_OBJ_MODE_Transparent = 1,
	GPU_OBJ_MODE_Window = 2,
	GPU_OBJ_MODE_Bitmap = 3
};

struct _OAM_
{
	//attr0
	u8 Y;
	u8 RotScale;
	u8 Mode;
	u8 Mosaic;
	u8 Depth;
	u8 Shape;
	//att1
	s16 X;
	u8 RotScalIndex;
	u8 HFlip, VFlip;
	u8 Size;
	//attr2
	u16 TileIndex;
	u8 Priority;
	u8 PaletteIndex;
	//attr3
	u16 attr3;
};

void SlurpOAM(_OAM_* oam_output, void* oam_buffer, int oam_index);
u16 SlurpOAMAffineParam(void* oam_buffer, int oam_index);

typedef struct
{
	 s16 x;
	 s16 y;
} size;


#define NB_PRIORITIES	4
#define NB_BG		4
//this structure holds information for rendering.
typedef struct
{
	u8 PixelsX[256];
	u8 BGs[NB_BG], nbBGs;
	u8 pad[1];
	u16 nbPixelsX;
	//256+8:
	u8 pad2[248];

	//things were slower when i organized this struct this way. whatever.
	//u8 PixelsX[256];
	//int BGs[NB_BG], nbBGs;
	//int nbPixelsX;
	////<-- 256 + 24
	//u8 pad2[256-24];
} itemsForPriority_t;
#define MMU_ABG		0x06000000
#define MMU_BBG		0x06200000
#define MMU_AOBJ	0x06400000
#define MMU_BOBJ	0x06600000
#define MMU_LCDC	0x06800000

extern CACHE_ALIGN u8 gpuBlendTable555[17][17][32][32];

enum BGType {
	BGType_Invalid=0, BGType_Text=1, BGType_Affine=2, BGType_Large8bpp=3, 
	BGType_AffineExt=4, BGType_AffineExt_256x16=5, BGType_AffineExt_256x1=6, BGType_AffineExt_Direct=7
};

extern const BGType GPU_mode2type[8][4];

struct GPU
{
	GPU()
		: debug(false)
	{}

	// some structs are becoming redundant
	// some functions too (no need to recopy some vars as it is done by MMU)
	REG_DISPx * dispx_st;

	//this indicates whether this gpu is handling debug tools
	bool debug;

	_BGxCNT & bgcnt(int num) { return (dispx_st)->dispx_BGxCNT[num].bits; }
	_DISPCNT & dispCnt() { return dispx_st->dispx_DISPCNT.bits; }
	template<bool MOSAIC> void modeRender(int layer);

	DISPCAPCNT dispCapCnt;
	BOOL LayersEnable[5];
	itemsForPriority_t itemsForPriority[NB_PRIORITIES];

#define BGBmpBB BG_bmp_ram
#define BGChBB BG_tile_ram

	u32 BG_bmp_large_ram[4];
	u32 BG_bmp_ram[4];
	u32 BG_tile_ram[4];
	u32 BG_map_ram[4];

	u8 BGExtPalSlot[4];
	u32 BGSize[4][2];
	BGType BGTypes[4];

	struct MosaicColor {
		u16 bg[4][256];
		struct Obj {
			u16 color;
			u8 alpha, opaque;
		} obj[256];
	} mosaicColors;

	u8 sprNum[256];
	u8 h_win[2][256];
	const u8 *curr_win[2];
	void update_winh(int WIN_NUM); 
	bool need_update_winh[2];
	
	template<int WIN_NUM> void setup_windows();

	u8 core;

	u8 dispMode;
	u8 vramBlock;
	u8 *VRAMaddr;

	//FIFO	fifo;

	u8 bgPrio[5];

	BOOL bg0HasHighestPrio;

	void * oam;
	u32	sprMem;
	u8 sprBoundary;
	u8 sprBMPBoundary;
	u8 sprBMPMode;
	u32 sprEnable;

	u8 WIN0H0;
	u8 WIN0H1;
	u8 WIN0V0;
	u8 WIN0V1;

	u8 WIN1H0;
	u8 WIN1H1;
	u8 WIN1V0;
	u8 WIN1V1;

	u8 WININ0;
	bool WININ0_SPECIAL;
	u8 WININ1;
	bool WININ1_SPECIAL;

	u8 WINOUT;
	bool WINOUT_SPECIAL;
	u8 WINOBJ;
	bool WINOBJ_SPECIAL;

	u8 WIN0_ENABLED;
	u8 WIN1_ENABLED;
	u8 WINOBJ_ENABLED;

	u16 BLDCNT;
	u8	BLDALPHA_EVA;
	u8	BLDALPHA_EVB;
	u8	BLDY_EVY;
	u16 *currentFadeInColors, *currentFadeOutColors;
	bool blend2[8];

	CACHE_ALIGN u16 tempScanlineBuffer[256];
	u8 *tempScanline;

	u8	MasterBrightMode;
	u32 MasterBrightFactor;

	CACHE_ALIGN u8 bgPixels[1024]; //yes indeed, this is oversized. map debug tools try to write to it

	u32 currLine;
	u8 currBgNum;
	bool blend1;
	u8* currDst;

	u8* _3dColorLine;


	static struct MosaicLookup {

		struct TableEntry {
			u8 begin, trunc;
		} table[16][256];

		MosaicLookup() {
			for(int m=0;m<16;m++)
				for(int i=0;i<256;i++) {
					int mosaic = m+1;
					TableEntry &te = table[m][i];
					te.begin = (i%mosaic==0);
					te.trunc = i/mosaic*mosaic;
				}
		}

		TableEntry *width, *height;
		int widthValue, heightValue;
		
	} mosaicLookup;
	bool curr_mosaic_enabled;

	u16 blend(u16 colA, u16 colB);

	template<bool BACKDROP, BlendFunc FUNC, bool WINDOW>
	FORCEINLINE FASTCALL bool _master_setFinalBGColor(u16 &color, const u32 x);

	template<BlendFunc FUNC, bool WINDOW>
	FORCEINLINE FASTCALL void _master_setFinal3dColor(int dstX, int srcX);

	int setFinalColorBck_funcNum;
	int bgFunc;
	int setFinalColor3d_funcNum;
	int setFinalColorSpr_funcNum;
	//Final3DColFunct setFinalColor3D;
	enum SpriteRenderMode {
		SPRITE_1D, SPRITE_2D
	} spriteRenderMode;

	template<GPU::SpriteRenderMode MODE>
	void _spriteRender(u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab);
	
	inline void spriteRender(u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
	{
		if(spriteRenderMode == SPRITE_1D)
			_spriteRender<SPRITE_1D>(dst,dst_alpha,typeTab, prioTab);
		else
			_spriteRender<SPRITE_2D>(dst,dst_alpha,typeTab, prioTab);
	}


	void setFinalColor3d(int dstX, int srcX);
	
	template<bool BACKDROP, int FUNCNUM> void setFinalColorBG(u16 color, const u32 x);
	template<bool MOSAIC, bool BACKDROP> FORCEINLINE void __setFinalColorBck(u16 color, const u32 x, const int opaque);
	template<bool MOSAIC, bool BACKDROP, int FUNCNUM> FORCEINLINE void ___setFinalColorBck(u16 color, const u32 x, const int opaque);

	void setAffineStart(int layer, int xy, u32 val);
	void setAffineStartWord(int layer, int xy, u16 val, int word);
	u32 getAffineStart(int layer, int xy);
	void refreshAffineStartRegs(const int num, const int xy);

	struct AffineInfo {
		AffineInfo() : x(0), y(0) {}
		u32 x, y;
	} affineInfo[2];

	void renderline_checkWindows(u16 x, bool &draw, bool &effect) const;

	// check whether (x,y) is within the rectangle (including wraparounds) 
	template<int WIN_NUM>
	u8 withinRect(u16 x) const;

	void setBLDALPHA(u16 val)
	{
		BLDALPHA_EVA = (val&0x1f) > 16 ? 16 : (val&0x1f); 
		BLDALPHA_EVB = ((val>>8)&0x1f) > 16 ? 16 : ((val>>8)&0x1f);
		updateBLDALPHA();
	}

	void setBLDALPHA_EVA(u8 val)
	{
		BLDALPHA_EVA = (val&0x1f) > 16 ? 16 : (val&0x1f);
		updateBLDALPHA();
	}
	
	void setBLDALPHA_EVB(u8 val)
	{
		BLDALPHA_EVB = (val&0x1f) > 16 ? 16 : (val&0x1f);
		updateBLDALPHA();
	}

	u32 getHOFS(int bg) { return T1ReadWord(&dispx_st->dispx_BGxOFS[bg].BGxHOFS,0) & 0x1FF; }
	u32 getVOFS(int bg) { return T1ReadWord(&dispx_st->dispx_BGxOFS[bg].BGxVOFS,0) & 0x1FF; }

	typedef u8 TBlendTable[32][32];
	TBlendTable *blendTable;

	void updateBLDALPHA()
	{
		blendTable = (TBlendTable*)&gpuBlendTable555[BLDALPHA_EVA][BLDALPHA_EVB][0][0];
	}
	
};
#if 0
// normally should have same addresses
static void REG_DISPx_pack_test(GPU * gpu)
{
	REG_DISPx * r = gpu->dispx_st;
	printf ("%08x %02x\n",  (u32)r, (u32)(&r->dispx_DISPCNT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispA_DISPSTAT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_VCOUNT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_BGxCNT[0]) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_BGxOFS[0]) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_BG2PARMS) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_BG3PARMS) - (u32)r);
//	printf ("\t%02x\n", (u32)(&r->dispx_WINCNT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispx_MISC) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispA_DISP3DCNT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispA_DISPCAPCNT) - (u32)r);
	printf ("\t%02x\n", (u32)(&r->dispA_DISPMMEMFIFO) - (u32)r);
}
#endif

CACHE_ALIGN extern u8 GPU_screen[4*256*192];


GPU * GPU_Init(u8 l);
void GPU_Reset(GPU *g, u8 l);
void GPU_DeInit(GPU *);

//these are functions used by debug tools which want to render layers etc outside the context of the emulation
namespace GPU_EXT
{
	void textBG(GPU * gpu, u8 num, u8 * DST);		//Draw text based background
	void rotBG(GPU * gpu, u8 num, u8 * DST);
	void extRotBG(GPU * gpu, u8 num, u8 * DST);
};
void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab);
void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab);

extern const size sprSizeTab[4][4];

typedef struct {
	GPU * gpu;
	u16 offset;
} NDS_Screen;

extern NDS_Screen MainScreen;
extern NDS_Screen SubScreen;

int Screen_Init();
void Screen_Reset(void);
void Screen_DeInit(void);

extern MMU_struct MMU;

void GPU_setVideoProp(GPU *, u32 p);
void GPU_setBGProp(GPU *, u16 num, u16 p);

void GPU_setBLDCNT(GPU *gpu, u16 v) ;
void GPU_setBLDY(GPU *gpu, u16 v) ;
void GPU_setMOSAIC(GPU *gpu, u16 v) ;


void GPU_remove(GPU *, u8 num);
void GPU_addBack(GPU *, u8 num);

int GPU_ChangeGraphicsCore(int coreid);

void GPU_set_DISPCAPCNT(u32 val) ;
void GPU_RenderLine(NDS_Screen * screen, u16 l, bool skip = false) ;
void GPU_setMasterBrightness (GPU *gpu, u16 val);

inline void GPU_setWIN0_H(GPU* gpu, u16 val) { gpu->WIN0H0 = val >> 8; gpu->WIN0H1 = val&0xFF; gpu->need_update_winh[0] = true; }
inline void GPU_setWIN0_H0(GPU* gpu, u8 val) { gpu->WIN0H0 = val;  gpu->need_update_winh[0] = true; }
inline void GPU_setWIN0_H1(GPU* gpu, u8 val) { gpu->WIN0H1 = val;  gpu->need_update_winh[0] = true; }

inline void GPU_setWIN0_V(GPU* gpu, u16 val) { gpu->WIN0V0 = val >> 8; gpu->WIN0V1 = val&0xFF;}
inline void GPU_setWIN0_V0(GPU* gpu, u8 val) { gpu->WIN0V0 = val; }
inline void GPU_setWIN0_V1(GPU* gpu, u8 val) { gpu->WIN0V1 = val; }

inline void GPU_setWIN1_H(GPU* gpu, u16 val) {gpu->WIN1H0 = val >> 8; gpu->WIN1H1 = val&0xFF;  gpu->need_update_winh[1] = true; }
inline void GPU_setWIN1_H0(GPU* gpu, u8 val) { gpu->WIN1H0 = val;  gpu->need_update_winh[1] = true; }
inline void GPU_setWIN1_H1(GPU* gpu, u8 val) { gpu->WIN1H1 = val;  gpu->need_update_winh[1] = true; }

inline void GPU_setWIN1_V(GPU* gpu, u16 val) { gpu->WIN1V0 = val >> 8; gpu->WIN1V1 = val&0xFF; }
inline void GPU_setWIN1_V0(GPU* gpu, u8 val) { gpu->WIN1V0 = val; }
inline void GPU_setWIN1_V1(GPU* gpu, u8 val) { gpu->WIN1V1 = val; }

inline void GPU_setWININ(GPU* gpu, u16 val) {
	gpu->WININ0=val&0x1F;
	gpu->WININ0_SPECIAL=((val>>5)&1)!=0;
	gpu->WININ1=(val>>8)&0x1F;
	gpu->WININ1_SPECIAL=((val>>13)&1)!=0;
}

inline void GPU_setWININ0(GPU* gpu, u8 val) { gpu->WININ0 = val&0x1F; gpu->WININ0_SPECIAL = (val>>5)&1; }
inline void GPU_setWININ1(GPU* gpu, u8 val) { gpu->WININ1 = val&0x1F; gpu->WININ1_SPECIAL = (val>>5)&1; }

inline void GPU_setWINOUT16(GPU* gpu, u16 val) {
	gpu->WINOUT=val&0x1F;
	gpu->WINOUT_SPECIAL=((val>>5)&1)!=0;
	gpu->WINOBJ=(val>>8)&0x1F;
	gpu->WINOBJ_SPECIAL=((val>>13)&1)!=0;
}
inline void GPU_setWINOUT(GPU* gpu, u8 val) { gpu->WINOUT = val&0x1F; gpu->WINOUT_SPECIAL = (val>>5)&1; }
inline void GPU_setWINOBJ(GPU* gpu, u8 val) { gpu->WINOBJ = val&0x1F; gpu->WINOBJ_SPECIAL = (val>>5)&1; }

// Blending
void SetupFinalPixelBlitter (GPU *gpu);
#define GPU_setBLDCNT_LOW(gpu, val) {gpu->BLDCNT = (gpu->BLDCNT&0xFF00) | (val); SetupFinalPixelBlitter (gpu);}
#define GPU_setBLDCNT_HIGH(gpu, val) {gpu->BLDCNT = (gpu->BLDCNT&0xFF) | (val<<8); SetupFinalPixelBlitter (gpu);}
#define GPU_setBLDCNT(gpu, val) {gpu->BLDCNT = (val); SetupFinalPixelBlitter (gpu);}



#define GPU_setBLDY_EVY(gpu, val) {gpu->BLDY_EVY = ((val)&0x1f) > 16 ? 16 : ((val)&0x1f);}

//these arent needed right now since the values get poked into memory via default mmu handling and dispx_st
//#define GPU_setBGxHOFS(bg, gpu, val) gpu->dispx_st->dispx_BGxOFS[bg].BGxHOFS = ((val) & 0x1FF)
//#define GPU_setBGxVOFS(bg, gpu, val) gpu->dispx_st->dispx_BGxOFS[bg].BGxVOFS = ((val) & 0x1FF)

void gpu_SetRotateScreen(u16 angle);

//#undef FORCEINLINE
//#define FORCEINLINE __forceinline


//---CUSTOM--->
//#include "X432R_BuildSwitch.h"
#include <map>
#include <utility>

#ifdef X432R_CUSTOMRENDERER_ENABLED
namespace X432R
{
	static const u8 BLENDALPHA_MAX2 = 16;
	static const u8 BLENDALPHA_MAX = 15;
	
   void ClearBuffers();
   u32 GetCurrentRenderMagnification();
	
	union RGBA8888
	{
		private:
		
		static u8 GetBlendColor(const u8 color1, const u8 color2, const u8 alpha1);
		static u8 GetBlendColor(const u8 color1, const u8 color2, const u8 alpha1, const u8 alpha2);
		
		
		public:
		
		u32 Color;
		
		#ifdef WORDS_BIGENDIAN
		// todo
		#else
		struct
		{	u8 B, G, R, A;		};
		#endif
		
		
		RGBA8888()
		{	Color = 0;			}
		
		RGBA8888(const u32 color)
		{	Color = color;		}
		
		
		static inline u16 ToRGB555(RGBA8888 color)
		{
			if(color.A == 0) return 0;
				
			#ifdef WORDS_BIGENDIAN
			// todo
			#else
			return ( ( (u16)(color.B >> 3) << 10 ) | ( (u16)(color.G >> 3) << 5 ) | (u16)(color.R >> 3) );
			#endif
		}
		
		
		static void InitBlendColorTable();
		
		
		inline void AlphaBlend(const RGBA8888 foreground_color)
		{
			const u8 foreground_alpha = foreground_color.A >> 4;
			
			switch(foreground_alpha)
			{
				case 0:
					return;
				
				case BLENDALPHA_MAX:
					Color = foreground_color.Color;
					return;
				
				default:
//					if( (A >> 4) == 0 )
					if(A == 0)
					{
						Color = foreground_color.Color;
						return;
					}
					
					R = GetBlendColor(foreground_color.R, R, foreground_alpha);
					G = GetBlendColor(foreground_color.G, G, foreground_alpha);
					B = GetBlendColor(foreground_color.B, B, foreground_alpha);
					A = 0xFF;
					
					return;
			}
		}
		
		static inline RGBA8888 AlphaBlend(const RGBA8888 foreground_color, const RGBA8888 background_color)
		{
			RGBA8888 result = background_color;
			
			result.AlphaBlend(foreground_color);
			
			return result;
		}
		
		static inline RGBA8888 AlphaBlend(const RGBA8888 color1, const RGBA8888 color2, const u8 alpha1)
		{
			if(color2.A == 0) return color1;
			if(color1.A == 0) return color2;
			
			RGBA8888 result;
			
			result.R = GetBlendColor(color1.R, color2.R, alpha1);
			result.G = GetBlendColor(color1.G, color2.G, alpha1);
			result.B = GetBlendColor(color1.B, color2.B, alpha1);
			result.A = 0xFF;
			
			return result;
		}
		
		static inline RGBA8888 AlphaBlend(const RGBA8888 color1, const RGBA8888 color2, const u8 alpha1, const u8 alpha2)
		{
			if(color2.A == 0) return color1;
			if(color1.A == 0) return color2;
			
			RGBA8888 result;
			
			result.R = GetBlendColor(color1.R, color2.R, alpha1, alpha2);
			result.G = GetBlendColor(color1.G, color2.G, alpha1, alpha2);
			result.B = GetBlendColor(color1.B, color2.B, alpha1, alpha2);
			result.A = 0xFF;
		
			return result;
		}
	};
	
	
	enum SOURCELAYER
	{
		SOURCELAYER_BG, SOURCELAYER_3D, SOURCELAYER_OBJ
	};
	
	enum BLENDMODE
	{
		BLENDMODE_DISABLED, BLENDMODE_ENABLED, BLENDMODE_BRIGHTUP, BLENDMODE_BRIGHTDOWN
	};
	
	class HighResolutionFramebuffers
	{
		public:
		
//		HighResolutionFramebuffers();
		void Clear();
		
		
		inline bool IsCurrentFrameRendered()
		{	return currentFrameRendered;						}
		
		inline bool IsHighResolutionRedneringSkipped()
		{	return renderLine_SkipHighResolutionRednering;		}
		
		inline u32 * GetHighResolution3DBuffer()
		{	return highResolutionBuffer_3D;						}
		
		#ifdef X432R_DISPCAPTURE_MAINMEMORYFIFO_TEST
		inline u32 * GetCurrentMainMemoryFIFOBuffer()
		{	return renderLine_MainMemoryFIFOBuffer;				}
		#endif
		
		
		void UpdateGpuParams();
		
		template <u32 RENDER_MAGNIFICATION>
		void UpdateFrontBufferAndDisplayCapture(u32 * const front_buffer, u32 * const master_brightness, bool * const is_highreso_screen);
		
		
		void InitRenderLineParams(const NDS_Screen * const screen, u32 y, const u16 backdrop_rgb555);
		
		template <SOURCELAYER LAYER, BLENDMODE MODE>
		void SetFinalColor(const u32 dest_x, const u32 source_x, const u16 color_rgb555, const u8 alpha, const u8 layer_num);
		
		
		//--------------------
		
		private:
		
		u32 highResolutionBuffer_3D[1024 * 768];
		u32 highResolutionBuffer_Vram[4][1024 * 768];
		
		u32 highResolutionBuffer_Final[1024 * 768 * 2];
		#ifndef X432R_BACKGROUNDBUFFER_DISABLED
		u32 backgroundBuffer[256 * 192 * 2];
		#else
		u32 backdropColor[2];
		#endif
		#ifdef X432R_FOREGROUNDBUFFER_TEST
		u32 foregroundBuffer[256 * 192 * 2];
		#endif
		
		#ifdef X432R_DISPCAPTURE_MAINMEMORYFIFO_TEST
		u32 MainMemoryFIFOBuffer[256 * 192];
		#endif
		
		u8 mainScreenIndex;
		u8 subScreenIndex;
		u8 mainGpuDisplayMode;
		u8 subGpuDisplayMode;
		
		u8 displayCaptureWriteBlockIndex;
		u8 displayCaptureReadBlockIndex;
		u8 displayCaptureSourceA;
		u8 displayCaptureSourceB;
		u8 displayCaptureBlendingRatioA;
		u8 displayCaptureBlendingRatioB;
		
		u8 vramBlockMainScreen;
		
		u8 vramBlockBG[2];
		u8 vramBlockOBJ[2];
		u8 highResolutionBGNum[2];
		
		u32 masterBrightness[2];
		
//		u32 highResolutionBG03DOffset;
		BGxPARMS highResolutionBGOffset[2];
		BGxPARMS highResolutionOBJOffset[2];
		bool highResolutionOBJFlipX[2];
		bool highResolutionOBJFlipY[2];
		
		VramConfiguration::Purpose vramPurpose[4];
		bool vramIsValid[4];
		bool skipHighResolutionRendering[2];
		bool mainGpuBG03DEnabled;
		bool currentFrameRendered;
		
		#ifdef X432R_SAMPLEDVRAMDATACHECK_TEST
		u16 vramSampledPixelData[4][9];
		#endif
		
		#ifdef X432R_3D_REARPLANE_TEST
		bool rearPlane3DEnabled;
		bool vramBankControl_VramEnabled[4];
		u8 vramBankControl_VramMST[4];
		u8 vramBankControl_VramOffset[4];
		#endif
		
		
		void ClearVramBuffer(const u8 vram_block);
		
		void UpdateMasterBrightness();
		
		bool IsVramValid(const u8 vram_block);
		#ifdef X432R_SAMPLEDVRAMDATACHECK_TEST
		void GetSampledVramPixelData(u16 * const sampled_data, const u8 vram_block);		// temp
		#endif
		
		bool CheckOBJParams(const GPU * const gpu, const _DISPCNT * const display_control, const u32 screen_index);
		bool CheckBGParams(const GPU * const gpu, const u32 layer_num);
		bool UpdateHighResolutionBGNum(const GPU * const gpu, const _DISPCNT * const display_control, const u32 screen_index);
		
		#if !defined(X432R_LAYERPOSITIONOFFSET_TEST2) || !defined(X432R_HIGHRESO_BG_OBJ_ROTSCALE_TEST)
		bool CheckBGOffset(const u32 screen_index);
		bool CheckOBJOffset(const u32 screen_index);
		
		#endif
		
		void UpdateDisplayCaptureParams(DISPCAPCNT params);
		
		template <u32 RENDER_MAGNIFICATION, bool HIGHRESO, u8 CAPTURESOURCE_A, u8 CAPTURESOURCE_B, u8 GPU_DISPLAYMODE>
		void UpdateFrontBufferAndDisplayCapture(u32 * const front_buffer, const u32 screen_index);
		
		template <u32 RENDER_MAGNIFICATION>
		void UpdateFrontBufferAndDisplayCapture(u32 *front_buffer, const u32 screen_index);
		
		
		//--------------------
		// RenderLine
		
		bool renderLine_SkipHighResolutionRednering;
		
		u32 renderLine_CurrentRenderMagnification;
		u32 renderLine_CurrentRenderWidth;
		u32 renderLine_CurrentRenderHeight;
		u32 renderLine_CurrentScreenIndex;
		u32 renderLine_CurrentY;
		u32 renderLine_CurrentHighResolutionYBegin;
		u32 renderLine_CurrentHighResolutionYEnd;
		
		u8 renderLine_CurrentBrightFactor;
		u8 renderLine_CurrentBlendAlphaA;
		u8 renderLine_CurrentBlendAlphaB;
		
		
		u32 *renderLine_HighResolutionFinalBuffer;
		#ifndef X432R_BACKGROUNDBUFFER_DISABLED
		u32 *renderLine_BackgroundBuffer;
		#else
		u32 renderLine_CurrentBackdropColor;
		#endif
		u32 *renderLine_VramBufferBG;
		u32 *renderLine_VramBufferOBJ;
		
		u8 renderLine_HighResolutionBGNum;
		
		#ifdef X432R_DISPCAPTURE_MAINMEMORYFIFO_TEST
		u32 *renderLine_MainMemoryFIFOBuffer;
		#endif
		
		bool renderLine_ExistsHighResolutionPixelData[256];
		
		#ifdef X432R_FOREGROUNDBUFFER_TEST
		u32 *renderLine_ForegroundBuffer;
		#endif
		
		#if defined(X432R_LAYERPOSITIONOFFSET_TEST2) || defined(X432R_HIGHRESO_BG_OBJ_ROTSCALE_TEST)
		BGxPARMS *renderLine_CurrentHighResolutionBGOffset;
		BGxPARMS *renderLine_CurrentHighResolutionOBJOffset;
		bool renderLine_CurrentHighResolutionOBJFlipX;
		bool renderLine_CurrentHighResolutionOBJFlipY;
		#endif
		
		
		template <SOURCELAYER LAYER, BLENDMODE MODE, bool HIGHRESO, bool LAYER_OFFSET, bool USE_ALPHA>
		void SetFinalColor(const u32 dest_x, const u32 source_x, const u16 color_rgb555, u8 alpha, const u32 * const source_buffer);
		
		#ifdef X432R_FOREGROUNDBUFFER_TEST
		template <SOURCELAYER LAYER, BLENDMODE MODE>
		void SetForegroundColor(const u32 x, const u16 color_rgb555, const u8 alpha);
		#endif
		
		#ifndef X432R_BACKGROUNDBUFFER_DISABLED
		template <SOURCELAYER LAYER, BLENDMODE MODE>
		void SetBackgroundColor(const u32 x, const u16 color_rgb555, const u8 alpha);
		#endif
      bool IsVramInvalid(const u8 vram_block);
	};
	
	extern HighResolutionFramebuffers backBuffer;
}
#endif
//<---CUSTOM---


#endif
