/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <linux/fb.h>

#include <math.h>

#include <directfb.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( matrox )

#include "regs.h"
#include "mmio.h"
#include "matrox.h"
#include "matrox_state.h"


static bool matroxBlit2D    ( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );

static bool matroxBlit2D_Old( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );

static bool matroxBlit3D    ( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy );




/* Millennium */

#define MATROX_2064W_DRAWING_FLAGS          (DSDRAW_NOFX)

#define MATROX_2064W_BLITTING_FLAGS         (DSBLIT_NOFX)

#define MATROX_2064W_DRAWING_FUNCTIONS      (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_2064W_BLITTING_FUNCTIONS     (DFXL_BLIT)


/* Old cards (Mystique, Millennium II) */

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
                                             DSBLIT_COLORIZE | \
                                             DSBLIT_DEINTERLACE)

#define MATROX_G200G400_DRAWING_FUNCTIONS   (DFXL_FILLRECTANGLE | \
                                             DFXL_DRAWRECTANGLE | \
                                             DFXL_DRAWLINE      | \
                                             DFXL_FILLTRIANGLE)

#define MATROX_G200G400_BLITTING_FUNCTIONS  (DFXL_BLIT          | \
                                             DFXL_STRETCHBLIT   | \
                                             DFXL_TEXTRIANGLES)


#define MATROX_USE_TMU(state, accel)                                     \
     ((state)->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |              \
                                DSBLIT_BLEND_COLORALPHA   |              \
                                DSBLIT_COLORIZE           |              \
                                DSBLIT_DEINTERLACE)            ||        \
      ((state)->destination->format != (state)->source->format &&        \
       (state)->destination->format != DSPF_I420               &&        \
       (state)->destination->format != DSPF_YV12)              ||        \
      (accel) & (DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES))


static void
matroxEngineReset( void *drv, void *dev )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mga_waitidle( mdrv, mdev );

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
     mga_out32( mmio, 0x100000, TMR8 );
}

static void
matroxEngineSync( void *drv, void *dev )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     mga_waitidle( mdrv, mdev );
}

static void
matroxFlushTextureCache( void *drv, void *dev )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     mga_waitfifo( mdrv, mdev, 1 );
     mga_out32( mdrv->mmio_base, 0, TEXORG1 );
}

static void
matrox2064WCheckState( void *drv, void *dev,
                       CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (!DFB_BLITTING_FUNCTION( accel ) ||
                   (state->source->format != DSPF_I420 &&
                    state->source->format != DSPF_YV12))
                    return;
          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_A8:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          if (state->drawingflags & ~MATROX_2064W_DRAWING_FLAGS)
               return;

          state->accel |= MATROX_2064W_DRAWING_FUNCTIONS;
     }
     else {
          switch (state->source->format) {
               case DSPF_I420:
               case DSPF_YV12:
                    if (state->destination->format == DSPF_I420 ||
                        state->destination->format == DSPF_YV12)
                         break;
               default:
                    if (state->source->format != state->destination->format)
                         return;
          }

          if (state->blittingflags & ~MATROX_2064W_BLITTING_FLAGS)
               return;

          state->accel |= MATROX_2064W_BLITTING_FUNCTIONS;
     }
}

static void
matroxOldCheckState( void *drv, void *dev,
                     CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (!DFB_BLITTING_FUNCTION( accel ) ||
                   (state->source->format != DSPF_I420 &&
                    state->source->format != DSPF_YV12))
                    return;
          case DSPF_RGB332:
          case DSPF_ARGB1555:
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
          switch (state->source->format) {
               case DSPF_I420:
               case DSPF_YV12:
                    if (state->destination->format == DSPF_I420 ||
                        state->destination->format == DSPF_YV12)
                         break;
               default:
                    if (state->source->format != state->destination->format)
                         return;
          }

          if (state->blittingflags & ~MATROX_OLD_BLITTING_FLAGS)
               return;

          state->accel |= MATROX_OLD_BLITTING_FUNCTIONS;
     }
}

static void
matroxG100CheckState( void *drv, void *dev,
                      CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (!DFB_BLITTING_FUNCTION( accel ) ||
                   (state->source->format != DSPF_I420 &&
                    state->source->format != DSPF_YV12))
                    return;
          case DSPF_RGB332:
          case DSPF_ARGB1555:
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
                    case DSPF_ARGB1555:
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
               switch (state->source->format) {
                    case DSPF_I420:
                    case DSPF_YV12:
                         if (state->destination->format != DSPF_I420 &&
                             state->destination->format != DSPF_YV12)
                              return;
                    default:
                         break;
               }
               /* source and destination formats equal, no stretching is done */
               state->accel |= accel;
          }
     }
}

static void
matroxG200CheckState( void *drv, void *dev,
                      CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (!DFB_BLITTING_FUNCTION( accel ) ||
                   (state->source->format != DSPF_I420 &&
                    state->source->format != DSPF_YV12))
                    return;
          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
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
          bool use_tmu = MATROX_USE_TMU( state, accel );

          switch (state->source->format) {
               case DSPF_I420:
               case DSPF_YV12:
                    if (state->destination->format != DSPF_I420 &&
                        state->destination->format != DSPF_YV12)
                         return;
               case DSPF_RGB332:
                    if (use_tmu)
                         return;
               case DSPF_ARGB1555:
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

static void
matroxG400CheckState( void *drv, void *dev,
                      CardState *state, DFBAccelerationMask accel )
{
     /* FIXME: 24bit support */
     switch (state->destination->format) {
          case DSPF_I420:
          case DSPF_YV12:
               if (!DFB_BLITTING_FUNCTION( accel ) ||
                   (state->source->format != DSPF_I420 &&
                    state->source->format != DSPF_YV12))
                    return;
          case DSPF_RGB332:
          case DSPF_ARGB1555:
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
          bool use_tmu = MATROX_USE_TMU( state, accel );

          switch (state->source->format) {
               case DSPF_RGB332:
                    if (use_tmu)
                         return;
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_A8:
               case DSPF_YUY2:
               case DSPF_UYVY:
                    break;
               case DSPF_I420:
               case DSPF_YV12:
                    if (state->destination->format == DSPF_I420 ||
                        state->destination->format == DSPF_YV12)
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

static void
matroxSetState( void *drv, void *dev,
                GraphicsDeviceFuncs *funcs,
                CardState *state, DFBAccelerationMask accel )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     if (state->modified == SMF_ALL) {
          mdev->valid = 0;

          /*
           * Work around TMU bug(?), under some unclear circumstances
           * the TMU's read address (src & dst) gets corrupted (negative offset
           * applied to written values) until soft reset occured.
           */
          if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG200)
               mga_waitidle( mdrv, mdev );
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
          case DFXL_TEXTRIANGLES: {
               DFBSurfaceBlittingFlags flags = state->blittingflags;

               mdev->blit_src_colorkey = flags & DSBLIT_SRC_COLORKEY;
               mdev->blit_deinterlace  = flags & DSBLIT_DEINTERLACE;

               if (MATROX_USE_TMU( state, accel )) {
                    if (flags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE))
                         matrox_validate_Color( mdrv, mdev, state );

                    funcs->Blit = matroxBlit3D;

                    matrox_validate_blitBlend( mdrv, mdev, state );
                    matrox_validate_Source( mdrv, mdev, state );

                    if (mdev->blit_src_colorkey)
                         matrox_validate_SrcKey( mdrv, mdev, state );

                    state->set = DFXL_BLIT |
                                 DFXL_STRETCHBLIT | DFXL_TEXTRIANGLES;
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
          }
          default:
               D_BUG( "unexpected drawing/blitting function!" );
               break;
     }

     if (state->modified & SMF_DESTINATION) {
          matrox_set_destination( mdrv, mdev, state->destination );

          /* On old cards the clip depends on the destination's pixel offset */
          if (mdev->old_matrox)
               state->modified |= SMF_CLIP;
     }

     if (state->modified & SMF_CLIP) {
          matrox_set_clip( mdrv, mdev, &state->clip );
          mdev->clip = state->clip;
     }

     state->modified = 0;
}

/******************************************************************************/

static bool
matroxFillRectangle( void *drv, void *dev, DFBRectangle *rect )
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

     return true;
}

static bool
matroxDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
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

     return true;
}

static bool
matroxDrawLine( void *drv, void *dev, DFBRegion *line )
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

     return true;
}

/******************************************************************************/

static void
matrox_fill_trapezoid( MatroxDriverData *mdrv,
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

static bool
matroxFillTriangle( void *drv, void *dev, DFBTriangle *tri )
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

     return true;
}

/******************************************************************************/

static void
matroxDoBlit2D_Old( MatroxDriverData *mdrv,
                    MatroxDeviceData *mdev,
                    int sx, int sy,
                    int dx, int dy,
                    int w,  int h,
                    int pitch, int offset )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO | ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = pitch;

     if (sx < dx)
          sgn |= BLIT_LEFT;
     if (sy < dy)
          sgn |= BLIT_UP;

     if (sgn & BLIT_UP) {
          sy += h - 1;
          dy += h - 1;
     }

     start = sy * pixelpitch + sx + offset;

     w--;

     end = w;

     if (sgn & BLIT_LEFT) {
          start += w;
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
     mga_out32( mmio, (RS16(dx+w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(h), YDSTLEN | EXECUTE );
}

static bool
matroxBlit2D_Old( void *drv, void *dev,
                  DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     matroxDoBlit2D_Old( mdrv, mdev,
                         rect->x, rect->y,
                         dx, dy,
                         rect->w, rect->h,
                         mdev->src_pitch,
                         mdev->src_offset );


     if (!mdev->planar)
          return true;

     rect->x /= 2; rect->y /= 2;
     rect->w /= 2; rect->h /= 2;
     dx /= 2; dy /= 2;

     /* Cb plane */
     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, mdev->dst_pitch/2, PITCH );
     mga_out32( mmio, mdev->dst_cb_offset, YDSTORG );

     mga_out32( mmio, (mdev->dst_cb_offset + mdev->dst_pitch * mdev->clip.y1 / 4) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_cb_offset + mdev->dst_pitch * mdev->clip.y2 / 4) & 0xFFFFFF, YBOT );
     mga_out32( mmio, ((mdev->clip.x2/2 & 0x0FFF) << 16) | (mdev->clip.x1/2 & 0x0FFF), CXBNDRY );

     matroxDoBlit2D_Old( mdrv, mdev,
                         rect->x, rect->y,
                         dx, dy,
                         rect->w, rect->h,
                         mdev->src_pitch/2,
                         mdev->src_cb_offset );

     /* Cr plane */
     mga_waitfifo( mdrv, mdev, 3 );
     mga_out32( mmio, mdev->dst_cr_offset, YDSTORG );

     mga_out32( mmio, (mdev->dst_cr_offset + mdev->dst_pitch * mdev->clip.y1 / 4) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_cr_offset + mdev->dst_pitch * mdev->clip.y2 / 4) & 0xFFFFFF, YBOT );

     matroxDoBlit2D_Old( mdrv, mdev,
                         rect->x, rect->y,
                         dx, dy,
                         rect->w, rect->h,
                         mdev->src_pitch/2,
                         mdev->src_cr_offset );

     /* Restore resisters */
     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, mdev->dst_pitch, PITCH );
     mga_out32( mmio, mdev->dst_offset, YDSTORG );

     mga_out32( mmio, (mdev->dst_offset + mdev->dst_pitch * mdev->clip.y1) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_offset + mdev->dst_pitch * mdev->clip.y2) & 0xFFFFFF, YBOT );
     mga_out32( mmio, ((mdev->clip.x2 & 0x0FFF) << 16) | (mdev->clip.x1 & 0x0FFF), CXBNDRY );

     return true;
}

/******************************************************************************/

static void
matroxDoBlit2D( MatroxDriverData *mdrv,
                MatroxDeviceData *mdev,
                int sx, int sy,
                int dx, int dy,
                int w,  int h,
                int pitch )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 dwgctl = BLTMOD_BFCOL | BOP_COPY | SHFTZERO | ATYPE_RSTR | OP_BITBLT;
     __u32 start, end;
     __u32 sgn = 0;
     __s32 pixelpitch = pitch;

     if (sx < dx)
          sgn |= BLIT_LEFT;
     if (sy < dy)
          sgn |= BLIT_UP;

     if (sgn & BLIT_UP) {
          sy += h - 1;
          dy += h - 1;
     }

     start = end = sy * pixelpitch + sx;

     w--;

     if (sgn & BLIT_LEFT)
          start += w;
     else
          end += w;

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
     mga_out32( mmio, (RS16(dx+w) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(h), YDSTLEN | EXECUTE );
}

static bool
matroxBlit2D( void *drv, void *dev,
              DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     matroxDoBlit2D( mdrv, mdev,
                     rect->x, rect->y,
                     dx, dy,
                     rect->w, rect->h,
                     mdev->src_pitch );


     if (!mdev->planar)
          return true;

     rect->x /= 2; rect->y /= 2;
     rect->w /= 2; rect->h /= 2;
     dx /= 2; dy /= 2;

     /* Cb plane */
     mga_waitfifo( mdrv, mdev, 6 );
     mga_out32( mmio, mdev->src_cb_offset, SRCORG );

     mga_out32( mmio, mdev->dst_pitch/2, PITCH );
     mga_out32( mmio, mdev->dst_cb_offset, DSTORG );

     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y1 / 4) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y2 / 4) & 0xFFFFFF, YBOT );
     mga_out32( mmio, ((mdev->clip.x2/2 & 0x0FFF) << 16) | (mdev->clip.x1/2 & 0x0FFF), CXBNDRY );

     matroxDoBlit2D( mdrv, mdev,
                     rect->x, rect->y,
                     dx, dy,
                     rect->w, rect->h,
                     mdev->src_pitch/2 );

     /* Cr plane */
     mga_waitfifo( mdrv, mdev, 2 );
     mga_out32( mmio, mdev->src_cr_offset, SRCORG );

     mga_out32( mmio, mdev->dst_cr_offset, DSTORG );

     matroxDoBlit2D( mdrv, mdev,
                     rect->x, rect->y,
                     dx, dy,
                     rect->w, rect->h,
                     mdev->src_pitch/2 );

     /* Restore registers */
     mga_waitfifo( mdrv, mdev, 6 );
     mga_out32( mmio, mdev->src_offset, SRCORG );

     mga_out32( mmio, mdev->dst_pitch, PITCH );
     mga_out32( mmio, mdev->dst_offset, DSTORG );

     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y1) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y2) & 0xFFFFFF, YBOT );
     mga_out32( mmio, ((mdev->clip.x2 & 0x0FFF) << 16) | (mdev->clip.x1 & 0x0FFF), CXBNDRY );

     return true;
}

/******************************************************************************/

static inline void
matroxDoBlitTMU( MatroxDriverData *mdrv,
                 MatroxDeviceData *mdev,
                 int sx, int sy,
                 int dx, int dy,
                 int sw, int sh,
                 int dw, int dh,
                 int w2, int h2,
                 bool filter )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __s32 startx, starty, incx, incy;

     if (mdev->blit_deinterlace) {
          sy /= 2;
          sh /= 2;
     }

     incx = (sw << (20 - w2))  /  dw;
     incy = (sh << (20 - h2))  /  dh;

     startx = sx << (20 - w2);
     starty = sy << (20 - h2);

     if (mdev->blit_deinterlace && !mdev->field)
          starty += (0x80000 >> h2);

     mga_waitfifo( mdrv, mdev, 8);

     mga_out32( mmio, BOP_COPY | SHFTZERO | SGNZERO | ARZERO |
                      ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );

     if (filter)
          mga_out32( mmio, (0x10<<21) | MAG_BILIN | MIN_BILIN, TEXFILTER );
     else
          mga_out32( mmio, (0x10<<21) | MAG_NRST  | MIN_NRST,  TEXFILTER );

     mga_out32( mmio, incx, TMR0 );
     mga_out32( mmio, incy, TMR3 );
     mga_out32( mmio, startx, TMR6 );
     mga_out32( mmio, starty, TMR7 );
     mga_out32( mmio, (RS16(dx+dw) << 16) | RS16(dx), FXBNDRY );
     mga_out32( mmio, (RS16(dy) << 16) | RS16(dh), YDSTLEN | EXECUTE );
}

static inline void
matroxBlitTMU( MatroxDriverData *mdrv,
               MatroxDeviceData *mdev,
               DFBRectangle *srect,
               DFBRectangle *drect,
               bool filter )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u32          texctl;

     matroxDoBlitTMU( mdrv, mdev,
                      srect->x, srect->y,
                      drect->x, drect->y,
                      srect->w, srect->h,
                      drect->w, drect->h,
                      mdev->w2, mdev->h2,
                      filter );


     if (!mdev->planar)
          return;

     srect->x /= 2; srect->y /= 2;
     srect->w /= 2; srect->h /= 2;
     drect->x /= 2; drect->y /= 2;
     drect->w /= 2; drect->h /= 2;

     texctl  = mdev->texctl & ~(0x7ff << 9);
     texctl |= (mdev->src_pitch/2 & 0x7ff) << 9;

     /* Cb plane */
     mga_waitfifo( mdrv, mdev, 9 );
     mga_out32( mmio, texctl, TEXCTL );
     mga_out32( mmio, ( (((__u32)(mdev->w/2 - 1) & 0x7ff) << 18) |
                        (((__u32)(3 - mdev->w2) & 0x3f) <<  9) |
                        (((__u32)(mdev->w2 + 3) & 0x3f)      )  ), TEXWIDTH );
     mga_out32( mmio, ( (((__u32)(mdev->h/2 - 1) & 0x7ff) << 18) |
                        (((__u32)(3 - mdev->h2) & 0x3f) <<  9) |
                        (((__u32)(mdev->h2 + 3) & 0x3f)      )  ), TEXHEIGHT );
     mga_out32( mmio, mdev->src_cb_offset, TEXORG );

     mga_out32( mmio, mdev->dst_pitch/2, PITCH );
     mga_out32( mmio, mdev->dst_cb_offset, DSTORG );

     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y1 / 4) & 0xFFFFFF, YTOP );
     mga_out32( mmio, (mdev->dst_pitch * mdev->clip.y2 / 4) & 0xFFFFFF, YBOT );
     mga_out32( mmio, ((mdev->clip.x2/2 & 0x0FFF) << 16) | (mdev->clip.x1/2 & 0x0FFF), CXBNDRY );

     matroxDoBlitTMU( mdrv, mdev,
                      srect->x, srect->y,
                      drect->x, drect->y,
                      srect->w, srect->h,
                      drect->w, drect->h,
                      mdev->w2-1, mdev->h2-1, true );

     /* Cr plane */
     mga_waitfifo( mdrv, mdev, 2 );
     mga_out32( mmio, mdev->src_cr_offset, TEXORG );

     mga_out32( mmio, mdev->dst_cr_offset, DSTORG );

     matroxDoBlitTMU( mdrv, mdev,
                      srect->x, srect->y,
                      drect->x, drect->y,
                      srect->w, srect->h,
                      drect->w, drect->h,
                      mdev->w2-1, mdev->h2-1, true );

     /* Restore registers */
     mga_waitfifo( mdrv, mdev, 6 );
     mga_out32( mmio, mdev->texctl, TEXCTL );
     mga_out32( mmio, ( (((__u32)(mdev->w - 1) & 0x7ff) << 18) |
                        (((__u32)(4 - mdev->w2) & 0x3f) <<  9) |
                        (((__u32)(mdev->w2 + 4) & 0x3f)      )  ), TEXWIDTH );
     mga_out32( mmio, ( (((__u32)(mdev->h - 1) & 0x7ff) << 18) |
                        (((__u32)(4 - mdev->h2) & 0x3f) <<  9) |
                        (((__u32)(mdev->h2 + 4) & 0x3f)      )  ), TEXHEIGHT );
     mga_out32( mmio, mdev->src_offset, TEXORG );

     mga_out32( mmio, mdev->dst_pitch, PITCH );
     mga_out32( mmio, mdev->dst_offset, DSTORG );

     matrox_set_clip( mdrv, mdev, &mdev->clip );
}

static bool
matroxStretchBlit( void *drv, void *dev,
                   DFBRectangle *srect, DFBRectangle *drect )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;

     matroxBlitTMU( mdrv, mdev, srect, drect, true );

     return true;
}

static bool
matroxBlit3D( void *drv, void *dev,
              DFBRectangle *rect, int dx, int dy )
{
     MatroxDriverData *mdrv  = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev  = (MatroxDeviceData*) dev;

     DFBRectangle      drect = { dx, dy, rect->w, rect->h };

     matroxBlitTMU( mdrv, mdev, rect, &drect, mdev->blit_deinterlace );

     return true;
}

/******************************************************************************/


#ifdef ARCH_X86
#define RINT(x) my_rint(x)
#define CEIL(x) my_ceil(x)
#define FLOOR(x) my_floor(x)
#else
#define RINT(x) ((__s32)(x))
#define CEIL(x) ((__s32)ceil(x))
#define FLOOR(x) ((__s32)floor(x))
#endif

static __inline__ long my_rint(const float x)
{
  register float arg = x;
  long result;
  __asm__ ("fistl %0" : "=m" (result) : "t" (arg));
  return result;
}

static __inline__ long my_ceil(const float x)
{
  register float arg = x;
  volatile long value;
  volatile short cw, cwtmp;

  __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
  cwtmp = (cw & 0xf3ff) | 0x0800; /* rounding up */
  __asm__ volatile ("fldcw %1\n"
                    "fistl %0\n"
                    "fldcw %2"
                    : "=m" (value)
                    : "m" (cwtmp), "m" (cw), "t" (arg));
  return value;
}

static __inline__ long my_floor(const float x)
{
  register float arg = x;
  volatile long value;
  volatile short cw, cwtmp;

  __asm__ volatile ("fnstcw %0" : "=m" (cw) : );
  cwtmp = (cw & 0xf3ff) | 0x0400;
  __asm__ volatile ("fldcw %1\n"
                    "fistl %0\n"
                    "fldcw %2"
                    : "=m" (value)
                    : "m" (cwtmp), "m" (cw), "t" (arg));
  return value;
}


#define F2COL(x) (RINT(x) & 0x00ffffff)

#define mgaF1800(x) (((__s32) (x)) & 0x0003ffff)
#define mgaF2400(x) (((__s32) (x)) & 0x00ffffff)
#define mgaF2200(x) (((__s32) (x)) & 0x003fffff)

#define OUTREG(r,d)  do { mga_out32( mmio, d, r ); } while (0)

#define MGA_S(start,xinc,yinc) do {		\
    OUTREG(TMR6,start);			\
    OUTREG(TMR0,xinc);			\
    OUTREG(TMR1,yinc);			\
  } while (0)
#define MGA_T(start,xinc,yinc) do {		\
    OUTREG(TMR7,start);			\
    OUTREG(TMR2,xinc);			\
    OUTREG(TMR3,yinc);			\
  } while (0)
#define MGA_Q(start,xinc,yinc) do {		\
    OUTREG(TMR8,start);			\
    OUTREG(TMR4,xinc);			\
    OUTREG(TMR5,yinc);			\
  } while (0)


#define MGA_LSLOPE(dx,dy,sgn,err) do {			\
    OUTREG(AR0,mgaF1800(dy));			\
    if ((dx) >= 0) {					\
      OUTREG(AR1,mgaF2400(-(dx)+(err)));		\
      OUTREG(AR2,mgaF1800(-(dx)));		\
      sgn &= ~SDXL;                                     \
    } else {						\
      OUTREG(AR1,mgaF2400((dx)+(dy)-(err)-1) );	\
      OUTREG(AR2,mgaF1800(dx));			\
      sgn |= SDXL;                                      \
    }							\
  } while(0)


#define MGA_G400_LSLOPE(dx,dy,sgn,err) do {                        \
     OUTREG(AR0,mgaF2200(dy));                  \
     if ((dx) >= 0) {                                  \
      OUTREG(AR1,mgaF2400(-(dx)+(err)));               \
      OUTREG(AR2,mgaF2200(-(dx)));             \
      sgn &= ~SDXL;                                     \
     } else {                                          \
      OUTREG(AR1,mgaF2400((dx)+(dy)-(err)-1) );        \
      OUTREG(AR2,mgaF2200(dx));                        \
      sgn |= SDXL;                                      \
   }                                                 \
  } while(0)

#define MGA_RSLOPE(dx,dy,sgn,err) do {			\
    OUTREG(AR6,mgaF1800(dy));			\
    if ((dx) >= 0) {					\
      OUTREG(AR4,mgaF1800(-(dx)+(err)));		\
      OUTREG(AR5,mgaF1800(-(dx)));		\
      sgn &= ~SDXR;                                      \
    } else {						\
      OUTREG(AR4,mgaF1800((dx)+(dy)-(err)-1));		\
      OUTREG(AR5,mgaF1800(dx));			\
      sgn |= SDXR;                                      \
    }							\
  } while(0)

#define MGA_G400_RSLOPE(dx,dy,sgn,err) do {                     \
     OUTREG(AR6,mgaF2200(dy));                       \
     if ((dx) >= 0) {                                  \
       OUTREG(AR4,mgaF2200(-(dx)+(err)));            \
       OUTREG(AR5,mgaF2200(-(dx)));          \
       sgn &= ~SDXR;                                      \
     } else {                                          \
       OUTREG(AR4,mgaF2200((dx)+(dy)-(err)-1));              \
       OUTREG(AR5,mgaF2200(dx));                     \
       sgn |= SDXR;                                      \
     }                                                 \
   } while(0)


typedef struct {
     DFBVertex *v0, *v1;  /* Y(v0) < Y(v1) */
     float      dx;          /* X(v1) - X(v0) */
     float      dy;          /* Y(v1) - Y(v0) */
     float      dxOOA;          /* dx * oneOverArea */
     float      dyOOA;          /* dy * oneOverArea */

     float      adjx,adjy; /* subpixel offset after rounding to integer */
     int        err;        /* error term ready for hardware */
     int        idx,idy;    /* delta-x & delta-y ready for hardware */
     int        sx,sy;          /* first sample point x,y coord */
     int        lines;      /* number of lines to be sampled on this edge */
} EdgeT;


static void
texture_triangle( MatroxDriverData *mdrv, MatroxDeviceData *mdev,
                  DFBVertex *v0, DFBVertex *v1, DFBVertex *v2 )
{
     EdgeT       eMaj, eTop, eBot;
     float       oneOverArea;
     DFBVertex  *vMin, *vMid, *vMax; /* Y(vMin)<=Y(vMid)<=Y(vMax) */
     int         Shape; /* 1 = Top half, 2 = bottom half, 3 = top+bottom */
//     float       bf = mga_bf_sign;

     volatile __u8 *mmio = mdrv->mmio_base;

/* find the order of the 3 vertices along the Y axis */
     {
          float y0 = v0->y;
          float y1 = v1->y;
          float y2 = v2->y;

          if (y0<=y1) {
               if (y1<=y2) {
                    vMin = v0;   vMid = v1;   vMax = v2;   /* y0<=y1<=y2 */
               }
               else if (y2<=y0) {
                    vMin = v2;   vMid = v0;   vMax = v1;   /* y2<=y0<=y1 */
               }
               else {
                    vMin = v0;   vMid = v2;   vMax = v1; /*bf = -bf;*/  /* y0<=y2<=y1 */
               }
          }
          else {
               if (y0<=y2) {
                    vMin = v1;   vMid = v0;   vMax = v2; /*bf = -bf;*/  /* y1<=y0<=y2 */
               }
               else if (y2<=y1) {
                    vMin = v2;   vMid = v1;   vMax = v0; /*bf = -bf;*/  /* y2<=y1<=y0 */
               }
               else {
                    vMin = v1;   vMid = v2;   vMax = v0;   /* y1<=y2<=y0 */
               }
          }
     }

/* vertex/edge relationship */
     eMaj.v0 = vMin;   eMaj.v1 = vMax;
     eTop.v0 = vMin;   eTop.v1 = vMid;
     eBot.v0 = vMid;   eBot.v1 = vMax;

/* compute deltas for each edge:  vertex[v1] - vertex[v0] */
     eMaj.dx = vMax->x - vMin->x;
     eMaj.dy = vMax->y - vMin->y;
     eTop.dx = vMid->x - vMin->x;
     eTop.dy = vMid->y - vMin->y;
     eBot.dx = vMax->x - vMid->x;
     eBot.dy = vMax->y - vMid->y;


/* compute oneOverArea */
     {
          float area = eMaj.dx * eBot.dy - eBot.dx * eMaj.dy;

          /* Do backface culling
           */
          //if ( area * bf < 0 || area == 0 )
               //return;

          oneOverArea = 1.0F / area;
     }

/* Edge setup.  For a triangle strip these could be reused... */
     {

#define DELTASCALE 16 /* Scaling factor for idx and idy. Note that idx and
                         idy are 18 bits signed, so don't choose too big
                         value. */

          int   ivMax_y;
          float temp;

          ivMax_y = CEIL(vMax->y);
          eTop.sy = eMaj.sy = CEIL(vMin->y);
          eBot.sy = CEIL(vMid->y);

          eMaj.lines = ivMax_y - eMaj.sy;
          if (eMaj.lines > 0) {
               float dxdy = eMaj.dx / eMaj.dy;
               eMaj.adjy = (float) eMaj.sy - vMin->y;
               temp = vMin->x + eMaj.adjy*dxdy;
               eMaj.sx = CEIL(temp);
               eMaj.adjx = (float) eMaj.sx - vMin->x;
               if (eMaj.lines == 1) {
                    eMaj.idy = 1;
                    eMaj.idx = 0;
                    eMaj.err = 0;
               }
               else {
                    eMaj.idy = RINT(eMaj.dy * DELTASCALE);
                    eMaj.idx = FLOOR(eMaj.idy * dxdy);
                    eMaj.err = RINT(((float) eMaj.sx - temp) * (float)eMaj.idy);
               }
          }
          else {
               return; /* CULLED */
          }

          Shape = 3;

          eBot.lines = ivMax_y - eBot.sy;
          if (eBot.lines > 0) {
               float dxdy = eBot.dx / eBot.dy;
               eBot.adjy = (float) eBot.sy - vMid->y;
               temp = vMid->x + eBot.adjy*dxdy;
               eBot.sx = CEIL(temp);
               eBot.adjx = (float) eBot.sx - vMid->x;
               if (eBot.lines == 1) {
                    eBot.idy = 1;
                    eBot.idx = 0;
                    eBot.err = 0;
               }
               else {
                    eBot.idy = RINT(eBot.dy * DELTASCALE);
                    eBot.idx = FLOOR(eBot.idy * dxdy);
                    eBot.err = RINT(((float) eBot.sx - temp) * (float)eBot.idy);
               }
          }
          else {
               Shape = 1;
          }

          eTop.lines = eBot.sy - eTop.sy;
          if (eTop.lines > 0) {
               float dxdy = eTop.dx / eTop.dy;
               eTop.adjy = eMaj.adjy;
               temp = vMin->x + eTop.adjy*dxdy;
               eTop.sx = CEIL(temp);
               eTop.adjx = (float) eTop.sx - vMin->x;
               if (eTop.lines == 1) {
                    eTop.idy = 1;
                    if (eBot.lines > 0) {
                         eTop.idx = eBot.sx - eTop.sx; /* needed for bottom half */
                    }
                    else {
                         eTop.idx = 0;
                    }
                    eTop.err = 0;
               }
               else {
                    eTop.idy = RINT(eTop.dy * DELTASCALE);
                    eTop.idx = FLOOR(eTop.idy * dxdy);
                    eTop.err = RINT(((float) eTop.sx - temp) * (float)eTop.idy);
               }
          }
          else {
               Shape = 2;
          }
     }

     {
          int ltor;        /* true if scanning left-to-right */
          EdgeT *eLeft, *eRight;
          int lines;
          DFBVertex *vTL;      /* Top left vertex */
          float adjx, adjy;

          /*
           * Execute user-supplied setup code
           */
#ifdef SETUP_CODE
          SETUP_CODE
#endif

          ltor = (oneOverArea > 0.0F);

          if (Shape == 2) {
               /* bottom half triangle */
               if (ltor) {
                    eLeft = &eMaj;
                    eRight = &eBot;
               }
               else {
                    eLeft = &eBot;
                    eRight = &eMaj;
               }
               lines = eBot.lines;
          }
          else {
               /* top half triangle */
               if (ltor) {
                    eLeft = &eMaj;
                    eRight = &eTop;
               }
               else {
                    eLeft = &eTop;
                    eRight = &eMaj;
               }
               lines = eTop.lines;
          }

          vTL = eLeft->v0;
          adjx = eLeft->adjx; adjy = eLeft->adjy;


          /* setup derivatives */
/* compute d?/dx and d?/dy derivatives */
          eBot.dxOOA = eBot.dx * oneOverArea;
          eBot.dyOOA = eBot.dy * oneOverArea;
          eMaj.dxOOA = eMaj.dx * oneOverArea;
          eMaj.dyOOA = eMaj.dy * oneOverArea;

#define DERIV( DZ, COMP) \
  { \
    float eMaj_DZ, eBot_DZ; \
    eMaj_DZ = vMax->COMP - vMin->COMP; \
    eBot_DZ = vMax->COMP - vMid->COMP; \
    DZ ## dx = eMaj_DZ * eBot.dyOOA - eMaj.dyOOA * eBot_DZ;    \
    DZ ## dy = eMaj.dxOOA * eBot_DZ - eMaj_DZ * eBot.dxOOA;    \
  }

          {
               float dsdx, dsdy;
               float dtdx, dtdy;
               float dvdx, dvdy;

               mga_waitfifo( mdrv, mdev, 9 );

               DERIV(ds,s);

               MGA_S(RINT( (vTL->s+dsdx*adjx+dsdy*adjy) ),
                     RINT( dsdx ), RINT( dsdy ));

               DERIV(dt,t);

               MGA_T(RINT( (vTL->t+dtdx*adjx+dtdy*adjy) ),
                     RINT( dtdx ), RINT( dtdy ));

               DERIV(dv,w);
               {
                    int sq = RINT( (vTL->w+dvdx*adjx+dvdy*adjy) );
                    MGA_Q((sq == 0) ? 1 : sq,RINT(dvdx),RINT(dvdy));
               }
          }

          {
               __u32 sgn = 0;

               mga_waitfifo( mdrv, mdev, 9 );

               /* Draw part #1 */
               if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400) {
                    MGA_G400_LSLOPE(eLeft->idx,eLeft->idy,sgn,eLeft->err);
                    MGA_G400_RSLOPE(eRight->idx,eRight->idy,sgn,eRight->err);
               }
               else {
                    MGA_LSLOPE(eLeft->idx,eLeft->idy,sgn,eLeft->err);
                    MGA_RSLOPE(eRight->idx,eRight->idy,sgn,eRight->err);
               }

               mga_out32( mmio, sgn,                            SGN );
               mga_out32( mmio, ((__u32)(eLeft->sx) & 0xFFFF) | ((__u32)(eRight->sx) << 16), FXBNDRY );
               mga_out32( mmio, lines     | ((__u32)(eLeft->sy)  << 16), YDSTLEN | EXECUTE );

               if (Shape != 3) { /* has only one half? */
                    return;
               }

               mga_waitfifo( mdrv, mdev, 6 );

               /* Draw part #2 */
               if (ltor) {
                    if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
                         MGA_G400_RSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);
                    else
                         MGA_RSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);

                    mga_out32( mmio, eBot.sx, FXRIGHT );
               }
               else {
                    sgn |= SGN_BRKLEFT;
                    mga_out32( mmio, eBot.sx, FXLEFT );
                    if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
                         MGA_G400_LSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);
                    else
                         MGA_LSLOPE(eBot.idx,eBot.idy,sgn,eBot.err);

               }

               mga_out32( mmio, sgn,        SGN );
               mga_out32( mmio, eBot.lines, LEN | EXECUTE );
          }
     }
}

static bool matroxTextureTriangles( void *drv, void *dev,
                                    DFBVertex *vertices, int num,
                                    DFBTriangleFormation formation )
{
     int               i;
     MatroxDriverData *mdrv = (MatroxDriverData*) drv;
     MatroxDeviceData *mdev = (MatroxDeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     float wScale = 1 << 20;


     for (i=0; i<num; i++) {
          DFBVertex *v = &vertices[i];

          v->x -= 0.5f;
          v->y -= 0.5f;
          v->z *= 1 << 15;
          v->w *= wScale;

          v->s *= v->w * (float) mdev->w / (float) (1 << mdev->w2);
          v->t *= v->w * (float) mdev->h / (float) (1 << mdev->h2);
//          v->t *= (1 << (20 - mdev->h2)) * (float) mdev->h * v->w;
     }

     mga_waitfifo( mdrv, mdev, 2 );

     mga_out32( mmio, BOP_COPY | SHFTZERO | ATYPE_I | OP_TEXTURE_TRAP, DWGCTL );
     mga_out32( mmio, (0x10<<21) | MAG_BILIN | MIN_ANISO, TEXFILTER );

     switch (formation) {
          case DTTF_LIST:
               for (i=0; i<num; i+=3)
                    texture_triangle( mdrv, mdev, &vertices[i], &vertices[i+1], &vertices[i+2] );

               break;

          case DTTF_STRIP:
               texture_triangle( mdrv, mdev, &vertices[0],
                                 &vertices[1], &vertices[2] );

               for (i=3; i<num; i++)
                    texture_triangle( mdrv, mdev, &vertices[i-2],
                                      &vertices[i-1], &vertices[i] );

               break;

          case DTTF_FAN:
               texture_triangle( mdrv, mdev, &vertices[0],
                                 &vertices[1], &vertices[2] );

               for (i=3; i<num; i++)
                    texture_triangle( mdrv, mdev, &vertices[0],
                                      &vertices[i-1], &vertices[i] );

               break;

          default:
               D_ONCE( "unknown formation" );
               return false;
     }

     mga_waitfifo( mdrv, mdev, 5 );
     mga_out32( mmio, 0, TMR1 );
     mga_out32( mmio, 0, TMR2 );
     mga_out32( mmio, 0, TMR4 );
     mga_out32( mmio, 0, TMR5 );
     mga_out32( mmio, 0x100000, TMR8 );

     return true;
}

/******************************************************************************/

static __u32 pci_config_in32( unsigned int bus,
                              unsigned int slot,
                              unsigned int func,
                              __u8 reg )
{
     char  filename[512];
     int   fd;
     __u32 val;

     snprintf( filename, 512,
               "/proc/bus/pci/%02x/%02x.%x",
               bus, slot, func );

     fd = open( filename, O_RDONLY );
     if (fd < 0)
          return 0;

     if (lseek( fd, reg, SEEK_SET ) != reg) {
          close( fd );
          return 0;
     }

     if (read( fd, &val, 4 ) != 4) {
          close( fd );
          return 0;
     }

     close( fd );
     return val;
}

static DFBResult matrox_find_pci_device( MatroxDeviceData *mdev,
                                         unsigned int     *bus,
                                         unsigned int     *slot,
                                         unsigned int     *func )
{
     unsigned int   vendor, device, devfn;
     unsigned long  addr0, addr1;
     char           line[512];
     FILE          *file;

     file = fopen( "/proc/bus/pci/devices", "r" );
     if (!file) {
          D_PERROR( "DirectFB/Matrox: "
                     "Error opening `/proc/bus/pci/devices'!\n" );
          return errno2result( errno );
     }

     while (fgets( line, 512, file )) {
          if (sscanf( line, "%02x%02x\t%04x%04x\t%*x\t%lx\t%lx",
                      bus, &devfn, &vendor, &device, &addr0, &addr1 ) != 6)
               continue;

          if (vendor != PCI_VENDOR_ID_MATROX)
               continue;

          *slot = (devfn >> 3) & 0x1F;
          *func = devfn & 0x07;

          addr0 &= ~0xFUL;
          addr1 &= ~0xFUL;

          switch (device) {
          case PCI_DEVICE_ID_MATROX_G550:
          case PCI_DEVICE_ID_MATROX_G400:
          case PCI_DEVICE_ID_MATROX_G200_PCI:
          case PCI_DEVICE_ID_MATROX_G200_AGP:
          case PCI_DEVICE_ID_MATROX_G100_MM:
          case PCI_DEVICE_ID_MATROX_G100_AGP:
          case PCI_DEVICE_ID_MATROX_MIL_2:
          case PCI_DEVICE_ID_MATROX_MIL_2_AGP:
               if (addr0 == mdev->fb.physical) {
                    fclose( file );
                    return DFB_OK;
               }
               break;

          case PCI_DEVICE_ID_MATROX_MYS:
               if ((pci_config_in32( *bus, *slot, *func, 0x08 ) & 0xFF) >= 0x02) {
                    /* Mystique 220 */
                    if (addr0 == mdev->fb.physical) {
                         fclose( file );
                         return DFB_OK;
                    }
               } else {
                    /* Mystique */
                    if (addr1 == mdev->fb.physical) {
                         fclose( file );
                         return DFB_OK;
                    }
               }
               break;

          case PCI_DEVICE_ID_MATROX_MIL:
               if (addr1 == mdev->fb.physical) {
                    fclose( file );
                    return DFB_OK;
               }
               break;
          }
     }

     D_ERROR( "DirectFB/Matrox: Can't find device in `/proc/bus/pci'!\n" );

     fclose( file );
     return DFB_INIT;
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_MATROX_MGA2064W:     /* Matrox MGA2064W (Millennium)   */
          case FB_ACCEL_MATROX_MGA1064SG:    /* Matrox MGA1064SG (Mystique)   */
          case FB_ACCEL_MATROX_MGA2164W:     /* Matrox MGA2164W (Millennium II)*/
          case FB_ACCEL_MATROX_MGA2164W_AGP: /* Matrox MGA2164W (Millennium II)*/
          case FB_ACCEL_MATROX_MGAG100:      /* Matrox G100                   */
          case FB_ACCEL_MATROX_MGAG200:      /* Matrox G200 (Myst, Mill, ...) */
          case FB_ACCEL_MATROX_MGAG400:      /* Matrox G400                   */
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
               "Matrox G550/G450/G400/G200/G100/Millennium/Mystique Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 7;

     info->driver_data_size = sizeof (MatroxDriverData);
     info->device_data_size = sizeof (MatroxDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     MatroxDriverData *mdrv = driver_data;

     mdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!mdrv->mmio_base)
          return DFB_IO;

     mdrv->device_data = device_data;
     mdrv->maven_fd    = -1;
     mdrv->accelerator = dfb_gfxcard_get_accelerator( device );

     switch (mdrv->accelerator) {
          case FB_ACCEL_MATROX_MGAG400:
               funcs->CheckState = matroxG400CheckState;
               break;

          case FB_ACCEL_MATROX_MGAG200:
               dfb_config->argb_font = true;
               funcs->CheckState = matroxG200CheckState;
               break;

          case FB_ACCEL_MATROX_MGAG100:
               funcs->CheckState = matroxG100CheckState;
               break;

          case FB_ACCEL_MATROX_MGA1064SG:
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               funcs->CheckState = matroxOldCheckState;
               break;

          case FB_ACCEL_MATROX_MGA2064W:
               funcs->CheckState = matrox2064WCheckState;
               break;
     }

     funcs->SetState          = matroxSetState;
     funcs->EngineReset       = matroxEngineReset;
     funcs->EngineSync        = matroxEngineSync;
     funcs->FlushTextureCache = matroxFlushTextureCache;

     funcs->FillRectangle     = matroxFillRectangle;
     funcs->DrawRectangle     = matroxDrawRectangle;
     funcs->DrawLine          = matroxDrawLine;
     funcs->FillTriangle      = matroxFillTriangle;
     funcs->StretchBlit       = matroxStretchBlit;
     funcs->TextureTriangles  = matroxTextureTriangles;

     /* will be set dynamically: funcs->Blit */

     /* Generic CRTC1 support */
     mdrv->primary = dfb_screens_at( DSCID_PRIMARY );

     /* G200/G400/G450/G550 Backend Scaler Support */
     if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG200 ||
         mdrv->accelerator == FB_ACCEL_MATROX_MGAG400)
          dfb_layers_register( mdrv->primary, driver_data, &matroxBesFuncs );

     /* G400/G450/G550 CRTC2 support */
     if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG400 &&
         dfb_config->matrox_crtc2)
     {
          mdrv->secondary = dfb_screens_register( device, driver_data,
                                                  &matroxCrtc2ScreenFuncs );

          dfb_layers_register( mdrv->secondary, driver_data, &matroxCrtc2Funcs );
          dfb_layers_register( mdrv->secondary, driver_data, &matroxSpicFuncs );
     }

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
     unsigned int      bus, slot, func;
     bool              g450, g550, sgram = false;
     DFBResult         ret;

     mdev->fb.physical = dfb_gfxcard_memory_physical( device, 0 );
     mdev->fb.offset   = mdev->fb.physical & 0x1FFFFFF;

     switch (mdrv->accelerator) {
          case FB_ACCEL_MATROX_MGAG400:
               if ((ret = matrox_find_pci_device( mdev, &bus, &slot, &func )))
                    return ret;

               g550  = ((pci_config_in32( bus, slot, func, 0x00 ) >> 16) ==
                        PCI_DEVICE_ID_MATROX_G550);
               g450  = ((pci_config_in32( bus, slot, func, 0x08 ) & 0xFF) >= 0x80);
               sgram = ((pci_config_in32( bus, slot, func, 0x40 ) & 0x4000) == 0x4000);
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "%s",
                         g550 ? "G550" : g450 ? "G450" : "G400" );
               mdev->g450_matrox = g450 || g550;
               break;
          case FB_ACCEL_MATROX_MGAG200:
               if ((ret = matrox_find_pci_device( mdev, &bus, &slot, &func )))
                    return ret;

               sgram = ((pci_config_in32( bus, slot, func, 0x40 ) & 0x4000) == 0x4000);
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "G200" );
               break;
          case FB_ACCEL_MATROX_MGAG100:
               mdev->old_matrox = true;
               sgram            = true;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "G100" );
               break;
          case FB_ACCEL_MATROX_MGA2064W:
               mdev->old_matrox = true;
               sgram            = true;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Millennium I" );
               break;
          case FB_ACCEL_MATROX_MGA1064SG:
               if ((ret = matrox_find_pci_device( mdev, &bus, &slot, &func )))
                    return ret;

               mdev->old_matrox = true;
               sgram            = ((pci_config_in32( bus, slot, func, 0x40 ) & 0x4000) == 0x4000);
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mystique" );
               break;
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               mdev->old_matrox = true;
               sgram            = true;
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Millennium II" );
               break;
     }

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Matrox" );


     /* set hardware capabilities */
     device_info->caps.flags = CCF_CLIPPING;

     switch (mdrv->accelerator) {
          case FB_ACCEL_MATROX_MGAG400:
               device_info->caps.accel    = MATROX_G200G400_DRAWING_FUNCTIONS |
                                            MATROX_G200G400_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_G200G400_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_G200G400_BLITTING_FLAGS;
               break;

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

          case FB_ACCEL_MATROX_MGA1064SG:
          case FB_ACCEL_MATROX_MGA2164W:
          case FB_ACCEL_MATROX_MGA2164W_AGP:
               device_info->caps.accel    = MATROX_OLD_DRAWING_FUNCTIONS |
                                            MATROX_OLD_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_OLD_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_OLD_BLITTING_FLAGS;
               break;

          case FB_ACCEL_MATROX_MGA2064W:
               device_info->caps.accel    = MATROX_2064W_DRAWING_FUNCTIONS |
                                            MATROX_2064W_BLITTING_FUNCTIONS;
               device_info->caps.drawing  = MATROX_2064W_DRAWING_FLAGS;
               device_info->caps.blitting = MATROX_2064W_BLITTING_FLAGS;
               break;
     }

     /* set hardware limitations */
     device_info->limits.surface_byteoffset_alignment = 128;
     device_info->limits.surface_pixelpitch_alignment = 32;
     device_info->limits.surface_bytepitch_alignment  = 64;

     /* soft reset to fix eventually corrupted TMU read offset on G200 */
     if (mdrv->accelerator == FB_ACCEL_MATROX_MGAG200) {
          __u32 ien = mga_in32( mmio, IEN );
          mga_out32( mmio, 1, RST );
          usleep(10);
          mga_out32( mmio, 0, RST );
          mga_out32( mmio, ien, IEN );
     }

     if (mdrv->accelerator == FB_ACCEL_MATROX_MGA2064W)
          mdev->idle_status = 0;
     else
          mdev->idle_status = ENDPRDMASTS;

     mdev->atype_blk_rstr = (sgram || dfb_config->matrox_sgram) ? ATYPE_BLK : ATYPE_RSTR;

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
     mga_out32( mdrv->mmio_base, 0, BESCTL );


     D_DEBUG( "DirectFB/Matrox: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/Matrox:  %9d matrox_waitfifo calls\n",
               mdev->waitfifo_calls );
     D_DEBUG( "DirectFB/Matrox:  %9d register writes (matrox_waitfifo sum)\n",
               mdev->waitfifo_sum );
     D_DEBUG( "DirectFB/Matrox:  %9d FIFO wait cycles (depends on CPU)\n",
               mdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/Matrox:  %9d IDLE wait cycles (depends on CPU)\n",
               mdev->idle_waitcycles );
     D_DEBUG( "DirectFB/Matrox:  %9d FIFO space cache hits (depends on CPU)\n",
               mdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/Matrox: Conclusion:\n" );
     D_DEBUG( "DirectFB/Matrox:  Average register writes/matrox_waitfifo call: %.2f\n",
               mdev->waitfifo_sum/(float)(mdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/Matrox:  Average wait cycles/matrox_waitfifo call:     %.2f\n",
               mdev->fifo_waitcycles/(float)(mdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/Matrox:  Average fifo space cache hits:                %02d%%\n",
               (int)(100 * mdev->fifo_cache_hits/(float)(mdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     MatroxDriverData *mdrv = (MatroxDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, mdrv->mmio_base, -1 );
}

