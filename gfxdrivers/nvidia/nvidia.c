/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan82@cheapnet.it>.

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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#include <linux/fb.h>

#ifdef USE_SYSFS
# include <sysfs/libsysfs.h>
#endif

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/system.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( nvidia )

#include "nvidia.h"
#include "nvidia_2d.h"
#include "nvidia_3d.h"
#include "nvidia_tables.h"


#define NV_LOAD_TABLE( dest, table )                             \
{                                                                \
     int _i = 0;                                                 \
     for (; _i < sizeof(nv##table) / sizeof(nv##table[0]); _i++) \
          dest[nv##table[_i][0]] = nv##table[_i][1];             \
}


/* TNT */
#define NV4_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV4_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV4_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_NOFX)

#define NV4_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)

/* TNT2 or newer */
#define NV5_SUPPORTED_DRAWINGFLAGS \
               (DSDRAW_BLEND)

#define NV5_SUPPORTED_DRAWINGFUNCTIONS \
               (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | \
                DFXL_FILLTRIANGLE | DFXL_DRAWLINE)

#define NV5_SUPPORTED_BLITTINGFLAGS \
               (DSBLIT_BLEND_COLORALPHA)

#define NV5_SUPPORTED_BLITTINGFUNCTIONS \
               (DFXL_BLIT | DFXL_STRETCHBLIT)



static inline void
nv_waitidle( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev )
{
     while (nvdrv->PGRAPH[0x700/4] & 1)
          nvdev->idle_waitcycles++;
}

static inline void
nv_set_format( NVidiaDriverData      *nvdrv,
               NVidiaDeviceData      *nvdev,
               DFBSurfacePixelFormat  format )
{
     volatile NVSurfaces2D *Surfaces2D = nvdrv->Surfaces2D;
     volatile NVSurfaces3D *Surfaces3D = nvdrv->Surfaces3D;
     volatile NVRectangle  *Rectangle  = nvdrv->Rectangle;
     volatile NVTriangle   *Triangle   = nvdrv->Triangle;
     volatile NVLine       *Line       = nvdrv->Line;
     __u32                  sformat2D  = 0;
     __u32                  sformat3D  = 0;
     __u32                  pformat    = 0;

     switch (format) {
          case DSPF_ARGB1555:
               sformat2D = 0x00000002;
               sformat3D = 0x00000101;
               pformat   = 0x00000002;
               break;
          case DSPF_RGB16:
               sformat2D = 0x00000004;
               sformat3D = 0x00000103;
               pformat   = 0x00000001;
               break;
          case DSPF_RGB32:
               sformat2D = 0x00000006;
               sformat3D = 0x00000106;
               pformat   = 0x00000003;
               break;
          case DSPF_ARGB:
               sformat2D = 0x0000000A;
               sformat3D = 0x00000108;
               pformat   = 0x00000003;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               return;
     }

     nv_waitfifo( nvdev, Surfaces2D, 4 );
     Surfaces2D->SetColorFormat = sformat2D;
     nvdrv->FIFO[0]             = 0x80000015;
     Surfaces3D->SetColorFormat = sformat3D;
     nvdrv->FIFO[0]             = 0x80000000;
     
     nv_waitfifo( nvdev, Rectangle, 1 );
     Rectangle->SetColorFormat  = pformat;

     nv_waitfifo( nvdev, Triangle, 1 );
     Triangle->SetColorFormat   = pformat;

     nv_waitfifo( nvdev, Line, 1 );
     Line->SetColorFormat       = pformat;
}

static inline void
nv_set_clip( NVidiaDriverData *nvdrv,
             NVidiaDeviceData *nvdev,
             DFBRegion        *clip )
{
     volatile NVClip *Clip   = nvdrv->Clip;
     int              width  = clip->x2 - clip->x1 + 1;
     int              height = clip->y2 - clip->y1 + 1;

     nv_waitfifo( nvdev, Clip, 2 );
     Clip->TopLeft     = (clip->y1 << 16) | clip->x1;
     Clip->WidthHeight = (height << 16) | width;
}

static inline void
nv_set_color( NVidiaDriverData *nvdrv,
              NVidiaDeviceData *nvdev,
              DFBColor         *color )
{
     switch (nvdev->dst_format) {
          case DSPF_ARGB1555:
               nvdev->color = PIXEL_ARGB1555( color->a,
                                              color->r,
                                              color->g,
                                              color->b );
               break;
          case DSPF_RGB16:
               nvdev->color = PIXEL_RGB16( color->r,
                                           color->g,
                                           color->b );
               break;
          case DSPF_RGB32:
               nvdev->color = PIXEL_RGB32( color->r,
                                           color->g,
                                           color->b );
               break;
          case DSPF_ARGB:
               nvdev->color = PIXEL_ARGB( color->a,
                                          color->r,
                                          color->g,
                                          color->b );
               break;

          case DSPF_YUY2:
          case DSPF_UYVY:
               break;

          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }

     nvdev->color3d = PIXEL_ARGB( color->a,
                                  color->r,
                                  color->g,
                                  color->b );
}


static void nvEngineSync( void *drv, void *dev )
{
     nv_waitidle( (NVidiaDriverData*) drv, (NVidiaDeviceData*) dev );
}

static void nv4CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     (void) nvdev;
     
     switch (destination->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
       
          /*case DSPF_YUY2:
           case DSPF_UYVY:
               if (accel != DFXL_BLIT)
                    return;
               break;*/

          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting flags first */
          if (state->blittingflags & ~NV4_SUPPORTED_BLITTINGFLAGS)
               return;

          switch (source->format) {
               case DSPF_ARGB1555:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
                    
               case DSPF_RGB16:
                    if (accel == DFXL_STRETCHBLIT || 
                        destination->format != DSPF_RGB16)
                         return;
                    break;
                    
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if ((destination->format == DSPF_YUY2  ||
                         destination->format == DSPF_UYVY) &&
                        destination->format != source->format)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags first */
          if (state->drawingflags & ~NV4_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= accel;
     }
}

static void nv5CheckState( void *drv, void *dev,
                           CardState *state, DFBAccelerationMask accel )
{
     NVidiaDeviceData *nvdev       = (NVidiaDeviceData*) dev;
     CoreSurface      *destination = state->destination;
     CoreSurface      *source      = state->source;

     (void) nvdev;

     switch (destination->format) {
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;
       
          /*case DSPF_YUY2:
           case DSPF_UYVY:
               if (accel != DFXL_BLIT)
                    return;
               break;*/
       
          default:
               return;
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          /* check unsupported blitting flags first */
          if (state->blittingflags & ~NV5_SUPPORTED_BLITTINGFLAGS)
               return;

          switch (source->format) {
               case DSPF_ARGB1555:
               case DSPF_RGB16:
               case DSPF_RGB32:
               case DSPF_ARGB:
                    break;
               
               case DSPF_YUY2:
               case DSPF_UYVY:
                    if ((destination->format == DSPF_YUY2  ||
                         destination->format == DSPF_UYVY) &&
                         destination->format != source->format)
                         return;
                    break;
               
               default:
                    return;
          }

          state->accel |= accel;
     }
     else {
          /* check unsupported drawing flags first */
          if (state->drawingflags & ~NV5_SUPPORTED_DRAWINGFLAGS)
               return;

          state->accel |= accel;
     }
}

static void nv4SetState( void *drv, void *dev,
                         GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     __u32             modify = state->modified;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (nvdev->reloaded) {
          modify |= SMF_DESTINATION | SMF_CLIP;
          nvdev->reloaded = false;
     }

     if (modify & SMF_DESTINATION) {
          SurfaceBuffer *buffer = state->destination->back_buffer;
          __u32          offset = buffer->video.offset & 0xFFFFFF;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x640/4] = offset;
               PGRAPH[0x648/4] = offset;
               PGRAPH[0x670/4] = buffer->video.pitch;
               PGRAPH[0x678/4] = buffer->video.pitch;

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          SurfaceBuffer *buffer = state->source->front_buffer;
          __u32          offset = buffer->video.offset & 0xFFFFFF;

          if (nvdev->src_format != buffer->format     ||
              nvdev->src_offset != offset             ||
              nvdev->src_pitch  != buffer->video.pitch)
          {
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x644/4] = offset;
               PGRAPH[0x674/4] = buffer->video.pitch;

               nvdev->src_format = buffer->format;
               nvdev->src_offset = offset;
               nvdev->src_pitch  = buffer->video.pitch;
          }

          nvdev->src_width  = state->source->width;
          nvdev->src_height = state->source->height;
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     if (modify & SMF_DRAWING_FLAGS) {
          if (state->drawingflags & DSDRAW_BLEND) {
               nvdev->state3d.blend &= 0x00FFFFFF;
               nvdev->state3d.blend |= (state->src_blend << 24) |
                                       (state->dst_blend << 28);

               funcs->FillRectangle = nvFillRectangle3D;
               funcs->FillTriangle  = nvFillTriangle3D;
               funcs->DrawRectangle = nvDrawRectangle3D;
               funcs->DrawLine      = nvDrawLine3D;
          } else {
               funcs->FillRectangle = nvFillRectangle2D;
               funcs->FillTriangle  = nvFillTriangle2D;
               funcs->DrawRectangle = nvDrawRectangle2D;
               funcs->DrawLine      = nvDrawLine2D;
          }
     }

     state->modified = 0;
}

static void nv5SetState( void *drv, void *dev,
                         GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     __u32             modify = state->modified;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (nvdev->reloaded) {
          modify |= SMF_DESTINATION | SMF_CLIP;
          nvdev->reloaded = false;
     }

     if (modify & SMF_DESTINATION) {
          SurfaceBuffer *buffer = state->destination->back_buffer;
          __u32          offset = buffer->video.offset & 0x1FFFFFF;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x640/4] = offset;
               PGRAPH[0x648/4] = offset;
               PGRAPH[0x670/4] = buffer->video.pitch;
               PGRAPH[0x678/4] = buffer->video.pitch;

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          SurfaceBuffer *buffer = state->source->front_buffer;
          __u32          offset = buffer->video.offset & 0x1FFFFFF;

          if (nvdev->src_format != buffer->format     ||
              nvdev->src_offset != offset             ||
              nvdev->src_pitch  != buffer->video.pitch)
          {
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x644/4] = offset;
               PGRAPH[0x674/4] = buffer->video.pitch;

               nvdev->src_format = buffer->format;
               nvdev->src_offset = offset;
               nvdev->src_pitch  = buffer->video.pitch;
          }

          nvdev->src_width  = state->source->width;
          nvdev->src_height = state->source->height;
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     if (modify & SMF_DRAWING_FLAGS) {
          if (state->drawingflags & DSDRAW_BLEND) {
               nvdev->state3d.blend &= 0x00FFFFFF;
               nvdev->state3d.blend |= (state->src_blend << 24) |
                                       (state->dst_blend << 28);

               funcs->FillRectangle = nvFillRectangle3D;
               funcs->FillTriangle  = nvFillTriangle3D;
               funcs->DrawRectangle = nvDrawRectangle3D;
               funcs->DrawLine      = nvDrawLine3D;
          } else {
               funcs->FillRectangle = nvFillRectangle2D;
               funcs->FillTriangle  = nvFillTriangle2D;
               funcs->DrawRectangle = nvDrawRectangle2D;
               funcs->DrawLine      = nvDrawLine2D;
          }
     }

     if (modify & SMF_BLITTING_FLAGS) {
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               nvdev->blitfx = 0x00000002;
               modify       |= SMF_COLOR;
          } else
               nvdev->blitfx = 0x00000000;
     }

     /* set alpha value if using transparency */
     if (nvdev->blitfx && (modify & SMF_COLOR))
          PGRAPH[0x608/4] = state->color.a << 23;

     state->modified = 0;
}

static void nv20SetState( void *drv, void *dev,
                          GraphicsDeviceFuncs *funcs,
                          CardState *state, DFBAccelerationMask accel )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) drv;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) dev;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     __u32             modify = state->modified;

     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_DRAWLINE:
               state->set |= DFXL_FILLRECTANGLE |
                             DFXL_FILLTRIANGLE  |
                             DFXL_DRAWRECTANGLE |
                             DFXL_DRAWLINE;
               break;

          case DFXL_BLIT:
          case DFXL_STRETCHBLIT:
               state->set |= DFXL_BLIT |
                             DFXL_STRETCHBLIT;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     if (nvdev->reloaded) {
          modify |= SMF_DESTINATION | SMF_CLIP;
          nvdev->reloaded = false;
     }

     if (modify & SMF_DESTINATION) {
          SurfaceBuffer *buffer = state->destination->back_buffer;
          __u32          offset = buffer->video.offset;

          if (nvdrv->chip == 0x2A0) /* GeForce3 XBox */
               offset += nvdrv->fb_base;
          offset &= 0x3FFFFFF;

          if (nvdev->dst_format != buffer->format     ||
              nvdev->dst_offset != offset             ||
              nvdev->dst_pitch  != buffer->video.pitch)
          {
               /* set offset & pitch */
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x820/4] = offset;
               PGRAPH[0x828/4] = offset;
               PGRAPH[0x850/4] = buffer->video.pitch;
               PGRAPH[0x858/4] = buffer->video.pitch;

               if (nvdev->dst_format != buffer->format)
                    nv_set_format( nvdrv, nvdev, buffer->format );

               nvdev->dst_format = buffer->format;
               nvdev->dst_offset = offset;
               nvdev->dst_pitch  = buffer->video.pitch;
          }
     }

     if (DFB_BLITTING_FUNCTION( accel )) {
          SurfaceBuffer *buffer = state->source->front_buffer;
          __u32          offset = buffer->video.offset;

          if (nvdrv->chip == 0x2A0) /* GeForce3 XBox */
               offset += nvdrv->fb_base;
          offset &= 0x3FFFFFF;

          if (nvdev->src_format != buffer->format     ||
              nvdev->src_offset != offset             ||
              nvdev->src_pitch  != buffer->video.pitch)
          {
               nv_waitidle( nvdrv, nvdev );

               PGRAPH[0x824/4] = offset;
               PGRAPH[0x854/4] = buffer->video.pitch;

               nvdev->src_format = buffer->format;
               nvdev->src_offset = offset;
               nvdev->src_pitch  = buffer->video.pitch;
          }

          nvdev->src_width  = state->source->width;
          nvdev->src_height = state->source->height;
     }

     if (modify & SMF_CLIP)
          nv_set_clip( nvdrv, nvdev, &state->clip );

     if (modify & (SMF_COLOR | SMF_DESTINATION))
          nv_set_color( nvdrv, nvdev, &state->color );

     if (modify & SMF_DRAWING_FLAGS) {
          if (state->drawingflags & DSDRAW_BLEND) {
               nvdev->state3d.blend &= 0x00FFFFFF;
               nvdev->state3d.blend |= (state->src_blend << 24) |
                                       (state->dst_blend << 28);

               funcs->FillRectangle = nvFillRectangle3D;
               funcs->FillTriangle  = nvFillTriangle3D;
               funcs->DrawRectangle = nvDrawRectangle3D;
               funcs->DrawLine      = nvDrawLine3D;
          } else {
               funcs->FillRectangle = nvFillRectangle2D;
               funcs->FillTriangle  = nvFillTriangle2D;
               funcs->DrawRectangle = nvDrawRectangle2D;
               funcs->DrawLine      = nvDrawLine2D;
          }
     }

     if (modify & SMF_BLITTING_FLAGS) {
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               nvdev->blitfx = 0x00000002;
               modify       |= SMF_COLOR;
          } else
               nvdev->blitfx = 0x00000000;
     }

     /* set alpha value if using transparency */
     if (nvdev->blitfx && (modify & SMF_COLOR))
          PGRAPH[0x608/4] = state->color.a << 23;

     state->modified = 0;
}


static void nvAfterSetVar( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) device_data;
     volatile __u32   *PRAMIN = nvdrv->PRAMIN;
     volatile __u32   *PGRAPH = nvdrv->PGRAPH;
     volatile __u32   *FIFO   = nvdrv->FIFO;
     
     /* write objects configuration */
     NV_LOAD_TABLE( PRAMIN, PRAMIN )

     /* set architecture specific configuration */
     switch (nvdrv->arch) {
          case NV_ARCH_04:
               NV_LOAD_TABLE( PRAMIN, PRAMIN04 )
               break;
          case NV_ARCH_05:
               NV_LOAD_TABLE( PRAMIN, PRAMIN05 )
               break;
          case NV_ARCH_10:
          case NV_ARCH_20:
          default:
               NV_LOAD_TABLE( PRAMIN, PRAMIN10 )
               NV_LOAD_TABLE( PGRAPH, PGRAPH10 )
               break;
     }

     switch (dfb_primary_layer_pixelformat()) {
          case DSPF_ARGB1555:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB1555 )
               NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB1555 )
               break;
          case DSPF_RGB16:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB16 )
               NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB16 )
               break;
          case DSPF_RGB32:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_RGB32 )
               NV_LOAD_TABLE( PGRAPH, PGRAPH_RGB32 )
               break;
          case DSPF_ARGB:
               NV_LOAD_TABLE( PRAMIN, PRAMIN_ARGB )
               NV_LOAD_TABLE( PGRAPH, PGRAPH_ARGB )
               break;
          default:
               break;
     }

     /* put objects into subchannels */
     NV_LOAD_TABLE( FIFO, FIFO )

     nvdev->reloaded   = true;
     nvdev->dst_format = DSPF_UNKNOWN;
     nvdev->src_format = DSPF_UNKNOWN;
}

static void nvEngineReset( void *driver_data,
                           void *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;
     
     /* set screen type (0=vga; 2=hsync) */
#ifdef WORDS_BIGENDIAN
     nvdrv->PCRTC[0x804/4] = 0x80000002;
#else
     nvdrv->PCRTC[0x804/4] = 0x00000002;
#endif

     /* set default Rop3 */
     nvdrv->PGRAPH[0x604/4] = 0x000000CC; /* copy */

     /* set default Beta (alpha) */
     nvdrv->PGRAPH[0x608/4] = 0x7F800000;
     
     /* set default pattern */
     nvdrv->PGRAPH[0x800/4] = 0xFFFFFFFF; /* Color0 */
     nvdrv->PGRAPH[0x804/4] = 0xFFFFFFFF; /* Color1 */
     nvdrv->PGRAPH[0x810/4] = 0x00000000; /* Shape */
}

/* exported symbols */

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_NV4:
          case FB_ACCEL_NV5:
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
               "nVidia TNT/TNT2/GeForce Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "convergence integrated media GmbH" );

     info->version.major = 0;
     info->version.minor = 3;

     info->driver_data_size = sizeof (NVidiaDriverData);
     info->device_data_size = sizeof (NVidiaDeviceData);
}

static void
nv_find_architecture( NVidiaDriverData *nvdrv )
{
     char  buf[512];
     __u32 chip  = 0;
     __u32 arch  = 0;

#ifdef USE_SYSFS
     if (!sysfs_get_mnt_path( buf, 512 )) {
          struct dlist           *devices;
          struct sysfs_device    *dev;
          struct sysfs_attribute *attr;
          int                     bus;

          devices = sysfs_open_bus_devices_list( "pci" );
          if (devices) {
               dlist_for_each_data( devices, dev, struct sysfs_device ) {
                    if (sscanf( dev->name, "0000:%02x:", &bus ) < 1 || bus < 1)
                         continue;

                    dev = sysfs_open_device( "pci", dev->name );
                    if (!dev)
                         continue;

                    attr = sysfs_get_device_attr( dev, "vendor" );
                    if (!attr || strncasecmp( attr->value, "0x10de", 6 )) {
                         sysfs_close_device( dev );
                         continue;
                    }

                    attr = sysfs_get_device_attr( dev, "device" );
                    if (attr)
                         sscanf( attr->value, "0x%04x", &chip );

                    sysfs_close_device( dev );
                    break;
               }

               sysfs_close_list( devices );
          }
     }
#endif /* USE_SYSFS */

     /* try /proc interface */
     if (!chip) {
          FILE  *fp;
          __u32  device;
          __u32  vendor;

          fp = fopen( "/proc/bus/pci/devices", "r" );
          if (!fp) {
               D_PERROR( "DirectFB/NVidia: "
                         "couldn't access /proc/bus/pci/devices!\n" );
               return;
          }

          while (fgets( buf, 512, fp )) {
               if (sscanf( buf, "%04x\t%04x%04x",
                              &device, &vendor, &chip ) != 3)
                    continue;
               
               if (device >= 0x0100 && vendor == 0x10DE)
                    break;
               
               chip = 0;
          }

          fclose( fp );
     }

     switch (chip) {
          case 0x0020:
               arch = NV_ARCH_04;
               break;
          case 0x0028 ... 0x00A0:
               arch = NV_ARCH_05;
               break;
          case 0x0100 ... 0x0153:
               arch = NV_ARCH_10;
               break;
          case 0x0200 ... 0x02A0:
               arch = NV_ARCH_20;
               break;
          default:
               break;
     }

     if (arch) {
          nvdrv->chip = chip;
          nvdrv->arch = arch;
          D_INFO( "DirectFB/NVidia: "
                  "found nVidia Architecture %02x (Chipset %04x)\n",
                  arch, chip );
     } else
          D_INFO( "DirectFB/NVidia: "
                  "assuming nVidia Architecture 04\n" );
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     nvdrv->device = device;
     nvdrv->arch   = NV_ARCH_04;

     nv_find_architecture( nvdrv );

     nvdrv->fb_base   = (__u32) dfb_gfxcard_memory_physical( device, 0 ); 
     nvdrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!nvdrv->mmio_base)
          return DFB_IO;

     nvdrv->PVIDEO      = (volatile __u32*) (nvdrv->mmio_base + 0x008000);
     nvdrv->PVIO        = (volatile __u8 *) (nvdrv->mmio_base + 0x0C0000);
     nvdrv->PFB         = (volatile __u32*) (nvdrv->mmio_base + 0x100000);
     nvdrv->PGRAPH      = (volatile __u32*) (nvdrv->mmio_base + 0x400000);
     nvdrv->PCRTC       = (volatile __u32*) (nvdrv->mmio_base + 0x600000);
     nvdrv->PCIO        = (volatile __u8 *) (nvdrv->mmio_base + 0x601000);
     nvdrv->PRAMIN      = (volatile __u32*) (nvdrv->mmio_base + 0x710000);
     nvdrv->FIFO        = (volatile __u32*) (nvdrv->mmio_base + 0x800000);

     nvdrv->Surfaces2D  = (volatile NVSurfaces2D         *) &nvdrv->FIFO[0x0000/4];
     nvdrv->Surfaces3D  = (volatile NVSurfaces3D         *) &nvdrv->FIFO[0x0000/4];
     nvdrv->Clip        = (volatile NVClip               *) &nvdrv->FIFO[0x2000/4];
     nvdrv->Rectangle   = (volatile NVRectangle          *) &nvdrv->FIFO[0x4000/4];
     nvdrv->Triangle    = (volatile NVTriangle           *) &nvdrv->FIFO[0x6000/4];
     nvdrv->Line        = (volatile NVLine               *) &nvdrv->FIFO[0x8000/4];
     nvdrv->Blt         = (volatile NVScreenBlt          *) &nvdrv->FIFO[0xA000/4];
     nvdrv->ScaledImage = (volatile NVScaledImage        *) &nvdrv->FIFO[0xC000/4];
     nvdrv->TexTri      = (volatile NVTexturedTriangle05 *) &nvdrv->FIFO[0xE000/4];

     funcs->AfterSetVar   = nvAfterSetVar;
     funcs->EngineReset   = nvEngineReset;
     funcs->EngineSync    = nvEngineSync;
     funcs->FillRectangle = nvFillRectangle2D; // dynamic
     funcs->FillTriangle  = nvFillTriangle2D;  // dynamic
     funcs->DrawRectangle = nvDrawRectangle2D; // dynamic
     funcs->DrawLine      = nvDrawLine2D;      // dynamic

     switch (nvdrv->arch) {
          case NV_ARCH_04:
               funcs->CheckState  = nv4CheckState;
               funcs->SetState    = nv4SetState;
               funcs->Blit        = nv4Blit;
               funcs->StretchBlit = nv4StretchBlit;
               break;
          case NV_ARCH_05:
          case NV_ARCH_10:
               funcs->CheckState  = nv5CheckState;
               funcs->SetState    = nv5SetState;
               funcs->Blit        = nv5Blit;
               funcs->StretchBlit = nv5StretchBlit;
               break;
          case NV_ARCH_20:
               funcs->CheckState  = nv5CheckState;
               funcs->SetState    = nv20SetState;
               funcs->Blit        = nv5Blit;
               funcs->StretchBlit = nv5StretchBlit;
               break;
          default:
               break;
     }

     /* probably this should be disabled until we have a real WaitForSync */
     dfb_screens_register_primary( device, driver_data,
                                   &nvidiaPrimaryScreenFuncs );
     
     dfb_layers_hook_primary( device, driver_data,
                              &nvidiaPrimaryLayerFuncs, 
                              &nvidiaOldPrimaryLayerFuncs,
                              &nvidiaOldPrimaryLayerDriverData );
     
     dfb_layers_register( dfb_screens_at( DSCID_PRIMARY ),
                          driver_data, &nvidiaOverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     NVidiaDriverData *nvdrv  = (NVidiaDriverData*) driver_data;
     NVidiaDeviceData *nvdev  = (NVidiaDeviceData*) device_data;
     __u32             len;
     int               offset;

     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "TNT/TNT2/GeForce" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "nVidia" );

     device_info->caps.flags = CCF_CLIPPING;

     if (nvdrv->arch >= NV_ARCH_05) {
          device_info->caps.accel    = NV5_SUPPORTED_DRAWINGFUNCTIONS |
                                       NV5_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = NV5_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = NV5_SUPPORTED_BLITTINGFLAGS;
     } else {
          device_info->caps.accel    = NV4_SUPPORTED_DRAWINGFUNCTIONS |
                                       NV4_SUPPORTED_BLITTINGFUNCTIONS;
          device_info->caps.drawing  = NV4_SUPPORTED_DRAWINGFLAGS;
          device_info->caps.blitting = NV4_SUPPORTED_BLITTINGFLAGS;
     }

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     dfb_config->pollvsync_after = 1;
     
     /* reserve memory for colors buffer (must be aligned at 256) */
     len    = 128 + ((dfb_system_videoram_length() - 128) & 0xFF);
     offset = dfb_gfxcard_reserve_memory( nvdrv->device, len );
     if (offset < 0) {
          D_ERROR( "DirectFB/NVidia: "
                   "couldn't reserve %i bytes of video memory.\n", len );
          return DFB_NOVIDEOMEMORY;
     }

     /* clear colors buffer */
     memset( dfb_system_video_memory_virtual( offset ), 0xFF, len );

     /* set default 3d state */
     nvdev->state3d.colorkey = 0;
     if (nvdrv->chip == 0x2A0) /* GeForce3 XBox */
          nvdev->state3d.offset = (nvdrv->fb_base + offset) & 0x3FFFFFF;
     else
          nvdev->state3d.offset = offset;
     nvdev->state3d.format   = 0x111115A1;
     nvdev->state3d.filter   = 0x11000000;
     nvdev->state3d.blend    = 0x65100164;
     nvdev->state3d.control  = 0x40180800;
     nvdev->state3d.fog      = 0;

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
                     void           *driver_data,
                     void           *device_data )
{
     NVidiaDeviceData *nvdev = (NVidiaDeviceData*) device_data;

     (void) nvdev;

     D_DEBUG( "DirectFB/NVidia: FIFO Performance Monitoring:\n" );
     D_DEBUG( "DirectFB/NVidia:  %9d nv_waitfifo calls\n",
               nvdev->waitfifo_calls );
     D_DEBUG( "DirectFB/NVidia:  %9d register writes (nv_waitfifo sum)\n",
               nvdev->waitfifo_sum );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO wait cycles (depends on CPU)\n",
               nvdev->fifo_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d IDLE wait cycles (depends on CPU)\n",
               nvdev->idle_waitcycles );
     D_DEBUG( "DirectFB/NVidia:  %9d FIFO space cache hits(depends on CPU)\n",
               nvdev->fifo_cache_hits );
     D_DEBUG( "DirectFB/NVidia: Conclusion:\n" );
     D_DEBUG( "DirectFB/NVidia:  Average register writes/nvidia_waitfifo"
               " call:%.2f\n",
               nvdev->waitfifo_sum/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/NVidia:  Average wait cycles/nvidia_waitfifo call:"
               " %.2f\n",
               nvdev->fifo_waitcycles/(float)(nvdev->waitfifo_calls) );
     D_DEBUG( "DirectFB/NVidia:  Average fifo space cache hits: %02d%%\n",
               (int)(100 * nvdev->fifo_cache_hits/
               (float)(nvdev->waitfifo_calls)) );
}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
     NVidiaDriverData *nvdrv = (NVidiaDriverData*) driver_data;

     dfb_gfxcard_unmap_mmio( device, nvdrv->mmio_base, -1 );
}

