#ifdef SH7722_DEBUG
#define DIRECT_FORCE_DEBUG
#endif


#include <config.h>

#include <asm/types.h>

#include <direct/debug.h>

#include <misc/conf.h>

#include "sh7722.h"


D_DEBUG_DOMAIN( SH7722_LCD, "SH7722/LCD", "Renesas SH7722 LCD" );

/**********************************************************************************************************************/


void
sh7722_lcd_setup( void                  *drv,
                  int                    width,
                  int                    height,
                  ulong                  phys,
                  int                    pitch,
                  DFBSurfacePixelFormat  format,
                  bool                   swap )
{
     u32 MLDDFR = 0;
     u32 LDDDSR = 0;

     D_DEBUG_AT( SH7722_LCD, "%s( %dx%d @%lu:%d )\n", __FUNCTION__, width, height, phys, pitch );

     D_ASSERT( width  > 7 );
     D_ASSERT( height > 0 );

     D_ASSERT( (phys & 7) == 0 );

     D_ASSERT( pitch > 0 );
     D_ASSERT( pitch < 0x10000 );
     D_ASSERT( (pitch & 3) == 0 );

     /* Choose input format. */
     switch (format) {
          case DSPF_RGB32:
          case DSPF_ARGB:
               MLDDFR = 0;
               break;

          case DSPF_RGB16:
               MLDDFR = 3;
               break;

          case DSPF_RGB444:
          case DSPF_ARGB4444:
               MLDDFR = 8;
               break;

          case DSPF_RGB24:
               MLDDFR = 11;
               break;

          case DSPF_NV12:
               MLDDFR = 0x10000;
               break;

          case DSPF_NV16:
               MLDDFR = 0x10100;
               break;

          default:
               D_BUG( "invalid format" );
               return;
     }

     /* Setup swapping. */
     switch (format) {
          case DSPF_NV12:     /* 1 byte */
          case DSPF_NV16:
          case DSPF_RGB24:
               LDDDSR = 7;
               break;

          case DSPF_RGB16:    /* 2 byte */
          case DSPF_RGB444:
          case DSPF_ARGB4444:
               LDDDSR = 6;
               break;

          case DSPF_RGB32:    /* 4 byte */
          case DSPF_ARGB:
               LDDDSR = 4;
               break;

          default:
               D_BUG( "invalid format" );
               return;
     }

     SH7722_SETREG32( drv, LCDC_MLDDCKPAT1R,  0x05555555 );
     SH7722_SETREG32( drv, LCDC_MLDDCKPAT2R,  0x55555555 );
     SH7722_SETREG32( drv, LCDC_LDDCKR,       0x0000003c );
     SH7722_SETREG32( drv, LCDC_MLDMT2R,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_MLDMT3R,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_MLDDFR,       MLDDFR );
     SH7722_SETREG32( drv, LCDC_MLDSM1R,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_MLDSM2R,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_MLDSA1R,	  phys );
     SH7722_SETREG32( drv, LCDC_MLDSA2R,	  phys + pitch * height );
     SH7722_SETREG32( drv, LCDC_MLDMLSR,      pitch );
     SH7722_SETREG32( drv, LCDC_MLDWBCNTR,    0x00000000 );
     SH7722_SETREG32( drv, LCDC_MLDWBAR,      0x00000000 );
#if 0
     SH7722_SETREG32( drv, LCDC_MLDMT1R,      0x18000006 );
     SH7722_SETREG32( drv, LCDC_MLDHCNR,      ((width / 8) << 16) | (1056 / 8) );
     SH7722_SETREG32( drv, LCDC_MLDHSYNR,     ((128 / 8) << 16) | (840 / 8) );
     SH7722_SETREG32( drv, LCDC_MLDVLNR,      (height << 16) | 525 );
     SH7722_SETREG32( drv, LCDC_MLDVSYNR,     (2 << 16) | 490 );
     SH7722_SETREG32( drv, LCDC_MLDPMR,       0xf6000f00 );
#elif 0
     SH7722_SETREG32( drv, LCDC_MLDMT1R,      0x1c00000a );
     SH7722_SETREG32( drv, LCDC_MLDHCNR,      0x00500060);
     SH7722_SETREG32( drv, LCDC_MLDHSYNR,     0x00010052);
     SH7722_SETREG32( drv, LCDC_MLDVLNR,      0x01e00200);
     SH7722_SETREG32( drv, LCDC_MLDVSYNR,     0x000301f0);
     SH7722_SETREG32( drv, LCDC_MLDPMR,       0x00000000 );	//igel
#elif defined(SH7722_ALGO_PANEL)
     SH7722_SETREG32( drv, LCDC_MLDMT1R,      0x1c00000a );
     SH7722_SETREG32( drv, LCDC_MLDHCNR,      0x00500060);
     SH7722_SETREG32( drv, LCDC_MLDHSYNR,     0x00010052);
     SH7722_SETREG32( drv, LCDC_MLDVLNR,      0x01e0020e);
     SH7722_SETREG32( drv, LCDC_MLDVSYNR,     0x000301f0);
     SH7722_SETREG32( drv, LCDC_MLDPMR,       0x00000000 );	//igel
#elif defined(ALGO_AP325)
     SH7722_SETREG32( drv, LCDC_MLDMT1R,      0x1800000a );
     SH7722_SETREG32( drv, LCDC_MLDHCNR,      ((width / 8) << 16) | (1000 / 8) );
     SH7722_SETREG32( drv, LCDC_MLDHSYNR,     ((8 / 8) << 16) | (960 / 8) );
     SH7722_SETREG32( drv, LCDC_MLDVLNR,      (height << 16) | 624 );
     SH7722_SETREG32( drv, LCDC_MLDVSYNR,     (1 << 16) | 560 );
     SH7722_SETREG32( drv, LCDC_MLDPMR,       0xf6000f00 );
#endif
     SH7722_SETREG32( drv, LCDC_LDINTR,       0x00000000 );
     SH7722_SETREG32( drv, LCDC_LDRCNTR,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_LDDDSR,       swap ? LDDDSR : 0 );
     SH7722_SETREG32( drv, LCDC_LDRCR,        0x00000000 );
     SH7722_SETREG32( drv, LCDC_LDPALCR,      0x00000000 );
     SH7722_SETREG32( drv, LCDC_LDCNT1R,      0x00000001 );
     SH7722_SETREG32( drv, LCDC_LDCNT2R,      0x00000003 );
}

