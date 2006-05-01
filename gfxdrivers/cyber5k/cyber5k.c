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

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <fbdev/fb.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <directfb.h>

#include <fusion/shmalloc.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/windows.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( cyber5k )

#include "regs.h"
#include "mmio.h"
#include "cyber5k.h"
#include "cyber5k_alpha.h"


/* HACK */
volatile __u8 *cyber_mmio = NULL;


/* FIXME: support for destination color keying */

#define CYBER5K_DRAWING_FLAGS \
               (DSDRAW_NOFX)

#define CYBER5K_DRAWING_FUNCTIONS \
               (DFXL_DRAWLINE | DFXL_DRAWRECTANGLE | DFXL_FILLRECTANGLE)

#define CYBER5K_BLITTING_FLAGS \
               (DSBLIT_SRC_COLORKEY)

#define CYBER5K_BLITTING_FUNCTIONS \
               (DFXL_BLIT)

static bool cyber5kFillRectangle( void *drv, void *dev, DFBRectangle *rect );
static bool cyber5kFillRectangle24( void *drv, void *dev, DFBRectangle *rect );
static bool cyber5kDrawRectangle( void *drv, void *dev, DFBRectangle *rect );
static bool cyber5kDrawRectangle24( void *drv, void *dev, DFBRectangle *rect );
static bool cyber5kBlit( void *drv, void *dev,
                         DFBRectangle *rect, int dx, int dy );
static bool cyber5kBlit24( void *drv, void *dev,
                           DFBRectangle *rect, int dx, int dy );

static void cyber5kEngineSync( void *drv, void *dev )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;

     cyber_waitidle( cdrv, cdev );
}

static void cyber5kCheckState( void *drv, void *dev,
                               CardState *state, DFBAccelerationMask accel )
{
     /* check destination format first */
     switch (state->destination->format) {
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
          default:
               return;
     }

     if (DFB_DRAWING_FUNCTION( accel )) {
          /* if there are no other drawing flags than the supported */
          if (state->drawingflags & ~CYBER5K_DRAWING_FLAGS)
               return;

          state->accel |= CYBER5K_DRAWING_FUNCTIONS;

          /* no line drawing in 24bit mode */
          if (state->destination->format == DSPF_RGB24)
               state->accel &= ~DFXL_DRAWLINE;
     }
     else {
          /* if there are no other blitting flags than the supported
             and the source and destination formats are the same */
          if (state->blittingflags & ~CYBER5K_BLITTING_FLAGS)
               return;
          if (state->source->format != state->destination->format)
               return;

          state->accel |= CYBER5K_BLITTING_FUNCTIONS;
     }
}

static inline void
cyber5k_validate_dst( CyberDriverData *cdrv, CyberDeviceData *cdev,
                      CardState *state, GraphicsDeviceFuncs *funcs )
{
     CoreSurface   *dest   = state->destination;
     SurfaceBuffer *buffer = dest->back_buffer;

     if (cdev->v_dst)
          return;

     cdev->dst_pixeloffset = buffer->video.offset /
                             DFB_BYTES_PER_PIXEL(dest->format);
     cdev->dst_pixelpitch  = buffer->video.pitch /
                             DFB_BYTES_PER_PIXEL(dest->format);

     switch (dest->format) {
          case DSPF_RGB16:
               funcs->FillRectangle = cyber5kFillRectangle;
               funcs->DrawRectangle = cyber5kDrawRectangle;
               funcs->Blit          = cyber5kBlit;
               cyber_out16( cdrv->mmio_base, DSTWIDTH, cdev->dst_pixelpitch - 1 );
               cyber_out8( cdrv->mmio_base, COPFMT, 1 );
               break;
          case DSPF_RGB24:
               funcs->FillRectangle = cyber5kFillRectangle24;
               funcs->DrawRectangle = cyber5kDrawRectangle24;
               funcs->Blit          = cyber5kBlit24;
               cyber_out16( cdrv->mmio_base, DSTWIDTH, cdev->dst_pixelpitch*3 -1);
               cyber_out8( cdrv->mmio_base, COPFMT, 2 );
               break;
          case DSPF_RGB32:
          case DSPF_ARGB:
               funcs->FillRectangle = cyber5kFillRectangle;
               funcs->DrawRectangle = cyber5kDrawRectangle;
               funcs->Blit          = cyber5kBlit;
               cyber_out16( cdrv->mmio_base, DSTWIDTH, cdev->dst_pixelpitch - 1 );
               cyber_out8( cdrv->mmio_base, COPFMT, 3 );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     cdev->v_dst = 1;
}

static inline void
cyber5k_validate_src( CyberDriverData *cdrv,
                      CyberDeviceData *cdev, CardState *state )
{
     CoreSurface   *source = state->source;
     SurfaceBuffer *buffer = source->front_buffer;

     if (cdev->v_src)
          return;

     cdev->src_pixeloffset = buffer->video.offset /
                             DFB_BYTES_PER_PIXEL(source->format);
     cdev->src_pixelpitch  = buffer->video.pitch /
                             DFB_BYTES_PER_PIXEL(source->format);

     cyber_out16( cdrv->mmio_base, SRC1WIDTH,
                  buffer->video.pitch /DFB_BYTES_PER_PIXEL(source->format) - 1);

     cdev->v_src = 1;
}

static inline void
cyber5k_validate_color( CyberDriverData *cdrv,
                        CyberDeviceData *cdev, CardState *state )
{
     __u32 fill_color = 0;

     if (cdev->v_color)
          return;

     switch (state->destination->format) {
          case DSPF_RGB16:
               fill_color = PIXEL_RGB16( state->color.r,
                                         state->color.g,
                                         state->color.b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               fill_color = PIXEL_RGB32( state->color.r,
                                         state->color.g,
                                         state->color.b );
               break;
          case DSPF_ARGB:
               fill_color = PIXEL_ARGB( state->color.a,
                                        state->color.r,
                                        state->color.g,
                                        state->color.b );
               break;
          default:
               D_BUG( "unexpected pixelformat!" );
               break;
     }

     cyber_out32( cdrv->mmio_base, FCOLOR, fill_color );

     cdev->v_src_colorkey = 0;

     cdev->v_color = 1;
}

static inline void
cyber5k_validate_src_colorkey( CyberDriverData *cdrv,
                               CyberDeviceData *cdev, CardState *state )
{
     if (cdev->v_src_colorkey)
          return;

     cyber_out32( cdrv->mmio_base, FCOLOR, state->src_colorkey );
     cyber_out32( cdrv->mmio_base, BCOLOR, state->src_colorkey );

     cdev->v_color = 0;

     cdev->v_src_colorkey = 1;
}

static inline void
cyber5k_validate_blitting_cmd( CyberDriverData *cdrv,
                               CyberDeviceData *cdev, CardState *state )
{
     if (cdev->v_blitting_cmd)
          return;

     cdev->blitting_cmd = COP_PXBLT | PAT_FIXFGD | FGD_IS_SRC1;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          cdev->blitting_cmd |= TRANS_ENABLE | TRANS_IS_SRC1 | TRANS_INVERT;

     cdev->v_blitting_cmd = 1;
}

static void cyber5kSetState( void *drv, void *dev, GraphicsDeviceFuncs *funcs,
                             CardState *state, DFBAccelerationMask accel )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;

     if (state->modified) {
          if (state->modified & SMF_DESTINATION)
               cdev->v_dst = cdev->v_color = 0;
          else if (state->modified & SMF_COLOR)
               cdev->v_color = 0;

          if (state->modified & SMF_SOURCE)
               cdev->v_src = cdev->v_src_colorkey = 0;
          else if (state->modified & SMF_SRC_COLORKEY)
               cdev->v_src_colorkey = 0;

          if (state->modified & SMF_BLITTING_FLAGS)
               cdev->v_blitting_cmd = 0;
     }

     cyber5k_validate_dst( cdrv, cdev, state, funcs );

     switch (accel) {
          case DFXL_DRAWLINE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_FILLRECTANGLE:
               cyber5k_validate_color( cdrv, cdev, state );

               state->set = CYBER5K_DRAWING_FUNCTIONS;
               break;

          case DFXL_BLIT:
               cyber5k_validate_src( cdrv, cdev, state );
               cyber5k_validate_blitting_cmd( cdrv, cdev, state );

               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    cyber5k_validate_src_colorkey( cdrv, cdev, state );

               state->set = CYBER5K_BLITTING_FUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function!" );
               break;
     }

     state->modified = 0;
}

static bool cyber5kFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     cyber_waitidle( cdrv, cdev );

     cyber_out32( mmio, DSTPTR, cdev->dst_pixeloffset +
                  rect->y * cdev->dst_pixelpitch +
                  rect->x );

     cyber_out32( mmio, HEIGHTWIDTH, ((rect->h-1) << 16) | (rect->w-1) );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     return true;
}

static bool cyber5kFillRectangle24( void *drv, void *dev, DFBRectangle *rect )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     cyber_waitidle( cdrv, cdev );

     cyber_out32( mmio, DSTPTR, (cdev->dst_pixeloffset +
                                 rect->y * cdev->dst_pixelpitch +
                                 rect->x) * 3 );
     cyber_out8( mmio, DSTXROT, rect->x & 7 );

     cyber_out32( mmio, HEIGHTWIDTH, ((rect->h-1) << 16) | (rect->w-1) );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     return true;
}

static bool cyber5kDrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     __u32 dst = cdev->dst_pixeloffset +
                 rect->y * cdev->dst_pixelpitch + rect->x;

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst );
     cyber_out32( mmio, DIMW, 0 );
     cyber_out32( mmio, DIMH, rect->h - 1 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst + rect->w - 1);
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst );
     cyber_out32( mmio, DIMW, rect->w - 1 );
     cyber_out32( mmio, DIMH, 0 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst + cdev->dst_pixelpitch * (rect->h - 1) );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     return true;
}

static bool cyber5kDrawRectangle24( void *drv, void *dev, DFBRectangle *rect )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     __u32 dst = cdev->dst_pixeloffset +
                (rect->y * cdev->dst_pixelpitch + rect->x) * 3;

     cyber_waitidle( cdrv, cdev );
     cyber_out8( mmio, DSTXROT, rect->x & 7 );
     cyber_out32( mmio, DSTPTR, dst );
     cyber_out32( mmio, DIMW, rect->w - 1 );
     cyber_out32( mmio, DIMH, 0 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst + cdev->dst_pixelpitch * (rect->h-1) * 3 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR, dst );
     cyber_out32( mmio, DIMW, 0 );
     cyber_out32( mmio, DIMH, rect->h - 1 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     cyber_waitidle( cdrv, cdev );
     cyber_out8( mmio, DSTXROT, (rect->x + rect->w - 1) & 7 );
     cyber_out32( mmio, DSTPTR, dst + (rect->w-1) * 3 );
     cyber_out32( mmio, PIXOP, COP_PXBLT | PAT_FIXFGD );

     return true;
}

static bool cyber5kDrawLine( void *drv, void *dev, DFBRegion *line )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     __u32 cmd = COP_LINE_DRAW | PAT_FIXFGD;

     int dx;
     int dy;

     dx = line->x2 - line->x1;
     dy = line->y2 - line->y1;

     if (dx < 0) {
          dx = -dx;
          cmd |= DX_NEG;
     }
     if (dy < 0) {
          dy = -dy;
          cmd |= DY_NEG;
     }
     if (dx < dy) {
          int tmp;
          cmd |= YMAJOR;
          tmp = dx;
          dx = dy;
          dy = tmp;
     }

     cyber_waitidle( cdrv, cdev );
     cyber_out32( mmio, DSTPTR,
                  cdev->dst_pixeloffset +
                  line->y1 * cdev->dst_pixelpitch + line->x1);

     cyber_out16( mmio, DIMW          , dx);
     cyber_out16( mmio, K1          , 2*dy);
     cyber_out16( mmio, ERRORTERM, 2*dy-dx);
     cyber_out32( mmio, K2      ,2*(dy-dx));
     cyber_out32( mmio, PIXOP        , cmd);

     return true;
}

static bool cyber5kBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     __u32 cmd = cdev->blitting_cmd;

     cyber_waitidle( cdrv, cdev );

     if (rect->x < dx) {
          cmd |= DEC_X;

          rect->x += rect->w - 1;
          dx      += rect->w - 1;
     }

     if (rect->y < dy) {
          cmd |= DEC_Y;

          rect->y += rect->h - 1;
          dy      += rect->h - 1;
     }

     cyber_out32( mmio, DSTPTR,
                  cdev->dst_pixeloffset + dy * cdev->dst_pixelpitch + dx );
     cyber_out32( mmio, SRC1PTR,
                  cdev->src_pixeloffset +
                  rect->y * cdev->src_pixelpitch + rect->x );
     cyber_out32( mmio, HEIGHTWIDTH, ((rect->h-1) << 16) | (rect->w-1) );
     cyber_out32( mmio, PIXOP      , cmd);

     return true;
}

static bool cyber5kBlit24( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
     CyberDriverData *cdrv = (CyberDriverData*) drv;
     CyberDeviceData *cdev = (CyberDeviceData*) dev;
     volatile __u8   *mmio = cdrv->mmio_base;

     __u32 cmd = cdev->blitting_cmd;
     __u32 src = 0;
     __u32 dst = 0;

     cyber_waitidle( cdrv, cdev );

     if (rect->x < dx) {
          cmd |= DEC_X;

          rect->x += rect->w - 1;
          dx      += rect->w - 1;

          src += 2;
          dst += 2;
     }

     if (rect->y < dy) {
          cmd |= DEC_Y;

          rect->y += rect->h - 1;
          dy      += rect->h - 1;
     }

     src += cdev->src_pixeloffset + rect->y * cdev->dst_pixelpitch + rect->x;
     dst += cdev->dst_pixeloffset + dy * cdev->dst_pixelpitch + dx;

     cyber_out32( mmio, DSTPTR     , src );
     cyber_out32( mmio, SRC1PTR    , dst );
     cyber_out32( mmio, HEIGHTWIDTH, ((rect->h-1) << 16) | (rect->w-1) );
     cyber_out32( mmio, PIXOP      , cmd );

     return true;
}

/* primary layer hooks */

#define OSD_OPTIONS      (DLOP_ALPHACHANNEL | DLOP_SRC_COLORKEY | DLOP_OPACITY)

DisplayLayerFuncs  oldPrimaryFuncs;
void              *oldPrimaryDriverData;

static DFBResult
osdInitLayer( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              DFBDisplayLayerDescription *description,
              DFBDisplayLayerConfig      *config,
              DFBColorAdjustment         *adjustment )
{
     DFBResult ret;

     /* call the original initialization function first */
     ret = oldPrimaryFuncs.InitLayer( layer,
                                      oldPrimaryDriverData,
                                      layer_data, description,
                                      config, adjustment );
     if (ret)
          return ret;

     /* set name */
     snprintf(description->name,
              DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "CyberPro OSD");

     /* add support for options */
     config->flags |= DLCONF_OPTIONS;

     /* add some capabilities */
     description->caps |= DLCAPS_ALPHACHANNEL |
                          DLCAPS_OPACITY | DLCAPS_SRC_COLORKEY;

     return DFB_OK;
}

static DFBResult
osdTestRegion( CoreLayer                  *layer,
               void                       *driver_data,
               void                       *layer_data,
               CoreLayerRegionConfig      *config,
               CoreLayerRegionConfigFlags *failed )
{
     DFBResult                  ret;
     CoreLayerRegionConfigFlags fail = 0;
     DFBDisplayLayerOptions     options = config->options;

     /* remove options before calling the original function */
     config->options = DLOP_NONE;

     /* call the original function */
     ret = oldPrimaryFuncs.TestRegion( layer, oldPrimaryDriverData,
                                       layer_data, config, &fail );

     /* check options if specified */
     if (options) {
          /* any unsupported option wanted? */
          if (options & ~OSD_OPTIONS)
               fail |= CLRCF_OPTIONS;

          /* opacity and alpha channel cannot be used at once */
          if ((options & (DLOP_OPACITY | DLOP_ALPHACHANNEL)) ==
              (DLOP_OPACITY | DLOP_ALPHACHANNEL))
          {
               fail |= CLRCF_OPTIONS;
          }
     }

     /* restore options */
     config->options = options;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return ret;
}

static DFBResult
osdSetRegion( CoreLayer                  *layer,
              void                       *driver_data,
              void                       *layer_data,
              void                       *region_data,
              CoreLayerRegionConfig      *config,
              CoreLayerRegionConfigFlags  updated,
              CoreSurface                *surface,
              CorePalette                *palette )
{
     DFBResult ret;

     /* call the original function */
     ret = oldPrimaryFuncs.SetRegion( layer, oldPrimaryDriverData,
                                      layer_data, region_data,
                                      config, updated, surface,
                                      palette );
     if (ret)
          return ret;

     /* select pixel based or global alpha */
     if (config->options & DLOP_ALPHACHANNEL)
          cyber_select_alpha_src( ALPHA_GRAPHICS );
     else
          cyber_select_alpha_src( ALPHA_REGISTER );

     cyber_set_alpha_reg( config->opacity,
                          config->opacity,
                          config->opacity );

     /* source color keying */
     cyber_select_RAM_addr( RAM_CPU );
     cyber_set_alpha_RAM_reg( 0, 0x00, 0x00, 0x00 );
     cyber_select_magic_alpha_src( ALPHA_LOOKUP );
     cyber_enable_magic_alpha_blend( config->options & DLOP_SRC_COLORKEY );

     /* FIXME: hardcoded black color key */
     cyber_set_magic_match_reg( 0, 0, 0 );

     return DFB_OK;
}

DisplayLayerFuncs newPrimaryFuncs = {
     InitLayer:          osdInitLayer,

     TestRegion:         osdTestRegion,
     SetRegion:          osdSetRegion
};

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_IGS_CYBER2000:
          case FB_ACCEL_IGS_CYBER2010:
          case FB_ACCEL_IGS_CYBER5000:
#ifdef FB_ACCEL_IGS_CYBER5K
          case FB_ACCEL_IGS_CYBER5K:          /* CyberPro 5xxx */
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
               "Cyber Pro Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "directfb.org" );

     info->version.major = 0;
     info->version.minor = 4;

     info->driver_data_size = sizeof (CyberDriverData);
     info->device_data_size = sizeof (CyberDeviceData);
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     CyberDriverData *cdrv = (CyberDriverData*) driver_data;

     /* gain access to memory mapped registers */
     cdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!cdrv->mmio_base)
          return DFB_IO;

     /* HACK */
     cyber_mmio = cdrv->mmio_base;

     /* fill acceleration function table */
     funcs->EngineSync    = cyber5kEngineSync;
     funcs->CheckState    = cyber5kCheckState;
     funcs->SetState      = cyber5kSetState;

     funcs->FillRectangle = cyber5kFillRectangle;
     funcs->DrawRectangle = cyber5kDrawRectangle;
     funcs->DrawLine      = cyber5kDrawLine;
     funcs->Blit          = cyber5kBlit;

     /* install primary layer hooks */
     dfb_layers_hook_primary( device, driver_data, &newPrimaryFuncs,
                              &oldPrimaryFuncs, &oldPrimaryDriverData );

     /* add the video underlay */
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_IGS_CYBER5000:
#ifdef FB_ACCEL_IGS_CYBER5K
          case FB_ACCEL_IGS_CYBER5K:          /* CyberPro 5xxx */
#endif
               dfb_layers_register( dfb_screens_at(DSCID_PRIMARY),
                                    driver_data, &cyberUnderlayFuncs );
     }

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     CyberDriverData *cdrv = (CyberDriverData*) driver_data;
     volatile __u8   *mmio = cdrv->mmio_base;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "Cyber Pro" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "TVIA" );


     device_info->caps.flags    = 0;
     device_info->caps.accel    = CYBER5K_DRAWING_FUNCTIONS |
                                  CYBER5K_BLITTING_FUNCTIONS;
     device_info->caps.drawing  = CYBER5K_DRAWING_FLAGS;
     device_info->caps.blitting = CYBER5K_BLITTING_FLAGS;

     device_info->limits.surface_byteoffset_alignment = 16;
     device_info->limits.surface_pixelpitch_alignment = 4;


     /* set fifo policy at startup */
     cyber_grphw(0x74, 0x1b);
     cyber_grphw(0x75, 0x1e);

     cyber_grphw(0xD9, 0x0f);
     cyber_grphw(0xDA, 0x1b);
     cyber_grphw(0xDD, 0x00);

     cyber_seqw(0xD9, 0x0f);
     cyber_seqw(0xDA, 0x1b);
     cyber_seqw(0xDD, 0x00);



     cyber_out8 (mmio, COPFLAGS, 1);
     cyber_out8 (mmio, FMIX , 0x03);

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     CyberDriverData *cdrv = (CyberDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, cdrv->mmio_base, -1 );
}

