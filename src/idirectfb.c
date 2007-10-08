/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <string.h>

#include <directfb.h>
#include <directfb_version.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/clipboard.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/surface.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windows_internal.h> /* FIXME */
#include <core/windowstack.h>

#include <display/idirectfbpalette.h>
#include <display/idirectfbscreen.h>
#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_layer.h>
#include <display/idirectfbsurface_window.h>
#include <display/idirectfbdisplaylayer.h>
#include <input/idirectfbinputbuffer.h>
#include <input/idirectfbinputdevice.h>
#include <media/idirectfbfont.h>
#include <media/idirectfbimageprovider.h>
#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>

#include <idirectfb.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <misc/util.h>

D_DEBUG_DOMAIN( IDFB, "IDirectFB", "DirectFB Main Interface" );

/**********************************************************************************************************************/

typedef struct {
     DFBScreenCallback  callback;
     void              *callback_ctx;
} EnumScreens_Context;

typedef struct {
     IDirectFBScreen **interface;
     DFBScreenID       id;
     DFBResult         ret;
} GetScreen_Context;

typedef struct {
     DFBDisplayLayerCallback  callback;
     void                    *callback_ctx;
} EnumDisplayLayers_Context;

typedef struct {
     IDirectFBDisplayLayer **interface;
     DFBDisplayLayerID       id;
     DFBResult               ret;
     CoreDFB                *core;
} GetDisplayLayer_Context;

typedef struct {
     DFBInputDeviceCallback  callback;
     void                   *callback_ctx;
} EnumInputDevices_Context;

typedef struct {
     IDirectFBInputDevice **interface;
     DFBInputDeviceID       id;
     DFBResult              ret;
} GetInputDevice_Context;

typedef struct {
     IDirectFBEventBuffer       **interface;
     DFBInputDeviceCapabilities   caps;
} CreateEventBuffer_Context;

/**********************************************************************************************************************/

static DFBEnumerationResult EnumScreens_Callback      ( CoreScreen  *screen,
                                                        void        *ctx );
static DFBEnumerationResult GetScreen_Callback        ( CoreScreen  *screen,
                                                        void        *ctx );

static DFBEnumerationResult EnumDisplayLayers_Callback( CoreLayer   *layer,
                                                        void        *ctx );
static DFBEnumerationResult GetDisplayLayer_Callback  ( CoreLayer   *layer,
                                                        void        *ctx );

static DFBEnumerationResult EnumInputDevices_Callback ( CoreInputDevice *device,
                                                        void            *ctx );
static DFBEnumerationResult GetInputDevice_Callback   ( CoreInputDevice *device,
                                                        void            *ctx );

static DFBEnumerationResult CreateEventBuffer_Callback( CoreInputDevice *device,
                                                        void            *ctx );

static ReactionResult focus_listener( const void *msg_data,
                                      void       *ctx );

static bool input_filter_local( DFBEvent *evt,
                                void     *ctx );

static bool input_filter_global( DFBEvent *evt,
                                 void     *ctx );

static void drop_window( IDirectFB_data *data );

/**********************************************************************************************************************/

/*
 * Destructor
 *
 * Free data structure and set the pointer to NULL,
 * to indicate the dead interface.
 */
void
IDirectFB_Destruct( IDirectFB *thiz )
{
     int             i;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     for (i=0; i<MAX_LAYERS; i++) {
          if (data->layers[i].context) {
               if (data->layers[i].palette)
                    dfb_palette_unref( data->layers[i].palette );

               dfb_surface_unref( data->layers[i].surface );
               dfb_layer_region_unref( data->layers[i].region );
               dfb_layer_context_unref( data->layers[i].context );
          }
     }

     if (data->primary.context)
          dfb_layer_context_unref( data->primary.context );

     dfb_layer_context_unref( data->context );

     drop_window( data );

     dfb_core_destroy( data->core, false );

     idirectfb_singleton = NULL;

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}


static DFBResult
IDirectFB_AddRef( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFB_Release( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDirectFB_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFB_SetCooperativeLevel( IDirectFB           *thiz,
                               DFBCooperativeLevel  level )
{
     DFBResult         ret;
     CoreLayerContext *context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %d )\n", __FUNCTION__, thiz, level );

     if (level == data->level)
          return DFB_OK;

     switch (level) {
          case DFSCL_NORMAL:
               data->primary.focused = false;

               dfb_layer_context_unref( data->primary.context );

               data->primary.context = NULL;
               break;

          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE:
               if (dfb_config->force_windowed || dfb_config->force_desktop)
                    return DFB_ACCESSDENIED;

               if (data->level == DFSCL_NORMAL) {
                    ret = dfb_layer_create_context( data->layer, &context );
                    if (ret)
                         return ret;

                    ret = dfb_layer_activate_context( data->layer, context );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }

                    drop_window( data );

                    data->primary.context = context;
               }

               data->primary.focused = true;
               break;

          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
IDirectFB_GetDeviceDescription( IDirectFB                    *thiz,
                                DFBGraphicsDeviceDescription *ret_desc )
{
     GraphicsDeviceInfo device_info;
     GraphicsDriverInfo driver_info;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!ret_desc)
          return DFB_INVARG;

     dfb_gfxcard_get_device_info( &device_info );
     dfb_gfxcard_get_driver_info( &driver_info );

     ret_desc->acceleration_mask = device_info.caps.accel;
     ret_desc->blitting_flags    = device_info.caps.blitting;
     ret_desc->drawing_flags     = device_info.caps.drawing;
     ret_desc->video_memory      = dfb_gfxcard_memory_length();

     snprintf( ret_desc->name,   DFB_GRAPHICS_DEVICE_DESC_NAME_LENGTH, device_info.name );
     snprintf( ret_desc->vendor, DFB_GRAPHICS_DEVICE_DESC_NAME_LENGTH, device_info.vendor );

     ret_desc->driver.major = driver_info.version.major;
     ret_desc->driver.minor = driver_info.version.minor;

     snprintf( ret_desc->driver.name,   DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,   driver_info.name );
     snprintf( ret_desc->driver.vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH, driver_info.vendor );

     return DFB_OK;
}

static DFBResult
IDirectFB_EnumVideoModes( IDirectFB            *thiz,
                          DFBVideoModeCallback  callbackfunc,
                          void                 *callbackdata )
{
     VideoMode *m;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %p, %p )\n", __FUNCTION__, thiz, callbackfunc, callbackdata );

     if (!callbackfunc)
          return DFB_INVARG;

     m = dfb_system_modes();
     while (m) {
          if (callbackfunc( m->xres, m->yres,
                            m->bpp, callbackdata ) == DFENUM_CANCEL)
               break;

          m = m->next;
     }

     return DFB_OK;
}

static DFBResult
IDirectFB_SetVideoMode( IDirectFB    *thiz,
                        int           width,
                        int           height,
                        int           bpp )
{
     DFBResult ret;
     DFBSurfacePixelFormat format;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %dx%d %dbit )\n", __FUNCTION__, thiz, width, height, bpp );

     if (width < 1 || height < 1 || bpp < 1)
          return DFB_INVARG;

     format = dfb_pixelformat_for_depth( bpp );
     if (format == DSPF_UNKNOWN)
          return DFB_INVARG;

     switch (data->level) {
          case DFSCL_NORMAL:
               if (data->primary.window) {
                    ret = dfb_window_resize( data->primary.window, width, height );
                    if (ret)
                         return ret;
               }
               break;

          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE: {
               DFBResult ret;
               DFBDisplayLayerConfig config;

               config.flags       = DLCONF_WIDTH | DLCONF_HEIGHT |
                                    DLCONF_PIXELFORMAT;
               config.width       = width;
               config.height      = height;
               config.pixelformat = format;

               ret = dfb_layer_context_set_configuration( data->primary.context,
                                                          &config );
               if (ret)
                    return ret;

               break;
          }
     }

     data->primary.width  = width;
     data->primary.height = height;
     data->primary.format = format;

     return DFB_OK;
}

static void
init_palette( CoreSurface *surface, const DFBSurfaceDescription *desc )
{
     int          num;
     CorePalette *palette = surface->palette;

     if (!palette || !(desc->flags & DSDESC_PALETTE))
          return;

     num = MIN( desc->palette.size, palette->num_entries );

     direct_memcpy( palette->entries, desc->palette.entries, num * sizeof(DFBColor));

     dfb_palette_update( palette, 0, num - 1 );
}

static DFBResult
IDirectFB_CreateSurface( IDirectFB                    *thiz,
                         const DFBSurfaceDescription  *desc,
                         IDirectFBSurface            **interface )
{
     IDirectFBSurface *iface;
     DFBResult ret;
     int width = 256;
     int height = 256;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps = DSCAPS_NONE;
     DFBDisplayLayerConfig  config;
     CoreSurface *surface = NULL;
     unsigned long resource_id = 0;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->primary.context)
          dfb_layer_context_get_configuration( data->primary.context, &config );
     else
          dfb_layer_context_get_configuration( data->context, &config );

     format = config.pixelformat;

     if (!desc || !interface)
          return DFB_INVARG;

     if (desc->flags & DSDESC_WIDTH) {
          width = desc->width;
          if (width < 1)
               return DFB_INVARG;
     }

     if (desc->flags & DSDESC_HEIGHT) {
          height = desc->height;
          if (height < 1)
               return DFB_INVARG;
     }

     if (desc->flags & DSDESC_PALETTE)
          if (!desc->palette.entries || !desc->palette.size)
               return DFB_INVARG;

     if (desc->flags & DSDESC_CAPS)
          caps = desc->caps;

     if (desc->flags & DSDESC_PIXELFORMAT)
          format = desc->pixelformat;

     if (desc->flags & DSDESC_RESOURCE_ID)
          resource_id = desc->resource_id;

     switch (format) {
          case DSPF_A1:
          case DSPF_A4:
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_ARGB1555:
          case DSPF_ARGB1666:
          case DSPF_ARGB6666:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AYUV:
          case DSPF_AiRGB:
          case DSPF_I420:
          case DSPF_LUT2:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB16:
          case DSPF_RGB18:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_RGB332:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_RGB444:
          case DSPF_RGB555:
               break;

          default:
               return DFB_INVARG;
     }

     if (caps & DSCAPS_PRIMARY) {
          if (desc->flags & DSDESC_PREALLOCATED)
               return DFB_INVARG;

          if (desc->flags & DSDESC_PIXELFORMAT)
               format = desc->pixelformat;
          else if (data->primary.format)
               format = data->primary.format;
          else if (dfb_config->mode.format)
               format = dfb_config->mode.format;
          else
               format = config.pixelformat;

          if (desc->flags & DSDESC_WIDTH)
               width = desc->width;
          else if (data->primary.width)
               width = data->primary.width;
          else if (dfb_config->mode.width)
               width = dfb_config->mode.width;
          else
               width = config.width;

          if (desc->flags & DSDESC_HEIGHT)
               height = desc->height;
          else if (data->primary.height)
               height = data->primary.height;
          else if (dfb_config->mode.height)
               height = dfb_config->mode.height;
          else
               height = config.height;

          /* FIXME: should we allow to create more primaries in windowed mode?
                    should the primary surface be a singleton?
                    or should we return an error? */
          switch (data->level) {
               case DFSCL_NORMAL:
                    if (dfb_config->force_desktop) {
                         CoreSurface *surface;

                         /* Source compatibility with older programs */
                         if ((caps & DSCAPS_FLIPPING) == DSCAPS_FLIPPING)
                              caps &= ~DSCAPS_TRIPLE;

                         ret = dfb_surface_create_simple( data->core,
                                                          width, height,
                                                          format, caps,
                                                          CSTF_SHARED, resource_id,
                                                          NULL, &surface );
                         if (ret)
                              return ret;

                         surface->notifications |= CSNF_FLIP;

                         init_palette( surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                         ret = IDirectFBSurface_Construct( iface, NULL,
                                                           NULL, NULL, NULL, surface, caps, data->core );
                         if (ret == DFB_OK) {
                              dfb_windowstack_set_background_image( data->stack, surface );
                              dfb_windowstack_set_background_mode( data->stack, DLBM_IMAGE );
                         }

                         dfb_surface_unref( surface );

                         if (!ret)
                              *interface = iface;

                         return ret;
                    }
                    else {
                         CoreWindow           *window;
                         DFBWindowDescription  wd;

                         if ((caps & DSCAPS_FLIPPING) == DSCAPS_TRIPLE)
                              return DFB_UNSUPPORTED;

                         memset( &wd, 0, sizeof(wd) );

                         wd.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                                    DWDESC_PIXELFORMAT | DWDESC_SURFACE_CAPS | DWDESC_CAPS;

                         if (dfb_config->scaled.width && dfb_config->scaled.height) {
                              wd.posx = (config.width  - dfb_config->scaled.width)  / 2;
                              wd.posy = (config.height - dfb_config->scaled.height) / 2;
                         }
                         else {
                              wd.posx = (config.width  - width)  / 2;
                              wd.posy = (config.height - height) / 2;
                         }

                         wd.width        = width;
                         wd.height       = height;
                         wd.pixelformat  = format;
                         wd.surface_caps = caps & ~DSCAPS_FLIPPING;

                         switch (format) {
                              case DSPF_ARGB4444:
                              case DSPF_ARGB2554:
                              case DSPF_ARGB1555:
                              case DSPF_ARGB:
                              case DSPF_AYUV:
                              case DSPF_AiRGB:
                                   wd.caps |= DWCAPS_ALPHACHANNEL;
                                   break;

                              default:
                                   break;
                         }

                         if ((caps & DSCAPS_FLIPPING) == DSCAPS_DOUBLE)
                              wd.caps |= DWCAPS_DOUBLEBUFFER;

                         ret = dfb_layer_context_create_window( data->core, data->context, &wd, &window );
                         if (ret)
                              return ret;

                         drop_window( data );

                         data->primary.window = window;

                         dfb_window_attach( window, focus_listener,
                                            data, &data->primary.reaction );

                         dfb_window_change_options( window, DWOP_NONE, DWOP_SCALE );
                         if (dfb_config->scaled.width && dfb_config->scaled.height)
                              dfb_window_resize( window, dfb_config->scaled.width,
                                                         dfb_config->scaled.height );

                         init_palette( window->surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                         ret = IDirectFBSurface_Window_Construct( iface, NULL,
                                                                  NULL, NULL, window,
                                                                  caps, data->core );

                         if (!ret)
                              *interface = iface;

                         return ret;
                    }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE: {
                    CoreLayerRegion  *region;
                    CoreLayerContext *context = data->primary.context;

                    config.flags |= DLCONF_BUFFERMODE | DLCONF_PIXELFORMAT |
                                    DLCONF_WIDTH | DLCONF_HEIGHT;

                    /* Source compatibility with older programs */
                    if ((caps & DSCAPS_FLIPPING) == DSCAPS_FLIPPING)
                         caps &= ~DSCAPS_TRIPLE;

                    if (caps & DSCAPS_PREMULTIPLIED) {
                          config.flags        |= DLCONF_SURFACE_CAPS;
                          config.surface_caps  = DSCAPS_PREMULTIPLIED;
                    }

                    if (caps & DSCAPS_TRIPLE) {
                         if (caps & DSCAPS_SYSTEMONLY)
                              return DFB_UNSUPPORTED;
                         config.buffermode = DLBM_TRIPLE;
                    } else if (caps & DSCAPS_DOUBLE) {
                         if (caps & DSCAPS_SYSTEMONLY)
                              config.buffermode = DLBM_BACKSYSTEM;
                         else
                              config.buffermode = DLBM_BACKVIDEO;
                    }
                    else
                         config.buffermode = DLBM_FRONTONLY;

                    config.pixelformat = format;
                    config.width       = width;
                    config.height      = height;

                    ret = dfb_layer_context_set_configuration( context, &config );
                    if (ret) {
                         if (!(caps & (DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY)) &&
                             config.buffermode == DLBM_BACKVIDEO) {
                              config.buffermode = DLBM_BACKSYSTEM;

                              ret = dfb_layer_context_set_configuration( context, &config );
                              if (ret)
                                   return ret;
                         }
                         else
                              return ret;
                    }

                    ret = dfb_layer_context_get_primary_region( context, true,
                                                                &region );
                    if (ret)
                         return ret;

                    ret = dfb_layer_region_get_surface( region, &surface );
                    if (ret) {
                         dfb_layer_region_unref( region );
                         return ret;
                    }

/* FIXME_SC_3                    if ((caps & DSCAPS_DEPTH) && !(surface->config.caps & DSCAPS_DEPTH)) {
                         ret = dfb_surface_allocate_depth( surface );
                         if (ret) {
                              dfb_surface_unref( surface );
                              dfb_layer_region_unref( region );
                              return ret;
                         }
                    }
                    else if (!(caps & DSCAPS_DEPTH) && (surface->config.caps & DSCAPS_DEPTH)) {
                         dfb_surface_deallocate_depth( surface );
                    }
*/

                    init_palette( surface, desc );

                    DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                    ret = IDirectFBSurface_Layer_Construct( iface, NULL,
                                                            NULL, NULL, region, caps, data->core );

                    dfb_surface_unref( surface );
                    dfb_layer_region_unref( region );

                    if (!ret)
                         *interface = iface;

                    return ret;
               }
          }
     }

     /* Source compatibility with older programs */
     if ((caps & DSCAPS_FLIPPING) == DSCAPS_FLIPPING)
          caps &= ~DSCAPS_TRIPLE;

     if (caps & DSCAPS_TRIPLE)
          return DFB_UNSUPPORTED;

     if (desc->flags & DSDESC_PREALLOCATED) {
          int min_pitch;
          CoreSurfaceConfig config;

          if (caps & DSCAPS_VIDEOONLY)
               return DFB_INVARG;

          min_pitch = DFB_BYTES_PER_LINE(format, width);

          if (!desc->preallocated[0].data ||
               desc->preallocated[0].pitch < min_pitch)
          {
               return DFB_INVARG;
          }

          if ((caps & DSCAPS_DOUBLE) &&
              (!desc->preallocated[1].data ||
                desc->preallocated[1].pitch < min_pitch))
          {
               return DFB_INVARG;
          }

          config.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS | CSCONF_PREALLOCATED;
          config.size.w = width;
          config.size.h = height;
          config.format = format;
          config.caps   = caps;

          config.preallocated[0].addr  = desc->preallocated[0].data;
          config.preallocated[0].pitch = desc->preallocated[0].pitch;

          config.preallocated[1].addr  = desc->preallocated[1].data;
          config.preallocated[1].pitch = desc->preallocated[1].pitch;

          ret = dfb_surface_create( data->core, &config, CSTF_PREALLOCATED, resource_id, NULL, &surface );
          if (ret)
               return ret;
     }
     else {
          CoreSurfaceConfig config;

          config.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
          config.size.w = width;
          config.size.h = height;
          config.format = format;
          config.caps   = caps;

          ret = dfb_surface_create( data->core, &config, CSTF_NONE, resource_id, NULL, &surface );
          if (ret)
               return ret;
     }

     init_palette( surface, desc );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

     ret = IDirectFBSurface_Construct( iface, NULL,
                                       NULL, NULL, NULL, surface, caps, data->core );

     dfb_surface_unref( surface );

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_CreatePalette( IDirectFB                    *thiz,
                         const DFBPaletteDescription  *desc,
                         IDirectFBPalette            **interface )
{
     DFBResult         ret;
     IDirectFBPalette *iface;
     unsigned int      size    = 256;
     CorePalette      *palette = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!interface)
          return DFB_INVARG;

     if (desc && desc->flags & DPDESC_SIZE) {
          if (!desc->size)
               return DFB_INVARG;

          size = desc->size;
     }

     ret = dfb_palette_create( data->core, size, &palette );
     if (ret)
          return ret;

     if (desc && desc->flags & DPDESC_ENTRIES) {
          direct_memcpy( palette->entries, desc->entries, size * sizeof(DFBColor));

          dfb_palette_update( palette, 0, size - 1 );
     }
     else
          dfb_palette_generate_rgb332_map( palette );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( iface, palette );

     dfb_palette_unref( palette );

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_EnumScreens( IDirectFB         *thiz,
                       DFBScreenCallback  callbackfunc,
                       void              *callbackdata )
{
     EnumScreens_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!callbackfunc)
          return DFB_INVARG;

     context.callback     = callbackfunc;
     context.callback_ctx = callbackdata;

     dfb_screens_enumerate( EnumScreens_Callback, &context );

     return DFB_OK;
}

static DFBResult
IDirectFB_GetScreen( IDirectFB        *thiz,
                     DFBScreenID       id,
                     IDirectFBScreen **interface )
{
     IDirectFBScreen   *iface;
     GetScreen_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %d )\n", __FUNCTION__, thiz, id );

     if (!interface)
          return DFB_INVARG;

     if (dfb_config->primary_only && id != DLID_PRIMARY)
          return DFB_IDNOTFOUND;

     context.interface = &iface;
     context.id        = id;
     context.ret       = DFB_IDNOTFOUND;

     dfb_screens_enumerate( GetScreen_Callback, &context );

     if (!context.ret)
          *interface = iface;

     return context.ret;
}

static DFBResult
IDirectFB_EnumDisplayLayers( IDirectFB               *thiz,
                             DFBDisplayLayerCallback  callbackfunc,
                             void                    *callbackdata )
{
     EnumDisplayLayers_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!callbackfunc)
          return DFB_INVARG;

     context.callback     = callbackfunc;
     context.callback_ctx = callbackdata;

     dfb_layers_enumerate( EnumDisplayLayers_Callback, &context );

     return DFB_OK;
}

static DFBResult
IDirectFB_GetDisplayLayer( IDirectFB              *thiz,
                           DFBDisplayLayerID       id,
                           IDirectFBDisplayLayer **interface )
{
     IDirectFBDisplayLayer   *iface;
     GetDisplayLayer_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %d )\n", __FUNCTION__, thiz, id );

     if (!interface)
          return DFB_INVARG;

     if (dfb_config->primary_only && id != DLID_PRIMARY)
          return DFB_IDNOTFOUND;

     context.interface = &iface;
     context.id        = id;
     context.ret       = DFB_IDNOTFOUND;
     context.core      = data->core;

     dfb_layers_enumerate( GetDisplayLayer_Callback, &context );

     if (!context.ret)
          *interface = iface;

     return context.ret;
}

static DFBResult
IDirectFB_EnumInputDevices( IDirectFB              *thiz,
                            DFBInputDeviceCallback  callbackfunc,
                            void                   *callbackdata )
{
     EnumInputDevices_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!callbackfunc)
          return DFB_INVARG;

     context.callback     = callbackfunc;
     context.callback_ctx = callbackdata;

     dfb_input_enumerate_devices( EnumInputDevices_Callback, &context, DICAPS_ALL );

     return DFB_OK;
}

static DFBResult
IDirectFB_GetInputDevice( IDirectFB             *thiz,
                          DFBInputDeviceID       id,
                          IDirectFBInputDevice **interface )
{
     IDirectFBInputDevice   *iface;
     GetInputDevice_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %d )\n", __FUNCTION__, thiz, id );

     if (!interface)
          return DFB_INVARG;

     context.interface = &iface;
     context.id        = id;
     context.ret       = DFB_IDNOTFOUND;

     dfb_input_enumerate_devices( GetInputDevice_Callback, &context, DICAPS_ALL );

     if (!context.ret)
          *interface = iface;

     return context.ret;
}

static DFBResult
IDirectFB_CreateEventBuffer( IDirectFB             *thiz,
                             IDirectFBEventBuffer **interface)
{
     DFBResult             ret;
     IDirectFBEventBuffer *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBEventBuffer );

     ret = IDirectFBEventBuffer_Construct( iface, NULL, NULL );

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_CreateInputEventBuffer( IDirectFB                   *thiz,
                                  DFBInputDeviceCapabilities   caps,
                                  DFBBoolean                   global,
                                  IDirectFBEventBuffer       **interface)
{
     DFBResult                  ret;
     IDirectFBEventBuffer      *iface;
     CreateEventBuffer_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBEventBuffer );

     ret = IDirectFBEventBuffer_Construct( iface, global ? input_filter_global :
                                           input_filter_local, data );
     if (ret)
          return ret;

     context.caps      = caps;
     context.interface = &iface;

     dfb_input_enumerate_devices( CreateEventBuffer_Callback, &context, caps );

     *interface = iface;

     return DFB_OK;
}

static DFBResult
IDirectFB_CreateImageProvider( IDirectFB               *thiz,
                               const char              *filename,
                               IDirectFBImageProvider **interface )
{
     DFBResult                 ret;
     DFBDataBufferDescription  desc;
     IDirectFBDataBuffer      *databuffer;
     IDirectFBImageProvider   *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, '%s' )\n", __FUNCTION__, thiz, filename );

     /* Check arguments */
     if (!filename || !interface)
          return DFB_INVARG;

     /* Create a data buffer. */
     desc.flags = DBDESC_FILE;
     desc.file  = filename;

     ret = thiz->CreateDataBuffer( thiz, &desc, &databuffer );
     if (ret)
          return ret;

     /* Create (probing) the image provider. */
     ret = IDirectFBImageProvider_CreateFromBuffer( databuffer, data->core, &iface );

     /* We don't need it anymore, image provider has its own reference. */
     databuffer->Release( databuffer );

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_CreateVideoProvider( IDirectFB               *thiz,
                               const char              *filename,
                               IDirectFBVideoProvider **interface )
{
     DFBResult                 ret;
     DFBDataBufferDescription  desc;
     IDirectFBDataBuffer      *databuffer;
     IDirectFBVideoProvider   *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, '%s' )\n", __FUNCTION__, thiz, filename );

     /* Check arguments */
     if (!interface || !filename)
          return DFB_INVARG;

     /* Create a data buffer. */
     desc.flags = DBDESC_FILE;
     desc.file  = filename;

     ret = thiz->CreateDataBuffer( thiz, &desc, &databuffer );
     if (ret)
          return ret;

     /* Create (probing) the video provider. */
     ret = IDirectFBVideoProvider_CreateFromBuffer( databuffer, data->core, &iface );

     /* We don't need it anymore, video provider has its own reference. */
     databuffer->Release( databuffer );

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_CreateFont( IDirectFB                 *thiz,
                      const char                *filename,
                      const DFBFontDescription  *desc,
                      IDirectFBFont            **interface )
{
     DFBResult                   ret;
     DirectInterfaceFuncs       *funcs = NULL;
     IDirectFBFont              *font;
     IDirectFBFont_ProbeContext  ctx;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, '%s' )\n", __FUNCTION__, thiz, filename );

     /* Check arguments */
     if (!interface)
          return DFB_INVARG;

     if (desc) {
          if ((desc->flags & DFDESC_HEIGHT) && desc->height < 1)
               return DFB_INVARG;

          if ((desc->flags & DFDESC_WIDTH) && desc->width < 1)
               return DFB_INVARG;
     }

     if (filename) {
          if (!desc)
               return DFB_INVARG;

          if (access( filename, R_OK ) != 0)
               return errno2result( errno );
     }

     /* Fill out probe context */
     ctx.filename = filename;

     /* Find a suitable implemenation */
     ret = DirectGetInterface( &funcs, "IDirectFBFont", NULL, DirectProbeInterface, &ctx );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( font, IDirectFBFont );

     /* Construct the interface */
     ret = funcs->Construct( font, data->core, filename, desc );
     if (ret)
          return ret;

     *interface = font;

     return DFB_OK;
}

static DFBResult
IDirectFB_CreateDataBuffer( IDirectFB                       *thiz,
                            const DFBDataBufferDescription  *desc,
                            IDirectFBDataBuffer            **interface )
{
     DFBResult            ret = DFB_INVARG;
     IDirectFBDataBuffer *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!interface)
          return DFB_INVARG;

     if (!desc) {
          DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_Streamed_Construct( iface, data->core );
     }
     else if (desc->flags & DBDESC_FILE) {
          if (!desc->file)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_File_Construct( iface, desc->file, data->core );
     }
     else if (desc->flags & DBDESC_MEMORY) {
          if (!desc->memory.data || !desc->memory.length)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_Memory_Construct( iface,
                                                      desc->memory.data,
                                                      desc->memory.length,
                                                      data->core );
     }

     if (!ret)
          *interface = iface;

     return ret;
}

static DFBResult
IDirectFB_SetClipboardData( IDirectFB      *thiz,
                            const char     *mime_type,
                            const void     *clip_data,
                            unsigned int    size,
                            struct timeval *timestamp )
{
     DFBClipboardCore *clip_core;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!mime_type || !data || !size)
          return DFB_INVARG;

     clip_core = DFB_CORE( data->core, CLIPBOARD );
     if (!clip_core)
          return DFB_NOCORE;

     return dfb_clipboard_set( clip_core, mime_type, clip_data, size, timestamp );
}

static DFBResult
IDirectFB_GetClipboardData( IDirectFB     *thiz,
                            char         **mime_type,
                            void         **clip_data,
                            unsigned int  *size )
{
     DFBClipboardCore *clip_core;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!mime_type && !data && !size)
          return DFB_INVARG;

     clip_core = DFB_CORE( data->core, CLIPBOARD );
     if (!clip_core)
          return DFB_NOCORE;

     return dfb_clipboard_get( clip_core, mime_type, clip_data, size );
}

static DFBResult
IDirectFB_GetClipboardTimeStamp( IDirectFB      *thiz,
                                 struct timeval *timestamp )
{
     DFBClipboardCore *clip_core;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!timestamp)
          return DFB_INVARG;

     clip_core = DFB_CORE( data->core, CLIPBOARD );
     if (!clip_core)
          return DFB_NOCORE;

     return dfb_clipboard_get_timestamp( clip_core, timestamp );
}

static DFBResult
IDirectFB_Suspend( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     return dfb_core_suspend( data->core );
}

static DFBResult
IDirectFB_Resume( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     return dfb_core_resume( data->core );
}

static DFBResult
IDirectFB_WaitIdle( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_gfxcard_sync();

     return DFB_OK;
}

static DFBResult
IDirectFB_WaitForSync( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     dfb_layer_wait_vsync( data->layer );

     return DFB_OK;
}

static DFBResult
IDirectFB_GetInterface( IDirectFB   *thiz,
                        const char  *type,
                        const char  *implementation,
                        void        *arg,
                        void       **interface )
{
     DFBResult             ret;
     DirectInterfaceFuncs *funcs = NULL;
     void                 *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, '%s' )\n", __FUNCTION__, thiz, type );

     if (!type || !interface)
          return DFB_INVARG;

     if (!strncmp( type, "IDirectFB", 9 )) {
          D_ERROR( "IDirectFB::GetInterface() "
                   "is not allowed for \"IDirectFB*\"!\n" );
          return DFB_ACCESSDENIED;
     }

     ret = DirectGetInterface( &funcs, type, implementation, DirectProbeInterface, arg );
     if (ret)
          return ret;

     ret = funcs->Allocate( &iface );
     if (ret)
          return ret;

     ret = funcs->Construct( iface, arg, data->core );

     if (!ret)
          *interface = iface;

     return ret;
}

static void
LoadBackgroundImage( IDirectFB       *dfb,
                     CoreWindowStack *stack,
                     DFBConfigLayer  *conf )
{
     DFBResult               ret;
     DFBSurfaceDescription   desc;
     IDirectFBImageProvider *provider;
     IDirectFBSurface       *image;
     IDirectFBSurface_data  *image_data;

     ret = dfb->CreateImageProvider( dfb, conf->background.filename, &provider );
     if (ret) {
          D_DERROR( ret, "Failed loading background image '%s'!\n", conf->background.filename );
          return;
     }

     if (conf->background.mode == DLBM_IMAGE) {
          desc.flags  = DSDESC_WIDTH | DSDESC_HEIGHT;
          desc.width  = conf->config.width;
          desc.height = conf->config.height;
     }
     else {
          provider->GetSurfaceDescription( provider, &desc );
     }

     desc.flags |= DSDESC_CAPS | DSDESC_PIXELFORMAT;
     desc.caps = DSCAPS_SHARED;
     desc.pixelformat = conf->config.pixelformat;

     ret = dfb->CreateSurface( dfb, &desc, &image );
     if (ret) {
          DirectFBError( "Failed creating surface for background image", ret );
          provider->Release( provider );
          return;
     }

     ret = provider->RenderTo( provider, image, NULL );
     if (ret) {
          DirectFBError( "Failed loading background image", ret );
          image->Release( image );
          provider->Release( provider );
          return;
     }

     provider->Release( provider );

     image_data = (IDirectFBSurface_data*) image->priv;

     dfb_windowstack_set_background_image( stack, image_data->surface );

     image->Release( image );
}

static DFBResult
InitLayerPalette( IDirectFB_data    *data,
                  DFBConfigLayer    *conf,
                  CoreSurface       *surface,
                  CorePalette      **ret_palette )
{
     DFBResult    ret;
     CorePalette *palette;

     ret = dfb_palette_create( data->core, 256, &palette );
     if (ret) {
          D_DERROR( ret, "InitLayerPalette: Could not create palette!\n" );
          return ret;
     }

     direct_memcpy( palette->entries, conf->palette, sizeof(DFBColor) * 256 );

     ret = dfb_surface_set_palette( surface, palette );
     if (ret) {
          D_DERROR( ret, "InitLayerPalette: Could not set palette!\n" );
          dfb_palette_unref( palette );
          return ret;
     }

     *ret_palette = palette;

     return DFB_OK;
}

static DFBResult
InitLayers( IDirectFB      *dfb,
            IDirectFB_data *data )
{
     DFBResult ret;
     int       i;
     int       num = dfb_layer_num();

     for (i=0; i<num; i++) {
          CoreLayer      *layer = dfb_layer_at_translated( i );
          DFBConfigLayer *conf  = &dfb_config->layers[i];

          if (conf->init) {
               CoreLayerContext           *context;
               CoreWindowStack            *stack;
               CardCapabilities            caps;
               DFBDisplayLayerConfigFlags  fail;

               ret = dfb_layer_get_primary_context( layer, false, &context );
               if (ret) {
                    D_DERROR( ret, "InitLayers: Could not get context of layer %d!\n", i );
                    goto error;
               }

               stack = dfb_layer_context_windowstack( context );
               D_ASSERT( stack != NULL );


               /* set default desktop configuration */
               if (!(conf->config.flags & DLCONF_BUFFERMODE)) {
                    dfb_gfxcard_get_capabilities( &caps );

                    conf->config.flags     |= DLCONF_BUFFERMODE;
                    conf->config.buffermode = (caps.accel & DFXL_BLIT) ? DLBM_BACKVIDEO : DLBM_BACKSYSTEM;
               }

               if (dfb_layer_context_test_configuration( context, &conf->config, &fail )) {
                    if (fail & (DLCONF_WIDTH | DLCONF_HEIGHT)) {
                         D_ERROR( "DirectFB/DirectFBCreate: "
                                  "Setting desktop resolution to %dx%d failed!\n"
                                  "     -> Using default resolution.\n",
                                  conf->config.width, conf->config.height );

                         conf->config.flags &= ~(DLCONF_WIDTH | DLCONF_HEIGHT);
                    }

                    if (fail & DLCONF_PIXELFORMAT) {
                         D_ERROR( "DirectFB/DirectFBCreate: "
                                  "Setting desktop format failed!\n"
                                  "     -> Using default format.\n" );

                         conf->config.flags &= ~DLCONF_PIXELFORMAT;
                    }

                    if (fail & DLCONF_BUFFERMODE) {
                         D_ERROR( "DirectFB/DirectFBCreate: "
                                  "Setting desktop buffer mode failed!\n"
                                  "     -> No virtual resolution support or not enough memory?\n"
                                  "        Falling back to system back buffer.\n" );

                         conf->config.buffermode = DLBM_BACKSYSTEM;

                         if (dfb_layer_context_test_configuration( context, &conf->config, &fail )) {
                              D_ERROR( "DirectFB/DirectFBCreate: "
                                       "Setting system memory desktop back buffer failed!\n"
                                       "     -> Using front buffer only mode.\n" );

                              conf->config.flags &= ~DLCONF_BUFFERMODE;
                         }
                    }
               }

               if (conf->config.flags) {
                    ret = dfb_layer_context_set_configuration( context, &conf->config );
                    if (ret) {
                         D_DERROR( ret, "InitLayers: Could not set configuration for layer %d!\n", i );
                         dfb_layer_context_unref( context );
                         goto error;
                    }
               }

               ret = dfb_layer_context_get_configuration( context, &conf->config );
               D_ASSERT( ret == DFB_OK );

               ret = dfb_layer_context_get_primary_region( context, true, &data->layers[i].region );
               if (ret) {
                    D_DERROR( ret, "InitLayers: Could not get primary region of layer %d!\n", i );
                    dfb_layer_context_unref( context );
                    goto error;
               }

               ret = dfb_layer_region_get_surface( data->layers[i].region, &data->layers[i].surface );
               if (ret) {
                    D_DERROR( ret, "InitLayers: Could not get surface of primary region of layer %d!\n", i );
                    dfb_layer_region_unref( data->layers[i].region );
                    dfb_layer_context_unref( context );
                    goto error;
               }

               if (conf->palette_set)
                    InitLayerPalette( data, conf, data->layers[i].surface, &data->layers[i].palette );

               if (conf->src_key_index >= 0 && conf->src_key_index < D_ARRAY_SIZE(conf->palette)) {
                    dfb_layer_context_set_src_colorkey( context,
                                                        conf->palette[conf->src_key_index].r,
                                                        conf->palette[conf->src_key_index].g,
                                                        conf->palette[conf->src_key_index].b,
                                                        conf->src_key_index );
               }
               else
                    dfb_layer_context_set_src_colorkey( context, conf->src_key.r, conf->src_key.g, conf->src_key.b, -1 );

               switch (conf->background.mode) {
                    case DLBM_COLOR:
                         dfb_windowstack_set_background_color( stack, &conf->background.color );
                         dfb_windowstack_set_background_color_index( stack, conf->background.color_index );
                         break;

                    case DLBM_IMAGE:
                    case DLBM_TILE:
                         LoadBackgroundImage( dfb, stack, conf );
                         break;

                    default:
                         break;
               }

               dfb_windowstack_set_background_mode( stack, conf->background.mode );

               data->layers[i].context = context;
          }

          data->layers[i].layer = layer;
     }

     for (i=0; i<num; i++) {
          if (data->layers[i].context)
               dfb_layer_activate_context( data->layers[i].layer, data->layers[i].context );
     }

     return DFB_OK;

error:
     for (i=num-1; i>=0; i--) {
          if (data->layers[i].context) {
               if (data->layers[i].palette)
                    dfb_palette_unref( data->layers[i].palette );

               dfb_surface_unref( data->layers[i].surface );
               dfb_layer_region_unref( data->layers[i].region );
               dfb_layer_context_unref( data->layers[i].context );

               data->layers[i].context = NULL;
          }
     }

     return ret;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
DFBResult
IDirectFB_Construct( IDirectFB *thiz, CoreDFB *core )
{
     DFBResult ret;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %p )\n", __FUNCTION__, thiz, core );

     data->ref   = 1;
     data->core  = core;
     data->level = DFSCL_NORMAL;

     if (dfb_layer_num() < 1) {
          D_ERROR( "%s: No layers available! Missing driver?\n", __FUNCTION__ );
          return DFB_UNSUPPORTED;
     }

     data->layer = dfb_layer_at_translated( DLID_PRIMARY );

     ret = dfb_layer_get_primary_context( data->layer, true, &data->context );
     if (ret) {
          D_ERROR( "%s: Could not get default context of primary layer!\n", __FUNCTION__ );
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return ret;
     }

     data->stack = dfb_layer_context_windowstack( data->context );

     thiz->AddRef = IDirectFB_AddRef;
     thiz->Release = IDirectFB_Release;
     thiz->SetCooperativeLevel = IDirectFB_SetCooperativeLevel;
     thiz->GetDeviceDescription = IDirectFB_GetDeviceDescription;
     thiz->EnumVideoModes = IDirectFB_EnumVideoModes;
     thiz->SetVideoMode = IDirectFB_SetVideoMode;
     thiz->CreateSurface = IDirectFB_CreateSurface;
     thiz->CreatePalette = IDirectFB_CreatePalette;
     thiz->EnumScreens = IDirectFB_EnumScreens;
     thiz->GetScreen = IDirectFB_GetScreen;
     thiz->EnumDisplayLayers = IDirectFB_EnumDisplayLayers;
     thiz->GetDisplayLayer = IDirectFB_GetDisplayLayer;
     thiz->EnumInputDevices = IDirectFB_EnumInputDevices;
     thiz->GetInputDevice = IDirectFB_GetInputDevice;
     thiz->CreateEventBuffer = IDirectFB_CreateEventBuffer;
     thiz->CreateInputEventBuffer = IDirectFB_CreateInputEventBuffer;
     thiz->CreateImageProvider = IDirectFB_CreateImageProvider;
     thiz->CreateVideoProvider = IDirectFB_CreateVideoProvider;
     thiz->CreateFont = IDirectFB_CreateFont;
     thiz->CreateDataBuffer = IDirectFB_CreateDataBuffer;
     thiz->SetClipboardData = IDirectFB_SetClipboardData;
     thiz->GetClipboardData = IDirectFB_GetClipboardData;
     thiz->GetClipboardTimeStamp = IDirectFB_GetClipboardTimeStamp;
     thiz->Suspend = IDirectFB_Suspend;
     thiz->Resume = IDirectFB_Resume;
     thiz->WaitIdle = IDirectFB_WaitIdle;
     thiz->WaitForSync = IDirectFB_WaitForSync;
     thiz->GetInterface = IDirectFB_GetInterface;

     if (dfb_core_is_master( core )) {
          ret = InitLayers( thiz, data );
          if (ret) {
               dfb_layer_context_unref( data->context );
               DIRECT_DEALLOCATE_INTERFACE(thiz);
               return ret;
          }
     }

     return DFB_OK;
}

DFBResult
IDirectFB_SetAppFocus( IDirectFB *thiz, DFBBoolean focused )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %s )\n", __FUNCTION__, thiz, focused ? "true" : "false" );

     data->app_focus = focused;

     return DFB_OK;
}

/*
 * internal functions
 */

static DFBEnumerationResult
EnumScreens_Callback( CoreScreen *screen, void *ctx )
{
     DFBScreenDescription  desc;
     DFBScreenID           id;
     EnumScreens_Context  *context = (EnumScreens_Context*) ctx;

     id = dfb_screen_id_translated( screen );

     if (dfb_config->primary_only && id != DSCID_PRIMARY)
          return DFENUM_OK;

     dfb_screen_get_info( screen, NULL, &desc );

     return context->callback( id, desc, context->callback_ctx );
}

static DFBEnumerationResult
GetScreen_Callback( CoreScreen *screen, void *ctx )
{
     GetScreen_Context *context = (GetScreen_Context*) ctx;

     if (dfb_screen_id_translated( screen ) != context->id)
          return DFENUM_OK;

     DIRECT_ALLOCATE_INTERFACE( *context->interface, IDirectFBScreen );

     context->ret = IDirectFBScreen_Construct( *context->interface, screen );

     return DFENUM_CANCEL;
}

static DFBEnumerationResult
EnumDisplayLayers_Callback( CoreLayer *layer, void *ctx )
{
     DFBDisplayLayerDescription  desc;
     DFBDisplayLayerID           id;
     EnumDisplayLayers_Context  *context = (EnumDisplayLayers_Context*) ctx;

     id = dfb_layer_id_translated( layer );

     if (dfb_config->primary_only && id != DLID_PRIMARY)
          return DFENUM_OK;

     dfb_layer_get_description( layer, &desc );

     return context->callback( id, desc, context->callback_ctx );
}

static DFBEnumerationResult
GetDisplayLayer_Callback( CoreLayer *layer, void *ctx )
{
     GetDisplayLayer_Context *context = (GetDisplayLayer_Context*) ctx;

     if (dfb_layer_id_translated( layer ) != context->id)
          return DFENUM_OK;

     DIRECT_ALLOCATE_INTERFACE( *context->interface, IDirectFBDisplayLayer );

     context->ret = IDirectFBDisplayLayer_Construct( *context->interface,
                                                     layer, context->core );

     return DFENUM_CANCEL;
}

static DFBEnumerationResult
EnumInputDevices_Callback( CoreInputDevice *device, void *ctx )
{
     DFBInputDeviceDescription  desc;
     EnumInputDevices_Context  *context = (EnumInputDevices_Context*) ctx;

     dfb_input_device_description( device, &desc );

     return context->callback( dfb_input_device_id( device ), desc,
                               context->callback_ctx );
}

static DFBEnumerationResult
GetInputDevice_Callback( CoreInputDevice *device, void *ctx )
{
     GetInputDevice_Context *context = (GetInputDevice_Context*) ctx;

     if (dfb_input_device_id( device ) != context->id)
          return DFENUM_OK;

     DIRECT_ALLOCATE_INTERFACE( *context->interface, IDirectFBInputDevice );

     context->ret = IDirectFBInputDevice_Construct( *context->interface, device );

     return DFENUM_CANCEL;
}

static DFBEnumerationResult
CreateEventBuffer_Callback( CoreInputDevice *device, void *ctx )
{
     DFBInputDeviceDescription   desc;
     CreateEventBuffer_Context  *context = (CreateEventBuffer_Context*) ctx;

     dfb_input_device_description( device, &desc );

     if (! (desc.caps & context->caps))
          return DFENUM_OK;

     IDirectFBEventBuffer_AttachInputDevice( *context->interface, device );

     return DFENUM_OK;
}

static ReactionResult
focus_listener( const void *msg_data,
                void       *ctx )
{
     const DFBWindowEvent *evt  = msg_data;
     IDirectFB_data       *data = ctx;

     switch (evt->type) {
          case DWET_DESTROYED:
               dfb_window_unref( data->primary.window );
               data->primary.window = NULL;
               data->primary.focused = false;
               return RS_REMOVE;

          case DWET_GOTFOCUS:
               data->primary.focused = true;
               break;

          case DWET_LOSTFOCUS:
               data->primary.focused = false;
               break;

          default:
               break;
     }

     return RS_OK;
}

static bool
input_filter_local( DFBEvent *evt,
                    void     *ctx )
{
     IDirectFB_data *data = (IDirectFB_data*) ctx;

     if (evt->clazz == DFEC_INPUT) {
          DFBInputEvent *event = &evt->input;

          if (!data->primary.focused && !data->app_focus)
               return true;

          switch (event->type) {
               case DIET_BUTTONPRESS:
                    if (data->primary.window)
                         dfb_windowstack_cursor_enable( data->core, data->stack, false );
                    break;
               case DIET_KEYPRESS:
                    if (data->primary.window)
                         dfb_windowstack_cursor_enable( data->core, data->stack,
                                                        (event->key_symbol ==
                                                         DIKS_ESCAPE) ||
                                                        (event->modifiers &
                                                         DIMM_META) );
                    break;
               default:
                    break;
          }
     }

     return false;
}

static bool
input_filter_global( DFBEvent *evt,
                     void     *ctx )
{
     IDirectFB_data *data = (IDirectFB_data*) ctx;

     if (evt->clazz == DFEC_INPUT) {
          DFBInputEvent *event = &evt->input;

          if (!data->primary.focused && !data->app_focus)
               event->flags |= DIEF_GLOBAL;
     }

     return false;
}

static void
drop_window( IDirectFB_data *data )
{
     if (!data->primary.window)
          return;

     dfb_window_detach( data->primary.window, &data->primary.reaction );
     dfb_window_unref( data->primary.window );

     data->primary.window  = NULL;
     data->primary.focused = false;

     dfb_windowstack_cursor_enable( data->core, data->stack, true );
}

