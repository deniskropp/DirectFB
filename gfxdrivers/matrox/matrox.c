/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( matrox )

#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_state.h"


static void matroxBlit2D    ( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );

static void matroxBlit2D_Old( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );

static void matroxBlit3D    ( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );




static void matroxEngineSync( void *drv, void *dev )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     mga_waitidle( mdrv, mdev );
}

static void matroxFlushTextureCache( void *drv, void *dev )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mdrv->mmio_base, 0, TEXORG1 );
}

/* Old cards (Mystique, Millenium, ...) */

#define MATROX_OLD_DRAWING_FLAGS            (DSDRAW_NOFX)

#define MATROX_OLD_BLITTING_FLAGS           (DSBLIT_SRC_COLORKEY)

#define MATROX_OLD_DRAWING_FUNCTIONS        (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_OLD_BLITTING_FUNCTIONS       (DFXL_BLIT)


/* G100 */

#define MATROX_G100_DRAWING_FLAGS           (DSDRAW_NOFX)

#define MATROX_G100_BLITTING_FLAGS          (DSBLIT_SRC_COLORKEY | \
                                           /*DSBLIT_BLEND_ALPHACHANNEL |*/ \
                                           /*DSBLIT_BLEND_COLORALPHA |*/ \
                                             DSBLIT_COLORIZE)

#define MATROX_G100_DRAWING_FUNCTIONS       (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_G100_BLITTING_FUNCTIONS      (DFXL_BLIT          | \
                                             DFXL_STRETCHBLIT)


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


#define MATROX_USE_TMU(state, accel)                                     \
     ((state)->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |              \
                                DSBLIT_BLEND_COLORALPHA   |              \
                                DSBLIT_COLORIZE)               ||        \
      (state)->destination->format != (state)->source->format  ||        \
      (accel) == DFXL_STRETCHBLIT)



static void matroxOldCheckState( void *drv, void *dev,
                                 CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
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

static void matroxG100CheckState( void *drv, void *dev,
                                  CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~MATROX_G100_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_G100_DRAWING_FUNCTIONS;
     }
     else {
          if (state->blittingflags & ~MATROX_G100_BLITTING_FLAGS)
               return;

          /* using the texture mapping unit? */
          if (MATROX_USE_TMU( state, accel )) {
               /* TMU has no 32bit support */
               switch (state->source->format) {
                    case DSPF_RGB15:
                    case DSPF_RGB16:
                         break;
                    default:
                         return;
               }

               /* TMU limits */
               if (state->source->width < 8 ||
                   state->source->height < 8 ||
                   state->source->width > 2048 ||
                   state->source->height > 2048)
                    return;

               state->accel |= MATROX_G100_BLITTING_FUNCTIONS;
          }
          else {
               /* source and destination formats equal, no stretching is done */
               state->accel |= accel;
          }
     }
}

static void matroxG200CheckState( void *drv, void *dev,
                                  CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~MATROX_G200G400_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_DRAWING_FUNCTIONS;
     }
     else {
          int use_tmu = MATROX_USE_TMU( state, accel );

          switch (state->source->format) {
               case DSPF_RGB332:
                    if (use_tmu)
                         return;
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_YUY2:
                    break;
               default:
                    return;
          }

          if (state->blittingflags & ~MATROX_G200G400_BLITTING_FLAGS)
               return;

          if (use_tmu) {
               if (state->source->width < 8 ||
                   state->source->height < 8 ||
                   state->source->width > 2048 ||
                   state->source->height > 2048)
                    return;

               state->accel |= MATROX_G200G400_BLITTING_FUNCTIONS;
          }
          else {
               /* source and destination formats equal, no stretching is done */
               state->accel |= accel;
          }
     }
}

static void matroxG400CheckState( void *drv, void *dev,
                                  CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_RGB332:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~MATROX_G200G400_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_G200G400_DRAWING_FUNCTIONS;
     }
     else {
          int use_tmu = MATROX_USE_TMU( state, accel );

          switch (state->source->format) {
               case DSPF_RGB332:
                    if (use_tmu)
                         return;
               case DSPF_RGB15:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_A8:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;
               default:
                    return;
          }

          if (state->blittingflags & ~MATROX_G200G400_BLITTING_FLAGS)
               return;

          if (use_tmu) {
               if (state->source->width < 8 ||
                   state->source->height < 8 ||
                   state->source->width > 2048 ||
                   state->source->height > 2048)
                    return;

               state->accel |= MATROX_G200G400_BLITTING_FUNCTIONS;
          }
          else {
               /* source and destination formats equal, no stretching is done */
               state->accel |= accel;
          }
     }
}

static void matroxSetState( void *drv, void *dev,
                            GraphicsDeviceFuncs *funcs,
                            CardState *state, DFBAccelerationMask accel )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     if (state->modified == SMF_ALL) {
          mdev->valid = 0;
     }
     else if (state->modified) {
          if (state->modified & SMF_COLOR)
               MGA_INVALIDATE( m_Color | m_color );
          else if (state->modified & SMF_DESTINATION)
               MGA_INVALIDATE( m_color );

          if (state->modified & SMF_SOURCE)
               MGA_INVALIDATE( m_Source | m_source | m_SrcKey | m_srckey | m_blitBlend );
          else if (state->modified & SMF_SRC_COLORKEY)
               MGA_INVALIDATE( m_SrcKey | m_srckey );

          if (state->modified & SMF_BLITTING_FLAGS)
               MGA_INVALIDATE( m_Source | m_blitBlend );

          if (state->modified & (SMF_DST_BLEND | SMF_SRC_BLEND))
               MGA_INVALIDATE( m_blitBlend | m_drawBlend );
     }

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
          case DFXL_FILLTRIANGLE:
               if (state->drawingflags & DSDRAW_BLEND) {
                    mdev->draw_blend = 1;
                    matrox_validate_Color( mdrv, mdev, state );
                    matrox_validate_drawBlend( mdrv, mdev, state );
               }
               else {
                    mdev->draw_blend = 0;
                    matrox_validate_color( mdrv, mdev, state );
               }

               state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE | DFXL_FILLTRIANGLE;

               break;
          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               mdev->blit_src_colorkey = (state->blittingflags &
                                          DSBLIT_SRC_COLORKEY) ? 1 : 0;

               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                           DSBLIT_COLORIZE))
                    matrox_validate_Color( mdrv, mdev, state );

               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                           DSBLIT_BLEND_COLORALPHA   |
                                           DSBLIT_COLORIZE)             ||
                   state->destination->format != state->source->format  ||
                   accel == DFXL_STRETCHBLIT)
               {
                    funcs->Blit = matroxBlit3D;

                    matrox_validate_blitBlend( mdrv, mdev, state );
                    matrox_validate_Source( mdrv, mdev, state );

                    if (mdev->blit_src_colorkey)
                         matrox_validate_SrcKey( mdrv, mdev, state );

                    state->set = DFXL_BLIT | DFXL_STRETCHBLIT;
               }
               else {
                    if (mdev->old_matrox)
                         funcs->Blit = matroxBlit2D_Old;
                    else
                         funcs->Blit = matroxBlit2D;

                    matrox_validate_source( mdrv, mdev, state );

                    if (mdev->blit_src_colorkey)
                         matrox_validate_srckey( mdrv, mdev, state );

                    state->set = DFXL_BLIT;
               }
               break;
          default:
               BUG( "unexpected drawing/blitting function!" );
               break;
     }

     if (state->modified & SMF_DESTINATION) {
          matrox_set_destination( mdrv, mdev, state->destination );

          /* On old cards the clip depends on the destination's pixel offset */
          if (mdev->old_matrox)
               state->modified |= SMF_CLIP;
     }

     if (state->modified & SMF_CLIP)
          matrox_set_clip( mdrv, mdev, &state->clip );

     state->modified = 0;
}

static void matroxFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 3 );

     if (mdev->draw_blend)
          mga_out32( mmio, BOP_COPY | SHFTZERO | SGNZERO |
                           ARZERO | ATYPE_I | OP_TRAP, DWGCTL );
     else
          mga_out32( mmio, TRANSC | BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                           SOLID | mdev->atype_blk_rstr | OP_TRAP, DWGCTL );

     mga_out32( mmio, (RS16(rect->x + rect->w) << 16) | RS16(rect->x), FXBNDRY );
     mga_out32( mmio, (RS16(rect->y) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 6 );

     if (mdev->draw_blend)
          mga_out32( mmio, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                           OP_AUTOLINE_OPEN, DWGCTL );
     else
          mga_out32( mmio, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                           ATYPE_RSTR | OP_AUTOLINE_OPEN, DWGCTL );

     mga_out32(mmio, RS16(rect->x) |
                    (RS16(rect->y) << 16),
                     XYSTRT);

     mga_out32(mmio, RS16(rect->x + rect->w-1) | (RS16(rect->y) << 16),
                     XYEND | EXECUTE);

     mga_out32(mmio, RS16(rect->x + rect->w-1) |
                    (RS16(rect->y + rect->h-1) << 16),
                     XYEND | EXECUTE);

     mga_out32(mmio, RS16(rect->x) |
                    (RS16(rect->y + rect->h-1) << 16),
                     XYEND | EXECUTE);

     mga_out32(mmio, RS16(rect->x) |
                    (RS16(rect->y) << 16),
                     XYEND | EXECUTE);
}

static void matroxDrawLine( void *drv, void *dev, DFBRegion *line )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 3 );

     if (mdev->draw_blend)
          mga_out32( mmio, BLTMOD_BFCOL | BOP_COPY | ATYPE_I |
                           OP_AUTOLINE_CLOSE,
                     DWGCTL );
     else
          mga_out32( mmio, BLTMOD_BFCOL | BOP_COPY | SHFTZERO | SOLID |
                           ATYPE_RSTR | OP_AUTOLINE_CLOSE,
                     DWGCTL );

     mga_out32( mmio, RS16(line->x1) | (RS16(line->y1) << 16),
                      XYSTRT );

     mga_out32( mmio, RS16(line->x2) | (RS16(line->y2) << 16),
                      XYEND | EXECUTE );
}

static void matrox_fill_trapezoid( MatroxDriverData *mdrv,
                                   MatroxDeviceData *mdev,
                                   int Xl, int Xr, int X2l,
                                   int X2r, int Y, int dY )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     int dxl = X2l - Xl;
     int dxr = ++X2r - ++Xr;

     int dXl = abs(dxl);
     int dXr = abs(dxr);

     __u32 sgn = 0;

     mga_waitfifo( mdrv, mdev, 6 );

     mga_out32( mmio, dY, AR0 );
     mga_out32( mmio, - dXl, AR1 );
     mga_out32( mmio, - dXl, AR2 );
     mga_out32( mmio, - dXr, AR4 );
     mga_out32( mmio, - dXr, AR5 );
     mga_out32( mmio, dY, AR6 );

     if (dxl < 0)
          sgn |= SDXL;
     if (dxr < 0)
          sgn |= SDXR;

     mga_waitfifo( mdrv, mdev, 3 );

     mga_out32( mmio, sgn, SGN );
     mga_out32( mmio, (RS16(Xr) << 16) | RS16(Xl), FXBNDRY );
     mga_out32( mmio, (RS16(Y) << 16) | RS16(dY), YDSTLEN | EXECUTE );
}

static void matroxFillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mga_waitfifo( mdrv, mdev, 1 );

     if (mdev->draw_blend)
          mga_out32( mmio, BOP_COPY | SHFTZERO | ATYPE_I | OP_TRAP,
                     DWGCTL );
     else
          mga_out32( mmio, TRANSC | BOP_COPY | SHFTZERO |
                           SOLID | mdev->atype_blk_rstr | OP_TRAP,
                     DWGCTL );

     dfb_sort_triangle( tri );

     if (tri->y2 == tri->y3) {
       matrox_fill_trapezoid( mdrv, mdev, tri->x1, tri->x1,
                MIN( tri->x2, tri->x3 ), MAX( tri->x2, tri->x3 ),
                tri->y1, tri->y3 - tri->y1 + 1 );
     } else
     if (tri->y1 == tri->y2) {
       matrox_fill_trapezoid( mdrv, mdev,
                MIN( tri->x1, tri->x2 ), MAX( tri->x1, tri->x2 ),
                tri->x3, tri->x3, tri->y1, tri->y3 - tri->y1 + 1 );
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

       matrox_fill_trapezoid( mdrv, mdev, tri->x1, tri->x1,
                MIN( X2a, majX2a ), MAX( X2a, majX2a ),
                tri->y1, topDy );
       matrox_fill_trapezoid( mdrv, mdev,
                MIN( tri->x2, majX2 ), MAX( tri->x2, majX2 ),
                tri->x3, tri->x3, tri->y2, botDy + 1 );
     }
}

static void matroxBlit2D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO | ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = mdev->src_pixelpitch;

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

     if (mdev->blit_src_colorkey)
          dwgctl |= TRANSC;

     mga_waitfifo( mdrv, mdev, 7 );
     mga_out32( mmio, dwgctl, DWGCTL );
     mga_out32( mmio, pixelpitch & 0x3FFFFF, AR5 );
     mga_out32( mmio, start & 0xFFFFFF, AR3 );
     mga_out32( mmio, end & 0x3FFFFF, AR0 );
     mga_out32( mmio, sgn, SGN );
     mga_out32( mmio, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxBlit2D_Old( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO | ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = mdev->src_pixelpitch;

     if (rect->x < dx)
          sgn |= BLIT_LEFT;
     if (rect->y < dy)
          sgn |= BLIT_UP;

     if (sgn & BLIT_UP) {
          rect->y += rect->h - 1;
          dy += rect->h - 1;
     }

     start = rect->y * pixelpitch + rect->x + mdev->src_pixeloffset;

     rect->w--;

     end = rect->w;

     if (sgn & BLIT_LEFT) {
          start += rect->w;
          end = -end;
     }

     if (sgn & BLIT_UP)
          pixelpitch = -pixelpitch;

     if (mdev->blit_src_colorkey)
          dwgctl |= TRANSC;

     mga_waitfifo( mdrv, mdev, 7 );
     mga_out32( mmio, dwgctl, DWGCTL );
     mga_out32( mmio, pixelpitch & 0x3FFFFF, AR5 );
     mga_out32( mmio, start & 0xFFFFFF, AR3 );
     mga_out32( mmio, end & 0x3FFFF, AR0 );
     mga_out32( mmio, sgn, SGN );
     mga_out32( mmio, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxBlit3D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     __s32 startx, starty;

     startx = rect->x << (20 - mdev->matrox_w2);
     starty = rect->y << (20 - mdev->matrox_h2);


     mga_waitfifo( mdrv, mdev, 8);

     mga_out32( mmio, BOP_COPY | SHFTZERO | SGNZERO |
                ARZERO | ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );

     mga_out32( mmio, (0x10<<21) | MAG_NRST | MIN_NRST, TEXFILTER );

     mga_out32( mmio, 0x100000 >> mdev->matrox_w2, TMR0 );
     mga_out32( mmio, 0x100000 >> mdev->matrox_h2, TMR3 );
     mga_out32( mmio, startx, TMR6 );
     mga_out32( mmio, starty, TMR7 );
     mga_out32( mmio, (RS16(dx+rect->w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(rect->h), YDSTLEN | EXECUTE );
}

static void matroxStretchBlit( void *drv, void *dev,
                               DFBRectangle *srect, DFBRectangle *drect )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     __s32 startx, starty, incx, incy;

     incx = (srect->w << (20 - mdev->matrox_w2))  /  drect->w;
     incy = (srect->h << (20 - mdev->matrox_h2))  /  drect->h;

     startx = srect->x << (20 - mdev->matrox_w2);
     starty = srect->y << (20 - mdev->matrox_h2);


     mga_waitfifo( mdrv, mdev, 8);

     mga_out32( mmio, BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                      ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );

     mga_out32( mmio, (0x10<<21) | MAG_BILIN | MIN_BILIN, TEXFILTER );

     mga_out32( mmio, incx, TMR0 );
     mga_out32( mmio, incy, TMR3 );
     mga_out32( mmio, startx, TMR6 );
     mga_out32( mmio, starty, TMR7 );

     mga_out32( mmio, (RS16(drect->x+drect->w) << 16) | RS16(drect->x), FXBNDRY );
     mga_out32( mmio, (RS16(drect->y) << 16) | RS16(drect->h), YDSTLEN | EXECUTE );
}


/* exported symbols */

static int
driver_get_abi_version()
{
     return DFB_GRAPHICS_DRIVER_ABI_VERSION;
}

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_MATROX_MGA2064W:     /* Matrox MGA2064W (Millenium)   */
          case FB_ACCEL_MATROX_MGA1064SG:    /* Matrox MGA1064SG (Mystique)   */
          case FB_ACCEL_MATROX_MGA2164W:     /* Matrox MGA2164W (Millenium II)*/
          case FB_ACCEL_MATROX_MGA2164W_AGP: /* Matrox MGA2164W (Millenium II)*/
          case FB_ACCEL_MATROX_MGAG100:      /* Matrox G100                   */
          case FB_ACCEL_MATROX_MGAG200:      /* Matrox G200 (Myst, Mill, ...) */
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:      /* Matrox G400                   */
#endif
               return 1;
     }

     return 0;
}

static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Matrox G550/G450/G400/G200/G100/Millenium/Mystique Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 6;

     info->driver_data_size = sizeof (MatroxDriverData);
     info->device_data_size = sizeof (MatroxDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;

     mdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!mdrv->mmio_base)
          return DFB_IO;

     mdrv->accelerator = dfb_gfxcard_get_accelerator( device );

     switch (mdrv->accelerator) {
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:
               funcs->CheckState = matroxG400CheckState;
               break;
#endif
          case FB_ACCEL_MATROX_MGAG200:
               funcs->CheckState = matroxG200CheckState;
               break;

          case FB_ACCEL_MATROX_MGAG100:
               funcs->CheckState = matroxG100CheckState;
               break;

          case FB_ACCEL_MATROX_MGA2064W:
          case FB_ACCEL_MATROX_MGA1064SG:
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               funcs->CheckState = matroxOldCheckState;
               break;
     }

     funcs->SetState          = matroxSetState;
     funcs->EngineSync        = matroxEngineSync;
     funcs->FlushTextureCache = matroxFlushTextureCache;

     funcs->FillRectangle     = matroxFillRectangle;
     funcs->DrawRectangle     = matroxDrawRectangle;
     funcs->DrawLine          = matroxDrawLine;
     funcs->FillTriangle      = matroxFillTriangle;
     funcs->StretchBlit       = matroxStretchBlit;

     /* will be set dynamically: funcs->Blit */

     
     /* G200/G400/G450/G550 Backend Scaler Support */
     if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG200 ||
         mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
          dfb_layers_register( device, driver_data, &matroxBesFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;
     MatroxDeviceData *mdev = (MatroxDeviceData*) device_data;
     volatile __u8    *mmio = mdrv->mmio_base;

     switch (mdrv->accelerator) {
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "G400/G450/G550" );
               break;
#endif
          case FB_ACCEL_MATROX_MGAG200:
               dfb_config->argb_font = true;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "G200" );
               break;
          case FB_ACCEL_MATROX_MGAG100:
               mdev->old_matrox = 1;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "G100" );
               break;
          case FB_ACCEL_MATROX_MGA2064W:
               mdev->old_matrox = 1;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Millenium I" );
               break;
          case FB_ACCEL_MATROX_MGA1064SG:
               mdev->old_matrox = 1;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mystique" );
               break;
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               mdev->old_matrox = 1;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Millenium II" );
               break;
     }

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Matrox" );


     /* set hardware capabilities */
     device_info->caps.flags = CCF_CLIPPING;

     switch (mdrv->accelerator) {
#ifdef FB_ACCEL_MATROX_MGAG400
          case FB_ACCEL_MATROX_MGAG400:
               device_info->caps.accel    = MATROX_G200G400_DRAWING_FUNCTIONS |
                                            MATROX_G200G400_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_G200G400_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_G200G400_BLITTING_FLAGS;
               break;
#endif
          case FB_ACCEL_MATROX_MGAG200:
               device_info->caps.accel    = MATROX_G200G400_DRAWING_FUNCTIONS |
                                            MATROX_G200G400_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_G200G400_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_G200G400_BLITTING_FLAGS;
               break;

          case FB_ACCEL_MATROX_MGAG100:
               device_info->caps.accel    = MATROX_G100_DRAWING_FUNCTIONS |
                                            MATROX_G100_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_G100_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_G100_BLITTING_FLAGS;
               break;

          case FB_ACCEL_MATROX_MGA2064W:
          case FB_ACCEL_MATROX_MGA1064SG:
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               device_info->caps.accel    = MATROX_OLD_DRAWING_FUNCTIONS |
                                            MATROX_OLD_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_OLD_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_OLD_BLITTING_FLAGS;
               break;
     }

     /* set hardware limitations */
     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;


     mga_waitfifo( mdrv, mdev, 11 );
     mga_out32( mmio, 0, TDUALSTAGE0 );   /* multi texture registers */
     mga_out32( mmio, 0, TDUALSTAGE1 );
     mga_out32( mmio, 0, ALPHAXINC );     /* alpha increments        */
     mga_out32( mmio, 0, ALPHAYINC );
     mga_out32( mmio, 0, DR6 );           /* red increments          */
     mga_out32( mmio, 0, DR7 );
     mga_out32( mmio, 0, DR10 );          /* green increments        */
     mga_out32( mmio, 0, DR11 );
     mga_out32( mmio, 0, DR14 );          /* blue increments         */
     mga_out32( mmio, 0, DR15 );
     mga_out32( mmio, 0, BCOL );

     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, 0, TMR1 );
     mga_out32( mmio, 0, TMR2 );
     mga_out32( mmio, 0, TMR4 );
     mga_out32( mmio, 0, TMR5 );
     mga_out32( mmio, 0x10000, TMR8 );

     mdev->atype_blk_rstr = dfb_config->matrox_sgram ? ATYPE_BLK : ATYPE_RSTR;

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;
     MatroxDeviceData *mdev = (MatroxDeviceData*) device_data;

     /* reset DSTORG as matroxfb does not */
     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mdrv->mmio_base, 0, DSTORG );

     /* make sure overlay is off */
     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mdrv->mmio_base, 0, BESCTL );


     DEBUGMSG( "DirectFB/Matrox: FIFO Performance Monitoring:\n" );
     DEBUGMSG( "DirectFB/Matrox:  %9d matrox_waitfifo calls\n",
               mdev->waitfifo_calls );
     DEBUGMSG( "DirectFB/Matrox:  %9d register writes (matrox_waitfifo sum)\n",
               mdev->waitfifo_sum );
     DEBUGMSG( "DirectFB/Matrox:  %9d FIFO wait cycles (depends on CPU)\n",
               mdev->fifo_waitcycles );
     DEBUGMSG( "DirectFB/Matrox:  %9d IDLE wait cycles (depends on CPU)\n",
               mdev->idle_waitcycles );
     DEBUGMSG( "DirectFB/Matrox:  %9d FIFO space cache hits (depends on CPU)\n",
               mdev->fifo_cache_hits );
     DEBUGMSG( "DirectFB/Matrox: Conclusion:\n" );
     DEBUGMSG( "DirectFB/Matrox:  Average register writes/matrox_waitfifo call: %.2f\n",
               mdev->waitfifo_sum/(float)(mdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/Matrox:  Average wait cycles/matrox_waitfifo call:     %.2f\n",
               mdev->fifo_waitcycles/(float)(mdev->waitfifo_calls) );
     DEBUGMSG( "DirectFB/Matrox:  Average fifo space cache hits:                %02d%%\n",
               (int)(100 * mdev->fifo_cache_hits/(float)(mdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, mdrv->mmio_base, -1 );
}

