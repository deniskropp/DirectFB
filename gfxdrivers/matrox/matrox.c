/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/util.h>
#include <misc/util.h>

#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_state.h"


volatile __u8 *mmio_base;

GfxCard *matrox = NULL;

/* Old cards are older than G200/G400, e.g. Mystique or Millenium */
int old_matrox = 0;

unsigned int matrox_fifo_space = 0;

unsigned int matrox_waitfifo_sum = 0;
unsigned int matrox_waitfifo_calls = 0;
unsigned int matrox_fifo_waitcycles = 0;
unsigned int matrox_idle_waitcycles = 0;
unsigned int matrox_fifo_cache_hits = 0;

static __u32 atype_blk_rstr = 0;

static void matroxBlit2D( DFBRectangle *rect, int dx, int dy );
static void matroxBlit2D_Old( DFBRectangle *rect, int dx, int dy );
static void matroxBlit3D( DFBRectangle *rect, int dx, int dy );


int m_Source = 0;
int m_source = 0;

int m_Color = 0;
int m_color = 0;

int m_SrcKey = 0;
int m_srckey = 0;

int m_drawBlend = 0;
int m_blitBlend = 0;

int dst_pixelpitch = 0;
int dst_pixeloffset = 0;
int src_pixelpitch = 0;
int src_pixeloffset = 0;


static void matroxEngineSync()
{
     mga_waitidle( mmio_base );
}

static void matroxFlushTextureCache()
{
     mga_waitfifo( mmio_base, 1 );
     mga_out32( mmio_base, 0, TEXORG1 );
}

/* Old cards (Mystique, Millenium, ...) */

#define MATROX_OLD_DRAWING_FLAGS            (DSDRAW_NOFX)

#define MATROX_OLD_BLITTING_FLAGS           (DSBLIT_SRC_COLORKEY)

#define MATROX_OLD_DRAWING_FUNCTIONS        (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_OLD_BLITTING_FUNCTIONS       (DFXL_BLIT)


/* G200/G400 */

#define MATROX_G200G400_DRAWING_FLAGS       (DSDRAW_BLEND)

#define MATROX_G200G400_BLITTING_FLAGS      (DSBLIT_SRC_COLORKEY | \
                                             DSBLIT_BLEND_ALPHACHANNEL | \
                                             DSBLIT_BLEND_COLORALPHA | \
                                             DSBLIT_COLORIZE)

#define MATROX_G200G400_DRAWING_FUNCTIONS   (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_G200G400_BLITTING_FUNCTIONS  (DFXL_BLIT          | \
                                             DFXL_STRETCHBLIT)

static void matroxOldCheckState( CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (accel & 0xFFFF) {
          if (state->drawingflags & ~MATROX_OLD_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_OLD_DRAWING_FUNCTIONS;
     }
     else {
          if (state->source->format != state->destination->format)
               return;

          if (state->blittingflags & ~MATROX_OLD_BLITTING_FLAGS)
               return;

          state->accel |= MATROX_OLD_BLITTING_FUNCTIONS;
     }
}

static void matroxG200CheckState( CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (accel & 0xFFFF) {
          if (state->drawingflags & ~MATROX_G200G400_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_DRAWING_FUNCTIONS;
     }
     else {
          switch (state->source->format) {
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
               default:
                    return;
          }

          if (state->source->width < 8 ||
              state->source->height < 8 ||
              state->source->width > 2048 ||
              state->source->height > 2048)
               return;

          if (state->blittingflags & ~MATROX_G200G400_BLITTING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_BLITTING_FUNCTIONS;
     }
}

static void matroxG400CheckState( CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (accel & 0xFFFF) {
          if (state->drawingflags & ~MATROX_G200G400_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_DRAWING_FUNCTIONS;
     }
     else {
          switch (state->source->format) {
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_A8:
                    break;
               default:
                    return;
          }

          if (state->source->width < 8 ||
              state->source->height < 8 ||
              state->source->width > 2048 ||
              state->source->height > 2048)
               return;

          if (state->blittingflags & ~MATROX_G200G400_BLITTING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_BLITTING_FUNCTIONS;
     }
}

static void matroxSetState( CardState *state, DFBAccelerationMask accel )
{
     if (state != matrox->state) {
          state->modified |= SMF_ALL;
          state->set = 0;
          matrox->state = state;

          m_Source = m_source = m_Color = m_color =
               m_SrcKey = m_srckey = m_drawBlend = m_blitBlend = 0;
     }
     else {
          if (state->modified) {
               if (state->modified & SMF_COLOR)
                    m_Color = m_color = 0;
               else if (state->modified & SMF_DESTINATION)
                    m_color = 0;

               if (state->modified & SMF_SOURCE)
                    m_Source = m_source = m_SrcKey = m_srckey = 0;
               else if (state->modified & SMF_SRC_COLORKEY)
                    m_SrcKey = m_srckey = 0;

               if (state->modified & SMF_BLITTING_FLAGS)
                    m_Source = m_blitBlend = 0;

               if (state->modified & (SMF_DST_BLEND | SMF_SRC_BLEND))
                    m_blitBlend = m_drawBlend = 0;
          }
     }

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    matrox_validate_Color();
                    matrox_validate_drawBlend();
               }
               else
                    matrox_validate_color();

               state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE | DFXL_FILLTRIANGLE;

               break;
          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                           DSBLIT_COLORIZE))
                    matrox_validate_Color();

               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                           DSBLIT_BLEND_COLORALPHA   |
                                           DSBLIT_COLORIZE)             ||
                   state->destination->format != state->source->format  ||
                   accel == DFXL_STRETCHBLIT)
               {
                    matrox->Blit = matroxBlit3D;

                    matrox_validate_blitBlend();
                    matrox_validate_Source();

                    if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                         matrox_validate_SrcKey();

                    state->set = DFXL_BLIT | DFXL_STRETCHBLIT;
               }
               else {
                    if (old_matrox)
                         matrox->Blit = matroxBlit2D_Old;
                    else
                         matrox->Blit = matroxBlit2D;

                    matrox_validate_source();

                    if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                         matrox_validate_srckey();

                    state->set = DFXL_BLIT;
               }
               break;
          default:
               BUG( "unexpected drawing/blitting function!" );
               break;
     }

     if (state->modified & SMF_DESTINATION) {
          matrox_set_destination();

          /* On old cards the clip depends on the destination's pixel offset */
          if (old_matrox)
               state->modified |= SMF_CLIP;
     }

     if (state->modified & SMF_CLIP)
          matrox_set_clip();

     state->modified = 0;
}

static void matroxFillRectangle( DFBRectangle *rect )
{
     mga_waitfifo( mmio_base, 3 );

     if (matrox->state->drawingflags & DSDRAW_BLEND)
          mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO |
                                ARZERO | ATYPE_I | OP_TRAP,
                     DWGCTL );
     else
          mga_out32( mmio_base, TRANSC | BOP_COPY | SHFTZERO | SGNZERO |
                                ARZERO | SOLID | atype_blk_rstr | OP_TRAP,
                     DWGCTL );

     mga_out32( mmio_base, (RS16(rect->x + rect->w) << 16) | RS16(rect->x), FXBNDRY );
     mga_out32( mmio_base, (RS16(rect->y) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxDrawRectangle( DFBRectangle *rect )
{
     mga_waitfifo( mmio_base, 6 );

     if (matrox->state->drawingflags & DSDRAW_BLEND)
          mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                                OP_AUTOLINE_OPEN,
                     DWGCTL );
     else
          mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                                ATYPE_RSTR | OP_AUTOLINE_OPEN,
                     DWGCTL );

     mga_out32(mmio_base, RS16(rect->x) |
                         (RS16(rect->y) << 16),
                          XYSTRT);

     mga_out32(mmio_base, RS16(rect->x + rect->w-1) | (RS16(rect->y) << 16),
                          XYEND | EXECUTE);

     mga_out32(mmio_base, RS16(rect->x + rect->w-1) |
                         (RS16(rect->y + rect->h-1) << 16),
                          XYEND | EXECUTE);

     mga_out32(mmio_base, RS16(rect->x) |
                         (RS16(rect->y + rect->h-1) << 16),
                          XYEND | EXECUTE);

     mga_out32(mmio_base, RS16(rect->x) |
                         (RS16(rect->y) << 16),
                          XYEND | EXECUTE);
}

static void matroxDrawLine( DFBRegion *line )
{
     mga_waitfifo( mmio_base, 3 );

     if (matrox->state->drawingflags & DSDRAW_BLEND)
          mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                                OP_AUTOLINE_CLOSE,
                     DWGCTL );
     else
          mga_out32( mmio_base, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                                ATYPE_RSTR | OP_AUTOLINE_CLOSE,
                     DWGCTL );

     mga_out32( mmio_base, RS16(line->x1) | (RS16(line->y1) << 16),
                           XYSTRT );

     mga_out32( mmio_base, RS16(line->x2) | (RS16(line->y2) << 16),
                           XYEND | EXECUTE );
}

static void matroxFillTrapezoid( int Xl, int Xr, int X2l, int X2r, int Y, int dY )
{
  int dxl = X2l - Xl;
  int dxr = ++X2r - ++Xr;

  int dXl = abs(dxl);
  int dXr = abs(dxr);

  __u32 sgn = 0;

  mga_waitfifo( mmio_base, 6 );

  mga_out32( mmio_base, dY, AR0 );
  mga_out32( mmio_base, - dXl, AR1 );
  mga_out32( mmio_base, - dXl, AR2 );
  mga_out32( mmio_base, - dXr, AR4 );
  mga_out32( mmio_base, - dXr, AR5 );
  mga_out32( mmio_base, dY, AR6 );

  if (dxl < 0)
    sgn |= SDXL;
  if (dxr < 0)
    sgn |= SDXR;

  mga_waitfifo( mmio_base, 3 );

  mga_out32( mmio_base, sgn, SGN );
  mga_out32( mmio_base, (RS16(Xr) << 16) | RS16(Xl), FXBNDRY );
  mga_out32( mmio_base, (RS16(Y) << 16) | RS16(dY), YDSTLEN | EXECUTE );
}

static void matroxFillTriangle( DFBTriangle *tri )
{
     mga_waitfifo( mmio_base, 1 );

     if (matrox->state->drawingflags & DSDRAW_BLEND)
          mga_out32( mmio_base, BOP_COPY | SHFTZERO | ATYPE_I | OP_TRAP,
                     DWGCTL );
     else
          mga_out32( mmio_base, TRANSC | BOP_COPY | SHFTZERO |
                                SOLID | atype_blk_rstr | OP_TRAP,
                     DWGCTL );

     sort_triangle( tri );

     if (tri->y2 == tri->y3) {
       matroxFillTrapezoid( tri->x1, tri->x1,
                MIN( tri->x2, tri->x3 ), MAX( tri->x2, tri->x3 ),
                tri->y1, tri->y3 - tri->y1 + 1 );
     } else
     if (tri->y1 == tri->y2) {
       matroxFillTrapezoid( MIN( tri->x1, tri->x2 ), MAX( tri->x1, tri->x2 ),
                tri->x3, tri->x3,
                tri->y1, tri->y3 - tri->y1 + 1 );
     }
     else {
       int majDx = tri->x3 - tri->x1;
       int majDy = tri->y3 - tri->y1;
       int topDx = tri->x2 - tri->x1;
       int topDy = tri->y2 - tri->y1;
       int botDy = tri->y3 - tri->y2;

       int topXperY = (topDx << 20) / topDy;
       int X2a = tri->x1 + (((topXperY * topDy) + (1<<19)) >> 20);

       int majXperY = (majDx << 20) / majDy;
       int majX2  = tri->x1 + (((majXperY * topDy) + (1<<19)) >> 20);
       int majX2a = majX2 - ((majXperY + (1<<19)) >> 20);

       matroxFillTrapezoid( tri->x1, tri->x1,
                MIN( X2a, majX2a ), MAX( X2a, majX2a ),
                tri->y1, topDy );
       matroxFillTrapezoid( MIN( tri->x2, majX2 ), MAX( tri->x2, majX2 ),
                tri->x3, tri->x3,
                tri->y2, botDy + 1 );
     }
}

static void matroxBlit2D( DFBRectangle *rect, int dx, int dy )
{
     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO |
                    ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = src_pixelpitch;

     if (rect->x < dx)
          sgn |= BLIT_LEFT;
     if (rect->y < dy)
          sgn |= BLIT_UP;

     if (sgn & BLIT_UP) {
          rect->y += rect->h - 1;
          dy += rect->h - 1;
     }

     start = end = rect->y * pixelpitch + rect->x;

     rect->w--;

     if (sgn & BLIT_LEFT)
          start += rect->w;
     else
          end += rect->w;

     if (sgn & BLIT_UP)
          pixelpitch = -pixelpitch;

     if (matrox->state->blittingflags & DSBLIT_SRC_COLORKEY)
          dwgctl |= TRANSC;

     mga_waitfifo( mmio_base, 7 );
     mga_out32( mmio_base, dwgctl, DWGCTL );
     mga_out32( mmio_base, pixelpitch & 0x3FFFFF, AR5 );
     mga_out32( mmio_base, start & 0xFFFFFF, AR3 );
     mga_out32( mmio_base, end & 0x3FFFFF, AR0 );
     mga_out32( mmio_base, sgn, SGN );
     mga_out32( mmio_base, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio_base, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxBlit2D_Old( DFBRectangle *rect, int dx, int dy )
{
     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO |
                    ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = src_pixelpitch;

     if (rect->x < dx)
          sgn |= BLIT_LEFT;
     if (rect->y < dy)
          sgn |= BLIT_UP;

     if (sgn & BLIT_UP) {
          rect->y += rect->h - 1;
          dy += rect->h - 1;
     }

     start = rect->y * pixelpitch + rect->x + src_pixeloffset;

     rect->w--;

     end = rect->w;

     if (sgn & BLIT_LEFT) {
          start += rect->w;
          end = -end;
     }

     if (sgn & BLIT_UP)
          pixelpitch = -pixelpitch;

     if (matrox->state->blittingflags & DSBLIT_SRC_COLORKEY)
          dwgctl |= TRANSC;

     mga_waitfifo( mmio_base, 7 );
     mga_out32( mmio_base, dwgctl, DWGCTL );
     mga_out32( mmio_base, pixelpitch & 0x3FFFFF, AR5 );
     mga_out32( mmio_base, start & 0xFFFFFF, AR3 );
     mga_out32( mmio_base, end & 0x3FFFF, AR0 );
     mga_out32( mmio_base, sgn, SGN );
     mga_out32( mmio_base, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio_base, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxBlit3D( DFBRectangle *rect, int dx, int dy )
{
     __s32 startx, starty;

     startx = ((rect->x << 20) | 0x80000) >> matrox_w2;
     starty = ((rect->y << 20) | 0x80000) >> matrox_h2;

     mga_waitfifo( mmio_base, 8);


     mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO |
                ARZERO | ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );

     mga_out32( mmio_base, MAG_NRST | MIN_NRST, TEXFILTER );

     mga_out32( mmio_base, 0x100000 >> matrox_w2, TMR0 );
     mga_out32( mmio_base, 0x100000 >> matrox_h2, TMR3 );
     mga_out32( mmio_base, startx, TMR6 );
     mga_out32( mmio_base, starty, TMR7 );
     mga_out32( mmio_base, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio_base, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxStretchBlit( DFBRectangle *srect, DFBRectangle *drect )
{
     __s32 startx, starty, incx, incy;

     incx = (srect->w << (20 - matrox_w2))  /  drect->w;
     incy = (srect->h << (20 - matrox_h2))  /  drect->h;

     startx = srect->x << (20 - matrox_w2);
     starty = srect->y << (20 - matrox_h2);

     mga_waitfifo( mmio_base, 8);


     mga_out32( mmio_base, BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                           ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );

     mga_out32( mmio_base, MAG_BILIN | MIN_BILIN, TEXFILTER );

     mga_out32( mmio_base, incx, TMR0 );
     mga_out32( mmio_base, incy, TMR3 );
     mga_out32( mmio_base, startx, TMR6 );
     mga_out32( mmio_base, starty, TMR7 );

     mga_out32( mmio_base, (RS16(drect->x+drect->w) << 16) | RS16(drect->x), FXBNDRY );
     mga_out32( mmio_base, (RS16(drect->y) << 16) | RS16(drect->h), YDSTLEN | EXECUTE );
}


/* exported symbols */

int driver_probe( int fd, GfxCard *card )
{
     switch (card->fix.accel) {
          case FB_ACCEL_MATROX_MGA2064W:     /* Matrox MGA2064W (Millenium)   */
          case FB_ACCEL_MATROX_MGA1064SG:    /* Matrox MGA1064SG (Mystique)   */
          case FB_ACCEL_MATROX_MGA2164W:     /* Matrox MGA2164W (Millenium II)*/
          case FB_ACCEL_MATROX_MGA2164W_AGP: /* Matrox MGA2164W (Millenium II)*/
               old_matrox = 1;

               /* fall through */

#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:      /* Matrox G400                   */
#endif
          case FB_ACCEL_MATROX_MGAG200:      /* Matrox G200 (Myst, Mill, ...) */
               return 1;
     }

     return 0;
}

int driver_init( int fd, GfxCard *card )
{
     mmio_base = (__u8*)mmap(NULL, card->fix.mmio_len,
                                      PROT_READ | PROT_WRITE, MAP_SHARED,
                                      fd, card->fix.smem_len);
     if (mmio_base == MAP_FAILED) {
          PERRORMSG("DirectFB/Matrox: Unable to map mmio region!\n");
          return DFB_IO;
     }

     switch (card->fix.accel) {
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:
               sprintf( card->info.driver_name, "Matrox G400/G450" );
               break;
#endif
          case FB_ACCEL_MATROX_MGAG200:
               sprintf( card->info.driver_name, "Matrox G200" );
               break;
          case FB_ACCEL_MATROX_MGA2064W:
               sprintf( card->info.driver_name, "Matrox Millenium I" );
               break;
          case FB_ACCEL_MATROX_MGA1064SG:
               sprintf( card->info.driver_name, "Matrox Mystique" );
               break;
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               sprintf( card->info.driver_name, "Matrox Millenium II" );
               break;
          default:
               BUG("id known by probe() not known by init()");
               munmap( (void*)mmio_base, card->fix.mmio_len);
               return DFB_BUG;
     }

     sprintf( card->info.driver_vendor, "convergence integrated media GmbH" );

     card->info.driver_version.major = 0;
     card->info.driver_version.minor = 5;

     card->caps.flags = CCF_CLIPPING;

     switch (card->fix.accel) {
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:
               card->caps.accel    = MATROX_G200G400_DRAWING_FUNCTIONS |
                                     MATROX_G200G400_BLITTING_FUNCTIONS;
               card->caps.drawing  = MATROX_G200G400_DRAWING_FLAGS;
               card->caps.blitting = MATROX_G200G400_BLITTING_FLAGS;

               card->CheckState = matroxG400CheckState;
               break;
#endif
          case FB_ACCEL_MATROX_MGAG200:
               card->caps.accel    = MATROX_G200G400_DRAWING_FUNCTIONS |
                                     MATROX_G200G400_BLITTING_FUNCTIONS;
               card->caps.drawing  = MATROX_G200G400_DRAWING_FLAGS;
               card->caps.blitting = MATROX_G200G400_BLITTING_FLAGS;

               card->CheckState = matroxG200CheckState;
               break;

          case FB_ACCEL_MATROX_MGA2064W:
          case FB_ACCEL_MATROX_MGA1064SG:
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               card->caps.accel    = MATROX_OLD_DRAWING_FUNCTIONS |
                                     MATROX_OLD_BLITTING_FUNCTIONS;
               card->caps.drawing  = MATROX_OLD_DRAWING_FLAGS;
               card->caps.blitting = MATROX_OLD_BLITTING_FLAGS;

               card->CheckState = matroxOldCheckState;
               break;
     }

     card->SetState = matroxSetState;
     card->EngineSync = matroxEngineSync;
     card->FlushTextureCache = matroxFlushTextureCache;

     card->FillRectangle = matroxFillRectangle;
     card->DrawRectangle = matroxDrawRectangle;
     card->DrawLine = matroxDrawLine;
     card->FillTriangle = matroxFillTriangle;
     card->StretchBlit = matroxStretchBlit;

     /* will be set dynamically: card->Blit */

     mga_waitfifo( mmio_base, 11 );
     mga_out32( mmio_base, 0, TDUALSTAGE0 );   // multi texture registers
     mga_out32( mmio_base, 0, TDUALSTAGE1 );
     mga_out32( mmio_base, 0, ALPHAXINC );     // alpha increments
     mga_out32( mmio_base, 0, ALPHAYINC );
     mga_out32( mmio_base, 0, DR6 );           // red increments
     mga_out32( mmio_base, 0, DR7 );
     mga_out32( mmio_base, 0, DR10 );          // green increments
     mga_out32( mmio_base, 0, DR11 );
     mga_out32( mmio_base, 0, DR14 );          // blue increments
     mga_out32( mmio_base, 0, DR15 );
     mga_out32( mmio_base, 0, BCOL );

     mga_waitfifo( mmio_base, 5 );
     mga_out32( mmio_base, 0, TMR1 );
     mga_out32( mmio_base, 0, TMR2 );
     mga_out32( mmio_base, 0, TMR4 );
     mga_out32( mmio_base, 0, TMR5 );
     mga_out32( mmio_base, 0x10000, TMR8 );

     atype_blk_rstr = dfb_config->matrox_sgram ? ATYPE_BLK : ATYPE_RSTR;

     /* set hardware limitations */
     card->byteoffset_align = 32*4;
     card->pixelpitch_align = 32;

     matrox = card;

     return DFB_OK;
}

void driver_deinit()
{
     /* reset DSTORG as matroxfb does not */
     mga_waitfifo( mmio_base, 1 );
     mga_out32( mmio_base, 0, DSTORG );

     /* make sure overlay is off */
     mga_waitfifo( mmio_base, 1 );
     mga_out32( mmio_base, 0, BESCTL );

     DEBUGMSG( "DirectFB/Matrox: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/Matrox:  %9d matrox_waitfifo calls\n",
               matrox_waitfifo_calls );
     DEBUGMSG( "DirectFB/Matrox:  %9d register writes (matrox_waitfifo sum)\n",
               matrox_waitfifo_sum );
     DEBUGMSG( "DirectFB/Matrox:  %9d FIFO wait cycles (depends on CPU)\n",
               matrox_fifo_waitcycles );
     DEBUGMSG( "DirectFB/Matrox:  %9d IDLE wait cycles (depends on CPU)\n",
               matrox_idle_waitcycles );
     DEBUGMSG( "DirectFB/Matrox:  %9d FIFO space cache hits (depends on CPU)\n",
               matrox_fifo_cache_hits );
     DEBUGMSG( "DirectFB/Matrox: Conclusion:\n" );
     DEBUGMSG( "DirectFB/Matrox:  Average register writes/matrox_waitfifo call: %.2f\n",
               matrox_waitfifo_sum/(float)(matrox_waitfifo_calls) );
     DEBUGMSG( "DirectFB/Matrox:  Average wait cycles/matrox_waitfifo call:     %.2f\n",
               matrox_fifo_waitcycles/(float)(matrox_waitfifo_calls) );
     DEBUGMSG( "DirectFB/Matrox:  Average fifo space cache hits:                %02d%%\n",
               (int)(100 * matrox_fifo_cache_hits/(float)(matrox_waitfifo_calls)) );

     munmap( (void*)mmio_base, matrox->fix.mmio_len);
}

