/*
 * $Workfile: nsc_galproto.h $
 * $Revision: 1.1 $
 *
 * File Contents: This file contains the main functions of the Geode 
 *                frame buffer device drivers GAL function prototypes and  
 *                data structures.
 *
 * Project:       Geode Frame buffer device driver
 *
 */

/* 
 * NSC_LIC_COPYRIGHT
 *
 * Copyright (c) 2001-2003 National Semiconductor Corporation ("NSC").
 *
 * All Rights Reserved.  Unpublished rights reserved under the copyright 
 * laws of the United States of America, other countries and international 
 * treaties.  The software is provided without fee.  Permission to use, 
 * copy, store, modify, disclose, transmit or distribute the software is 
 * granted, provided that this copyright notice must appear in any copy, 
 * modification, disclosure, transmission or distribution of the software.
 *  
 * NSC retains all ownership, copyright, trade secret and proprietary rights 
 * in the software. 
 * THIS SOFTWARE HAS BEEN PROVIDED "AS IS," WITHOUT EXPRESS OR IMPLIED 
 * WARRANTY INCLUDING, WITHOUT LIMITATION, IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR USE AND NON-INFRINGEMENT.
 *
 * NSC does not assume or authorize any other person to assume for it any 
 * liability in connection with the Software. NSC SHALL NOT BE LIABLE TO 
 * COMPANY, OR ANY THIRD PARTY, IN CONTRACT, TORT, WARRANTY, STRICT 
 * LIABILITY, OR OTHERWISE FOR ANY DIRECT DAMAGES, OR FOR ANY SPECIAL, 
 * INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING BUT NOT 
 * LIMITED TO, BUSINESS INTERRUPTION, LOST PROFITS OR GOODWILL, OR LOSS 
 * OF INFORMATION EVEN IF NSC IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * END_NSC_LIC_COPYRIGHT */

#ifndef __GALPROTO_SEP_20_2000
#define __GALPROTO_SEP_20_2000

/* durango reg definitions and type's */
#include <gfx_type.h>
#include <gfx_regs.h>

/* Panel related definition */
#include <pnl_defs.h>

typedef int SWORD;
typedef unsigned int DWORD;
typedef unsigned short WORD;
typedef unsigned char CHAR;
typedef unsigned char BOOLEAN;
typedef unsigned int *PDWORD;

/***************************************/
/* Applications/User mode drivers use this ioctl to
 * send a graphics device request to the frame buffer
 * driver
 */
#define FBIOGAL_API      0x4700

/*
 * Applications must sign the I/O packet with this value
 */

#define FBGAL_SIGNATURE 0xC0C0BABE

/*
 * Version is a 16:16 fixed value
 * Current version is 1.0000
 */

#define FBGAL_VERSION 0x10000

/*
 * Definitions for Graphics Subfunctions
 *
 */

typedef enum GALFN_CODES
{
/* General Adapter level functions */
   GALFN_GETADAPTERINFO = 0,
   GALFN_SETSOFTVGASTATE,
   GALFN_GETSOFTVGASTATE,
   GALFN_WAITUNTILIDLE,
   GALFN_WAITVERTICALBLANK,
   GALFN_SETCRTENABLE,
   GALFN_WRITEREG,
   GALFN_READREG,

/* Change/Get Display hardware state */

   GALFN_ISDISPLAYMODESUPPORTED,
   GALFN_SETDISPLAYMODE,
   GALFN_GETDISPLAYMODE,
   GALFN_SETBPP,
   GALFN_SETDISPLAYBPP,
   GALFN_GETDISPLAYBPP,
   GALFN_SETDISPLAYPITCH,
   GALFN_GETDISPLAYPITCH,
   GALFN_SETDISPLAYOFFSET,
   GALFN_GETDISPLAYOFFSET,
   GALFN_DOTCLKTOREFRESH,
   GALFN_GETDISPLAYTIMINGS,
   GALFN_SETDISPLAYTIMINGS,
   GALFN_SETPALETTE,
   GALFN_GETPALETTE,
   GALFN_SETPALETTE_ENTRY,
   GALFN_GETPALETTE_ENTRY,
   GALFN_SETFIXEDTIMINGS,

/* Hardware cursor funtions */

   GALFN_SETCURSORENABLE,
   GALFN_GETCURSORENABLE,
   GALFN_SETCURSORPOSITION,
   GALFN_GETCURSORPOSITION,
   GALFN_SETCURSORCOLORS,
   GALFN_GETCURSORCOLORS,
   GALFN_SETCURSORSHAPE,
   GALFN_SETCURSORSHAPE_RCLD,

/* grafix rendering funtions */
   GALFN_SETSOLIDPATTERN,
   GALFN_SETRASTEROPERATION,
   GALFN_SETSOLIDSOURCE,
   GALFN_PATTERNFILL,
   GALFN_SETMONOSOURCE,
   GALFN_SETMONOPATTERN,
   GALFN_SCREENTOSCREENBLT,
   GALFN_SCREENTOSCREENXBLT,
   GALFN_BRESENHAMLINE,
   GALFN_COLOR_PATTERNFILL,
   GALFN_COLOR_BITMAP_TO_SCREEN_BLT,
   GALFN_COLOR_BITMAP_TO_SCREEN_XBLT,
   GALFN_MONO_BITMAP_TO_SCREEN_BLT,
   GALFN_TEXT_BLT,

/* VGA Support functions */

   GALFN_VGAMODESWITCH,
   GALFN_VGACLEARCRTEXT,
   GALFN_VGASETPITCH,
   GALFN_VGARESTORE,
   GALFN_VGASAVE,
   GALFN_VGASETMODE,

/* Compression functions */
   GALFN_SETCOMPRESSIONSTATE,
   GALFN_GETCOMPRESSIONSTATE,
   GALFN_SETCOMPRESSIONPARAMS,
   GALFN_GETCOMPRESSIONPARAMS,

/* Panel Support functions */

   GALFN_PNLSETPARAMS,
   GALFN_PNLGETPARAMS,
   GALFN_PNLINITPANEL,
   GALFN_PNLSAVESTATE,
   GALFN_PNLRESTORESTATE,
   GALFN_PNLPOWERUP,
   GALFN_PNLPOWERDOWN,
   GALFN_PNLBIOSENABLE,
   GALFN_PNLBIOSINFO,
   GALFN_ENABLEPANNING,

/* TV Support functions */

   GALFN_SETTVPARAMS,
   GALFN_GETTVPARAMS,
   GALFN_SETTVTIMING,
   GALFN_GETTVTIMING,
   GALFN_SETENABLE,
   GALFN_GETENABLE,
   GALFN_ISTVMODESUPPORTED,

/* Video Support functions */

   GALFN_SETVIDEOENABLE,
   GALFN_SETVIDEOFORMAT,
   GALFN_SETVIDEOSIZE,
   GALFN_SETVIDEOOFFSET,
   GALFN_SETVIDEOWINDOW,
   GALFN_SETVIDEOSCALE,
   GALFN_SETVIDEOFILTER,
   GALFN_SETVIDEOCOLORKEY,
   GALFN_SETVIDEODOWNSCALEENABLE,
   GALFN_SETVIDEODOWNSCALECONFIG,
   GALFN_SETVIDEODOWNSCALECOEFF,
   GALFN_SETVIDEOSOURCE,
   GALFN_SETVIDEOINTERLACED,
   GALFN_SETVIDEOCURSOR,
   GALFN_SETVIDEOREQUEST,
   GALFN_SETALPHAENABLE,
   GALFN_SETALPHAWINDOW,
   GALFN_SETALPHAVALUE,
   GALFN_SETALPHAPRIORITY,
   GALFN_SETALPHACOLOR,
   GALFN_SETALPHAREGION,
   GALFN_SETVIDEOOUTSIDEALPHA,
   GALFN_SETVIDEOPALETTE,
   GALFN_GETVIDEOINFO,
   GALFN_SETVIDEOCOLORSPACE,

/* VIP Supported functions */

   GALFN_SETVIPENABLE,
   GALFN_SETVIPCAPTURERUNMODE,
   GALFN_SETVIPBASE,
   GALFN_SETVIPPITCH,
   GALFN_SETVIPMODE,
   GALFN_SETVIPBRTH,
   GALFN_SETVIPLASTLINE,
   GALFN_TESTVIPODDFIELD,
   GALFN_TESTVIPBASESUPDATED,
   GALFN_SETVBIENABLE,
   GALFN_SETVBIMODE,
   GALFN_SETVBIBASE,
   GALFN_SETVBIPITCH,
   GALFN_SETVBIDIRECT,
   GALFN_SETVBIINTERRUPT,
   GALFN_SETGENLOCKENABLE,
   GALFN_SETTOPLINEINODD,
   GALFN_SETGENLOCKDELAY,
   GALFN_SETMACROVISIONENABLE,

   GALFN_GETVIPENABLE,
   GALFN_GETVIPBASE,
   GALFN_GETVIPPITCH,
   GALFN_GETVIPMODE,
   GALFN_GETVIPBRTH,
   GALFN_GETVIPLINE,
   GALFN_GETVBIENABLE,
   GALFN_GETVBIBASE,
   GALFN_GETVBIPITCH,
   GALFN_GETVBIMODE,
   GALFN_GETVBIDIRECT,
   GALFN_GETVBIINTERRUPT,
   GALFN_TESTVIPFIFOOVERFLOW,

/* Second generation rendering routines */

   GALFN_SETICONENABLE,
   GALFN_SETICONCOLORS,
   GALFN_SETICONPOSITION,
   GALFN_SETICONSHAPE64,

   GALFN_SETSOURCESTRIDE,
   GALFN_SETDESTINATIONSTRIDE,
   GALFN_SETSOURCETRANSPARENCY,
   GALFN_SETPATTERNORIGIN,
   GALFN_GFX2SETALPHAMODE,
   GALFN_GFX2SETALPHAVALUE,
   GALFN_GFX2PATTERNFILL,
   GALFN_GFX2COLORPATTERNFILL,
   GALFN_GFX2SCREENTOSCREENBLT,
   GALFN_GFX2MONOEXPANDBLT,
   GALFN_GFX2COLORBMPTOSCRBLT,
   GALFN_GFX2MONOBMPTOSCRBLT,
   GALFN_GFX2TEXTBLT,
   GALFN_GFX2BRESENHAMLINE,
   GALFN_GFX2SYNCTOVBLANK,

/* Change/Get Video routines */

   GALFN_SETCOLORSPACEYUV,
   GALFN_SETVIDEOYUVPITCH,
   GALFN_SETVIDEOYUVOFFSETS,
   GALFN_SETVIDEOLEFTCROP,
   GALFN_SETVIDEOVERTICALDOWNSCALE,
   GALFN_SETVBISOURCE,
   GALFN_SETVBILINES,
   GALFN_SETVBITOTAL,
   GALFN_SETVSCALEROFFSET,

   GALFN_GETVBISOURCE,
   GALFN_GETVBILINES,
   GALFN_GETVBITOTAL,
   GALFN_GETVSCALEROFFSET,
   GALFN_GETVIDEOINTERLACED,
   GALFN_GETCOLORSPACEYUV,
   GALFN_GETGENLOCKENABLE,
   GALFN_GETGENLOCKDELAY,
   GALFN_GETVIDEOCURSOR,
   GALFN_READCRC,
   GALFN_READWINDOWCRC,
   GALFN_GETMACROVISIONENABLE,
   GALFN_GETALPHAENABLE,
   GALFN_GETALPHASIZE,
   GALFN_GETALPHAVALUE,
   GALFN_GETALPHAPRIORITY,
   GALFN_GETALPHACOLOR,
   GALFN_GETVIDEOYUVPITCH,
   GALFN_GETVIDEOYUVOFFSETS,

/* Additional VGA Support functions */

   GALFN_VGATESTPCI,
   GALFN_VGAGETPCICOMMAND,
   GALFN_VGASEQRESET,
   GALFN_VGASETGRAPHICSBITS,

/* This is last function supported.
 * If you want to define ioctl function. 
 * You should define before this function.
 * Update that the lastfunction supported to new value.
 */
   GALFN_LASTFUNCTION_SUPPORTED
}
GALFN_CODES;

/* end of GAL function list */

#define GAL_HEADER\
        DWORD  dwSignature;    /* Sign all structs with FBGAL_SIGNATURE    */\
        DWORD  dwSize;         /* Size of struct for that subfunction      */\
        DWORD  dwVersion;      /* Current version of the API               */\
        DWORD  dwSubfunction;  /* GAL subfunction                          */\
        DWORD  dwReturnValue;           /* Return value from subfunction */

/*
 * #define GALFN_PNLPOWERUP
 * #define GALFN_PNLPOWERDOWN
 */
typedef struct __GAL_BASE
{
GAL_HEADER}
GAL_BASE, *PGAL_BASE;

/*
 * #define GALFN_GETADAPTERINFO
 */
typedef struct __GAL_GETADAPTERINFO
{
   GAL_HEADER DWORD dwCPUVersion;
   DWORD dwCPUType;
   DWORD dwFrameBufferBase;
   DWORD dwFrameBufferSize;
   DWORD dwGfxRegisterBase;
   DWORD dwGpsRegisterBase;
   DWORD dwVidRegisterBase;
   DWORD dwVipRegisterBase;
   DWORD dwVideoVersion;
   DWORD dwMaxSupportedPixelClock;

}
GAL_ADAPTERINFO, *PGAL_ADAPTERINFO;

#define GAL_SOFTVGASTATE_ENABLE   1
#define GAL_SOFTVGASTATE_DISABLE  0
/*
 * #define GALFN_SOFTVGASTATE
 */
typedef struct __GAL_SOFTVGASTATE
{
   GAL_HEADER BOOLEAN bSoftVgaEnable;

}
GAL_SOFTVGASTATE, *PGAL_SOFTVGASTATE;

/*
 * #define GALFN_WAITUNTILIDLE
 */
typedef struct __GAL_WAITUNTILIDLE
{
GAL_HEADER}
GAL_WAITUNTILIDLE, *PGAL_WAITUNTILIDLE;

/*
 * #define GALFN_WAITVERTICALBLANK
 */
typedef struct __GAL_WAITVERTICALBLANK
{
GAL_HEADER}
GAL_WAITVERTICALBLANK, *PGAL_WAITVERTICALBLANK;

#define GAL_REG 0x1
#define GAL_VID 0x2
#define GAL_VIP 0x4
/*
 * #define GALFN_WRITEREG
 * #define GALFN_READREG
 */
typedef struct __GAL_HWACCESS
{
   GAL_HEADER DWORD dwType;
   DWORD dwOffset;
   DWORD dwValue;
   DWORD dwByteCount;

}
GAL_HWACCESS, *PGAL_HWACCESS;

/*
 * #define GALFN_ISDISPLAYMODESUPPORTED
 * #define GALFN_SETDISPLAYMODE
 * #define GALFN_GETDISPLAYMODE
 */
typedef struct __GAL_DISPLAYMODE
{
   GAL_HEADER WORD wXres;
   WORD wYres;
   WORD wBpp;
   WORD wRefresh;
   DWORD dwSupported;

}
GAL_DISPLAYMODE, *PGAL_DISPLAYMODE;

/*
 * #define GALFN_SETBPP
 * #define GALFN_GETBPP
 * #define GALFN_SETPITCH                  
 * #define GALFN_GETPITCH                  
 * #define GALFN_SETOFFSET                 
 * #define GALFN_GETOFFSET                 
 */
typedef struct __GAL_DISPLAYPARAMS
{
   GAL_HEADER DWORD dwOffset;
   WORD wBpp;
   WORD wPitch;

}
GAL_DISPLAYPARAMS, *PGAL_DISPLAYPARAMS;

/*
 * #define GALFN_DOTCLKTOREFRESH
 */
typedef struct __GAL_DOTCLKTOREFRESH
{
   GAL_HEADER DWORD dwDotClock;
   WORD wXres;
   WORD wYres;
   WORD wBpp;
   WORD wRefreshRate;

}
GAL_DOTCLKTOREFRESH, *PGAL_DOTCLKTOREFRESH;

/*
 * #define GALFN_GETDISPLAYTIMINGS
 * #define GALFN_SETDISPLAYTIMINGS
 */
typedef struct __GAL_DISPLAYTIMING
{
   GAL_HEADER DWORD dwDotClock;
   WORD wPitch;
   WORD wBpp;
   WORD wHTotal;
   WORD wHActive;
   WORD wHSyncStart;
   WORD wHSyncEnd;
   WORD wHBlankStart;
   WORD wHBlankEnd;
   WORD wVTotal;
   WORD wVActive;
   WORD wVSyncStart;
   WORD wVSyncEnd;
   WORD wVBlankStart;
   WORD wVBlankEnd;
   WORD wPolarity;

}
GAL_DISPLAYTIMING, *PGAL_DISPLAYTIMING;

/*
 * #define GALFN_SETPALETTE_ENTRY
 * #define GALFN_GETPALETTE_ENTRY
 */
typedef struct __GAL_PALETTE_ENTRY
{
   GAL_HEADER DWORD dwIndex;
   DWORD dwPalette;
}
GAL_PALETTE_ENTRY, *PGAL_PALETTE_ENTRY;

/*
 * #define GALFN_SETPALETTE
 * #define GALFN_GETPALETTE
 */
typedef struct __GAL_PALETTE
{
   GAL_HEADER DWORD dwColors[256];
}
GAL_PALETTE, *PGAL_PALETTE;

/*
 * #define GALFN_COMPRESSIONSTATE  
 */
typedef struct __GAL_COMPRESSIONSTATE
{
   GAL_HEADER BOOLEAN bCompressionState;
}
GAL_COMPRESSIONSTATE, *PGAL_COMPRESSIONSTATE;

#define GAL_COMPRESSION_ENABLE   1
#define GAL_COMPRESSION_DISABLE  0

#define GAL_COMPRESSION_OFFSET  1
#define GAL_COMPRESSION_PITCH   2
#define GAL_COMPRESSION_SIZE    4
#define GAL_COMPRESSION_ALL     7

/*
 * #define GALFN_COMPRESSIONPARAMS
 */
typedef struct __GAL_COMPRESSIONPARAMS
{
   GAL_HEADER DWORD dwFlags;
   DWORD dwCompOffset;
   WORD dwCompPitch;
   WORD dwCompSize;
}
GAL_COMPRESSIONPARAMS, *PGAL_COMPRESSIONPARAMS;

#define GAL_SETCURSORENABLE_ENABLE    1
#define GAL_SETCURSORENABLE_DISABLE   0
/*
 * #define GALFN_CURSORENABLE
 */
typedef struct __GAL_CURSORENABLE
{
   GAL_HEADER BOOLEAN bCursorEnable;
}
GAL_CURSORENABLE, *PGAL_CURSORENABLE;

/*
 * #define GALFN_CURSORPOSITION
 */
typedef struct __GAL_CURSORPOSITION
{
   GAL_HEADER DWORD dwMemOffset;
   WORD wXPos;
   WORD wYPos;
   WORD wXHot;
   WORD wYHot;
}
GAL_CURSORPOSITION, *PGAL_CURSORPOSITION;

/*
 * #define GALFN_SETCURSORSHAPE
 */
typedef struct __GAL_SETCURSORSHAPE
{
   GAL_HEADER DWORD dwMemOffset;
   DWORD dwAndMask[32];                 /* Most gfx hardware support only 32x32 */
   DWORD dwXorMask[32];
}
GAL_SETCURSORSHAPE, *PGAL_SETCURSORSHAPE;

/*
 * #define GALFN_SETCURSORCOLORS
 */
typedef struct __GAL_CURSORCOLORS
{
   GAL_HEADER DWORD dwBgColor;
   DWORD dwFgColor;
}
GAL_CURSORCOLORS, *PGAL_CURSORCOLORS;

/*
 * #define GALFN_SETSOLIDPATTERN
 */
typedef struct __GAL_SETSOLIDPATTERN
{
   GAL_HEADER DWORD dwColor;
}
GAL_SETSOLIDPATTERN, *PGAL_SETSOLIDPATTERN;

/*
 * #define GALFN_SETRASTEROPERATION
 */
typedef struct __GAL_SETRASTEROPERATION
{
   GAL_HEADER CHAR cRop;
}
GAL_RASTEROPERATION, *PGAL_RASTEROPERATION;

/*
 * #define GALFN_SETSOLIDSOURCE
 */
typedef struct __GAL_SETSOLIDSOURCE
{
   GAL_HEADER DWORD dwColor;
}
GAL_SETSOLIDSOURCE, *PGAL_SETSOLIDSOURCE;

/*
 * #define GALFN_PATTERNFILL
 */
typedef struct __GAL_PATTERNFILL
{
   GAL_HEADER WORD wXPos;
   WORD wYPos;
   WORD wWidth;
   WORD wHeight;
}
GAL_PATTERNFILL, *PGAL_PATTERNFILL;

/*
 * #define GALFN_SETMONOSOURCE
 */
typedef struct __GAL_SETMONOSOURCE
{
   GAL_HEADER DWORD dwBgColor;
   DWORD dwFgColor;
   CHAR cTransparency;
}
GAL_SETMONOSOURCE, *PGAL_SETMONOSOURCE;

/*
 * #define GALFN_SETMONOPATTERN
 */
typedef struct __GAL_SETMONOPATTERN
{
   GAL_HEADER DWORD dwBgColor;
   DWORD dwFgColor;
   DWORD dwData0;
   DWORD dwData1;
   CHAR cTransparency;
}
GAL_SETMONOPATTERN, *PGAL_SETMONOPATTERN;

/*
 * #define GALFN_SCREENTOSCREENBLT
 */
typedef struct __GAL_SCREENTOSCREENBLT
{
   GAL_HEADER WORD wXStart;
   WORD wYStart;
   WORD wXEnd;
   WORD wYEnd;
   WORD wWidth;
   WORD wHeight;
}
GAL_SCREENTOSCREENBLT, *PGAL_SCREENTOSCREENBLT;

/*
 * #define GALFN_SCREENTOSCREENXBLT
 */
typedef struct __GAL_SCREENTOSCREENXBLT
{
   GAL_HEADER WORD wXStart;
   WORD wYStart;
   WORD wXEnd;
   WORD wYEnd;
   WORD wWidth;
   WORD wHeight;
   DWORD dwColor;
}
GAL_SCREENTOSCREENXBLT, *PGAL_SCREENTOSCREENXBLT;

/*
 * #define GALFN_BRESENHAMLINE
 */
typedef struct __GAL_BRESENHAMLINE
{
   GAL_HEADER WORD wX1;
   WORD wY1;
   WORD wLength;
   WORD wErr;
   WORD wE1;
   WORD wE2;
   WORD wFlags;
}
GAL_BRESENHAMLINE, *PGAL_BRESENHAMLINE;

/*
 * #define GALFN_COLOR_PATTERNFILL
 */
typedef struct __GAL_COLOR_PATTERNFILL
{
   GAL_HEADER WORD wDsty;
   WORD wDstx;
   WORD wWidth;
   WORD wHeight;
   DWORD dwPattern;
}
GAL_COLOR_PATTERNFILL, *PGAL_COLOR_PATTERNFILL;

/*
 * #define GALFN_COLOR_BITMAP_TO_SCREEN_BLT
 */
typedef struct __GAL_COLOR_BITMAP_TO_SCREEN_BLT
{
   GAL_HEADER WORD wSrcx;
   WORD wSrcy;
   WORD wDstx;
   WORD wDsty;
   WORD wWidth;
   WORD wHeight;
   DWORD dwData;
   WORD wPitch;
}
GAL_COLOR_BITMAP_TO_SCREEN_BLT, *PGAL_COLOR_BITMAP_TO_SCREEN_BLT;

/*
 * #define GALFN_COLOR_BITMAP_TO_SCREEN_XBLT
 */
typedef struct __GAL_COLOR_BITMAP_TO_SCREEN_XBLT
{
   GAL_HEADER WORD wSrcx;
   WORD wSrcy;
   WORD wDstx;
   WORD wDsty;
   WORD wWidth;
   WORD wHeight;
   DWORD dwData;
   WORD wPitch;
   DWORD dwColor;
}
GAL_COLOR_BITMAP_TO_SCREEN_XBLT, *PGAL_COLOR_BITMAP_TO_SCREEN_XBLT;

/*
 * #define GALFN_MONO_BITMAP_TO_SCREEN_BLT
 */
typedef struct __GAL_MONO_BITMAP_TO_SCREEN_BLT
{
   GAL_HEADER WORD wSrcx;
   WORD wSrcy;
   WORD wDstx;
   WORD wDsty;
   WORD wWidth;
   WORD wHeight;
   DWORD dwData;
   WORD wPitch;
}
GAL_MONO_BITMAP_TO_SCREEN_BLT, *PGAL_MONO_BITMAP_TO_SCREEN_BLT;

/*
 * #define GALFN_TEXT_BLT
 */
typedef struct __GAL_TEXT_BLT
{
   GAL_HEADER WORD wDstx;
   WORD wDsty;
   WORD wWidth;
   WORD wHeight;
   DWORD dwData;
}
GAL_TEXT_BLT, *PGAL_TEXT_BLT;

 /*
  * * #define GALFN_VGAMODESWITCH 
  * * #define GALFN_VGACLEARCRTEXT
  * * #define GALFN_VGASETPITCH   
  * * #define GALFN_VGARESTORE
  * * #define GALFN_VGASAVE   
  * * #define GALFN_VGASETMODE
  */

typedef struct __GAL_VGAREGS
{
   int xsize;
   int ysize;
   int hz;
   int clock;
   unsigned char miscOutput;
   unsigned char stdCRTCregs[GFX_STD_CRTC_REGS];
   unsigned char extCRTCregs[GFX_EXT_CRTC_REGS];
}
GAL_VGAREGS, *PGAL_VGAREGS;

typedef struct __GAL_VGAMODEDATA
{
   GAL_HEADER DWORD dwFlags;            /* Flags for this subfunction */
   GAL_VGAREGS sVgaRegs;                /* CRT+SEQ+SEQ register data block */
   WORD wXres;
   WORD wYres;
   WORD wBpp;
   WORD wRefresh;
}
GAL_VGAMODEDATA, *PGAL_VGAMODEDATA;

typedef struct __GAL_VGATESTPCI
{
   GAL_HEADER SWORD softvga;
}
GAL_VGATESTPCI, *PGAL_VGATESTPCI;

typedef struct __GAL_VGAGETPCICOMMAND
{
   GAL_HEADER unsigned char value;
}
GAL_VGAGETPCICOMMAND, *PGAL_VGAGETPCICOMMAND;

typedef struct __GAL_VGASEQRESET
{
   GAL_HEADER SWORD reset;
   SWORD statusok;
}
GAL_VGASEQRESET, *PGAL_VGASEQRESET;

typedef struct __GAL_VGASETGRAPHICSBITS
{
   GAL_HEADER SWORD statusok;
}
GAL_VGASETGRAPHICSBITS, *PGAL_VGASETGRAPHICSBITS;

/******** Panel Support functions *********************/
/*
* #define GALFN_PNLSETPARAMS
* #define GALFN_PNLGETPARAMS
* #define GALFN_PNLINITPANEL
* #define GALFN_PNLSAVESTATE
* #define GALFN_PNLRESTORESTATE
*/
typedef struct __GAL_PNLPARAMS
{
   GAL_HEADER Pnl_PanelParams PanelParams;
}
GAL_PNLPARAMS, *PGAL_PNLPARAMS;

/*
* #define GALFN_PNLBIOSENABLE
* #define GALFN_PNLBIOSINFO
*/
typedef struct __GAL_PNLBIOS
{
   GAL_HEADER int state;
   int XRes;
   int YRes;
   int Bpp;
   int Freq;
}
GAL_PNLBIOS, *PGAL_PNLBIOS;

typedef struct __GAL_ENABLEPANNING
{
   GAL_HEADER int x;
   int y;
}
GAL_ENABLEPANNING, *PGAL_ENABLEPANNING;

/*
 * #define GALFN_SETCRTENABLE
 * #define GALFN_GETCRTENABLE
 */
typedef struct __GAL_CRTENABLE
{
   GAL_HEADER WORD wCrtEnable;
}
GAL_CRTENABLE, *PGAL_CRTENABLE;

#define GAL_TVSTATE       0x01
#define GAL_TVOUTPUT      0x02
#define GAL_TVFORMAT      0x04
#define GAL_TVRESOLUTION  0x08
#define GAL_TVALL         0x0F
/*
 * #define GALFN_SETTVPARAMS
 * #define GALFN_GETTVPARAMS
 * #define GALFN_SETENABLE
 * #define GALFN_GETENABLE
 * #define GALFN_ISTVMODESUPPORTED
 */
typedef struct __GAL_TVPARAMS
{
   GAL_HEADER DWORD dwFlags;
   WORD wWidth;
   WORD wHeight;
   WORD wStandard;
   WORD wType;
   WORD wOutput;
   WORD wResolution;
   BOOLEAN bState;

}
GAL_TVPARAMS, *PGAL_TVPARAMS;

/*
 * #define GALFN_SETTVTIMING
 * #define GALFN_GETTVTIMING
 */
typedef struct __GAL_TVTIMING
{
   GAL_HEADER DWORD dwFlags;            /* not used currently */
   unsigned long HorzTim;
   unsigned long HorzSync;
   unsigned long VertSync;
   unsigned long LineEnd;
   unsigned long VertDownscale;
   unsigned long HorzScaling;
   unsigned long TimCtrl1;
   unsigned long TimCtrl2;
   unsigned long Subfreq;
   unsigned long DispPos;
   unsigned long DispSize;
   unsigned long Debug;
   unsigned long DacCtrl;
   unsigned long DotClock;
}
GAL_TVTIMING, *PGAL_TVTIMING;

/******** Video Support functions *********************/

typedef struct __GAL_SETVIDEOENABLE
{
   GAL_HEADER BOOLEAN enable;
}
GAL_VIDEOENABLE, *PGAL_VIDEOENABLE;

typedef struct __GAL_SETVIDEOFORMAT
{
   GAL_HEADER int format;
}
GAL_VIDEOFORMAT, *PGAL_VIDEOFORMAT;

typedef struct __GAL_SETVIDEOSIZE
{
   GAL_HEADER unsigned short width;
   unsigned short height;
}
GAL_VIDEOSIZE, *PGAL_VIDEOSIZE;

typedef struct __GAL_SETVIDEOOFFSET
{
   GAL_HEADER unsigned long offset;
}
GAL_VIDEOOFFSET, *PGAL_VIDEOOFFSET;

typedef struct __GAL_SETVIDEOWINDOW
{
   GAL_HEADER short x;
   short y;
   short w;
   short h;
}
GAL_VIDEOWINDOW, *PGAL_VIDEOWINDOW;

typedef struct __GAL_SETVIDEOSCALE
{
   GAL_HEADER unsigned short srcw;
   unsigned short srch;
   unsigned short dstw;
   unsigned short dsth;
}
GAL_VIDEOSCALE, *PGAL_VIDEOSCALE;

typedef struct __GAL_SETVIDEOFILTER
{
   GAL_HEADER int xfilter;
   int yfilter;
}
GAL_VIDEOFILTER, *PGAL_VIDEOFILTER;

typedef struct __GAL_SETVIDEOCOLORKEY
{
   GAL_HEADER unsigned long key;
   unsigned long mask;
   int bluescreen;
}
GAL_VIDEOCOLORKEY, *PGAL_VIDEOCOLORKEY;

typedef struct __GAL_SETVIDEODOWNSCALEENABLE
{
   GAL_HEADER int enable;
}
GAL_VIDEODOWNSCALEENABLE, *PGAL_VIDEODOWNSCALEENABLE;

typedef struct __GAL_SETVIDEODOWNSCALECONFIG
{
   GAL_HEADER unsigned short type;
   unsigned short m;
}
GAL_VIDEODOWNSCALECONFIG, *PGAL_VIDEODOWNSCALECONFIG;

typedef struct __GAL_SETVIDEODOWNSCALECOEFF
{
   GAL_HEADER unsigned short coef1;
   unsigned short coef2;
   unsigned short coef3;
   unsigned short coef4;
}
GAL_VIDEODOWNSCALECOEFF, *PGAL_VIDEODOWNSCALECOEFF;

#define GAL_VIDEO_SOURCE_MEMORY 0x0
#define GAL_VIDEO_SOURCE_DVIP   0x1
typedef struct __GAL_SETVIDEOSOURCE
{
   GAL_HEADER int source;
}
GAL_VIDEOSOURCE, *PGAL_VIDEOSOURCE;

typedef struct __GAL_SETVIDEOINTERLACED
{
   GAL_HEADER int enable;
}
GAL_SETVIDEOINTERLACED, *PGAL_SETVIDEOINTERLACED;

typedef struct __GAL_GETVIDEOINTERLACED
{
   GAL_HEADER int interlaced;
}
GAL_GETVIDEOINTERLACED, *PGAL_GETVIDEOINTERLACED;

typedef struct __GAL_COLORSPACEYUV
{
   GAL_HEADER int colorspace;
}
GAL_COLORSPACEYUV, *PGAL_COLORSPACEYUV;

typedef struct __GAL_SETGENLOCKENABLE
{
   GAL_HEADER int enable;
}
GAL_GENLOCKENABLE, *PGAL_GENLOCKENABLE;

typedef struct __GAL_SETGENLOCKDELAY
{
   GAL_HEADER int delay;
}
GAL_GENLOCKDELAY, *PGAL_GENLOCKDELAY;

typedef struct __GAL_SETTOPLINEINODD
{
   GAL_HEADER int enable;
}
GAL_TOPLINEINODD, *PGAL_TOPLINEINODD;

typedef struct __GAL_SETVIDEOCURSOR
{
   GAL_HEADER unsigned long key;
   unsigned long mask;
   unsigned short select_color2;
   unsigned long color1;
   unsigned long color2;
}
GAL_VIDEOCURSOR, *PGAL_VIDEOCURSOR;

typedef struct __GAL_READCRC
{
   GAL_HEADER DWORD crc;
}
GAL_READCRC, *PGAL_READCRC;

typedef struct __GAL_READWINDOWCRC
{
   GAL_HEADER SWORD source;
   WORD x;
   WORD y;
   WORD width;
   WORD height;
   SWORD crc32;
   DWORD crc;
}
GAL_READWINDOWCRC, *PGAL_READWINDOWCRC;

typedef struct __GAL_GETALPHASIZE
{
   GAL_HEADER WORD * x;
   WORD *y;
   WORD *width;
   WORD *height;
}
GAL_ALPHASIZE, *PGAL_ALPHASIZE;

typedef struct __GAL_SETMACROVISIONENABLE
{
   GAL_HEADER SWORD enable;
}
GAL_MACROVISIONENABLE, *PGAL_MACROVISIONENABLE;

typedef struct __GAL_SETVIDEOREQUEST
{
   GAL_HEADER short x;
   short y;
}
GAL_VIDEOREQUEST, *PGAL_VIDEOREQUEST;

typedef struct __GAL_ALPHAENABLE
{
   GAL_HEADER int enable;
}
GAL_ALPHAENABLE, *PGAL_ALPHAENABLE;

typedef struct __GAL_SETALPHAWINDOW
{
   GAL_HEADER short x;
   short y;
   unsigned short width;
   unsigned short height;
}
GAL_ALPHAWINDOW, *PGAL_ALPHAWINDOW;

typedef struct __GAL_ALPHAVALUE
{
   GAL_HEADER unsigned char alpha;
   char delta;
}
GAL_ALPHAVALUE, *PGAL_ALPHAVALUE;

typedef struct __GAL_ALPHAPRIORITY
{
   GAL_HEADER int priority;
}
GAL_ALPHAPRIORITY, *PGAL_ALPHAPRIORITY;

typedef struct __GAL_ALPHACOLOR
{
   GAL_HEADER unsigned long color;
}
GAL_ALPHACOLOR, *PGAL_ALPHACOLOR;

typedef struct __GAL_SETALPHAREGION
{
   GAL_HEADER int region;
}
GAL_ALPHAREGION, *PGAL_ALPHAREGION;

typedef struct __GAL_SETVIDEOOUTSIDEALPHA
{
   GAL_HEADER int enable;
}
GAL_VIDEOOUTSIDEALPHA, *PGAL_VIDEOOUTSIDEALPHA;

typedef struct __GAL_SETVIDEOPALETTE
{
   GAL_HEADER int identity;
   unsigned long palette[256];
}
GAL_VIDEOPALETTE, *PGAL_VIDEOPALETTE;

typedef struct __GAL_VIDEOINFO
{
   GAL_HEADER int enable;
   int format;
   int filter;

   unsigned long src_size;
   unsigned long dst_size;
   unsigned long line_size;
   unsigned long xclip;
   unsigned long offset;
   unsigned long scale;
   unsigned long position;

   int color_key_src;
   unsigned long color_key;
   unsigned long color_key_mask;

   int downscale_enable;

   unsigned short downscale_type;

   unsigned short downscale_mask;
   unsigned short downscale_coef1;
   unsigned short downscale_coef2;
   unsigned short downscale_coef3;
   unsigned short downscale_coef4;
}
GAL_VIDEOINFO, *PGAL_VIDEOINFO;

/* ICON related data strucures */
typedef struct __GAL_SETICONENABLE
{
   GAL_HEADER SWORD enable;
}
GAL_ICONENABLE, *PGAL_ICONENABLE;

typedef struct __GAL_SETICONCOLORS
{
   GAL_HEADER DWORD color0;
   DWORD color1;
   DWORD color2;
}
GAL_ICONCOLORS, *PGAL_ICONCOLORS;

typedef struct __GAL_SETICONPOSITION
{
   GAL_HEADER DWORD memoffset;
   WORD xpos;
}
GAL_ICONPOSITION, *PGAL_ICONPOSITION;

typedef struct __GAL_SETICONSHAPE64
{
   GAL_HEADER DWORD memoffset;
   DWORD *andmask;
   DWORD *xormask;
   DWORD lines;
}
GAL_ICONSHAPE64, *PGAL_ICONSHAPE64;

/* VIP related data strucures */

typedef struct __GAL_SETVIPENABLE
{
   GAL_HEADER SWORD enable;
}
GAL_VIPENABLE, *PGAL_VIPENABLE;

typedef struct __GAL_SETVIPCAPTURERUNMODE
{
   GAL_HEADER SWORD mode;
}
GAL_VIPCAPTURERUNMODE, *PGAL_VIPCAPTURERUNMODE;

typedef struct __GAL_SETVIPBASE
{
   GAL_HEADER DWORD even;
   DWORD odd;
   DWORD address;
}
GAL_VIPBASE, *PGAL_VIPBASE;

typedef struct __GAL_SETVIPPITCH
{
   GAL_HEADER DWORD pitch;
}
GAL_VIPPITCH, *PGAL_VIPPITCH;

typedef struct __GAL_SETVIPMODE
{
   GAL_HEADER SWORD mode;
}
GAL_VIPMODE, *PGAL_VIPMODE;

typedef struct __GAL_SETVIPBUS_RTH
{
   GAL_HEADER SWORD enable;
}
GAL_VIPBUS_RTH, *PGAL_VIPBUS_RTH;

typedef struct __GAL_SETVIPLASTLINE
{
   GAL_HEADER SWORD last_line;
}
GAL_VIPLASTLINE, *PGAL_VIPLASTLINE;

typedef struct __GAL_TESTVIPODDFIELD
{
   GAL_HEADER SWORD status;
}
GAL_TESTVIPODDFIELD, *PGAL_TESTVIPODDFIELD;

typedef struct __GAL_TESTVIPBASESUPDATED
{
   GAL_HEADER SWORD status;
}
GAL_TESTVIPBASESUPDATED, *PGAL_TESTVIPBASESUPDATED;

typedef struct __GAL_TESTVIPFIFOOVERFLOW
{
   GAL_HEADER SWORD status;
}
GAL_TESTVIPOVERFLOW, *PGAL_TESTVIPOVERFLOW;

typedef struct __GAL_GETVIPLINE
{
   GAL_HEADER SWORD status;
}
GAL_VIPLINE, *PGAL_VIPLINE;

/* VBI related data strucures */

typedef struct __GAL_VBIENABLE
{
   GAL_HEADER SWORD enable;
}
GAL_VBIENABLE, *PGAL_VBIENABLE;

typedef struct __GAL_VBIBASE
{
   GAL_HEADER DWORD even;
   DWORD odd;
   DWORD address;
}
GAL_VBIBASE, *PGAL_VBIBASE;

typedef struct __GAL_VBIPITCH
{
   GAL_HEADER DWORD pitch;
}
GAL_VBIPITCH, *PGAL_VBIPITCH;

typedef struct __GAL_VBIMODE
{
   GAL_HEADER SWORD mode;
}
GAL_VBIMODE, *PGAL_VBIMODE;

typedef struct __GAL_SETVBIDIRECT
{
   GAL_HEADER DWORD even_lines;
   DWORD odd_lines;
}
GAL_SETVBIDIRECT, *PGAL_SETVBIDIRECT;

typedef struct __GAL_GETVBIDIRECT
{
   GAL_HEADER SWORD odd;
   DWORD direct_lines;
}
GAL_GETVBIDIRECT, *PGAL_GETVBIDIRECT;

typedef struct __GAL_VBIINTERRUPT
{
   GAL_HEADER SWORD enable;
}
GAL_VBIINTERRUPT, *PGAL_VBIINTERRUPT;

/* Second generation rendering routines data structures */

typedef struct __GAL_SETSTRIDE
{
   GAL_HEADER WORD stride;
}
GAL_STRIDE, *PGAL_STRIDE;

typedef struct __GAL_SETPATTERNORIGIN
{
   GAL_HEADER int x;
   int y;
}
GAL_PATTERNORIGIN, *PGAL_PATTERNORIGIN;

typedef struct __GAL_SETSOURCETRANSPARENCY
{
   GAL_HEADER DWORD color;
   DWORD mask;
}
GAL_SOURCETRANSPARENCY, *PGAL_SOURCETRANSPARENCY;

typedef struct __GAL_GFX2SETALPHAMODE
{
   GAL_HEADER SWORD mode;
}
GAL_GFX2ALPHAMODE, *PGAL_GFX2ALPHAMODE;

typedef struct __GAL_GFX2SETALPHAVALUE
{
   GAL_HEADER CHAR value;
}
GAL_GFX2ALPHAVALUE, *PGAL_GFX2ALPHAVALUE;

typedef struct __GAL_GFX2PATTERNFILL
{
   GAL_HEADER DWORD dstoffset;
   WORD width;
   WORD height;
}
GAL_GFX2PATTERNFILL, *PGAL_GFX2PATTERNFILL;

typedef struct __GAL_GFX2COLORPATTERNFILL
{
   GAL_HEADER DWORD dstoffset;
   WORD width;
   WORD height;
   DWORD pattern;
}
GAL_GFX2COLORPATTERNFILL, *PGAL_GFX2COLORPATTERNFILL;

typedef struct __GAL_GFX2SCREENTOSCREENBLT
{
   GAL_HEADER DWORD srcoffset;
   DWORD dstoffset;
   WORD width;
   WORD height;
   SWORD flags;
}
GAL_GFX2SCREENTOSCREENBLT, *PGAL_GFX2SCREENTOSCREENBLT;

typedef struct __GAL_GFX2MONOEXPANDBLT
{
   GAL_HEADER unsigned long srcbase;
   WORD srcx;
   WORD srcy;
   DWORD dstoffset;
   WORD width;
   WORD height;
   WORD byte_packed;
}
GAL_GFX2MONOEXPANDBLT, *PGAL_GFX2MONOEXPANDBLT;

typedef struct __GAL_GFX2COLORBMPTOSCRBLT
{
   GAL_HEADER WORD srcx;
   WORD srcy;
   DWORD dstoffset;
   WORD width;
   WORD height;
   DWORD data;
   WORD pitch;
}
GAL_GFX2COLORBMPTOSCRBLT, *PGAL_GFX2COLORBMPTOSCRBLT;

typedef struct __GAL_GFX2MONOBMPTOSCRBLT
{
   GAL_HEADER WORD srcbase;
   WORD srcx;
   WORD srcy;
   DWORD dstoffset;
   WORD width;
   WORD height;
   DWORD data;
   WORD pitch;
}
GAL_GFX2MONOBMPTOSCRBLT, *PGAL_GFX2MONOBMPTOSCRBLT;

typedef struct __GAL_GFX2TEXTBLT
{
   GAL_HEADER DWORD dstoffset;
   WORD width;
   WORD height;
   DWORD data;
}
GAL_GFX2TEXTBLT, *PGAL_GFX2TEXTBLT;

typedef struct __GAL_GFX2BRESENHAMLINE
{
   GAL_HEADER DWORD dstoffset;
   WORD length;
   WORD initerr;
   WORD axialerr;
   WORD diagerr;
   WORD flags;
}
GAL_GFX2BRESENHAMLINE, *PGAL_GFX2BRESENHAMLINE;

typedef struct __GAL_GFX2SYNCTOVBLANK
{
GAL_HEADER}
GAL_GFX2SYNCTOVBLANK, *PGAL_GFX2SYNCTOVBLANK;

/*
 GALFN_SETVIDEOYUVPITCH
 */
typedef struct _GAL_SETVIDEOYUVPITCH
{
   GAL_HEADER unsigned long y_pitch;
   unsigned long uv_pitch;
}
GAL_VIDEOYUVPITCH, *PGAL_VIDEOYUVPITCH;

/*
  GALFN_SETVIDEOYUVOFFSETS
*/
typedef struct _GAL_VIDEOYUVOFFSETS
{
   GAL_HEADER unsigned long dwYoffset;
   unsigned long dwUoffset;
   unsigned long dwVoffset;
}
GAL_VIDEOYUVOFFSETS, *PGAL_VIDEOYUVOFFSETS;

typedef struct __GAL_SETVIDEOLEFTCROP
{
   GAL_HEADER WORD x;
   SWORD status;
}
GAL_VIDEOLEFTCROP, *PGAL_VIDEOLEFTCROP;

typedef struct __GAL_SETVIDEOVERTICALDOWNSCALE
{
   GAL_HEADER WORD srch;
   WORD dsth;
   SWORD status;
}
GAL_VIDEOVERTICALDOWNSCALE, *PGAL_VIDEOVERTICALDOWNSCALE;

typedef struct __GAL_VBISOURCE
{
   GAL_HEADER VideoSourceType source;
   SWORD status;
}
GAL_VBISOURCE, *PGAL_VBISOURCE;

typedef struct __GAL_VBILINES
{
   GAL_HEADER DWORD even;
   DWORD odd;
   SWORD status;
   DWORD lines;
}
GAL_VBILINES, *PGAL_VBILINES;

typedef struct __GAL_VBITOTAL
{
   GAL_HEADER DWORD even;
   DWORD odd;
   SWORD status;
   DWORD total;
}
GAL_VBITOTAL, *PGAL_VBITOTAL;

typedef struct __GAL_VSCALEROFFSET
{
   GAL_HEADER char offset;
   SWORD status;
}
GAL_VSCALEROFFSET, *PGAL_VSCALEROFFSET;

/* MSR  data strucures */

typedef struct __GAL_IDMSRDEVICE
{
   GAL_HEADER MSR * pDev;
   DWORD address;
   DEV_STATUS dev_status;
}
GAL_IDMSRDEVICE, *PGAL_IDMSRDEVICE;

typedef struct __GAL_GETMSRDEVADDRESS
{
   GAL_HEADER WORD device;
   unsigned long address;
   DEV_STATUS dev_status;
}
GAL_GETMSRDEVADDRESS, *PGAL_GETMSRDEVADDRESS;

typedef struct __GAL_GETMBUSIDATADDRESS
{
   GAL_HEADER unsigned int device;
   unsigned long address;
   DEV_STATUS dev_status;
}
GAL_GETMBUSIDATADDRESS, *PGAL_GETMBUSIDATADDRESS;

/* Gal device function's prototye declarations */

/** Init **********************************************************/
BOOLEAN Gal_initialize_interface(void);
BOOLEAN Gal_cleanup_interface(void);
BOOLEAN Gal_get_adapter_info(PGAL_ADAPTERINFO pAdapterInfo);
BOOLEAN Gal_set_softvga_state(BOOLEAN);
BOOLEAN Gal_get_softvga_state(int *bState);
BOOLEAN Gal_set_crt_enable(int);
BOOLEAN Gal_wait_until_idle(void);
BOOLEAN Gal_wait_vertical_blank(void);
BOOLEAN Gal_write_register(int type, unsigned long offset,
                           unsigned long value, int size);

BOOLEAN Gal_read_register(int type, unsigned long offset,
                          unsigned long *value, int size);
/** Display Engine ******************************************************/
BOOLEAN Gal_is_display_mode_supported(int xres, int yres, int bpp, int hz,
                                      int *supported);
BOOLEAN Gal_set_display_mode(int xres, int yres, int bpp, int hz);
BOOLEAN Gal_get_display_mode(int *xres, int *yres, int *bpp, int *hz);
BOOLEAN Gal_set_bpp(unsigned short bpp);
BOOLEAN Gal_set_display_bpp(unsigned short bpp);
BOOLEAN Gal_get_display_bpp(unsigned short *bpp);
BOOLEAN Gal_set_display_pitch(unsigned short pitch);
BOOLEAN Gal_get_display_pitch(unsigned short *pitch);
BOOLEAN Gal_set_display_offset(unsigned long offset);
BOOLEAN Gal_get_display_offset(unsigned long *offset);
BOOLEAN Gal_get_refreshrate_from_dotclock(int xres, int yres, int bpp,
                                          int *hz, unsigned long frequency);
BOOLEAN Gal_get_display_timing(PGAL_DISPLAYTIMING pDisplayTiming);
BOOLEAN Gal_set_display_timing(PGAL_DISPLAYTIMING pDisplayTiming);
BOOLEAN Gal_set_fixed_timings(int pnlXres, int pnlYres, int totXres,
                              int totYres, int bpp);
BOOLEAN Gal_set_display_palette_entry(unsigned long index,
                                      unsigned long palette);
BOOLEAN Gal_get_display_palette_entry(unsigned long index,
                                      unsigned long *palette);
BOOLEAN Gal_set_display_palette(PGAL_PALETTE);
BOOLEAN Gal_get_display_palette(PGAL_PALETTE);
BOOLEAN Gal_set_cursor_enable(int enable);
BOOLEAN Gal_get_cursor_enable(int *enable);
BOOLEAN Gal_set_cursor_colors(unsigned long bkcolor, unsigned long fgcolor);
BOOLEAN Gal_get_cursor_colors(unsigned long *bkcolor, unsigned long *fgcolor);
BOOLEAN Gal_set_cursor_position(unsigned long memoffset,
                                unsigned short xpos, unsigned short ypos,
                                unsigned short xhotspot,
                                unsigned short yhotspot);
BOOLEAN Gal_get_cursor_position(unsigned long *memoffset,
                                unsigned short *xpos, unsigned short *ypos,
                                unsigned short *xhotspot,
                                unsigned short *yhotspot);
BOOLEAN Gal_set_cursor_shape32(unsigned long memoffset,
                               unsigned long *andmask,
                               unsigned long *xormask);

BOOLEAN Gal_set_cursor_shape64(unsigned long memoffset,
                               unsigned long *andmask,
                               unsigned long *xormask);

/** Render ********************************************************/
BOOLEAN Gal_set_solid_pattern(unsigned long color);

BOOLEAN Gal_set_mono_source(unsigned long bgcolor, unsigned long fgcolor,
                            unsigned char transparency);
BOOLEAN Gal_set_mono_pattern(unsigned long bgcolor, unsigned long fgcolor,
                             unsigned long data0, unsigned long data1,
                             unsigned char transparency);

BOOLEAN Gal_set_raster_operation(unsigned char rop);

BOOLEAN Gal_pattern_fill(unsigned short x, unsigned short y,
                         unsigned short width, unsigned short height);

BOOLEAN Gal_set_solid_source(unsigned long color);

BOOLEAN Gal_screen_to_screen_blt(unsigned short srcx, unsigned short srcy,
                                 unsigned short dstx, unsigned short dsty,
                                 unsigned short width, unsigned short height);

BOOLEAN Gal_screen_to_screen_xblt(unsigned short srcx,
                                  unsigned short srcy,
                                  unsigned short dstx,
                                  unsigned short dsty,
                                  unsigned short width,
                                  unsigned short height, unsigned long color);

BOOLEAN Gal_bresenham_line(unsigned short x, unsigned short y,
                           unsigned short length, unsigned short initerr,
                           unsigned short axialerr, unsigned short diagerr,
                           unsigned short flags);

BOOLEAN Gal_color_pattern_fill(unsigned short x, unsigned short y,
                               unsigned short width, unsigned short height,
                               unsigned long pattern);

BOOLEAN Gal_color_bitmap_to_screen_blt(unsigned short srcx,
                                       unsigned short srcy,
                                       unsigned short dstx,
                                       unsigned short dsty,
                                       unsigned short width,
                                       unsigned short height,
                                       unsigned long data, long pitch);

BOOLEAN Gal_color_bitmap_to_screen_xblt(unsigned short srcx,
                                        unsigned short srcy,
                                        unsigned short dstx,
                                        unsigned short dsty,
                                        unsigned short width,
                                        unsigned short height,
                                        unsigned long data, long pitch,
                                        unsigned long color);

BOOLEAN Gal_mono_bitmap_to_screen_blt(unsigned short srcx,
                                      unsigned short srcy,
                                      unsigned short dstx,
                                      unsigned short dsty,
                                      unsigned short width,
                                      unsigned short height,
                                      unsigned long data, short pitch);

BOOLEAN Gal_text_blt(unsigned short dstx, unsigned short dsty,
                     unsigned short width, unsigned short height,
                     unsigned long data);

/** Compression*******************************************************/
BOOLEAN Gal_set_compression_enable(BOOLEAN);
BOOLEAN Gal_get_compression_enable(int *flag);
BOOLEAN Gal_set_compression_parameters(unsigned long flags,
                                       unsigned long offset,
                                       unsigned short pitch,
                                       unsigned short size);
BOOLEAN Gal_get_compression_parameters(unsigned long flags,
                                       unsigned long *offset,
                                       unsigned short *pitch,
                                       unsigned short *size);

/** VGA **********************************************************/
BOOLEAN Gal_vga_mode_switch(int active);
BOOLEAN Gal_vga_clear_extended(void);
BOOLEAN Gal_vga_pitch(PGAL_VGAMODEDATA pvregs, unsigned short pitch);
BOOLEAN Gal_vga_restore(PGAL_VGAMODEDATA pvregs);
BOOLEAN Gal_vga_save(PGAL_VGAMODEDATA pvregs);
BOOLEAN Gal_vga_mode(PGAL_VGAMODEDATA pvregs);
BOOLEAN Gal_vga_test_pci(int *softvga);
BOOLEAN Gal_vga_get_pci_command(unsigned char *value);
BOOLEAN Gal_vga_seq_reset(int reset);
BOOLEAN Gal_vga_set_graphics(void);

/** Panel **********************************************************/
BOOLEAN Gal_pnl_set_params(unsigned long flags, PPnl_PanelParams pParam);
BOOLEAN Gal_pnl_get_params(unsigned long flags, PPnl_PanelParams pParam);
BOOLEAN Gal_pnl_init(PPnl_PanelParams pParam);
BOOLEAN Gal_pnl_save(void);
BOOLEAN Gal_pnl_restore(void);
BOOLEAN Gal_pnl_powerup(void);
BOOLEAN Gal_pnl_powerdown(void);
BOOLEAN Gal_enable_panning(int x, int y);
BOOLEAN Gal_pnl_enabled_in_bios(int *state);
BOOLEAN Gal_pnl_info_from_bios(int *xres, int *yres, int *bpp, int *hz);

/** TV **********************************************************/
BOOLEAN Gal_tv_set_params(unsigned long flags, PGAL_TVPARAMS pTV);
BOOLEAN Gal_tv_get_params(unsigned long flags, PGAL_TVPARAMS pTV);
BOOLEAN Gal_tv_set_timings(unsigned long flags, PGAL_TVTIMING pTV);
BOOLEAN Gal_tv_get_timings(unsigned long flags, PGAL_TVTIMING pTV);
BOOLEAN Gal_set_tv_enable(int bState);
BOOLEAN Gal_get_tv_enable(unsigned int *bState);
BOOLEAN Gal_is_tv_mode_supported(unsigned long flags, PGAL_TVPARAMS pTV,
                                 int *bState);

/** Video **********************************************************/
BOOLEAN Gal_set_video_enable(int enable);
BOOLEAN Gal_set_video_format(int format);
BOOLEAN Gal_set_video_size(unsigned short width, unsigned short height);
BOOLEAN Gal_set_video_offset(unsigned long offset);
BOOLEAN Gal_set_video_yuv_offsets(unsigned long yoffset,
                                  unsigned long uoffset,
                                  unsigned long voffset);
BOOLEAN Gal_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch);

BOOLEAN Gal_set_video_window(short x, short y, short w, short h);
BOOLEAN Gal_set_video_scale(unsigned short srcw, unsigned short srch,
                            unsigned short dstw, unsigned short dsth);
BOOLEAN Gal_set_video_filter(int xfilter, int yfilter);
BOOLEAN Gal_set_video_color_key(unsigned long key,
                                unsigned long mask, int bluescreen);
BOOLEAN Gal_set_video_downscale_enable(int enable);
BOOLEAN Gal_set_video_downscale_config(unsigned short type, unsigned short m);
BOOLEAN Gal_set_video_downscale_coefficients(unsigned short coef1,
                                             unsigned short coef2,
                                             unsigned short coef3,
                                             unsigned short coef4);
BOOLEAN Gal_set_video_source(int source);
BOOLEAN Gal_set_video_interlaced(int enable);
BOOLEAN Gal_get_video_interlaced(int *interlaced);
BOOLEAN Gal_set_color_space_YUV(int enable);
BOOLEAN Gal_get_color_space_YUV(int *colorspace);
BOOLEAN Gal_set_video_cursor(unsigned long key,
                             unsigned long mask,
                             unsigned short select_color2,
                             unsigned long color1, unsigned long color2);
BOOLEAN Gal_get_video_cursor(unsigned long *key,
                             unsigned long *mask,
                             unsigned short *select_color2,
                             unsigned long *color1, unsigned long *color2);
BOOLEAN Gal_set_video_request(short x, short y);
BOOLEAN Gal_set_alpha_enable(int enable);
BOOLEAN Gal_get_alpha_enable(int *enable);
BOOLEAN Gal_get_alpha_size(unsigned short *x, unsigned short *y,
                           unsigned short *width, unsigned short *height);

BOOLEAN Gal_set_video_request(short x, short y);
BOOLEAN Gal_set_alpha_window(short x, short y,
                             unsigned short width, unsigned short height);
BOOLEAN Gal_set_alpha_value(unsigned char alpha, char delta);
BOOLEAN Gal_get_alpha_value(unsigned char *alpha, char *delta);
BOOLEAN Gal_set_alpha_priority(int priority);
BOOLEAN Gal_get_alpha_priority(int *priority);
BOOLEAN Gal_set_alpha_color(unsigned long color);
BOOLEAN Gal_get_alpha_color(unsigned long *color);
BOOLEAN Gal_select_alpha_region(int region);
BOOLEAN Gal_set_video_outside_alpha(int enable);
BOOLEAN Gal_set_video_palette(unsigned long *palette);

/* Icon related prototypes */

BOOLEAN Gal_set_icon_enable(int enable);
BOOLEAN Gal_set_icon_colors(unsigned long color0, unsigned long color1,
                            unsigned long color2);

BOOLEAN Gal_set_icon_position(unsigned long memoffset, unsigned short xpos);
BOOLEAN Gal_set_icon_shape64(unsigned long memoffset, unsigned long *andmask,
                             unsigned long *xormask, unsigned int lines);

/* Icon related prototypes */

BOOLEAN Gal_set_vip_enable(int enable);
BOOLEAN Gal_get_vip_enable(int *enable);
BOOLEAN Gal_set_vip_capture_run_mode(int mode);
BOOLEAN Gal_set_vip_base(unsigned long even, unsigned long odd);
BOOLEAN Gal_get_vip_base(unsigned long *address, int odd);
BOOLEAN Gal_set_vip_pitch(unsigned long pitch);
BOOLEAN Gal_get_vip_pitch(unsigned long *pitch);
BOOLEAN Gal_set_vip_mode(int mode);
BOOLEAN Gal_get_vip_mode(int *mode);
BOOLEAN Gal_set_vbi_enable(int enable);
BOOLEAN Gal_get_vbi_enable(int *enable);
BOOLEAN Gal_set_vbi_mode(int mode);
BOOLEAN Gal_get_vbi_mode(int *mode);
BOOLEAN Gal_set_vbi_base(unsigned long even, unsigned long odd);
BOOLEAN Gal_get_vbi_base(unsigned long *address, int odd);
BOOLEAN Gal_set_vbi_pitch(unsigned long pitch);
BOOLEAN Gal_get_vbi_pitch(unsigned long *pitch);
BOOLEAN Gal_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines);
BOOLEAN Gal_get_vbi_direct(int odd, unsigned long *vbi_direct);
BOOLEAN Gal_set_vbi_interrupt(int enable);
BOOLEAN Gal_get_vbi_interrupt(int *enable);
BOOLEAN Gal_set_vip_bus_request_threshold_high(int enable);
BOOLEAN Gal_get_vip_bus_request_threshold_high(int *enable);
BOOLEAN Gal_set_vip_last_line(int last_line);
BOOLEAN Gal_test_vip_odd_field(int *status);
BOOLEAN Gal_test_vip_bases_updated(int *status);
BOOLEAN Gal_test_vip_fifo_overflow(int *status);
BOOLEAN Gal_get_vip_line(int *status);

/* Second generation rendering routines  */

BOOLEAN Gal_set_source_stride(unsigned short stride);
BOOLEAN Gal_set_destination_stride(unsigned short stride);
BOOLEAN Gal_set_source_transparency(unsigned long color, unsigned long mask);
BOOLEAN Gal2_set_source_transparency(unsigned long color, unsigned long mask);
BOOLEAN Gal2_set_source_stride(unsigned short stride);
BOOLEAN Gal2_set_destination_stride(unsigned short stride);
BOOLEAN Gal2_set_pattern_origin(int x, int y);
BOOLEAN Gal_set_alpha_mode(int mode);
BOOLEAN Gal2_set_alpha_value(unsigned char value);
BOOLEAN Gal2_pattern_fill(unsigned long dstoffset, unsigned short width,
                          unsigned short height);
BOOLEAN Gal2_color_pattern_fill(unsigned long dstoffset, unsigned short width,
                                unsigned short height, unsigned long pattern);
BOOLEAN Gal2_screen_to_screen_blt(unsigned long srcoffset,
                                  unsigned long dstoffset,
                                  unsigned short width, unsigned short height,
                                  int flags);

BOOLEAN Gal2_mono_expand_blt(unsigned long srcbase, unsigned short srcx,
                             unsigned short srcy, unsigned long dstoffset,
                             unsigned short width, unsigned short height,
                             int byte_packed);

BOOLEAN Gal2_color_bitmap_to_screen_blt(unsigned short srcx,
                                        unsigned short srcy,
                                        unsigned long dstoffset,
                                        unsigned short width,
                                        unsigned short height,
                                        unsigned char *data,
                                        unsigned short pitch);
BOOLEAN Gal2_mono_bitmap_to_screen_blt(unsigned short srcx,
                                       unsigned short srcy,
                                       unsigned long dstoffset,
                                       unsigned short width,
                                       unsigned short height,
                                       unsigned char *data,
                                       unsigned short pitch);

BOOLEAN Gal2_text_blt(unsigned long dstoffset,
                      unsigned short width,
                      unsigned short height, unsigned long data);
BOOLEAN Gal2_bresenham_line(unsigned long dstoffset,
                            unsigned short length, unsigned short initerr,
                            unsigned short axialerr, unsigned short diagerr,
                            unsigned short flags);
BOOLEAN Gal2_sync_to_vblank(void);

/* Video routines */

BOOLEAN Gal_set_video_yuv_pitch(unsigned long ypitch, unsigned long uvpitch);
BOOLEAN Gal_get_video_yuv_pitch(unsigned long *ypitch,
                                unsigned long *uvpitch);

BOOLEAN Gal_set_video_yuv_offsets(unsigned long yoffset,
                                  unsigned long uoffset,
                                  unsigned long voffset);
BOOLEAN Gal_get_video_yuv_offsets(unsigned long *yoffset,
                                  unsigned long *uoffset,
                                  unsigned long *voffset);

BOOLEAN Gal_set_video_left_crop(unsigned short x);
BOOLEAN Gal_set_video_vertical_downscale(unsigned short srch,
                                         unsigned short dsth);

BOOLEAN Gal_set_vbi_source(VbiSourceType source);
BOOLEAN Gal_get_vbi_source(VbiSourceType * source);

BOOLEAN Gal_set_vbi_lines(unsigned long even, unsigned long odd);
BOOLEAN Gal_get_vbi_lines(int odd, unsigned long *lines);

BOOLEAN Gal_set_vbi_total(unsigned long even, unsigned long odd);
BOOLEAN Gal_get_vbi_total(int odd, unsigned long *total);

BOOLEAN Gal_set_vertical_scaler_offset(char offset);
BOOLEAN Gal_get_vertical_scaler_offset(char *offset);
BOOLEAN Gal_get_genlock_enable(int *enable);
BOOLEAN Gal_set_genlock_enable(int flags);
BOOLEAN Gal_get_genlock_delay(unsigned long *delay);
BOOLEAN Gal_set_genlock_delay(unsigned long delay);
BOOLEAN Gal_set_top_line_in_odd(int enable);

BOOLEAN Gal_read_crc(unsigned long *crc);
BOOLEAN Gal_read_window_crc(int source, unsigned short x, unsigned short y,
                            unsigned short width, unsigned short height,
                            int crc32, unsigned long *crc);

BOOLEAN Gal_set_macrovision_enable(int enable);
BOOLEAN Gal_get_macrovision_enable(int *enable);

/* MSR routines */

BOOLEAN Gal_id_msr_dev_address(MSR * pDev, unsigned long address);
BOOLEAN Gal_get_msr_dev_address(unsigned int device, unsigned long *address);

#endif
