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

#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#include <directfb.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/screens.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/util.h>

#include <core/graphics_driver.h>


DFB_GRAPHICS_DRIVER( mach64 )


#include "regs.h"
#include "mmio.h"
#include "mach64_state.h"
#include "mach64.h"


/* driver capability flags */


#define MACH64_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_DST_COLORKEY)

#define MACH64_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY)

#define MACH64_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE)

#define MACH64_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT)


#define MACH64GT_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_DST_COLORKEY | DSDRAW_BLEND)

#define MACH64GT_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY | DSBLIT_BLEND_COLORALPHA)
/* FIXME: DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE */

#define MACH64GT_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE)

#define MACH64GT_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)

#define USE_SCALER( state, accel )                                      \
     (((accel) == DFXL_STRETCHBLIT) ||                                  \
      ((state)->blittingflags & DSBLIT_BLEND_COLORALPHA) ||             \
      ((state)->source->format != (state)->destination->format))

static bool mach64Blit2D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy );
static bool mach64Blit3D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy );

/* required implementations */

static void mach64EngineReset( void *drv, void *dev )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mach64_waitidle( mdrv, mdev );

     mach64_waitfifo( mdrv, mdev, 2 );

     mach64_out32( mmio, DP_WRITE_MASK, 0xFFFFFFFF );
     mach64_out32( mmio, DP_MIX, FRGD_MIX_SRC | BKGD_MIX_DST );

     if (mdrv->accelerator == FB_ACCEL_ATI_MACH64GT) {
          mach64_waitfifo( mdrv, mdev, 13 );

          /* Some 3D registers aren't accessible without this. */
          mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE );

          mach64_out32( mmio, ALPHA_TEST_CNTL, 0 );
          mach64_out32( mmio, TEX_CNTL, 0 );
          mach64_out32( mmio, Z_CNTL, 0 );

          mach64_out32( mmio, RED_X_INC, 0 );
          mach64_out32( mmio, RED_Y_INC, 0 );
          mach64_out32( mmio, GREEN_X_INC, 0 );
          mach64_out32( mmio, GREEN_Y_INC, 0 );
          mach64_out32( mmio, BLUE_X_INC, 0 );
          mach64_out32( mmio, BLUE_Y_INC, 0 );
          mach64_out32( mmio, ALPHA_X_INC, 0 );
          mach64_out32( mmio, ALPHA_Y_INC, 0 );

          mach64_out32( mmio, SCALE_3D_CNTL, 0 );
     }
}

static void mach64EngineSync( void *drv, void *dev )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64_waitidle( mdrv, mdev );
}

static bool mach64_check_blend( CardState *state )
{
     switch (state->src_blend) {
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
               return false;
          default:
               break;
     }

     switch (state->dst_blend) {
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               return false;
          default:
               break;
     }

     return true;
}

static void mach64CheckState( void *drv, void *dev,
                              CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
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
          if (state->drawingflags & ~MACH64_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= MACH64_SUPPORTED_DRAWINGFUNCTIONS;
     } else {
          if (state->source->format != state->destination->format)
               return;

          if (state->blittingflags & ~MACH64_SUPPORTED_BLITTINGFLAGS)
               return;

          /* Can't do source and destination color keying at the same time. */
          if (state->blittingflags & DSBLIT_SRC_COLORKEY &&
              state->blittingflags & DSBLIT_DST_COLORKEY)
               return;

          state->accel |= MACH64_SUPPORTED_BLITTINGFUNCTIONS;
     }
}

static void mach64GTCheckState( void *drv, void *dev,
                                CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->format) {
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
          if (state->drawingflags & ~MACH64GT_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              !mach64_check_blend( state ))
               return;

          state->accel |= MACH64GT_SUPPORTED_DRAWINGFUNCTIONS;
     } else {
          switch (state->source->format) {
          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
          default:
               return;
          }

          if (state->blittingflags & ~MACH64GT_SUPPORTED_BLITTINGFLAGS)
               return;

          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA) &&
              !mach64_check_blend( state ))
               return;

          /* Can't do alpha modulation. */
          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL &&
              state->blittingflags & DSBLIT_BLEND_COLORALPHA)
               return;

          /* Can't do source and destination color keying at the same time. */
          if (state->blittingflags & DSBLIT_SRC_COLORKEY &&
              state->blittingflags & DSBLIT_DST_COLORKEY)
               return;

          state->accel |= MACH64GT_SUPPORTED_BLITTINGFUNCTIONS;
     }
}

static void mach64SetState( void *drv, void *dev,
                            GraphicsDeviceFuncs *funcs,
                            CardState *state, DFBAccelerationMask accel )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     if (state->modified == SMF_ALL) {
          mdev->valid = 0;
     } else if (state->modified) {
          if (state->modified & SMF_SOURCE)
               MACH64_INVALIDATE( m_source | m_srckey | m_srckey_scale | m_blit_blend );

          if (state->modified & SMF_SRC_COLORKEY)
               MACH64_INVALIDATE( m_srckey | m_srckey_scale );

          if (state->modified & SMF_DESTINATION)
               MACH64_INVALIDATE( m_color | m_dstkey );

          if (state->modified & SMF_COLOR)
               MACH64_INVALIDATE( m_color | m_color_3d );

          if (state->modified & SMF_DST_COLORKEY)
               MACH64_INVALIDATE( m_dstkey );

          if (state->modified & SMF_BLITTING_FLAGS)
               MACH64_INVALIDATE( m_srckey | m_srckey_scale | m_dstkey | m_disable_key | m_blit_blend );

          if (state->modified & SMF_DRAWING_FLAGS)
               MACH64_INVALIDATE( m_dstkey | m_disable_key | m_draw_blend );

          if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
               MACH64_INVALIDATE( m_draw_blend | m_blit_blend );
     }

     if (state->modified & SMF_DESTINATION)
          mach64_set_destination( mdrv, mdev, state );

     switch (accel) {
     case DFXL_FILLRECTANGLE:
     case DFXL_DRAWRECTANGLE:
     case DFXL_DRAWLINE:
     case DFXL_FILLTRIANGLE:
          if (state->drawingflags & DSDRAW_BLEND) {
               mach64_waitfifo( mdrv, mdev, 4 );

               mach64_out32( mmio, DP_PIX_WIDTH, mdev->dst_pix_width | mdev->src_pix_width );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_SCALE );
               mach64_out32( mmio, SRC_CNTL, 0 );

               /* Some 3D registers aren't accessible without this. */
               mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE );

               mach64_set_color_3d( mdrv, mdev, state );
               mach64_set_draw_blend( mdrv, mdev, state );

               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, SCALE_3D_CNTL, mdev->draw_blend );

               state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE |
                            DFXL_DRAWLINE | DFXL_FILLTRIANGLE;
          } else {
               mach64_waitfifo( mdrv, mdev, 4 );

               mach64_out32( mmio, DP_PIX_WIDTH, mdev->dst_pix_width | mdev->src_pix_width );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_FRGD_CLR );

               if (accel == DFXL_FILLRECTANGLE || accel == DFXL_FILLTRIANGLE) {
                    mach64_out32( mmio, SRC_CNTL, mdev->src_cntl );
                    state->set = DFXL_FILLRECTANGLE | DFXL_FILLTRIANGLE;
               } else {
                    mach64_out32( mmio, SRC_CNTL, 0 );
                    state->set = DFXL_DRAWRECTANGLE | DFXL_DRAWLINE;
               }

               mach64_out32( mmio, SCALE_3D_CNTL, 0 );

               mach64_set_color( mdrv, mdev, state );
          }

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               mach64_set_dst_colorkey( mdrv, mdev, state );
          else
               mach64_disable_colorkey( mdrv, mdev );

          break;
     case DFXL_BLIT:
     case DFXL_STRETCHBLIT:
          mach64_set_source( mdrv, mdev, state );

          if (USE_SCALER( state, accel )) {
               mach64_waitfifo( mdrv, mdev, 4 );

               mach64_out32( mmio, DP_PIX_WIDTH, mdev->dst_pix_width | mdev->src_pix_width );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_SCALE );
               mach64_out32( mmio, SRC_CNTL, 0 );

               /* Some 3D registers aren't accessible without this. */
               mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SCALE );

               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                           DSBLIT_COLORIZE))
                    mach64_set_color_3d( mdrv, mdev, state );

               mach64_set_blit_blend( mdrv, mdev, state );

               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, SCALE_3D_CNTL, mdev->blit_blend );

               if (state->blittingflags & DSBLIT_DST_COLORKEY)
                    mach64_set_dst_colorkey( mdrv, mdev, state );
               else if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    mach64_set_src_colorkey_scale( mdrv, mdev, state );
               else
                    mach64_disable_colorkey( mdrv, mdev );

               funcs->Blit = mach64Blit3D;

               state->set = DFXL_BLIT | DFXL_STRETCHBLIT;
          } else {
               mach64_waitfifo( mdrv, mdev, 4 );

               mach64_out32( mmio, DP_PIX_WIDTH, mdev->dst_pix_width | mdev->src_pix_width );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_BLIT );
               mach64_out32( mmio, SRC_CNTL, 0 );

               mach64_out32( mmio, SCALE_3D_CNTL, 0 );

               if (state->blittingflags & DSBLIT_DST_COLORKEY)
                    mach64_set_dst_colorkey( mdrv, mdev, state );
               else if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    mach64_set_src_colorkey( mdrv, mdev, state );
               else
                    mach64_disable_colorkey( mdrv, mdev );

               funcs->Blit = mach64Blit2D;

               state->set = DFXL_BLIT;
          }
          break;
     default:
          D_BUG( "unexpected drawing/blitting function" );
          break;
     }

     if (state->modified & SMF_CLIP)
          mach64_set_clip( mdrv, mdev, state );

     state->modified = 0;
}

/* */

static bool mach64FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 3 );

     mach64_out32( mmio, DST_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     mach64_out32( mmio, DST_Y_X, (rect->x << 16) | rect->y );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (rect->w << 16) | rect->h );

     return true;
}

static bool mach64DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     int x2 = rect->x + rect->w - 1;
     int y2 = rect->y + rect->h - 1;

     mach64_waitfifo( mdrv, mdev, 8 );

     mach64_out32( mmio, DST_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     mach64_out32( mmio, DST_Y_X, (rect->x << 16) | rect->y );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->h );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (rect->w << 16) | 1 );

     mach64_out32( mmio, DST_CNTL, DST_X_RIGHT_TO_LEFT | DST_Y_BOTTOM_TO_TOP );
     mach64_out32( mmio, DST_Y_X, (x2 << 16) | y2 );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->h );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (rect->w << 16) | 1 );

     return true;
}

static bool mach64DrawLine( void *drv, void *dev, DFBRegion *line )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     __u32 dst_cntl = 0;
     int   dx, dy;

     dx = line->x2 - line->x1;
     dy = line->y2 - line->y1;

     if (dx < 0) {
          dx = -dx;
          dst_cntl |= DST_X_RIGHT_TO_LEFT;
     } else
          dst_cntl |= DST_X_LEFT_TO_RIGHT;

     if (dy < 0) {
          dy = -dy;
          dst_cntl |= DST_Y_BOTTOM_TO_TOP;
     } else
          dst_cntl |= DST_Y_TOP_TO_BOTTOM;

     if (!dx || !dy) {
          /* horizontal / vertical line */
          mach64_waitfifo( mdrv, mdev, 3 );

          mach64_out32( mmio, DST_CNTL, dst_cntl);
          mach64_out32( mmio, DST_Y_X, (line->x1 << 16) | line->y1 );
          mach64_out32( mmio, DST_HEIGHT_WIDTH, ((dx+1) << 16) | (dy+1) );

          return true;
     }

     if (dx < dy) {
          int tmp = dx;
          dx = dy;
          dy = tmp;
          dst_cntl |= DST_Y_MAJOR;
     } else
          dst_cntl |= DST_X_MAJOR;

     mach64_waitfifo( mdrv, mdev, 6 );

     mach64_out32( mmio, DST_CNTL, DST_LAST_PEL | dst_cntl );
     mach64_out32( mmio, DST_Y_X, (line->x1 << 16) | line->y1 );

     mach64_out32( mmio, DST_BRES_ERR, -dx );
     mach64_out32( mmio, DST_BRES_INC,  2*dy );
     mach64_out32( mmio, DST_BRES_DEC, -2*dx );
     mach64_out32( mmio, DST_BRES_LNTH, dx+1 );

     return true;
}

static void mach64_fill_trapezoid( Mach64DriverData *mdrv,
                                   Mach64DeviceData *mdev,
                                   int X1l, int X1r,
                                   int X2l, int X2r,
                                   int Y, int dY )
{
     volatile __u8 *mmio = mdrv->mmio_base;
     __u32 dst_cntl;
     int dXl, dXr;

     dst_cntl = DST_Y_TOP_TO_BOTTOM | TRAP_FILL_LEFT_TO_RIGHT;

     dXl = X2l - X1l;
     if (dXl < 0) {
          dXl = -dXl;
          dst_cntl |= DST_X_RIGHT_TO_LEFT;
     } else
          dst_cntl |= DST_X_LEFT_TO_RIGHT;

     dXr = X2r - X1r;
     if (dXr < 0) {
          dXr = -dXr;
          dst_cntl |= TRAIL_X_RIGHT_TO_LEFT;
     } else
          dst_cntl |= TRAIL_X_LEFT_TO_RIGHT;

     mach64_waitfifo( mdrv, mdev, 9 );

     mach64_out32( mmio, DST_CNTL, dst_cntl );
     mach64_out32( mmio, DST_Y_X, (X1l << 16) | Y );

     mach64_out32( mmio, TRAIL_BRES_ERR, -dY );
     mach64_out32( mmio, TRAIL_BRES_INC,  2*dXr );
     mach64_out32( mmio, TRAIL_BRES_DEC, -2*dY );

     mach64_out32( mmio, DST_BRES_ERR, -dY );
     mach64_out32( mmio, DST_BRES_INC,  2*dXl );
     mach64_out32( mmio, DST_BRES_DEC, -2*dY );
     mach64_out32( mmio, DST_BRES_LNTH, (X1r << 16) | (dY+1) | DRAW_TRAP | LINE_DIS );
}

static bool mach64FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     dfb_sort_triangle( tri );

     if (tri->y2 == tri->y3) {
          mach64_fill_trapezoid( mdrv, mdev, tri->x1, tri->x1,
                                 MIN( tri->x2, tri->x3), MAX( tri->x2, tri->x3 ),
                                 tri->y1, tri->y3 - tri->y1 );
     } else if (tri->y1 == tri->y2) {
          mach64_fill_trapezoid( mdrv, mdev,
                                 MIN( tri->x1, tri->x2), MAX( tri->x1, tri->x2 ),
                                 tri->x3, tri->x3,
                                 tri->y1, tri->y3 - tri->y1 );
     } else {
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

          mach64_fill_trapezoid( mdrv, mdev, tri->x1, tri->x1,
                                 MIN( X2a, majX2a ), MAX( X2a, majX2a ),
                                 tri->y1, topDy - 1 );
          mach64_fill_trapezoid( mdrv, mdev,
                                 MIN( tri->x2, majX2 ), MAX( tri->x2, majX2 ),
                                 tri->x3, tri->x3, tri->y2, botDy );
     }

     return true;
}

static void mach64DoBlit2D( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            DFBRectangle *srect,
                            DFBRectangle *drect )
{
     volatile __u8    *mmio = mdrv->mmio_base;

     __u32 dst_cntl = 0;

     if (srect->x <= drect->x) {
          srect->x += srect->w - 1;
          drect->x += drect->w - 1;
          dst_cntl |= DST_X_RIGHT_TO_LEFT;
     } else
          dst_cntl |= DST_X_LEFT_TO_RIGHT;

     if (srect->y <= drect->y) {
          srect->y += srect->h - 1;
          drect->y += drect->h - 1;
          dst_cntl |= DST_Y_BOTTOM_TO_TOP;
     } else
          dst_cntl |= DST_Y_TOP_TO_BOTTOM;

     mach64_waitfifo( mdrv, mdev, 5 );

     mach64_out32( mmio, SRC_Y_X, (srect->x << 16) | srect->y );
     mach64_out32( mmio, SRC_HEIGHT1_WIDTH1, (srect->w << 16) | srect->h );

     mach64_out32( mmio, DST_CNTL, dst_cntl );
     mach64_out32( mmio, DST_Y_X, (drect->x << 16) | drect->y );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );
}

static void mach64DoBlitScale( Mach64DriverData *mdrv,
                               Mach64DeviceData *mdev,
                               DFBRectangle *srect,
                               DFBRectangle *drect )
{
     volatile __u8    *mmio = mdrv->mmio_base;
     CoreSurface *source = mdev->source;
     SurfaceBuffer *buffer = source->front_buffer;

     mach64_waitfifo( mdrv, mdev, 10 );

     mach64_out32( mmio, SCALE_WIDTH, srect->w );
     mach64_out32( mmio, SCALE_HEIGHT, srect->h );

     mach64_out32( mmio, SCALE_X_INC, (srect->w << 16) / drect->w );
     mach64_out32( mmio, SCALE_Y_INC, (srect->h << 16) / drect->h );

     mach64_out32( mmio, SCALE_OFF, buffer->video.offset +
                   srect->y * buffer->video.pitch +
                   srect->x * DFB_BYTES_PER_PIXEL( source->format ) );
     mach64_out32( mmio, SCALE_HACC, 0 );
     mach64_out32( mmio, SCALE_VACC, 0 );

     mach64_out32( mmio, DST_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     mach64_out32( mmio, DST_Y_X, (drect->x << 16) | drect->y );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );
}

static bool mach64Blit2D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlit2D( mdrv, mdev, rect, &drect );

     return true;
}

static bool mach64Blit3D( void *drv, void *dev,
                          DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlitScale( mdrv, mdev, rect, &drect );

     return true;
}

static bool mach64StretchBlit( void *drv, void *dev,
                               DFBRectangle *srect, DFBRectangle *drect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64DoBlitScale( mdrv, mdev, srect, drect );

     return true;
}

/* */

static bool mach64_detect_rage_pro( Mach64DriverData *mdrv,
                                    GraphicsDeviceInfo *device_info )
{
     switch (mach64_in32( mdrv->mmio_base, CONFIG_CHIP_ID ) & 0xFFFF) {
     case 0x4742: case 0x4744: case 0x4749: case 0x4750: case 0x4751:
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage Pro" );
          return true;
     case 0x4C42: case 0x4C44: case 0x4C49: case 0x4C50: case 0x4C51:
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage LT Pro" );
          return true;
     case 0x4752: case 0x474F: case 0x474D:
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage XL" );
          return true;
     case 0x4753: case 0x474C: case 0x474E:
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage XC" );
          return true;
     case 0x4C4D: case 0x4C4E: case 0x4C52: case 0x4C53:
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage Mobility" );
          return true;
     default:
          return false;
     }
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_ATI_MACH64GX:
          case FB_ACCEL_ATI_MACH64CT:
          case FB_ACCEL_ATI_MACH64VT:
          case FB_ACCEL_ATI_MACH64GT:
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
               "ATI Mach64 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Ville Syrjala" );

     info->version.major = 0;
     info->version.minor = 12;

     info->driver_data_size = sizeof (Mach64DriverData);
     info->device_data_size = sizeof (Mach64DeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;

     mdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!mdrv->mmio_base)
          return DFB_IO;

     mdrv->device_data = (Mach64DeviceData*) device_data;

     mdrv->accelerator = dfb_gfxcard_get_accelerator( device );

     funcs->EngineReset   = mach64EngineReset;
     funcs->CheckState    = mach64CheckState;
     funcs->SetState      = mach64SetState;
     funcs->EngineSync    = mach64EngineSync;

     funcs->FillRectangle = mach64FillRectangle;
     funcs->DrawRectangle = mach64DrawRectangle;
     funcs->DrawLine      = mach64DrawLine;
     funcs->FillTriangle  = mach64FillTriangle;
     funcs->StretchBlit   = mach64StretchBlit;

     /* Set dynamically: funcs->Blit */

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64GT:
               funcs->CheckState = mach64GTCheckState;
          case FB_ACCEL_ATI_MACH64VT:
               mdrv->mmio_base += 0x400;

               dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                                    driver_data, &mach64OverlayFuncs );
               break;
     }

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = (Mach64DeviceData*) device_data;

     /* fill device info */
     device_info->caps.flags    = CCF_CLIPPING;

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64GT:
               device_info->caps.drawing  = MACH64GT_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = MACH64GT_SUPPORTED_BLITTINGFLAGS;
               device_info->caps.accel    = MACH64GT_SUPPORTED_DRAWINGFUNCTIONS |
                                            MACH64GT_SUPPORTED_BLITTINGFUNCTIONS;
               break;
          default:
               device_info->caps.drawing  = MACH64_SUPPORTED_DRAWINGFLAGS;
               device_info->caps.blitting = MACH64_SUPPORTED_BLITTINGFLAGS;
               device_info->caps.accel    = MACH64_SUPPORTED_DRAWINGFUNCTIONS |
                                            MACH64_SUPPORTED_BLITTINGFUNCTIONS;
               break;
     }

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64GX:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 GX" );
               break;
          case FB_ACCEL_ATI_MACH64CT:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 CT" );
               break;
          case FB_ACCEL_ATI_MACH64VT:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 VT" );
               break;
          case FB_ACCEL_ATI_MACH64GT:
               mdev->rage_pro = mach64_detect_rage_pro( mdrv, device_info );
               if (!mdev->rage_pro)
                    snprintf( device_info->name,
                              DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 GT" );
               break;
     }

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

     device_info->limits.surface_byteoffset_alignment = 64;
     device_info->limits.surface_bytepitch_alignment  = 16;
     device_info->limits.surface_pixelpitch_alignment = 8;

     if (mdrv->accelerator != FB_ACCEL_ATI_MACH64GX &&
         (mach64_in32( mdrv->mmio_base, CONFIG_STAT0 ) & 0x7) == 0x5)
          /* SGRAM */
          mdev->src_cntl = FAST_FILL_EN | BLOCK_WRITE_EN;
     else
          mdev->src_cntl = 0;

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;
     Mach64DeviceData *mdev = (Mach64DeviceData*) device_data;
     volatile __u8    *mmio = mdrv->mmio_base;

     D_DEBUG( "DirectFB/Mach64: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/Mach64:  %9d mach64_waitfifo calls\n",
               mdev->waitfifo_calls );
     D_DEBUG( "DirectFB/Mach64:  %9d register writes (mach64_waitfifo sum)\n",
               mdev->waitfifo_sum );
     D_DEBUG( "DirectFB/Mach64:  %9d FIFO wait cycles (depends on CPU)\n",
               mdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/Mach64:  %9d IDLE wait cycles (depends on CPU)\n",
               mdev->idle_waitcycles );
     D_DEBUG( "DirectFB/Mach64:  %9d FIFO space cache hits(depends on CPU)\n",
               mdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/Mach64: Conclusion:\n" );
     D_DEBUG( "DirectFB/Mach64:  Average register writes/mach64_waitfifo"
               "call:%.2f\n",
               mdev->waitfifo_sum/(float)(mdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/Mach64:  Average wait cycles/mach64_waitfifo call:"
               " %.2f\n",
               mdev->fifo_waitcycles/(float)(mdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/Mach64:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * mdev->fifo_cache_hits/
               (float)(mdev->waitfifo_calls)) );

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64GT:
               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, SCALE_3D_CNTL, 0 );
          case FB_ACCEL_ATI_MACH64VT:
               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, OVERLAY_SCALE_CNTL, 0 );
               break;
     }
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) driver_data;

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64VT:
          case FB_ACCEL_ATI_MACH64GT:
               mdrv->mmio_base -= 0x400;
               break;
     }

     dfb_gfxcard_unmap_mmio( device, mdrv->mmio_base, -1 );
}
