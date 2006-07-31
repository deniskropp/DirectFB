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
#include <core/surfaces.h>
#include <core/surfacemanager.h>
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

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                         ref;      /* reference counter */
     CoreDFB                    *core;

     DFBCooperativeLevel         level;    /* current cooperative level */

     CoreLayer                  *layer;    /* primary display layer */
     CoreLayerContext           *context;  /* shared context of primary layer */
     CoreWindowStack            *stack;    /* window stack of primary layer */

     struct {
          int                    width;    /* IDirectFB stores window width    */
          int                    height;   /* and height and the pixel depth   */
          DFBSurfacePixelFormat  format;   /* from SetVideoMode() parameters.  */

          CoreWindow            *window;   /* implicitly created window */
          Reaction               reaction; /* for the focus listener */
          bool                   focused;  /* primary's window has the focus */

          CoreLayerContext      *context;  /* context for fullscreen primary */
     } primary;                            /* Used for DFSCL_NORMAL's primary. */

     bool                        app_focus;
} IDirectFB_data;

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
} GetDisplayLayer_Context;

typedef struct {
     DFBInputDeviceCallback  callback;
     void                   *callback_ctx;
} EnumInputDevices_Context;

typedef struct {
     IDirectFBInputDevice **interface;
     DFBInputDeviceID       id;
} GetInputDevice_Context;

typedef struct {
     IDirectFBEventBuffer       **interface;
     DFBInputDeviceCapabilities   caps;
} CreateEventBuffer_Context;


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

/*
 * Destructor
 *
 * Free data structure and set the pointer to NULL,
 * to indicate the dead interface.
 */
void
IDirectFB_Destruct( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

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

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFB_Release( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     DFBResult ret;
     int width = 256;
     int height = 256;
     int policy = CSP_VIDEOLOW;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps = DSCAPS_NONE;
     DFBDisplayLayerConfig  config;
     CoreSurface *surface = NULL;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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

     switch (format) {
          case DSPF_A1:
          case DSPF_A4:
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_ARGB1555:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_AYUV:
          case DSPF_AiRGB:
          case DSPF_I420:
          case DSPF_LUT8:
          case DSPF_ALUT44:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_RGB332:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
               break;

          default:
               return DFB_INVARG;
     }

     /* Source compatibility with older programs */
     if ((caps & DSCAPS_FLIPPING) == DSCAPS_FLIPPING)
          caps &= ~DSCAPS_TRIPLE;

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
          else if (data->primary.format)
               width = data->primary.width;
          else if (dfb_config->mode.width)
               width = dfb_config->mode.width;
          else
               width = config.width;

          if (desc->flags & DSDESC_HEIGHT)
               height = desc->height;
          else if (data->primary.format)
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

                         if (caps & DSCAPS_VIDEOONLY)
                              policy = CSP_VIDEOONLY;
                         else if (caps & DSCAPS_SYSTEMONLY)
                              policy = CSP_SYSTEMONLY;

                         ret = dfb_surface_create( data->core,
                                                   width, height,
                                                   format, policy, caps, NULL,
                                                   &surface );
                         if (ret)
                              return ret;

                         init_palette( surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                         ret = IDirectFBSurface_Construct( *interface, 
                                                            NULL, NULL, NULL, surface, caps );
                         if (ret == DFB_OK) {
                              dfb_windowstack_set_background_image( data->stack, surface );
                              dfb_windowstack_set_background_mode( data->stack, DLBM_IMAGE );
                         }

                         dfb_surface_unref( surface );

                         return ret;
                    }
                    else {
                         int                    x, y;
                         CoreWindow            *window;
                         DFBWindowCapabilities  window_caps = DWCAPS_NONE;

                         if (caps & DSCAPS_TRIPLE)
                              return DFB_UNSUPPORTED;

                         x = (config.width  - width)  / 2;
                         y = (config.height - height) / 2;

                         switch (format) {
                              case DSPF_ARGB:
                              case DSPF_AYUV:
                              case DSPF_AiRGB:
                                   window_caps |= DWCAPS_ALPHACHANNEL;
                                   break;

                              default:
                                   break;
                         }

                         if (caps & DSCAPS_DOUBLE)
                              window_caps |= DWCAPS_DOUBLEBUFFER;

                         ret = dfb_layer_context_create_window( data->context, x, y,
                                                                width, height, window_caps,
                                                                caps, format, &window );
                         if (ret)
                              return ret;

                         drop_window( data );

                         data->primary.window = window;

                         dfb_window_attach( window, focus_listener,
                                            data, &data->primary.reaction );

                         init_palette( window->surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                         return IDirectFBSurface_Window_Construct( *interface, NULL,
                                                                   NULL, window,
                                                                   caps );
                    }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE: {
                    CoreLayerRegion  *region;
                    CoreLayerContext *context = data->primary.context;

                    config.flags |= DLCONF_BUFFERMODE | DLCONF_PIXELFORMAT |
                                    DLCONF_WIDTH | DLCONF_HEIGHT;

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

                    if ((caps & DSCAPS_DEPTH) && !(surface->caps & DSCAPS_DEPTH)) {
                         ret = dfb_surface_allocate_depth( surface );
                         if (ret) {
                              dfb_surface_unref( surface );
                              dfb_layer_region_unref( region );
                              return ret;
                         }
                    }
                    else if (!(caps & DSCAPS_DEPTH) && (surface->caps & DSCAPS_DEPTH)) {
                         dfb_surface_deallocate_depth( surface );
                    }

                    init_palette( surface, desc );

                    DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    ret = IDirectFBSurface_Layer_Construct( *interface, NULL,
                                                            NULL, region, caps );

                    dfb_surface_unref( surface );
                    dfb_layer_region_unref( region );

                    return ret;
               }
          }
     }

     if (caps & DSCAPS_TRIPLE)
          return DFB_UNSUPPORTED;

     if (caps & DSCAPS_VIDEOONLY)
          policy = CSP_VIDEOONLY;
     else if (caps & DSCAPS_SYSTEMONLY)
          policy = CSP_SYSTEMONLY;

     if (desc->flags & DSDESC_PREALLOCATED) {
          int min_pitch;

          if (policy == CSP_VIDEOONLY)
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

          ret = dfb_surface_create_preallocated( data->core,
                                                 width, height,
                                                 format, policy, caps, NULL,
                                                 desc->preallocated[0].data,
                                                 desc->preallocated[1].data,
                                                 desc->preallocated[0].pitch,
                                                 desc->preallocated[1].pitch,
                                                 &surface );
          if (ret)
               return ret;
     }
     else {
          ret = dfb_surface_create( data->core,
                                    width, height, format,
                                    policy, caps, NULL, &surface );
          if (ret)
               return ret;
     }

     init_palette( surface, desc );

     DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

     ret = IDirectFBSurface_Construct( *interface, 
                                        NULL, NULL, NULL, surface, caps );

     dfb_surface_unref( surface );

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
     GetScreen_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     context.interface = interface;
     context.id        = id;
     context.ret       = DFB_IDNOTFOUND;

     dfb_screens_enumerate( GetScreen_Callback, &context );

     return context.ret;
}

static DFBResult
IDirectFB_EnumDisplayLayers( IDirectFB               *thiz,
                             DFBDisplayLayerCallback  callbackfunc,
                             void                    *callbackdata )
{
     EnumDisplayLayers_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     GetDisplayLayer_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     context.interface = interface;
     context.id        = id;
     context.ret       = DFB_IDNOTFOUND;

     dfb_layers_enumerate( GetDisplayLayer_Callback, &context );

     return context.ret;
}

static DFBResult
IDirectFB_EnumInputDevices( IDirectFB              *thiz,
                            DFBInputDeviceCallback  callbackfunc,
                            void                   *callbackdata )
{
     EnumInputDevices_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     GetInputDevice_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     context.interface = interface;
     context.id        = id;

     dfb_input_enumerate_devices( GetInputDevice_Callback, &context, DICAPS_ALL );

     return (*interface) ? DFB_OK : DFB_IDNOTFOUND;
}

static DFBResult
IDirectFB_CreateEventBuffer( IDirectFB             *thiz,
                             IDirectFBEventBuffer **interface)
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBEventBuffer );

     return IDirectFBEventBuffer_Construct( *interface, NULL, NULL );
}

static DFBResult
IDirectFB_CreateInputEventBuffer( IDirectFB                   *thiz,
                                  DFBInputDeviceCapabilities   caps,
                                  DFBBoolean                   global,
                                  IDirectFBEventBuffer       **interface)
{
     CreateEventBuffer_Context context;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBEventBuffer );

     IDirectFBEventBuffer_Construct( *interface, global ? input_filter_global :
                                     input_filter_local, data );

     context.caps      = caps;
     context.interface = interface;

     dfb_input_enumerate_devices( CreateEventBuffer_Callback, &context, caps );

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

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     ret = IDirectFBImageProvider_CreateFromBuffer( databuffer, interface );

     /* We don't need it anymore, image provider has its own reference. */
     databuffer->Release( databuffer );

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

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     ret = IDirectFBVideoProvider_CreateFromBuffer( databuffer, interface );

     /* We don't need it anymore, video provider has its own reference. */
     databuffer->Release( databuffer );

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
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     if (!desc) {
          DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_Streamed_Construct( *interface );
     }

     if (desc->flags & DBDESC_FILE) {
          if (!desc->file)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_File_Construct( *interface, desc->file );
     }

     if (desc->flags & DBDESC_MEMORY) {
          if (!desc->memory.data || !desc->memory.length)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_Memory_Construct( *interface,
                                                       desc->memory.data,
                                                       desc->memory.length );
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFB_SetClipboardData( IDirectFB      *thiz,
                            const char     *mime_type,
                            const void     *data,
                            unsigned int    size,
                            struct timeval *timestamp )
{
     if (!mime_type || !data || !size)
          return DFB_INVARG;

     return dfb_clipboard_set( mime_type, data, size, timestamp );
}

static DFBResult
IDirectFB_GetClipboardData( IDirectFB     *thiz,
                            char         **mime_type,
                            void         **data,
                            unsigned int  *size )
{
     if (!mime_type && !data && !size)
          return DFB_INVARG;

     return dfb_clipboard_get( mime_type, data, size );
}

static DFBResult
IDirectFB_GetClipboardTimeStamp( IDirectFB      *thiz,
                                 struct timeval *timestamp )
{
     if (!timestamp)
          return DFB_INVARG;

     return dfb_clipboard_get_timestamp( timestamp );
}

static DFBResult
IDirectFB_Suspend( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     return dfb_core_suspend( data->core );
}

static DFBResult
IDirectFB_Resume( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     return dfb_core_resume( data->core );
}

static DFBResult
IDirectFB_WaitIdle( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     dfb_gfxcard_sync();

     return DFB_OK;
}

static DFBResult
IDirectFB_WaitForSync( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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

     ret = funcs->Allocate( interface );
     if (ret)
          return ret;

     ret = funcs->Construct( *interface, arg );
     if (ret)
          *interface = NULL;

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

     data->ref   = 1;
     data->core  = core;
     data->level = DFSCL_NORMAL;

     data->layer = dfb_layer_at_translated( DLID_PRIMARY );

     ret = dfb_layer_get_primary_context( data->layer, true, &data->context );
     if (ret) {
          D_ERROR( "IDirectFB_Construct: "
                    "Could not get default context of primary layer!\n" );
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

     return DFB_OK;
}

DFBResult
IDirectFB_SetAppFocus( IDirectFB *thiz, DFBBoolean focused )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

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
     EnumScreens_Context  *context = (EnumScreens_Context*) ctx;

     dfb_screen_get_info( screen, NULL, &desc );

     return context->callback( dfb_screen_id_translated( screen ),
                               desc, context->callback_ctx );
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
     EnumDisplayLayers_Context  *context = (EnumDisplayLayers_Context*) ctx;

     dfb_layer_get_description( layer, &desc );

     return context->callback( dfb_layer_id_translated( layer ), desc,
                               context->callback_ctx );
}

static DFBEnumerationResult
GetDisplayLayer_Callback( CoreLayer *layer, void *ctx )
{
     GetDisplayLayer_Context *context = (GetDisplayLayer_Context*) ctx;

     if (dfb_layer_id_translated( layer ) != context->id)
          return DFENUM_OK;

     DIRECT_ALLOCATE_INTERFACE( *context->interface, IDirectFBDisplayLayer );

     context->ret = IDirectFBDisplayLayer_Construct( *context->interface,
                                                     layer );

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

     IDirectFBInputDevice_Construct( *context->interface, device );

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
                         dfb_windowstack_cursor_enable( data->stack, false );
                    break;
               case DIET_KEYPRESS:
                    if (data->primary.window)
                         dfb_windowstack_cursor_enable( data->stack,
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

     dfb_windowstack_cursor_enable( data->stack, true );
}

