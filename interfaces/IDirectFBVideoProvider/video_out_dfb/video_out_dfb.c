/*
 * Copyright (C) 2004 Claudio "KLaN" Ciccani <klan82@cheapnet.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  video_out_dfb: unofficial xine video output driver using DirectFB
 *
 *
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>

#include <xine.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>

#include "video_out_dfb.h"



#define V_RED_FACTOR    22970
#define V_GREEN_FACTOR  11700
#define U_GREEN_FACTOR   5638
#define U_BLUE_FACTOR   29032



static struct
{
     uint8_t y[256];  /* luma    */
     uint8_t cr[256]; /* chroma  */
     int16_t vr[256]; /* v red   */
     int16_t vg[256]; /* v green */
     int16_t ug[256]; /* u green */
     int16_t ub[256]; /* u blue  */

} __attribute__ ((aligned( 2 ))) dm_table;


#define GEN_Y() \
{\
     int y;\
     y  = ((i + brightness) * contrast) >> 7;\
     dm_table.y[i] = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);\
}

#define GEN_CR() \
{\
     int cr;\
     cr = (((i - 128) * saturation) >> 7) + 128;\
     dm_table.cr[i] = (cr < 0) ? 0 : ((cr > 0xff) ? 0xff : cr);\
}

#define GEN_VR() \
{\
     dm_table.vr[i] = ((dm_table.cr[i] - 128) * V_RED_FACTOR) >> 14;\
}

#define GEN_VG() \
{\
     dm_table.vg[i] = ((dm_table.cr[i] - 128) * -V_GREEN_FACTOR) >> 14;\
}

#define GEN_UG() \
{\
     dm_table.ug[i] = ((dm_table.cr[i] - 128) * -U_GREEN_FACTOR) >> 14;\
}

#define GEN_UB() \
{\
     dm_table.ub[i] = ((dm_table.cr[i] - 128) * U_BLUE_FACTOR) >> 14;\
}


#include "video_out_dfb_blend.h"
#ifdef ARCH_X86
# include "video_out_dfb_mmx.h"
#endif


#define PACCEL  dummy

/* initialize chroma components */
#define LOADCR( ui, vi ) \
{\
     register int u = ui;\
     register int v = vi;\
     cr1 = dm_table.vr[v];\
     cr2 = dm_table.vg[v] + dm_table.ug[u];\
     cr3 = dm_table.ub[u];\
}

#define YUV2RGB( yi ) \
{\
     register int y;\
     y = dm_table.y[yi];\
     r = y + cr1;\
     g = y + cr2;\
     b = y + cr3;\
}

#define CLAMP2RGB8() \
{\
     r = (r < 0) ? 0 : ((r > 0xff) ? 0xe0 : (r & 0xe0));\
     g = (g < 0) ? 0 : ((g > 0xff) ? 0x1c : ((g & 0xe0) >> 3));\
     b = (b < 0) ? 0 : ((b > 0xff) ? 0x03 : (b >> 6));\
}

#define CLAMP2RGB15() \
{\
     r = (r < 0) ? 0 : ((r > 0xff) ? 0x7c00 : ((r & 0xf8) << 7));\
     g = (g < 0) ? 0 : ((g > 0xff) ? 0x03e0 : ((g & 0xf8) << 2));\
     b = (b < 0) ? 0 : ((b > 0xff) ? 0x001f : (b >> 3));\
}

#define CLAMP2RGB16() \
{\
     r = (r < 0) ? 0 : ((r > 0xff) ? 0xf800 : ((r & 0xf8) << 8));\
     g = (g < 0) ? 0 : ((g > 0xff) ? 0x07e0 : ((g & 0xfc) << 3));\
     b = (b < 0) ? 0 : ((b > 0xff) ? 0x001f : (b >> 3));\
}

#define CLAMP2RGB24() \
{\
     r = (r < 0) ? 0 : ((r > 0xff) ? 0xff : r);\
     g = (g < 0) ? 0 : ((g > 0xff) ? 0xff : g);\
     b = (b < 0) ? 0 : ((b > 0xff) ? 0xff : b);\
}

#define CLAMP2RGB32() \
{\
     r = (r < 0) ? 0 : ((r > 0xff) ? 0x00ff0000 : (r << 16));\
     g = (g < 0) ? 0 : ((g > 0xff) ? 0x0000ff00 : (g << 8));\
     b = (b < 0) ? 0 : ((b > 0xff) ? 0x000000ff : b);\
}


/******************************** Begin YUY2 callbacks **********************************/

static
DFB_PFUNCTION( yuy2, yuy2 )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 2;
     int      mmf = this->mixer.modified;

     do
     {
          register uint32_t pix;

          if (mmf & MMF_S)
          {
               pix  = dm_table.cr[*(src + 1)] << 8;
               pix |= dm_table.cr[*(src + 3)] << 24;
          } else
               pix  = *((uint32_t*) src) & 0xff00ff00;

          if (mmf & (MMF_B | MMF_C))
          {
               pix |= dm_table.y[*src];
               pix |= dm_table.y[*(src + 2)] << 16;
          } else
          {
               pix |= *src;
               pix |= *(src + 2) << 16;
          }

          *((uint32_t*) dst) = pix;


          if (mmf & MMF_S)
          {
               pix  = dm_table.cr[*(src + 5)] << 8;
               pix |= dm_table.cr[*(src + 7)] << 24;
          } else
               pix  = *((uint32_t*) (src + 4)) & 0xff00ff00;

          if (mmf & (MMF_B | MMF_C))
          {
               pix |= dm_table.y[*(src + 4)];
               pix |= dm_table.y[*(src + 6)] << 16;
          } else
          {
               pix |= *(src + 4);
               pix |= *(src + 6) << 16;
          }

          *((uint32_t*) (dst + 4)) = pix;


          dst += 8;
          src += 8;

     } while (--n);
}


static
DFB_PFUNCTION( yuy2, uyvy )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 2;
     int      mmf = this->mixer.modified;

     do
     {
          register uint32_t pix;

          if (mmf & MMF_S)
          {
               pix  = dm_table.cr[*(src + 1)];
               pix |= dm_table.cr[*(src + 3)] << 16;
          } else
               pix  = (*((uint32_t*) src) >> 8) & 0xff00ff;
          
          if (mmf & (MMF_B | MMF_C))
          {
               pix |= dm_table.y[*src] << 8;
               pix |= dm_table.y[*(src + 2)] << 24;
          } else
          {
               pix |= *src << 8;
               pix |= *(src + 2) << 24;
          }

          *((uint32_t*) dst) = pix;

          
          if (mmf & MMF_S)
          {
               pix  = dm_table.cr[*(src + 5)];
               pix |= dm_table.cr[*(src + 7)] << 16;
          } else
               pix  = (*((uint32_t*) (src + 4)) >> 8) & 0xff00ff;

          if (mmf & (MMF_B | MMF_C))
          {
               pix |= dm_table.y[*(src + 4)] << 8;
               pix |= dm_table.y[*(src + 6)] << 24;
          } else
          {
               pix |= *(src + 4) << 8;
               pix |= *(src + 6) << 24;
          }

          *((uint32_t*) (dst + 4)) = pix;

          
          dst += 8;
          src += 8;

     } while (--n);
}


static
DFB_PFUNCTION( yuy2, yv12 )
{
     uint8_t *src  = frame->vo_frame.base[0];
     uint8_t *dsty = dst;
     uint8_t *dstu;
     uint8_t *dstv;
     int      line = frame->width.cur >> 2;
     int      n    = (frame->width.cur * frame->height.cur) >> 3;
     int      p    = frame->vo_frame.pitches[0];
     int      mmf  = this->mixer.modified;

     if (frame->dstfmt.cur == DSPF_YV12)
     {
          dstv = (uint8_t*) dsty + (pitch * frame->height.cur);
          dstu = (uint8_t*) dstv + ((pitch * frame->height.cur) >> 2);
     } else
     {
          dstu = (uint8_t*) dsty + (pitch * frame->height.cur);
          dstv = (uint8_t*) dstu + ((pitch * frame->height.cur) >> 2);
     }

     do
     {
          register uint32_t chunk;

          if (mmf & (MMF_B | MMF_C))
          {
               chunk  = dm_table.y[*src];
               chunk |= dm_table.y[*(src + 2)] << 8;
               chunk |= dm_table.y[*(src + 4)] << 16;
               chunk |= dm_table.y[*(src + 6)] << 24;

               *((uint32_t*) dsty) = chunk;

               chunk  = dm_table.y[*(src + p)];
               chunk |= dm_table.y[*(src + p + 2)] << 8;
               chunk |= dm_table.y[*(src + p + 4)] << 16;
               chunk |= dm_table.y[*(src + p + 6)] << 24;

               *((uint32_t*) (dsty + pitch)) = chunk;
          } else
          {
               chunk  = *src;
               chunk |= *(src + 2) << 8;
               chunk |= *(src + 4) << 16;
               chunk |= *(src + 6) << 24;

               *((uint32_t*) dsty) = chunk;

               chunk  = *(src + p);
               chunk |= *(src + p + 2) << 8;
               chunk |= *(src + p + 4) << 16;
               chunk |= *(src + p + 6) << 24;

               *((uint32_t*) (dsty + pitch)) = chunk;
          }

          if (mmf & MMF_S)
          {
               register uint32_t cr;
               
               cr          = (*(src + 1)    +
                              *(src + p + 1)) >> 1;
               *dstu       = dm_table.cr[cr];


               cr          = (*(src + 5)   +
                                       *(src + p + 5)) >> 1;
               *(dstu + 1) = dm_table.cr[cr];

               cr          = (*(src + 3)   +
                              *(src + p + 3)) >> 1;
               *dstv       = dm_table.cr[cr];

               cr          = (*(src + 7)   +
                              *(src + p + 7)) >> 1;
               *(dstv + 1) = dm_table.cr[cr];
          } else
          {
               *dstu       = (*(src + 1)   +
                                 *(src + p + 1)) >> 1;

               *(dstu + 1) = (*(src + 5)   +
                                 *(src + p + 5)) >> 1;

               *dstv       = (*(src + 3)   +
                              *(src + p + 3)) >> 1;

               *(dstv + 1) = (*(src + 7)   +
                              *(src + p + 7)) >> 1;
          }

          src  += 8;
          dsty += 4;
          dstu += 2;
          dstv += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 2;
               src  += p;
               dsty += pitch;
          }

     } while (--n);
}


static
DFB_PFUNCTION( yuy2, rgb8 )
{
     uint8_t  *src = frame->vo_frame.base[0];
     int       n   = (frame->width.cur * frame->height.cur) >> 1;
     int       cr1, cr2, cr3;
     int       r, g, b;

     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB8();

          *dst = (r | g | b);

          YUV2RGB( *(src + 2) );
          CLAMP2RGB8();

          *(dst + 1) = (r | g | b);

          dst += 2;
          src += 4;

     } while (--n);
}


static
DFB_PFUNCTION( yuy2, rgb15 )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 1;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB15();

          *((uint16_t*) dst)  = (r | g | b);

          YUV2RGB( *(src + 2) );
          CLAMP2RGB15();

          *((uint16_t*) (dst + 2)) = (r | g | b);

          dst += 4;
          src += 4;

     } while (--n);
}


static
DFB_PFUNCTION( yuy2, rgb16 )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 1;
     int      cr1, cr2, cr3;
     int      r, g, b;
     
     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB16();

          *((uint16_t*) dst) = (r | g | b);

          YUV2RGB( *(src + 2) );
          CLAMP2RGB16();

          *((uint16_t*) (dst + 2)) = (r | g | b);

          dst += 4;
          src += 4;

     } while (--n);
}


/* unrolling here seems to speed up */
static
DFB_PFUNCTION( yuy2, rgb24 )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB24();

          *dst       = b;
          *(dst + 1) = g;
          *(dst + 2) = r;

          YUV2RGB( *(src + 2) );
          CLAMP2RGB24();

          *(dst + 3) = b;
          *(dst + 4) = g;
          *(dst + 5) = r;

          LOADCR( *(src + 5), *(src + 7) );

          YUV2RGB( *(src + 4) );
          CLAMP2RGB24();

          *(dst + 6) = b;
          *(dst + 7) = g;
          *(dst + 8) = r;

          YUV2RGB( *(src + 6) );
          CLAMP2RGB24();

          *(dst + 9)  = b;
          *(dst + 10) = g;
          *(dst + 11) = r;

          dst += 12;
          src += 8;

     } while (--n);
}


/* unrolling here seems to speed up */
static
DFB_PFUNCTION( yuy2, rgb32 )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB32();

          *((uint32_t*) dst) = (r | g | b);

          YUV2RGB( *(src + 2) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 4)) = (r | g | b);

          LOADCR( *(src + 5), *(src + 7) );

          YUV2RGB( *(src + 4) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 8)) = (r | g | b);

          YUV2RGB( *(src + 6) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 12)) = (r | g | b);

          dst += 16;
          src += 8;

     } while (--n);
}


/* unrolling here seems to speed up */
static
DFB_PFUNCTION( yuy2, argb )
{
     uint8_t *src = frame->vo_frame.base[0];
     int      n   = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *(src + 1), *(src + 3) );

          YUV2RGB( *src );
          CLAMP2RGB32();

          *((uint32_t*) dst) = (0xff000000 | r | g | b);

          YUV2RGB( *(src + 2) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 4)) = (0xff000000 | r | g | b);

          LOADCR( *(src + 5), *(src + 7) );

          YUV2RGB( *(src + 4) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 8)) = (0xff000000 | r | g | b);

          YUV2RGB( *(src + 6) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 12)) = (0xff000000 | r | g | b);

          dst += 16;
          src += 8;

     } while (--n);
}


/******************************** End of YV12 callbacks *********************************/



/******************************** Begin YV12 callbacks **********************************/

static
DFB_PFUNCTION( yv12, yuy2 )
{
     uint8_t  *srcy = frame->vo_frame.base[0];
     uint8_t  *srcu = frame->vo_frame.base[1];
     uint8_t  *srcv = frame->vo_frame.base[2];
     int       yp   = frame->vo_frame.pitches[0];
     int       n    = frame->height.cur >> 1;
     int       mmf  = this->mixer.modified;
     int       i;

     do
     {
          for (i = 0; i < (frame->width.cur >> 1); i++)
          {
               register uint32_t pix;
               
               if (mmf & MMF_S)
               {
                    pix  = dm_table.cr[srcu[i]] << 8;
                    pix |= dm_table.cr[srcv[i]] << 24;
               } else
               {
                    pix  = srcu[i] << 8;
                    pix |= srcv[i] << 24;
               }
               
               if (mmf & (MMF_B | MMF_C))
               {
                    pix |= dm_table.y[*srcy];
                    pix |= dm_table.y[*(srcy + 1)] << 16;
               } else
               {
                    pix |= *srcy;
                    pix |= *(srcy + 1) << 16;
               }

               *((uint32_t*) dst) = pix;

               pix &= 0xff00ff00;

               if (mmf & (MMF_B | MMF_C))
               {
                    pix |= dm_table.y[*(srcy + yp)];
                    pix |= dm_table.y[*(srcy + yp + 1)] << 16;
               } else
               {
                    pix |= *(srcy + yp);
                    pix |= *(srcy + yp + 1) << 16;
               }

               *((uint32_t*) (dst + pitch)) = pix;

               dst  += 4;
               srcy += 2;
          }

          dst  += pitch;
          srcy += yp;
          srcu += yp >> 1;
          srcv += yp >> 1;
     
     } while (--n);
}


static
DFB_PFUNCTION( yv12, uyvy )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      n    = frame->height.cur >> 1;
     int      mmf  = this->mixer.modified;
     int      i;

     do
     {
          for (i = 0; i < (frame->width.cur >> 1); i++)
          {
               register uint32_t pix;

               if (mmf & MMF_S)
               {
                    pix  = dm_table.cr[srcu[i]];
                    pix |= dm_table.cr[srcv[i]] << 16;
               } else
               {
                    pix  = srcu[i];
                    pix |= srcv[i] << 16;
               }

               if (mmf & (MMF_B | MMF_C))
               {
                    pix |= dm_table.y[*srcy] << 8;
                    pix |= dm_table.y[*(srcy + 1)] << 24;
               } else
               {
                    pix |= *srcy << 8;
                    pix |= *(srcy + 1) << 24;
               }

               *((uint32_t*) dst) = pix;

               pix &= 0x00ff00ff;
               
               if (mmf & (MMF_B | MMF_C))
               {
                    pix |= dm_table.y[*(srcy + yp)] << 8;
                    pix |= dm_table.y[*(srcy + yp + 1)] << 24;
               } else
               {
                    pix |= *(srcy + yp) << 8;
                    pix |= *(srcy + yp + 1) << 24;
               }
               
               *((uint32_t*) (dst + pitch)) = pix;

               dst  += 4;
               srcy += 2;
          }

          dst  += pitch;
          srcy += yp;
          srcu += yp >> 1;
          srcv += yp >> 1;
     
     } while (--n);
}


static
DFB_PFUNCTION( yv12, yv12 )
{
     uint8_t *srcy  = frame->vo_frame.base[0];
     uint8_t *srcu;
     uint8_t *srcv;
     uint8_t *dsty  = dst;
     uint8_t *dstu;
     uint8_t *dstv;
     int      ys    = pitch * frame->height.cur;
     int      crs   = (pitch * frame->height.cur) >> 2;
     int      mmf   = this->mixer.modified;

     if (frame->dstfmt.cur == DSPF_YV12)
     {
          srcu = frame->vo_frame.base[1];
          srcv = frame->vo_frame.base[2];
          dstv  = (uint8_t*) dsty + ys;
          dstu  = (uint8_t*) dstv + crs;
     } else
     {
          srcu = frame->vo_frame.base[2];
          srcv = frame->vo_frame.base[1];
          dstu  = (uint8_t*) dsty + ys;
          dstv  = (uint8_t*) dstu + crs;
     }

     if (mmf & (MMF_B | MMF_C))
     {
          do
          {
               *dsty++ = dm_table.y[*srcy++];

          } while (--ys);

     } else
          xine_fast_memcpy( dsty, srcy, ys );

     if (mmf && MMF_S)
     {
          do
          {
               *dstu++ = dm_table.cr[*srcu++];
               *dstv++ = dm_table.cr[*srcv++];

          } while (--crs);

     } else
     {
          xine_fast_memcpy( dstu, srcu, crs );
          xine_fast_memcpy( dstv, srcv, crs );
     }
}


static
DFB_PFUNCTION( yv12, rgb8 )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB8();

          *dst = (r | g | b);

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB8();

          *(dst + 1) = (r | g | b);

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB8();

          *(dst + pitch) = (r | g | b);

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB8();

          *(dst + pitch + 1) = (r | g | b);

          dst  += 2;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);

}


static
DFB_PFUNCTION( yv12, rgb15 )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB15();

          *((uint16_t*) dst)  = (r | g | b);

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB15();

          *((uint16_t*) (dst + 2)) = (r | g | b);

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB15();

          *((uint16_t*) (dst + pitch)) = (r | g | b);

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB15();

          *((uint16_t*) (dst + pitch + 2)) = (r | g | b);

          dst  += 4;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);
}


static
DFB_PFUNCTION( yv12, rgb16 )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB16();

          *((uint16_t*) dst) = (r | g | b);

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB16();

          *((uint16_t*) (dst + 2)) = (r | g | b);

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB16();

          *((uint16_t*) (dst + pitch)) = (r | g | b);

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB16();

          *((uint16_t*) (dst + pitch + 2)) = (r | g | b);

          dst  += 4;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);
}


static
DFB_PFUNCTION( yv12, rgb24 )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB24();

          *dst       = b;
          *(dst + 1) = g;
          *(dst + 2) = r;

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB24();

          *(dst + 3) = b;
          *(dst + 4) = g;
          *(dst + 5) = r;

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB24();

          *(dst + pitch)     = b;
          *(dst + pitch + 1) = g;
          *(dst + pitch + 2) = r;

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB24();

          *(dst + pitch + 3) = b;
          *(dst + pitch + 4) = g;
          *(dst + pitch + 5) = r;

          dst  += 6;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);
}


static
DFB_PFUNCTION( yv12, rgb32 )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB32();

          *((uint32_t*) dst) = (r | g | b);

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 4)) = (r | g | b);

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + pitch)) = (r | g | b);

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + pitch + 4)) = (r | g | b);

          dst  += 8;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);
}


static
DFB_PFUNCTION( yv12, argb )
{
     uint8_t *srcy = frame->vo_frame.base[0];
     uint8_t *srcu = frame->vo_frame.base[1];
     uint8_t *srcv = frame->vo_frame.base[2];
     int      yp   = frame->vo_frame.pitches[0];
     int      line = frame->width.cur >> 1;
     int      n    = (frame->width.cur * frame->height.cur) >> 2;
     int      cr1, cr2, cr3;
     int      r, g, b;

     do
     {
          LOADCR( *srcu++, *srcv++ );

          YUV2RGB( *srcy );
          CLAMP2RGB32();

          *((uint32_t*) dst) = (0xff000000 | r | g | b);

          YUV2RGB( *(srcy + 1) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + 4)) = (0xff000000 | r | g | b);

          YUV2RGB( *(srcy + yp) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + pitch)) = (0xff000000 | r | g | b);

          YUV2RGB( *(srcy + yp + 1) );
          CLAMP2RGB32();

          *((uint32_t*) (dst + pitch + 4)) = (0xff000000 | r | g | b);

          dst  += 8;
          srcy += 2;

          if (!(--line))
          {
               line  = frame->width.cur >> 1;
               srcy += yp;
               dst  += pitch;
          }

     } while (--n);
}


/******************************** End of YV12 callbacks *********************************/


static const DVProcFunc PACCEL[2][DFB_NUM_PIXELFORMATS] =
{
     /* YUY2 */
     {
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb15 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb16 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb24 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb32 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, argb ),
          NULL,
          DFB_PFUNCTION_NAME( PACCEL, yuy2, yuy2 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb8 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, uyvy ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, yv12 ),
          DFB_PFUNCTION_NAME( PACCEL, yuy2, yv12 ),
          NULL,
          NULL,
          DFB_PFUNCTION_NAME( PACCEL, yuy2, rgb32 )
     },
     /* YV12 */
     {
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb15 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb16 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb24 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb32 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, argb ),
          NULL,
          DFB_PFUNCTION_NAME( PACCEL, yv12, yuy2 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb8 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, uyvy ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, yv12 ),
          DFB_PFUNCTION_NAME( PACCEL, yv12, yv12 ),
          NULL,
          NULL,
          DFB_PFUNCTION_NAME( PACCEL, yv12, rgb32 )
     }
};


#undef PACCEL





static uint32_t
dfb_get_capabilities( vo_driver_t *vo_driver )
{
     return (VO_CAP_YV12 | VO_CAP_YUY2);
}


static void
dfb_proc_frame( vo_frame_t *vo_frame )
{
     dfb_frame_t *frame = (dfb_frame_t*) vo_frame;

     if (!frame->surface)
          return;

     vo_frame->proc_called = 1;

     if (frame->proc_needed)
     {
          uint8_t  *dst;
          uint32_t  pitch;

          dst   = (uint8_t*) frame->surface->back_buffer->system.addr;
          pitch = (uint32_t) frame->surface->back_buffer->system.pitch;

          if (frame->procf)
               SPEED( frame->procf( (dfb_driver_t*) vo_frame->driver,
                                     frame, dst, pitch ) );
     }
}


static void
dfb_frame_dispose( vo_frame_t *vo_frame )
{
     dfb_frame_t *frame = (dfb_frame_t*) vo_frame;

     if (frame)
     {
          if (frame->surface)
               dfb_surface_unref( frame->surface );
          release( frame->chunks[0] );
          release( frame->chunks[1] );
          release( frame->chunks[2] );
          free( frame );
     }
}


static vo_frame_t*
dfb_alloc_frame( vo_driver_t *vo_driver )
{
     dfb_frame_t *frame = NULL;

     frame = (dfb_frame_t*) calloc( 1, sizeof(dfb_frame_t) );
     if (!frame)
          return NULL;

     pthread_mutex_init( &frame->vo_frame.mutex, NULL );

     frame->vo_frame.proc_slice = NULL;
     frame->vo_frame.proc_frame = dfb_proc_frame;
     frame->vo_frame.field      = NULL;
     frame->vo_frame.dispose    = dfb_frame_dispose;
     frame->vo_frame.driver     = vo_driver;

     return (vo_frame_t*) frame;
}


static void
dfb_update_frame_format( vo_driver_t *vo_driver,
                         vo_frame_t  *vo_frame,
                         uint32_t     width,
                         uint32_t     height,
                         double       ratio,
                         int          format,
                         int          flags )
{
     dfb_driver_t *this   = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame  = (dfb_frame_t*) vo_frame;
     int           bsz[3] = {0, 0, 0}; 
     int           sfmt;
     int           dfmt;

     if (!this->dest)
          goto failure;
     
     if( !this->dest_data)
          goto failure;

     frame->width.prev  = frame->width.cur;
     frame->height.prev = frame->height.cur;
     frame->imgfmt.prev = frame->imgfmt.cur;
     frame->dstfmt.prev = frame->dstfmt.cur;

     frame->width.cur   = (width + 3) & ~3;
     frame->height.cur  = (height + 1) & ~1;
     frame->imgfmt.cur  = format;
     frame->dstfmt.cur  = this->dest_data->surface->format;
     frame->ratio       = ratio;
     frame->proc_needed = 1;

     if (frame->width.cur  != frame->width.prev  ||
         frame->height.cur != frame->height.prev ||
         frame->imgfmt.cur != frame->imgfmt.prev ||
         frame->dstfmt.cur != frame->dstfmt.prev)
     {
          if (frame->surface)
               dfb_surface_reformat( NULL, frame->surface, frame->width.cur,
                               frame->height.cur, frame->dstfmt.cur );

          release( frame->chunks[0] );
          release( frame->chunks[1] );
          release( frame->chunks[2] );
     }

     if (!frame->surface)
     {
          dfb_surface_create( NULL, frame->width.cur, frame->height.cur,
                        frame->dstfmt.cur, CSP_SYSTEMONLY,
                        DSCAPS_SYSTEMONLY, NULL, &frame->surface );
          if (!frame->surface)
               goto failure;
     }

     if (frame->imgfmt.cur == XINE_IMGFMT_YUY2)
     {
          ONESHOT( "video frame format is YUY2" );

          sfmt = 0;
          vo_frame->pitches[0] = frame->width.cur << 1;

          if (frame->dstfmt.cur == DSPF_YUY2 &&
              this->mixer.modified == 0)
          {
               frame->proc_needed = 0;
               vo_frame->base[0]  = (uint8_t*)
                         frame->surface->back_buffer->system.addr;
          } else
               bsz[0] = frame->vo_frame.pitches[0] * frame->height.cur;
               
     } else /* assume XINE_IMGFMT_YV12 */
     {
          ONESHOT( "video frame format is YV12" );

          sfmt = 1;
          vo_frame->pitches[0] = frame->width.cur;
          vo_frame->pitches[1] = frame->width.cur >> 1;
          vo_frame->pitches[2] = frame->width.cur >> 1;

          if ((frame->dstfmt.cur == DSPF_YV12  ||
               frame->dstfmt.cur == DSPF_I420) &&
              this->mixer.modified == 0)
          {
               frame->proc_needed = 0;
               vo_frame->base[0]  = (uint8_t*)
                         frame->surface->back_buffer->system.addr;
               vo_frame->base[2]  = (uint8_t*) vo_frame->base[0] +
                         (frame->width.cur * frame->height.cur);
               vo_frame->base[1]  = (uint8_t*) vo_frame->base[2] +
                         ((frame->width.cur * frame->height.cur) >> 2);

               if (frame->dstfmt.cur == DSPF_I420)
               {
                    uint8_t *tmp;
                    tmp               = vo_frame->base[1];
                    vo_frame->base[1] = vo_frame->base[2];
                    vo_frame->base[2] = tmp;
               }
          } else
          {
               bsz[0] = vo_frame->pitches[0] * frame->height.cur;
               bsz[1] = vo_frame->pitches[1] * (frame->height.cur >> 1) + 2;
               bsz[2] = vo_frame->pitches[2] * (frame->height.cur >> 1) + 2;
          }
     }

     if (!frame->chunks[0] && bsz[0]) 
     {
          vo_frame->base[0] = (uint8_t*)
                    xine_xmalloc_aligned( 16, bsz[0], &frame->chunks[0] );
          if (!vo_frame->base[0])
               goto failure;
     }
     
     if (!frame->chunks[1] && bsz[1])
     {
          vo_frame->base[1] = (uint8_t*)
                    xine_xmalloc_aligned( 16, bsz[1], &frame->chunks[1] );
          if (!vo_frame->base[1])
               goto failure;
     }

     if (!frame->chunks[2] && bsz[2])
     {
          vo_frame->base[2] = (uint8_t*)
                    xine_xmalloc_aligned( 16, bsz[2], &frame->chunks[2] );
          if (!vo_frame->base[2])
               goto failure;
     }

     dfmt         = DFB_PIXELFORMAT_INDEX( frame->dstfmt.cur );
     frame->procf = this->proc.funcs[sfmt][dfmt];

     return;

failure:
     if (frame->surface)
     {
          dfb_surface_unref( frame->surface );
          frame->surface = NULL;
     }
}


static void
dfb_overlay_blend( vo_driver_t  *vo_driver,
                   vo_frame_t   *vo_frame,
                   vo_overlay_t *overlay )
{
     dfb_frame_t *frame = (dfb_frame_t*) vo_frame;

     if (!frame->surface)
          return;

     if (!overlay->rle)
          return;

     if (overlay->x < 0 || overlay->x > frame->width.cur)
          return;

     if (overlay->y < 0 || overlay->y > frame->height.cur)
          return;
          
     switch (frame->dstfmt.cur)
     {
          case DSPF_YUY2:
               dfb_overlay_blend_yuy2( frame, overlay );
          break;

          case DSPF_UYVY:
               dfb_overlay_blend_uyvy( frame, overlay );
          break;

          case DSPF_YV12:
           case DSPF_I420:
               dfb_overlay_blend_yv12( frame, overlay );
          break;

          default:
               dfb_overlay_blend_rgb( frame, overlay );
          break;
     }
}


static int
dfb_redraw_needed( vo_driver_t *vo_driver )
{
     return 0;
}


static void
dfb_display_frame( vo_driver_t *vo_driver,
                   vo_frame_t  *vo_frame )
{
     dfb_driver_t *this      = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame     = (dfb_frame_t*) vo_frame;
     DFBRectangle  rect      = {0, 0, 0, 0};
     DFBRectangle  src_rect  = {0, 0, };
     DFBRectangle  dst_rect;

     if (!frame->surface)
          goto failure;

     src_rect.w = frame->vo_frame.width;
     src_rect.h = frame->vo_frame.height;

     this->output_cb( this->output_cdata, frame->vo_frame.width,
                frame->vo_frame.height, frame->ratio, &rect );

     dst_rect    = rect;
     dst_rect.x += this->dest_data->area.wanted.x;
     dst_rect.y += this->dest_data->area.wanted.y;

     if (dst_rect.w < 1 || dst_rect.h < 1)
          rect = dst_rect = this->dest_data->area.wanted;

     if (!dfb_rectangle_intersect( &dst_rect,
                    &this->dest_data->area.current ))
          goto failure;
   
     this->state.clip.x1   = dst_rect.x;
     this->state.clip.x2   = dst_rect.x + dst_rect.w - 1;
     this->state.clip.y1   = dst_rect.y;
     this->state.clip.y2   = dst_rect.y + dst_rect.h - 1;
     this->state.source    = frame->surface;
     this->state.modified |= (SMF_CLIP | SMF_SOURCE);

     if (dst_rect.w == src_rect.w && dst_rect.h == src_rect.h)
          dfb_gfxcard_blit( &src_rect, dst_rect.x,
                      dst_rect.y, &this->state );
     else
          dfb_gfxcard_stretchblit( &src_rect, &dst_rect, &this->state );

     if (this->frame_cb)
     {
          this->frame_cb( this->frame_cdata );
     } else
     if (this->dest_data->caps & DSCAPS_FLIPPING)
     {
          DFBRegion reg = 
          {
               .x1 = rect.x,
               .x2 = rect.x + rect.w,
               .y1 = rect.y,
               .y2 = rect.y + rect.h
          };
          
          this->dest->Flip( this->dest, &reg, 0 );
     }

failure:
     vo_frame->free( vo_frame );
}


static void
dfb_tables_regen( dfb_driver_t *this,
                  int           flags )
{
     int brightness = this->mixer.b + this->correction.used;
     int contrast   = this->mixer.c;
     int saturation = this->mixer.s;
     int i;

     if (flags & MMF_B)
     {
          if (this->mixer.b == 0)
               this->mixer.modified &= ~MMF_B;
          else
               this->mixer.modified |=  MMF_B;
     }

     if (flags & MMF_C)
     {
          if (this->mixer.c == 128)
               this->mixer.modified &= ~MMF_C;
          else
               this->mixer.modified |=  MMF_C;
     }

     if (flags & MMF_S)
     {
          if (this->mixer.s == 128)
               this->mixer.modified &= ~MMF_S;
          else
               this->mixer.modified |=  MMF_S;
     }

#ifdef ARCH_X86
     if (this->proc.accel == MM_MMX)
     {
          if (flags & MMF_B)
               MMX_GEN_BR();

          if (flags & MMF_C)
               MMX_GEN_CN();

          if (flags & MMF_S)
          {
               MMX_GEN_ST();
               MMX_GEN_VR();
               MMX_GEN_VG();
               MMX_GEN_UG();
               MMX_GEN_UB();
          }

          return;
     }
#endif

     for (i = 0; i < 256; i++)
     {
          if (flags & (MMF_B | MMF_C))
               GEN_Y();

          if (flags & MMF_S)
          {
               GEN_CR();
               GEN_VR();
               GEN_VG();
               GEN_UG();
               GEN_UB();
          }
     }
}


static int
dfb_get_property( vo_driver_t *vo_driver,
                  int          property )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property)
     {
          case VO_PROP_INTERLACED:
          {
               DBUG( "frame is %s interlaced", 
                      (this->state.blittingflags & DSBLIT_DEINTERLACE)
                      ? "" : "not" );
               return (this->state.blittingflags & DSBLIT_DEINTERLACE);
          }
          break;
          
          case VO_PROP_BRIGHTNESS:
          {
               DBUG( "brightness is %i", this->mixer.b );
               return this->mixer.b;
          }
          break;

          case VO_PROP_CONTRAST:
          {
               DBUG( "contrast is %i", this->mixer.c );
               return this->mixer.c;
          }
          break;

          case VO_PROP_SATURATION:
          {
               DBUG( "saturation is %i", this->mixer.s );
               return this->mixer.s;
          }
          break;

          case VO_PROP_MAX_NUM_FRAMES:
          {
               DBUG( "maximum number of frames is %i", this->max_num_frames );
               return this->max_num_frames;
          }
          break;

          default:
               DBUG( "tryed to get unsupported property %i", property );
          break;
     }
     
     return 0;
}


static int
dfb_set_property( vo_driver_t *vo_driver,
                  int          property,
                  int          value )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property)
     {
          case VO_PROP_INTERLACED:
          {
               if (value)
                    this->state.blittingflags |= DSBLIT_DEINTERLACE;
               else
                    this->state.blittingflags &= ~DSBLIT_DEINTERLACE;

               this->state.modified |= SMF_BLITTING_FLAGS;
          }
          break;
          
          case VO_PROP_BRIGHTNESS:
          {
               if (value >= -128 && value <= 127)
               {
                    DBUG( "setting brightness to %i", value );
                    this->mixer.b = value;
                    dfb_tables_regen( this, MMF_B );
               }
          }
          break;

          case VO_PROP_CONTRAST:
          {
               if (value >= 0 && value <= 255)
               {                    
                    DBUG( "setting contrast to %i", value );
                    this->mixer.c = value;
                    dfb_tables_regen( this, MMF_C );
               }
          }
          break;

          case VO_PROP_SATURATION:
          {
               if (value >= 0 && value <= 255)
               {
                    DBUG( "setting saturation to %i", value );
                    this->mixer.s = value;
                    dfb_tables_regen( this, MMF_S );
               }
          }
          break;
          
          default:
               DBUG( "tryed to set unsupported property %i", property );
          break;
     }

     return value;
}


static void
dfb_get_property_min_max( vo_driver_t *vo_driver,
                          int          property,
                          int         *min,
                          int         *max )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property)
     {
          case VO_PROP_BRIGHTNESS:
          {
               *min = -128;
               *max = +127;
          }
          break;

          case VO_PROP_CONTRAST:
          {
               *min = 0;
               *max = 255;
          }
          break;

          case VO_PROP_SATURATION:
          {
               *min = 0;
               *max = 255;
          }
          break;

          default:
          {
               DBUG( "requested min/max for unsupported property %i", property );
               *min = 0;
               *max = 0;
          }
          break;
     }
}


static int
dfb_gui_data_exchange( vo_driver_t *vo_driver,
                       int          data_type,
                       void        *data )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (data_type)
     {
          /* update destination Surface */
          case XINE_GUI_SEND_DRAWABLE_CHANGED:
          {
               IDirectFBSurface *surface = (IDirectFBSurface*) data;

               if (!surface || !surface->priv)
               {
                    fprintf( stderr, THIS ": bad surface\n" );
                    return 0;
               }

               if (this->dest)
                    this->dest->Release( this->dest );

               this->dest      = surface;
               this->dest_data = (IDirectFBSurface_data*) surface->priv;

               switch (this->dest_data->surface->format)
               {
                    case DSPF_YUY2:
                    case DSPF_UYVY:
                    case DSPF_YV12:
                    case DSPF_I420:
                    {
                         DBUG( "we have a new surface [format: YUV(%#x)]",
                               this->dest_data->surface->format );
                         this->correction.used = 0;
                    }
                    break;

                    case DSPF_RGB332:
                    case DSPF_ARGB1555:
                    case DSPF_RGB16:
                    case DSPF_RGB24:
                    case DSPF_RGB32:
                    case DSPF_ARGB:
                    case DSPF_AiRGB:
                    {
                         DBUG( "we have a new surface [format: RGB(%#x)]",
                               this->dest_data->surface->format );
                         this->correction.used = this->correction.defined;
                    }
                    break;

                    default:
                    {
                         SAY( "unsupported surface format [%#x]",
                              this->dest_data->surface->format );
                         this->dest      = NULL;
                         this->dest_data = NULL;
                         return 0;
                    }
                    break;
               }

               dfb_tables_regen( this, MMF_A );

               this->state.destination  = this->dest_data->surface;
               this->state.modified    |= SMF_DESTINATION;

               this->dest->AddRef( this->dest );

               return 1;
          }
          break;

          /* register DVFrameCallback */
          case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
          {
               dfb_frame_callback_t *frame_callback;

               frame_callback = (dfb_frame_callback_t*) data;

               this->frame_cb    = frame_callback->frame_cb;
               this->frame_cdata = frame_callback->cdata;

               DBUG( "%s DVFrameCallback",
                     (this->frame_cb) ? "registered new" : "unregistered" );

               return 1;
          }
          break;
          
          default:
               DBUG( "unknown data type %i", data_type );
          break;
     }
     
     return 0;
}


static void
dfb_dispose( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     if (this)
     {
          if (this->dest)
               this->dest->Release( this->dest );
          free( this );
     }
}


static vo_driver_t*
open_plugin( video_driver_class_t *vo_class,
             const void           *vo_visual )
{
     dfb_driver_class_t *class  = (dfb_driver_class_t*) vo_class;
     dfb_visual_t       *visual = (dfb_visual_t*) vo_visual;
     config_values_t    *config = class->xine->config;
     dfb_driver_t       *this;

     if (!vo_visual)
          return NULL;
     
     if (!visual->output_cb)
          return NULL;

     if (class->xine->verbosity)
          fprintf( stderr, "DFB [Unofficial DirectFB video driver]\n" );

     this = (dfb_driver_t*) xine_xmalloc( sizeof(dfb_driver_t) );
     if (!this)
          return NULL;
     
     this->verbosity = class->xine->verbosity;

     this->vo_driver.get_capabilities     = dfb_get_capabilities;
     this->vo_driver.alloc_frame          = dfb_alloc_frame;
     this->vo_driver.update_frame_format  = dfb_update_frame_format;
     this->vo_driver.overlay_begin        = NULL;
     this->vo_driver.overlay_blend        = dfb_overlay_blend;
     this->vo_driver.overlay_end          = NULL;
     this->vo_driver.display_frame        = dfb_display_frame;
     this->vo_driver.get_property         = dfb_get_property;
     this->vo_driver.set_property         = dfb_set_property;
     this->vo_driver.get_property_min_max = dfb_get_property_min_max;
     this->vo_driver.gui_data_exchange    = dfb_gui_data_exchange;
     this->vo_driver.redraw_needed        = dfb_redraw_needed;
     this->vo_driver.dispose              = dfb_dispose;


     this->max_num_frames     = config->register_num( config,
                              "video.dfb.max_num_frames", 15,
                              "Maximum number of allocated frames (at least 5)",
                              NULL, 10, NULL, NULL );

     this->correction.defined = config->register_range( config,
                              "video.dfb.gamma_correction", -16,
                              -128, 127, "RGB gamma correction",
                              NULL, 10, NULL, NULL );

     this->proc.funcs[0] = &dummy[0][0];
     this->proc.funcs[1] = &dummy[1][0];
     
#ifdef ARCH_X86
     if ((xine_mm_accel() & MM_MMX) == MM_MMX)
     {
          int use_mmx = config->register_bool( config, 
                                   "video.dfb.enable_mmx", 1,
                                   "Enable MMX when available",
                                   NULL, 10, NULL,NULL );
          if (use_mmx)
          {
               SAY( "MMX detected and enabled" );
               this->proc.accel    = MM_MMX;
               this->proc.funcs[0] = &mmx[0][0];
               this->proc.funcs[1] = &mmx[1][0];
          } else
               SAY( "MMX detected but disabled" );
     }
#endif

     this->state.src_blend    = DSBF_SRCALPHA;
     this->state.dst_blend    = DSBF_INVSRCALPHA;
     this->state.drawingflags = DSDRAW_BLEND;
     this->state.modified     = SMF_ALL;
     
     D_MAGIC_SET( &this->state, CardState );

     if (visual->surface)
     {
          this->dest      = visual->surface;
          this->dest_data = (IDirectFBSurface_data*) this->dest->priv;

          if (!this->dest_data)
          {
               fprintf( stderr, THIS ": bad surface\n" );
               goto failure;
          }

          switch (this->dest_data->surface->format)
          {
               case DSPF_YUY2:
               case DSPF_UYVY:
               case DSPF_YV12:
               case DSPF_I420:
               {
                    DBUG( "surface format is YUV [%#x]",
                          this->dest_data->surface->format );
                    this->correction.used = 0;
               }
               break;

               case DSPF_RGB332:
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB24:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
               {
                    DBUG( "surface format is RGB [%#x]",
                          this->dest_data->surface->format );
                    this->correction.used = this->correction.defined;
               }
               break;

               default:
                    SAY( "unsupported surface format [%#x]",
                          this->dest_data->surface->format );
               goto failure;
          }

          this->state.destination = this->dest_data->surface;
          
          this->dest->AddRef( this->dest );
     }

     this->mixer.b = 0;
     this->mixer.c = 128;
     this->mixer.s = 128;

     dfb_tables_regen( this, MMF_A );

     this->output_cb    = visual->output_cb;
     this->output_cdata = visual->cdata;
     
     return (vo_driver_t*) this;

failure:
     release( this );
     return NULL;
}


static char*
get_identifier( video_driver_class_t *vo_class )
{
     return "DFB";
}


static char*
get_description( video_driver_class_t *vo_class)
{
     return "Unofficial DirectFB video driver.";
}


static void
dispose_class( video_driver_class_t *vo_class )
{
     release( vo_class );
}


static void*
init_class( xine_t *xine,
            void   *vo_visual )
{
     dfb_driver_class_t *class;

     class = (dfb_driver_class_t*) xine_xmalloc( sizeof(dfb_driver_class_t) );
     if (!class)
          return NULL;

     class->vo_class.open_plugin     = open_plugin;
     class->vo_class.get_identifier  = get_identifier;
     class->vo_class.get_description = get_description;
     class->vo_class.dispose         = dispose_class;
     class->xine                     = xine;

     return class;
}


static vo_info_t vo_info_dfb =
{
     8,
     XINE_VISUAL_TYPE_DFB
};


plugin_info_t xine_plugin_info[] =
{
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, VIDEO_OUT_DRIVER_IFACE_VERSION, "DFB",
       XINE_VERSION_CODE, &vo_info_dfb, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

