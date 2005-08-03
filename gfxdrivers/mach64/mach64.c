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

#include <core/accelerators.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include <misc/conf.h>

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
               (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY | DSBLIT_BLEND_COLORALPHA | \
                DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE | DSBLIT_DEINTERLACE)

#define MACH64GT_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE)

#define MACH64GT_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)


#define USE_SCALER( state, accel )                                                      \
     ((accel) & DFXL_STRETCHBLIT ||                                                     \
      (state)->source->format != (state)->destination->format ||                        \
      (state)->blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_DEINTERLACE))

#define USE_TEX( state, accel )                                                         \
     ((state)->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE))

#define USE_SCALER_3D( state, accel )                                                   \
     ((DFB_DRAWING_FUNCTION( accel ) && (state)->drawingflags & DSDRAW_BLEND) ||        \
      (DFB_BLITTING_FUNCTION( accel ) &&                                                \
       ((accel) & DFXL_STRETCHBLIT ||                                                   \
        (state)->source->format != (state)->destination->format ||                      \
        (state)->blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_DEINTERLACE |        \
                                  DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE))))


static bool mach64DrawLine2D( void *drv, void *dev, DFBRegion *line );
static bool mach64DrawLine3D( void *drv, void *dev, DFBRegion *line );

static bool mach64Blit2D( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );

static bool mach64BlitScaleOld( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
static bool mach64StretchBlitScaleOld( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );

static bool mach64BlitScale( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
static bool mach64StretchBlitScale( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );

static bool mach64BlitTexOld( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
static bool mach64StretchBlitTexOld( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );

static bool mach64BlitTex( void *drv, void *dev, DFBRectangle *rect, int dx, int dy );
static bool mach64StretchBlitTex( void *drv, void *dev, DFBRectangle *srect, DFBRectangle *drect );

/* required implementations */

static void mach64EngineReset( void *drv, void *dev )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mach64_waitidle( mdrv, mdev );

     mach64_waitfifo( mdrv, mdev, 2 );

     mach64_out32( mmio, DP_WRITE_MSK, 0xFFFFFFFF );
     mach64_out32( mmio, DP_MIX, FRGD_MIX_SRC | BKGD_MIX_DST );

     if (mdrv->accelerator == FB_ACCEL_ATI_MACH64GT) {
          mach64_waitfifo( mdrv, mdev, 12 );

          /* Some 3D registers aren't accessible without this. */
          mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE );

          mach64_out32( mmio, SRC_CNTL, 0 );
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

     if (mdev->chip >= CHIP_3D_RAGE_PRO)
          mach64_out32( mmio, HW_DEBUG, mdev->hw_debug );
}

static void mach64EngineSync( void *drv, void *dev )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64_waitidle( mdrv, mdev );
}

static void mach64FlushTextureCache( void *drv, void *dev )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mach64_waitfifo( mdrv, mdev, 1 );
          mach64_out32( mmio, TEX_CNTL, TEX_CACHE_FLUSH );
     }
}

static bool mach64_check_blend( Mach64DeviceData *mdev, CardState *state )
{
     switch (state->src_blend) {
          case DSBF_SRCCOLOR:
          case DSBF_INVSRCCOLOR:
               return false;
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
          case DSBF_SRCALPHASAT:
               if (mdev->chip < CHIP_3D_RAGE_PRO)
                    return false;
          default:
               break;
     }

     switch (state->dst_blend) {
          case DSBF_DESTCOLOR:
          case DSBF_INVDESTCOLOR:
          case DSBF_SRCALPHASAT:
               return false;
          case DSBF_DESTALPHA:
          case DSBF_INVDESTALPHA:
               if (mdev->chip < CHIP_3D_RAGE_PRO)
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
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     switch (state->destination->format) {
          case DSPF_ARGB4444:
               if (mdev->chip < CHIP_3D_RAGE_PRO ||
                   (mdev->chip < CHIP_3D_RAGE_XLXC && USE_SCALER_3D( state, accel )))
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
          if (state->drawingflags & ~MACH64GT_SUPPORTED_DRAWINGFLAGS)
               return;

          if (state->drawingflags & DSDRAW_BLEND &&
              !mach64_check_blend( mdev, state ))
               return;

          state->accel |= MACH64GT_SUPPORTED_DRAWINGFUNCTIONS;
     } else {
          CoreSurface *source = state->source;

          switch (source->format) {
               case DSPF_RGB332:
               case DSPF_ARGB1555:
               case DSPF_ARGB4444:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
               default:
                    return;
          }

          if (state->blittingflags & ~MACH64GT_SUPPORTED_BLITTINGFLAGS)
               return;

          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA) &&
              !mach64_check_blend( mdev, state ))
               return;

          /* Can't do alpha modulation. */
          if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL &&
              state->blittingflags & DSBLIT_BLEND_COLORALPHA)
               return;

          /* Can't do source and destination color keying at the same time. */
          if (state->blittingflags & DSBLIT_SRC_COLORKEY &&
              state->blittingflags & DSBLIT_DST_COLORKEY)
               return;

          /* Max texture size is 1024x1024. */
          if (USE_TEX( state, accel ) && (source->width > 1024 || source->height > 1024))
               return;

          /* Max scaler source size depends on the chip type. */
          if (USE_SCALER( state, accel )) {
               if (mdev->chip < CHIP_3D_RAGE_PRO) {
                    /* Tested on 3D Rage II+ and IIC. */
                    if (source->width > 4095 || source->height > 4095)
                         return;
               } else {
                    /* Tested on 3D Rage LT Pro, XL and Mobility. */
                    if (source->width > 4096 || source->height > 16384)
                         return;
               }
          }

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
               MACH64_INVALIDATE( m_source | m_srckey );

          if (state->modified & SMF_SRC_COLORKEY)
               MACH64_INVALIDATE( m_srckey );

          if (state->modified & SMF_DESTINATION)
               MACH64_INVALIDATE( m_color | m_dstkey );

          if (state->modified & SMF_COLOR)
               MACH64_INVALIDATE( m_color );

          if (state->modified & SMF_DST_COLORKEY)
               MACH64_INVALIDATE( m_dstkey );

          if (state->modified & SMF_BLITTING_FLAGS)
               MACH64_INVALIDATE( m_srckey | m_dstkey | m_disable_key );

          if (state->modified & SMF_DRAWING_FLAGS)
               MACH64_INVALIDATE( m_dstkey | m_disable_key );
     }

     if (state->modified & SMF_DESTINATION)
          mach64_set_destination( mdrv, mdev, state );

     switch (accel) {
     case DFXL_FILLRECTANGLE:
     case DFXL_DRAWRECTANGLE:
     case DFXL_DRAWLINE:
          mach64_waitfifo( mdrv, mdev, 2 );
          mach64_out32( mmio, DP_SRC, FRGD_SRC_FRGD_CLR );
          mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

          mach64_set_color( mdrv, mdev, state );

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               mach64_set_dst_colorkey( mdrv, mdev, state );
          else
               mach64_disable_colorkey( mdrv, mdev );

          funcs->DrawLine = mach64DrawLine2D;

          state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE;
          break;
     case DFXL_BLIT:
          mach64_set_source( mdrv, mdev, state );

          mach64_waitfifo( mdrv, mdev, 2 );
          mach64_out32( mmio, DP_SRC, FRGD_SRC_BLIT );
          mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

          if (state->blittingflags & DSBLIT_DST_COLORKEY)
               mach64_set_dst_colorkey( mdrv, mdev, state );
          else if (state->blittingflags & DSBLIT_SRC_COLORKEY)
               mach64_set_src_colorkey( mdrv, mdev, state );
          else
               mach64_disable_colorkey( mdrv, mdev );

          funcs->Blit = mach64Blit2D;

          state->set = DFXL_BLIT;
          break;
     default:
          D_BUG( "unexpected drawing/blitting function" );
          break;
     }

     if (state->modified & SMF_CLIP) {
          mach64_set_clip( mdrv, mdev, state );
          mdev->clip = state->clip;
     }

     state->modified = 0;
}

static void mach64GTSetState( void *drv, void *dev,
                              GraphicsDeviceFuncs *funcs,
                              CardState *state, DFBAccelerationMask accel )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     bool use_scaler_3d;

     if (state->modified == SMF_ALL) {
          mdev->valid = 0;
     } else if (state->modified) {
          if (state->modified & SMF_SOURCE)
               MACH64_INVALIDATE( m_source | m_source_scale | m_srckey | m_srckey_scale | m_blit_blend );

          if (state->modified & SMF_SRC_COLORKEY)
               MACH64_INVALIDATE( m_srckey | m_srckey_scale );

          if (state->modified & SMF_DESTINATION)
               MACH64_INVALIDATE( m_color | m_dstkey );

          if (state->modified & SMF_COLOR)
               MACH64_INVALIDATE( m_color | m_color_3d );

          if (state->modified & SMF_DST_COLORKEY)
               MACH64_INVALIDATE( m_dstkey );

          if (state->modified & SMF_BLITTING_FLAGS)
               MACH64_INVALIDATE( m_source_scale | m_srckey | m_srckey_scale | m_dstkey | m_disable_key | m_blit_blend );

          if (state->modified & SMF_DRAWING_FLAGS)
               MACH64_INVALIDATE( m_dstkey | m_disable_key | m_draw_blend );

          if (state->modified & (SMF_SRC_BLEND | SMF_DST_BLEND))
               MACH64_INVALIDATE( m_draw_blend | m_blit_blend );
     }

     use_scaler_3d = USE_SCALER_3D( state, accel );

     /* At least 3D Rage II+ and IIC chips _will_ lock up without this. */
     if (mdev->chip < CHIP_3D_RAGE_PRO && use_scaler_3d != mdev->use_scaler_3d)
          mach64_waitidle( mdrv, mdev );

     mdev->use_scaler_3d = use_scaler_3d;

     if (state->modified & SMF_DESTINATION)
          mach64gt_set_destination( mdrv, mdev, state );

     switch (accel) {
     case DFXL_FILLRECTANGLE:
     case DFXL_DRAWRECTANGLE:
     case DFXL_DRAWLINE:
     case DFXL_FILLTRIANGLE:
          if (state->drawingflags & DSDRAW_BLEND) {
               mach64_waitfifo( mdrv, mdev, 3 );
               /* Some 3D registers aren't accessible without this. */
               mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE );

               mach64_out32( mmio, DP_SRC, FRGD_SRC_SCALE );
               mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

               mach64_set_color_3d( mdrv, mdev, state );

               mach64_set_draw_blend( mdrv, mdev, state );

               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE | mdev->draw_blend );

               funcs->DrawLine = mach64DrawLine3D;
          } else {
               mach64_waitfifo( mdrv, mdev, 3 );
               mach64_out32( mmio, SCALE_3D_CNTL, 0 );

               mach64_out32( mmio, DP_SRC, FRGD_SRC_FRGD_CLR );
               mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

               mach64_set_color( mdrv, mdev, state );

               funcs->DrawLine = mach64DrawLine2D;
          }

          if (state->drawingflags & DSDRAW_DST_COLORKEY)
               mach64_set_dst_colorkey( mdrv, mdev, state );
          else
               mach64_disable_colorkey( mdrv, mdev );

          state->set = DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_DRAWLINE | DFXL_FILLTRIANGLE;
          break;
     case DFXL_BLIT:
     case DFXL_STRETCHBLIT:
          mdev->blit_deinterlace = state->blittingflags & DSBLIT_DEINTERLACE;

          if (USE_SCALER( state, accel ) || USE_TEX( state, accel )) {
               mach64_waitfifo( mdrv, mdev, 1 );
               /* Some 3D registers aren't accessible without this. */
               mach64_out32( mmio, SCALE_3D_CNTL, SCALE_3D_FCN_SHADE );

               mach64gt_set_source_scale ( mdrv, mdev, state );

               mach64_waitfifo( mdrv, mdev, 2 );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_SCALE );
               mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE))
                    mach64_set_color_3d( mdrv, mdev, state );

               mach64_set_blit_blend( mdrv, mdev, state );

               if (state->blittingflags & DSBLIT_DST_COLORKEY)
                    mach64_set_dst_colorkey( mdrv, mdev, state );
               else if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    mach64_set_src_colorkey_scale( mdrv, mdev, state );
               else
                    mach64_disable_colorkey( mdrv, mdev );

               if (mdev->chip < CHIP_3D_RAGE_PRO) {
                    if (USE_TEX( state, accel )) {
                         funcs->Blit        = mach64BlitTexOld;
                         funcs->StretchBlit = mach64StretchBlitTexOld;
                    } else {
                         funcs->Blit        = mach64BlitScaleOld;
                         funcs->StretchBlit = mach64StretchBlitScaleOld;
                    }
               } else {
                    if (USE_TEX( state, accel )) {
                         funcs->Blit        = mach64BlitTex;
                         funcs->StretchBlit = mach64StretchBlitTex;
                    } else {
                         funcs->Blit        = mach64BlitScale;
                         funcs->StretchBlit = mach64StretchBlitScale;
                    }
               }

               state->set = DFXL_BLIT | DFXL_STRETCHBLIT;
          } else {
               mach64_waitfifo( mdrv, mdev, 1 );
               mach64_out32( mmio, SCALE_3D_CNTL, 0 );

               mach64gt_set_source( mdrv, mdev, state );

               mach64_waitfifo( mdrv, mdev, 2 );
               mach64_out32( mmio, DP_SRC, FRGD_SRC_BLIT );
               mach64_out32( mmio, DP_PIX_WIDTH, mdev->pix_width );

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

     if (state->modified & SMF_CLIP) {
          mach64_set_clip( mdrv, mdev, state );
          mdev->clip = state->clip;
     }

     state->modified = 0;
}

/* */

static bool mach64FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;
     volatile __u8    *mmio = mdrv->mmio_base;

     mach64_waitfifo( mdrv, mdev, 3 );

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( rect->x ) << 16) | S14( rect->y ) );
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

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( rect->x ) << 16) | S14( rect->y ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->h );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (rect->w << 16) | 1 );

     mach64_out32( mmio, DST_CNTL, 0 );
     mach64_out32( mmio, DST_Y_X, (S13( x2 ) << 16) | S14( y2 ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (1 << 16) | rect->h );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (rect->w << 16) | 1 );

     return true;
}

static void mach64_draw_line( Mach64DriverData *mdrv,
                              Mach64DeviceData *mdev,
                              int x1, int y1,
                              int x2, int y2,
                              bool draw_3d )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 dst_cntl = 0;
     int   dx, dy;

     dx = x2 - x1;
     dy = y2 - y1;

     if (dx < 0)
          dx = -dx;
     else
          dst_cntl |= DST_X_DIR;

     if (dy < 0)
          dy = -dy;
     else
          dst_cntl |= DST_Y_DIR;

     if (!dx || !dy) {
          /* horizontal / vertical line */
          mach64_waitfifo( mdrv, mdev, 3 );

          mach64_out32( mmio, DST_CNTL, dst_cntl);
          mach64_out32( mmio, DST_Y_X, (S13( x1 ) << 16) | S14( y1 ) );
          mach64_out32( mmio, DST_HEIGHT_WIDTH, ((dx+1) << 16) | (dy+1) );

          return;
     }

     if (dx < dy) {
          int tmp = dx;
          dx = dy;
          dy = tmp;
          dst_cntl |= DST_Y_MAJOR;
     }

     mach64_waitfifo( mdrv, mdev, 6 );

     mach64_out32( mmio, DST_CNTL, DST_LAST_PEL | dst_cntl );
     mach64_out32( mmio, DST_Y_X, (S13( x1 ) << 16) | S14( y1 ) );

     /* Bresenham parameters must be calculated differently
      * for the 2D and 3D engines.
      */
     if (draw_3d) {
          mach64_out32( mmio, DST_BRES_ERR, -dx );
          mach64_out32( mmio, DST_BRES_INC, 2*dy );
          mach64_out32( mmio, DST_BRES_DEC, -2*dx );
          mach64_out32( mmio, DST_BRES_LNTH, dx+1 );
     } else {
          mach64_out32( mmio, DST_BRES_ERR, 2*dy-dx );
          mach64_out32( mmio, DST_BRES_INC, 2*dy );
          mach64_out32( mmio, DST_BRES_DEC, 2*dy-2*dx );
          mach64_out32( mmio, DST_BRES_LNTH, dx+1 );
     }
}

static bool mach64DrawLine2D( void *drv, void *dev, DFBRegion *line )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64_draw_line( mdrv, mdev,
                       line->x1, line->y1,
                       line->x2, line->y2,
                       false );

     return true;
}

static bool mach64DrawLine3D( void *drv, void *dev, DFBRegion *line )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64_draw_line( mdrv, mdev,
                       line->x1, line->y1,
                       line->x2, line->y2,
                       true );

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
     int   dXl, dXr;

     X1r++; X2r++;

     dst_cntl = DST_Y_DIR | TRAP_FILL_DIR;

     dXl = X2l - X1l;
     if (dXl < 0)
          dXl = -dXl;
     else
          dst_cntl |= DST_X_DIR;

     dXr = X2r - X1r;
     if (dXr < 0)
          dXr = -dXr;
     else
          dst_cntl |= TRAIL_X_DIR;

     mach64_waitfifo( mdrv, mdev, 9 );

     mach64_out32( mmio, DST_CNTL, dst_cntl );
     mach64_out32( mmio, DST_Y_X, (S13( X1l ) << 16) | S14( Y ) );

     mach64_out32( mmio, LEAD_BRES_ERR, -dY );
     mach64_out32( mmio, LEAD_BRES_INC,  2*dXl );
     mach64_out32( mmio, LEAD_BRES_DEC, -2*dY );

     mach64_out32( mmio, TRAIL_BRES_ERR, -dY );
     mach64_out32( mmio, TRAIL_BRES_INC,  2*dXr );
     mach64_out32( mmio, TRAIL_BRES_DEC, -2*dY );

     mach64_out32( mmio, LEAD_BRES_LNTH, (S14( X1r ) << 16) | (dY+1) | DRAW_TRAP | LINE_DIS );
}

static bool mach64FillTriangle( void *drv, void *dev, DFBTriangle *tri )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     dfb_sort_triangle( tri );

     if (tri->y2 == tri->y3) {
          mach64_fill_trapezoid( mdrv, mdev,
                                 tri->x1, tri->x1,
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

          mach64_fill_trapezoid( mdrv, mdev,
                                 tri->x1, tri->x1,
                                 MIN( X2a, majX2a ), MAX( X2a, majX2a ),
                                 tri->y1, topDy - 1 );
          mach64_fill_trapezoid( mdrv, mdev,
                                 MIN( tri->x2, majX2 ), MAX( tri->x2, majX2 ),
                                 tri->x3, tri->x3,
                                 tri->y2, botDy );
     }

     return true;
}

static void mach64DoBlit2D( Mach64DriverData *mdrv,
                            Mach64DeviceData *mdev,
                            DFBRectangle *srect,
                            DFBRectangle *drect )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 dst_cntl = 0;

     if (srect->x <= drect->x) {
          srect->x += srect->w - 1;
          drect->x += drect->w - 1;
     } else
          dst_cntl |= DST_X_DIR;

     if (srect->y <= drect->y) {
          srect->y += srect->h - 1;
          drect->y += drect->h - 1;
     } else
          dst_cntl |= DST_Y_DIR;

     mach64_waitfifo( mdrv, mdev, 5 );

     mach64_out32( mmio, SRC_Y_X, (S13( srect->x ) << 16) | S14( srect->y ) );
     mach64_out32( mmio, SRC_HEIGHT1_WIDTH1, (srect->w << 16) | srect->h );

     mach64_out32( mmio, DST_CNTL, dst_cntl );
     mach64_out32( mmio, DST_Y_X, (S13( drect->x ) << 16) | S14( drect->y ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );
}

static void mach64DoBlitScaleOld( Mach64DriverData *mdrv,
                                  Mach64DeviceData *mdev,
                                  DFBRectangle     *srect,
                                  DFBRectangle     *drect,
                                  bool              filter )
{
     volatile __u8 *mmio   = mdrv->mmio_base;
     CoreSurface   *source = mdev->source;

     __u32 scale_3d_cntl = SCALE_3D_FCN_SCALE | mdev->blit_blend;
     int   hacc, vacc;

     if (!filter)
          scale_3d_cntl |= SCALE_PIX_REP;

     if (mdev->blit_deinterlace) {
          srect->y /= 2;
          srect->h /= 2;
     }

     srect->x <<= 16;
     srect->y <<= 16;
     srect->w <<= 16;
     srect->h <<= 16;

     /*
      * SCALE_HACC and SCALE_VACC have limited scale so we need to change
      * SCALE_Y_OFF in order to handle the full range of source coordinates.
      */
     hacc = srect->x & 0xFFFF0; /* s4.12 */
     vacc = srect->y & 0xFFFF0; /* s4.12 */
     srect->x &= ~0xFFFFF;
     srect->y &= ~0xFFFFF;

     mach64_waitfifo( mdrv, mdev, 14 );

     mach64_out32( mmio, SCALE_3D_CNTL, scale_3d_cntl );
     mach64_out32( mmio, SCALE_Y_OFF, mdev->scale_offset +
                   (srect->y >> 16) * mdev->scale_pitch +
                   (srect->x >> 16) * DFB_BYTES_PER_PIXEL( source->format ) );


     mach64_out32( mmio, SCALE_WIDTH, (srect->w + hacc) >> 16 );
     mach64_out32( mmio, SCALE_HEIGHT, (srect->h + vacc) >> 16 );

     mach64_out32( mmio, SCALE_Y_PITCH, mdev->scale_pitch / DFB_BYTES_PER_PIXEL( source->format ) );

     mach64_out32( mmio, SCALE_X_INC, srect->w / drect->w );
     mach64_out32( mmio, SCALE_Y_INC, srect->h / drect->h );

     if (mdev->blit_deinterlace && mdev->field)
          vacc += 0x8000;

     mach64_out32( mmio, SCALE_VACC, vacc );
     mach64_out32( mmio, SCALE_HACC, hacc );
     mach64_out32( mmio, SCALE_XUV_INC, (srect->w/2) / (drect->w/2) );
     mach64_out32( mmio, SCALE_UV_HACC, hacc >> 1 );

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( drect->x ) << 16) | S14( drect->y ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );

     /* Some scaler and 3D color registers are shared. */
     MACH64_INVALIDATE( m_color_3d );
}

static void mach64DoBlitScale( Mach64DriverData *mdrv,
                               Mach64DeviceData *mdev,
                               DFBRectangle     *srect,
                               DFBRectangle     *drect,
                               bool              filter )
{
     volatile __u8 *mmio   = mdrv->mmio_base;
     CoreSurface   *source = mdev->source;

     __u32 scale_3d_cntl = SCALE_3D_FCN_SCALE | mdev->blit_blend;
     int   hacc, vacc;

     if (!filter)
          scale_3d_cntl |= SCALE_PIX_REP;

     if (mdev->blit_deinterlace) {
          srect->y /= 2;
          srect->h /= 2;
     }

     srect->x <<= 16;
     srect->y <<= 16;
     srect->w <<= 16;
     srect->h <<= 16;

     /* Hardware bug: Hitting SC_TOP results in incorrect rendering. */
     if (drect->y < mdev->clip.y1) {
          int sy, dy;
          dy = mdev->clip.y1 - drect->y;
          sy = (__u64) srect->h * dy / drect->h;
          srect->y += sy;
          srect->h -= sy;
          drect->y += dy;
          drect->h -= dy;
     }

     /*
      * SCALE_HACC and SCALE_VACC have limited scale so we need to change
      * SCALE_OFF in order to handle the full range of source coordinates.
      */
     hacc = srect->x & 0xFFFFF0; /* s8.12 */
     vacc = srect->y & 0xFFFF0; /* s4.12 */
     srect->x &= ~0xFFFFFF;
     srect->y &= ~0xFFFFF;

     mach64_waitfifo( mdrv, mdev, 12 );

     mach64_out32( mmio, SCALE_3D_CNTL, scale_3d_cntl );
     mach64_out32( mmio, SCALE_OFF, mdev->scale_offset +
                   (srect->y >> 16) * mdev->scale_pitch +
                   (srect->x >> 16) * DFB_BYTES_PER_PIXEL( source->format ) );

     mach64_out32( mmio, SCALE_WIDTH, (srect->w + hacc) >> 16 );
     mach64_out32( mmio, SCALE_HEIGHT, (srect->h + vacc) >> 16 );

     mach64_out32( mmio, SCALE_PITCH, mdev->scale_pitch / DFB_BYTES_PER_PIXEL( source->format ) );

     mach64_out32( mmio, SCALE_X_INC, srect->w / drect->w );
     mach64_out32( mmio, SCALE_Y_INC, srect->h / drect->h );

     if (mdev->blit_deinterlace && mdev->field)
          vacc += 0x8000;

     mach64_out32( mmio, SCALE_VACC, vacc );
     mach64_out32( mmio, SCALE_HACC, hacc );

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( drect->x ) << 16) | S14( drect->y ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );

     /* Some scaler and 3D color registers are shared. */
     MACH64_INVALIDATE( m_color_3d );
}

static void mach64DoBlitTexOld( Mach64DriverData *mdrv,
                                Mach64DeviceData *mdev,
                                DFBRectangle     *srect,
                                DFBRectangle     *drect,
                                bool              filter )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 scale_3d_cntl = SCALE_3D_FCN_TEXTURE | MIP_MAP_DISABLE | mdev->blit_blend;

     if (mdev->blit_deinterlace) {
          srect->y /= 2;
          srect->h /= 2;
     }

     srect->x <<= 1;
     srect->y <<= 1;
     srect->w <<= 1;
     srect->h <<= 1;

     /* Must add 0.5 to get correct rendering. */
     srect->x += 0x1;
     srect->y += 0x1;

     if (filter) {
          /* Avoid using texels outside of texture. */
          srect->w -= 0x2;
          srect->h -= 0x2;

          scale_3d_cntl |= BILINEAR_TEX_EN | TEX_BLEND_FCN_LINEAR_MIPMAP_NEAREST;
     }

     if (mdev->blit_deinterlace && mdev->field)
          srect->y += 0x1;

     mach64_waitfifo( mdrv, mdev, 14 );

     mach64_out32( mmio, SCALE_3D_CNTL, scale_3d_cntl );
     mach64_out32( mmio, TEX_0_OFF + (mdev->tex_size << 2), mdev->tex_offset );

     mach64_out32( mmio, S_X_INC2, 0 );
     mach64_out32( mmio, S_Y_INC2, 0 );
     mach64_out32( mmio, S_XY_INC2, 0 );
     mach64_out32( mmio, S_X_INC_START, (srect->w << (25 - mdev->tex_size)) / drect->w );
     mach64_out32( mmio, S_Y_INC, 0 );
     mach64_out32( mmio, S_START, (srect->x << (25 - mdev->tex_size)) );

     mach64_out32( mmio, T_X_INC2, 0 );
     mach64_out32( mmio, T_Y_INC2, 0 );
     mach64_out32( mmio, T_XY_INC2, 0 );
     mach64_out32( mmio, T_X_INC_START, 0 );
     mach64_out32( mmio, T_Y_INC, (srect->h << (25 - mdev->tex_size)) / drect->h );
     mach64_out32( mmio, T_START, (srect->y << (25 - mdev->tex_size)) );

     mach64_waitfifo( mdrv, mdev, 3 );

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( drect->x ) << 16) | S14( drect->y ) );
     mach64_out32( mmio, DST_HEIGHT_WIDTH, (drect->w << 16) | drect->h );
}

static void mach64DoBlitTex( Mach64DriverData *mdrv,
                             Mach64DeviceData *mdev,
                             DFBRectangle     *srect,
                             DFBRectangle     *drect,
                             bool              filter )
{
     volatile __u8 *mmio = mdrv->mmio_base;

     __u32 scale_3d_cntl = SCALE_3D_FCN_TEXTURE | MIP_MAP_DISABLE | mdev->blit_blend;

     if (mdev->blit_deinterlace) {
          srect->y /= 2;
          srect->h /= 2;
     }

     srect->x <<= 1;
     srect->y <<= 1;
     srect->w <<= 1;
     srect->h <<= 1;

     /* Must add 0.5 to get correct rendering. */
     srect->x += 0x1;
     srect->y += 0x1;

     if (filter) {
          /* Avoid using texels outside of texture. */
          srect->w -= 0x2;
          srect->h -= 0x2;

          scale_3d_cntl |= BILINEAR_TEX_EN | TEX_BLEND_FCN_LINEAR_MIPMAP_NEAREST;
     }

     if (mdev->blit_deinterlace && mdev->field)
          srect->y += 0x1;

     mach64_waitfifo( mdrv, mdev, 13 );

     mach64_out32( mmio, SCALE_3D_CNTL, scale_3d_cntl );
     mach64_out32( mmio, TEX_0_OFF + (mdev->tex_size << 2), mdev->tex_offset );

     mach64_out32( mmio, STW_EXP, (1 << 16) | (0 << 8) | (0 << 0) );

     /* This register doesn't seem to have any effect on the result. */
     mach64_out32( mmio, LOG_MAX_INC, 0 );

     mach64_out32( mmio, S_X_INC, (srect->w << (23 - mdev->tex_pitch)) / drect->w );
     mach64_out32( mmio, S_Y_INC, 0 );
     mach64_out32( mmio, S_START, (srect->x << (23 - mdev->tex_pitch)) );

     mach64_out32( mmio, W_X_INC, 0 );
     mach64_out32( mmio, W_Y_INC, 0 );
     mach64_out32( mmio, W_START, 1 << 23 );

     mach64_out32( mmio, T_X_INC, 0 );
     mach64_out32( mmio, T_Y_INC, (srect->h << (23 - mdev->tex_height)) / drect->h );
     mach64_out32( mmio, T_START, (srect->y << (23 - mdev->tex_height)) );

     mach64_waitfifo( mdrv, mdev, 3 );

     mach64_out32( mmio, DST_CNTL, DST_X_DIR | DST_Y_DIR );
     mach64_out32( mmio, DST_Y_X, (S13( drect->x ) << 16) | S14( drect->y ) );
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

static bool mach64BlitScaleOld( void *drv, void *dev,
                                DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlitScaleOld( mdrv, mdev, rect, &drect, mdev->blit_deinterlace );

     return true;
}

static bool mach64StretchBlitScaleOld( void *drv, void *dev,
                                       DFBRectangle *srect, DFBRectangle *drect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64DoBlitScaleOld( mdrv, mdev, srect, drect, true );

     return true;
}

static bool mach64BlitScale( void *drv, void *dev,
                             DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlitScale( mdrv, mdev, rect, &drect, mdev->blit_deinterlace );

     return true;
}

static bool mach64StretchBlitScale( void *drv, void *dev,
                                    DFBRectangle *srect, DFBRectangle *drect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64DoBlitScale( mdrv, mdev, srect, drect, true );

     return true;
}

static bool mach64BlitTexOld( void *drv, void *dev,
                              DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlitTexOld( mdrv, mdev, rect, &drect, mdev->blit_deinterlace );

     return true;
}

static bool mach64StretchBlitTexOld( void *drv, void *dev,
                                     DFBRectangle *srect, DFBRectangle *drect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64DoBlitTexOld( mdrv, mdev, srect, drect, true );

     return true;
}

static bool mach64BlitTex( void *drv, void *dev,
                           DFBRectangle *rect, int dx, int dy )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     DFBRectangle drect = { dx, dy, rect->w, rect->h };

     mach64DoBlitTex( mdrv, mdev, rect, &drect, mdev->blit_deinterlace );

     return true;
}

static bool mach64StretchBlitTex( void *drv, void *dev,
                                  DFBRectangle *srect, DFBRectangle *drect )
{
     Mach64DriverData *mdrv = (Mach64DriverData*) drv;
     Mach64DeviceData *mdev = (Mach64DeviceData*) dev;

     mach64DoBlitTex( mdrv, mdev, srect, drect, true );

     return true;
}

/* */

#define MACH64_CFG_CHIP_TYPE( a, b ) (((a) << 8) | (b))

static Mach64ChipType
mach64_chip_type_vt( Mach64DriverData   *mdrv,
                     GraphicsDeviceInfo *device_info )
{
     __u32 config_chip_id = mach64_in32( mdrv->mmio_base, CONFIG_CHIP_ID );
     __u32 cfg_chip_type = config_chip_id & CFG_CHIP_TYPE;

     switch (cfg_chip_type) {
     case MACH64_CFG_CHIP_TYPE( 'V', 'T' ):
          switch ((config_chip_id & CFG_CHIP_MAJOR) >> 24) {
          case 0:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,
                         (config_chip_id & CFG_CHIP_MINOR) ? "ATI-264VT2 (%c%c)" : "ATI-264VT (%c%c)",
                         cfg_chip_type >> 8, cfg_chip_type & 0xFF );
               return CHIP_264VT;
          case 1:
          case 2:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "ATI-264VT3 (%c%c)",
                         cfg_chip_type >> 8, cfg_chip_type & 0xFF );
               return CHIP_264VT3;
          }
          break;
     case MACH64_CFG_CHIP_TYPE( 'V', 'U' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "ATI-264VT3 (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_264VT3;
     case MACH64_CFG_CHIP_TYPE( 'V', 'V' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "ATI-264VT4 (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_264VT4;
     }
     D_WARN( "DirectFB/Mach64: Unknown VT chip type %c%c (0x%08x)",
             cfg_chip_type >> 8, cfg_chip_type & 0xFF, config_chip_id );
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 VT" );
     return CHIP_UNKNOWN;
}

static Mach64ChipType
mach64_chip_type_gt( Mach64DriverData   *mdrv,
                     GraphicsDeviceInfo *device_info )
{
     __u32 config_chip_id = mach64_in32( mdrv->mmio_base, CONFIG_CHIP_ID );
     __u32 cfg_chip_type = config_chip_id & CFG_CHIP_TYPE;

     switch (cfg_chip_type) {
     case MACH64_CFG_CHIP_TYPE( 'G', 'T' ):
          switch ((config_chip_id & CFG_CHIP_MAJOR) >> 24) {
          case 0:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage (%c%c)",
                         cfg_chip_type >> 8, cfg_chip_type & 0xFF );
               return CHIP_3D_RAGE;
          case 1:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage II (%c%c)",
                         cfg_chip_type >> 8, cfg_chip_type & 0xFF );
               return CHIP_3D_RAGE_II;
          case 2:
               snprintf( device_info->name,
                         DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage II+ (%c%c)",
                         cfg_chip_type >> 8, cfg_chip_type & 0xFF );
               return CHIP_3D_RAGE_IIPLUS;
          }
          break;
     case MACH64_CFG_CHIP_TYPE( 'G', 'U' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage II+ (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_IIPLUS;
     case MACH64_CFG_CHIP_TYPE( 'L', 'G' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage LT (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_LT;
     case MACH64_CFG_CHIP_TYPE( 'G', 'V' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'W' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'Y' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'Z' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage IIC (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_IIC;
     case MACH64_CFG_CHIP_TYPE( 'G', 'B' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'D' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'I' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'P' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'Q' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage Pro (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_PRO;
     case MACH64_CFG_CHIP_TYPE( 'L', 'B' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'D' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'I' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'P' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'Q' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage LT Pro (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_LT_PRO;
     case MACH64_CFG_CHIP_TYPE( 'G', 'M' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'O' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'R' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage XL (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_XLXC;
     case MACH64_CFG_CHIP_TYPE( 'G', 'L' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'N' ):
     case MACH64_CFG_CHIP_TYPE( 'G', 'S' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage XC (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_XLXC;
     case MACH64_CFG_CHIP_TYPE( 'L', 'M' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'N' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'R' ):
     case MACH64_CFG_CHIP_TYPE( 'L', 'S' ):
          snprintf( device_info->name,
                    DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "3D Rage Mobility (%c%c)",
                    cfg_chip_type >> 8, cfg_chip_type & 0xFF );
          return CHIP_3D_RAGE_MOBILITY;
     }
     D_WARN( "DirectFB/Mach64: Unknown GT chip type %c%c (0x%08x)",
             cfg_chip_type >> 8, cfg_chip_type & 0xFF, config_chip_id );
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Mach64 GT" );
     return CHIP_UNKNOWN;
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
     info->version.minor = 13;

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
     funcs->EngineSync    = mach64EngineSync;
     funcs->CheckState    = mach64CheckState;
     funcs->SetState      = mach64SetState;
     funcs->FillRectangle = mach64FillRectangle;
     funcs->DrawRectangle = mach64DrawRectangle;

     /* Set dynamically: funcs->DrawLine, funcs->Blit, funcs->StretchBlit */

     switch (mdrv->accelerator) {
          case FB_ACCEL_ATI_MACH64GT:
               if (!dfb_config->font_format)
                    dfb_config->font_format = DSPF_ARGB;
               funcs->FlushTextureCache = mach64FlushTextureCache;
               funcs->CheckState        = mach64GTCheckState;
               funcs->SetState          = mach64GTSetState;
               funcs->FillTriangle      = mach64FillTriangle;
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
     volatile __u8    *mmio = mdrv->mmio_base;

     /* fill device info */
     device_info->caps.flags = CCF_CLIPPING;

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
               mdev->chip = mach64_chip_type_vt( mdrv, device_info );
               break;
          case FB_ACCEL_ATI_MACH64GT:
               mdev->chip = mach64_chip_type_gt( mdrv, device_info );

               /* Max texture size is 1024x1024 */
               device_info->limits.surface_max_power_of_two_pixelpitch = 1024;
               device_info->limits.surface_max_power_of_two_height     = 1024;
               break;
     }

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "ATI" );

     device_info->limits.surface_byteoffset_alignment = 8;
     device_info->limits.surface_bytepitch_alignment  = 16;
     device_info->limits.surface_pixelpitch_alignment = 8;

     /* 3D Rage Pro is the first chip that supports auto fast fill/block write. */
     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          mdev->hw_debug = mach64_in32( mmio, HW_DEBUG );

          /* Save original HW_DEBUG. */
          mdev->hw_debug_orig = mdev->hw_debug;

          /* Enable auto fast fill and fast fill/block write scissoring. */
          mdev->hw_debug &= ~(AUTO_FF_DIS | INTER_PRIM_DIS);

          if ((mach64_in32( mmio, CONFIG_STAT0 ) & CFG_MEM_TYPE) == CFG_MEM_TYPE_SGRAM) {
               /* Enable auto block write and auto color register updates. */
               mdev->hw_debug &= ~(AUTO_BLKWRT_DIS | AUTO_BLKWRT_COLOR_DIS);

               device_info->limits.surface_byteoffset_alignment = 64;
               device_info->limits.surface_bytepitch_alignment  = 64;
          }
     }

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

     if (mdev->chip >= CHIP_3D_RAGE_PRO) {
          /* Restore original HW_DEBUG. */
          mach64_out32( mmio, HW_DEBUG, mdev->hw_debug_orig );
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
