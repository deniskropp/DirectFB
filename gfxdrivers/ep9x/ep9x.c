/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Antonino Daplas <adaplas@pol.net>

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

#include <fbdev/fbdev.h>  /* FIXME: Needs to be included before dfb_types.h to work around a type clash with asm/types.h */
#include <dfb_types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/screens.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface_buffer.h>

#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( ep9x )

#include "ep9x.h"


D_DEBUG_DOMAIN( ep9x, "ep9x", "Cirrus Logic EP9xx" );


#define EP9X_SUPPORTED_DRAWINGFLAGS (DSDRAW_NOFX)


#define EP9X_SUPPORTED_DRAWINGFUNCTIONS (DFXL_FILLRECTANGLE | DFXL_DRAWLINE)

#define EP9X_SUPPORTED_BLITTINGFLAGS (DSBLIT_NOFX)

#define EP9X_SUPPORTED_BLITTINGFUNCTIONS  (DFXL_NONE)


static inline void 
ep9x_set_destination( EP9XDriverData *ep9xdrv,
                      EP9XDeviceData *ep9xdev,
                      CardState        *state )
{
     CoreSurfaceBuffer *buffer = state->dst.buffer;

     if (ep9xdev->smf_destination)
          return;

     ep9xdev->destaddr =  state->dst.offset;

     ep9xdev->destpitch = state->dst.pitch;

     switch (buffer->format) {
          case DSPF_RGB16:
               ep9xdev->pixeldepth = 2;
               ep9xdev->pixelformat = DSPF_RGB16;
               break;
          case DSPF_RGB24:
               ep9xdev->pixeldepth = 3;
               ep9xdev->pixelformat = DSPF_RGB24;
               break;
          case DSPF_RGB32:
               ep9xdev->pixeldepth = 3;
               ep9xdev->pixelformat = DSPF_RGB32;
               break;
          default:
               D_BUG("unexpected pixelformat~");
     }

     ep9xdev->smf_destination = 1;
}

static inline void
ep9x_set_src(EP9XDriverData *ep9xdrv,
             EP9XDeviceData *ep9xdev,
             CardState      *state)
{

     if (ep9xdev->smf_source)
          return;

     if ( state->src.phys ) {
          D_DEBUG_AT(ep9x,"%s:video data is stored in fb and offset is %lx\n",__FUNCTION__,state->src.offset);
          ep9xdev->srcaddr = state->src.offset;
          ep9xdev->fb_store = true;
     }
     else if ( state->src.addr ) {
          D_DEBUG_AT( ep9x,"%s:video data is stored in system\n",__FUNCTION__);
          ep9xdev->srcaddr = (unsigned long) state->src.addr;
          ep9xdev->fb_store = false;
     }
     else
          D_ERROR("NOT valid addr\n");

     ep9xdev->srcpitch = state->src.pitch;

     ep9xdev->smf_source = 1;

}

static inline void ep9x_set_color( EP9XDriverData *ep9xdrv,
                                   EP9XDeviceData *ep9xdev,
                                   CardState        *state )
{
     CoreSurfaceBuffer *buffer = state->dst.buffer;

     if (ep9xdev->smf_color)
          return;

     switch (buffer->format) {
          case DSPF_RGB16:
               ep9xdev->fill_color = PIXEL_RGB16( state->color.r,
                                                  state->color.g,
                                                  state->color.b );
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
               ep9xdev->fill_color = PIXEL_RGB32( state->color.r,
                                                  state->color.g,
                                                  state->color.b );
               break;

          default:
               D_ERROR( "unexpected pixelformat!" );
     }

     ep9xdev->smf_color = 1;

}

static inline void ep9x_set_clip( EP9XDriverData *ep9xdrv,
                                  EP9XDeviceData *ep9xdev,
                                  DFBRegion      *clip )
{
     if (ep9xdev->smf_clip)
          return;

     ep9xdev->clip.x1 = clip->x1;
     ep9xdev->clip.y1 = clip->y1;
     ep9xdev->clip.x2 = clip->x2 + 1;
     ep9xdev->clip.y2 = clip->y2 + 1;

     ep9xdev->smf_clip = 1;
}


static void
ep9xCheckState(void *drv, void *dev,
               CardState *state, DFBAccelerationMask accel )
{
     switch (state->destination->config.format) {
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
               break;
          default:
               return;
     }

     if (!(accel & ~EP9X_SUPPORTED_DRAWINGFUNCTIONS) &&
         !(state->drawingflags & ~EP9X_SUPPORTED_DRAWINGFLAGS))
          state->accel |= EP9X_SUPPORTED_DRAWINGFUNCTIONS;


     if (!(accel & ~EP9X_SUPPORTED_BLITTINGFUNCTIONS) &&
         !(state->blittingflags & ~EP9X_SUPPORTED_BLITTINGFLAGS)) {
          if (state->source->config.format == state->destination->config.format)
               state->accel |= EP9X_SUPPORTED_BLITTINGFUNCTIONS;
     }

}

static void
ep9xSetState( void *drv, void *dev,
              GraphicsDeviceFuncs *funcs,
              CardState *state, DFBAccelerationMask accel )
{
     EP9XDriverData *ep9xdrv = (EP9XDriverData *) drv;
     EP9XDeviceData *ep9xdev = (EP9XDeviceData *) dev;

     if (state->modified & SMF_SOURCE && state->source )
          ep9xdev->smf_source = 0;

     if (state->modified & SMF_DESTINATION)
          ep9xdev->smf_destination = ep9xdev->smf_color = 0;

     if (state->modified & SMF_COLOR)
          ep9xdev->smf_color = 0;

     if (state->modified & SMF_CLIP)
          ep9xdev->smf_clip = 0;

     ep9x_set_destination( ep9xdrv, ep9xdev, state);

     switch (accel) {
          case DFXL_FILLRECTANGLE:
               ep9x_set_color( ep9xdrv, ep9xdev, state );
               state->set |= DFXL_FILLRECTANGLE;
               break;
          case DFXL_DRAWLINE:
               ep9x_set_color( ep9xdrv, ep9xdev, state );
               state->set |= DFXL_DRAWLINE ;
               break;

          case DFXL_BLIT:
               ep9x_set_src( ep9xdrv, ep9xdev, state );
               state->set |= DFXL_BLIT;
               break;
          default:
               D_ERROR( "unexpected drawing/blitting function" );
               break;

     }

     if (state->modified & SMF_CLIP)
          ep9x_set_clip( ep9xdrv, ep9xdev, &state->clip);

     state->modified = 0;

}

static void
ep9xFlushTextureCache(void *drv, void *dev)
{
     EP9XDeviceData *ep9xdev = (EP9XDeviceData *) dev;

     ep9xdev->srcaddr = ep9xdev->destaddr = 0;
     ep9xdev->srcpitch = ep9xdev->destpitch = 0;
     ep9xdev->fb_store = false;

}

static DFBResult
ep9xEngineSync(void *drv, void *dev)
{

     return DFB_OK;
}

static void
ep9xEngineReset(void *drv, void *dev)
{
     memset((void*)dfb_system_video_memory_virtual(0),0,dfb_gfxcard_memory_length());
}


static bool ep9xFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     EP9XDriverData *ep9xdrv = (EP9XDriverData *) drv;
     EP9XDeviceData *ep9xdev = (EP9XDeviceData *) dev;
     struct ep9x_fill fill;
     D_DEBUG_AT(ep9x,"%s:enter\n",__FUNCTION__);

     fill.dx = rect->x;
     fill.dy = rect->y;
     fill.width = rect->w;
     fill.height = rect->h;
     fill.color = ep9xdev->fill_color;
     ioctl(ep9xdrv->dfb_fbdev->fd,FBIO_EP9X_FILL, &fill);
     D_DEBUG_AT(ep9x,"%s:exit\n",__FUNCTION__);
     return true;
}

static bool ep9xDrawLine( void *drv, void *dev, DFBRegion *line )
{
     EP9XDriverData *ep9xdrv = (EP9XDriverData *) drv;
     EP9XDeviceData *ep9xdev = (EP9XDeviceData *) dev;

     struct ep9x_line drawline;
     D_DEBUG_AT(ep9x,"%s:enter\n",__FUNCTION__);
     drawline.flags = 0;
     drawline.x1 = line->x1;
     drawline.x2 = line->x2;
     drawline.y1 = line->y1;
     drawline.y2 = line->y2;
     drawline.fgcolor = ep9xdev->fill_color;
     drawline.bgcolor = 0;
     drawline.pattern = 0;

     ioctl(ep9xdrv->dfb_fbdev->fd,FBIO_EP9X_LINE, &drawline);
     D_DEBUG_AT(ep9x,"%s:exit\n",__FUNCTION__);
     return true;
}


static bool
ep9xBlit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{

     EP9XDriverData *ep9xdrv = (EP9XDriverData *) drv;
     EP9XDeviceData *ep9xdev = (EP9XDeviceData *) dev;
     struct fb_image image;

     D_DEBUG_AT(ep9x,"%s:enter\n",__FUNCTION__);

     if (!(ep9xdev->clip.x1 <= dx) || !(ep9xdev->clip.y1 <= dy) || 
         !( ep9xdev->clip.x2 >= (dx + rect->w - 1) ) || !( ep9xdev->clip.y2 >= (dy + rect->h - 1) )) {
          D_ERROR("the blit region is not valid\n");
          return false;
     }

     image.dx = ep9xdev->destaddr + dx;
     image.dy = dy;
     image.width = rect->w; 
     image.height = rect->h;
     image.depth = ep9xdev->pixeldepth;
     if ( ep9xdev->fb_store == true )
          image.data = (void*)ep9xdev->fb_addr + ep9xdev->srcaddr + DFB_BYTES_PER_LINE( ep9xdev->pixelformat,rect->x ) + (rect->y * ep9xdev->srcpitch );
     else
          image.data = (void*)ep9xdev->srcaddr + DFB_BYTES_PER_LINE( ep9xdev->pixelformat, rect->x ) + (rect->y * ep9xdev->srcpitch );

     ioctl(ep9xdrv->dfb_fbdev->fd,FBIO_EP9X_BLIT,&image);

     D_DEBUG_AT(ep9x,"%s:exit\n",__FUNCTION__);

     return true;   
}

static int
driver_probe( CoreGraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_EP9X:          /* cirrus ep93xx serials */
               return 1;
     }
     return 0;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{

     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "ep9x 07/07A/12/15/15A Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "cirruslogic" );

     snprintf( info->url,
               DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
               "http://arm.cirrus.com" );

     snprintf( info->license,
               DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major = 0;
     info->version.minor = 1;

     info->driver_data_size = sizeof (EP9XDriverData);
     info->device_data_size = sizeof (EP9XDeviceData);


}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{

     EP9XDriverData *ep9xdrv = (EP9XDriverData*) driver_data;    
     EP9XDeviceData *ep9xdev = (EP9XDeviceData*) device_data;

     ep9xdrv->dfb_fbdev = dfb_system_data();

     ioctl(ep9xdrv->dfb_fbdev->fd,FBIO_EP9X_GET_ADDR,&(ep9xdev->fb_addr));

     funcs->CheckState    = ep9xCheckState;
     funcs->SetState      = ep9xSetState;
     funcs->EngineSync    = ep9xEngineSync;
     funcs->EngineReset   = ep9xEngineReset;
     funcs->FlushTextureCache  = ep9xFlushTextureCache;

     funcs->FillRectangle = ep9xFillRectangle;
     funcs->DrawLine          = ep9xDrawLine;
     funcs->Blit          = ep9xBlit;

     return DFB_OK;
}


static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{

     EP9XDeviceData *ep9xdev = (EP9XDeviceData*) device_data;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "ep9x" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "cirruslogic" );

     device_info->caps.flags    = 0;
     device_info->caps.accel    = EP9X_SUPPORTED_DRAWINGFUNCTIONS |
                                  EP9X_SUPPORTED_BLITTINGFUNCTIONS;

     device_info->caps.drawing  = EP9X_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = EP9X_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     ep9xdev->fb_store = false;


     return DFB_OK;

}


static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{

     EP9XDriverData *ep9xdrv = (EP9XDriverData*) driver_data;

     ep9xdrv->dfb_fbdev = NULL;
}
