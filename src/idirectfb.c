/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <core/CoreDFB.h>
#include <core/CoreLayer.h>
#include <core/CoreLayerContext.h>
#include <core/CoreLayerRegion.h>
#include <core/CorePalette.h>
#include <core/CoreScreen.h>
#include <core/CoreSurface.h>
#include <core/CoreWindow.h>
#include <core/CoreWindowStack.h>

#include <core/clipboard.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/layer_context.h>
#include <core/layer_control.h>
#include <core/layer_region.h>
#include <core/layers.h>
#include <core/layers_internal.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/surface.h>
#include <core/surface_pool.h>
#include <core/system.h>
#include <core/windows.h>
#include <core/windows_internal.h> /* FIXME */
#include <core/windowstack.h>
#include <core/wm.h>

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

#include <direct/direct.h>
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
     IDirectFB              *idirectfb;
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

static void drop_window( IDirectFB_data *data, bool enable_cursor );

/**********************************************************************************************************************/

/* The containers_XXXX function family will maintain the connections between
 * event buffers created by IDirectFB::CreateInputEventBuffer and input
 * devices that are hot-plugged in.
 */

static  DirectLink      *containers      = NULL;
static  pthread_mutex_t  containers_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct {
     DirectLink                  link;
     DFBInputDeviceCapabilities  caps;
     IDirectFBEventBuffer       *iface;

} Event_Buffer_Container;

void
containers_attach_device(CoreInputDevice *device)
{
     Event_Buffer_Container  *container;
     DFBInputDeviceCapabilities dev_caps;

     D_ASSERT( device != NULL );

     dev_caps = dfb_input_device_caps(device);

     pthread_mutex_lock( &containers_lock );

     direct_list_foreach(container, containers) {
          if (dev_caps & container->caps) {
               IDirectFBEventBuffer_AttachInputDevice( container->iface, device );
          }
     }

     pthread_mutex_unlock( &containers_lock );
}

void
containers_detach_device(CoreInputDevice *device)
{
     Event_Buffer_Container  *container;

     pthread_mutex_lock( &containers_lock );

     direct_list_foreach(container, containers) {
          IDirectFBEventBuffer_DetachInputDevice(container->iface, device);
     }

     pthread_mutex_unlock( &containers_lock );
}

static void
containers_add_input_eventbuffer(CreateEventBuffer_Context * context)
{
     Event_Buffer_Container  *container;

     pthread_mutex_lock( &containers_lock );

     container = D_CALLOC(1, sizeof(Event_Buffer_Container));
     if (!container) {
          D_OOM();
          D_ERROR( "Can not allocate memory! in %s\n", __FUNCTION__ );
     }
     container->caps = context->caps;
     container->iface = *context->interface;

     direct_list_append(&containers, &container->link);

     pthread_mutex_unlock( &containers_lock );
}

void
containers_remove_input_eventbuffer(IDirectFBEventBuffer  *thiz)
{
     Event_Buffer_Container  *container = NULL;

     pthread_mutex_lock( &containers_lock );

     direct_list_foreach(container, containers) {
          if (thiz == container->iface) {
               direct_list_remove(&containers, &container->link);
               D_FREE(container);
          }
     }

     pthread_mutex_unlock( &containers_lock );
}

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

     drop_window( data, false );

     if (data->primary.context)
          dfb_layer_context_unref( data->primary.context );

     dfb_layer_context_unref( data->context );

     for (i=0; i<MAX_LAYERS; i++) {
          if (data->layers[i].context) {
               if (data->layers[i].palette)
                    dfb_palette_unref( data->layers[i].palette );

               dfb_surface_unref( data->layers[i].surface );
               dfb_layer_region_unref( data->layers[i].region );
               dfb_layer_context_unref( data->layers[i].context );
          }
     }

     dfb_core_destroy( data->core, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     direct_shutdown();

     if (thiz == idirectfb_singleton)
          idirectfb_singleton = NULL;
}


static DirectResult
IDirectFB_AddRef( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
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
               if (dfb_config->primary_id)
                    return DFB_ACCESSDENIED;

               if (dfb_config->force_windowed || dfb_config->force_desktop)
                    return DFB_ACCESSDENIED;

               if (data->level == DFSCL_NORMAL) {
                    ret = CoreLayer_CreateContext( data->layer, &context );
                    if (ret)
                         return ret;

                    ret = CoreLayer_ActivateContext( data->layer, context );
                    if (ret) {
                         dfb_layer_context_unref( context );
                         return ret;
                    }

                    drop_window( data, true );

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

     direct_snputs( ret_desc->name,   device_info.name,   DFB_GRAPHICS_DEVICE_DESC_NAME_LENGTH );
     direct_snputs( ret_desc->vendor, device_info.vendor, DFB_GRAPHICS_DEVICE_DESC_NAME_LENGTH );

     ret_desc->driver.major = driver_info.version.major;
     ret_desc->driver.minor = driver_info.version.minor;

     direct_snputs( ret_desc->driver.name,   driver_info.name,   DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH );
     direct_snputs( ret_desc->driver.vendor, driver_info.vendor, DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH );

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
     DFBSurfaceColorSpace colorspace;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %dx%d %dbit )\n", __FUNCTION__, thiz, width, height, bpp );

     if (width < 1 || height < 1 || bpp < 1)
          return DFB_INVARG;

     format = dfb_pixelformat_for_depth( bpp );
     if (format == DSPF_UNKNOWN)
          return DFB_INVARG;
     colorspace = DFB_COLORSPACE_DEFAULT(format);

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
               config.colorspace  = colorspace;

               ret = CoreLayerContext_SetConfiguration( data->primary.context,
                                                          &config );
               if (ret)
                    return ret;

               break;
          }
     }

     data->primary.width      = width;
     data->primary.height     = height;
     data->primary.format     = format;
     data->primary.colorspace = colorspace;

     return DFB_OK;
}

static DFBResult
init_palette( CoreSurface *surface, const DFBSurfaceDescription *desc )
{
     DFBResult    ret;
     CorePalette *palette;

     if (!(desc->flags & DSDESC_PALETTE))
          return DFB_OK;

     ret = CoreSurface_GetPalette( surface, &palette );
     if (ret)
          return ret;

     ret = CorePalette_SetEntries( palette, desc->palette.entries, MIN( desc->palette.size, palette->num_entries ), 0 );

     dfb_palette_unref( palette );

     return ret;
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
     DFBSurfaceColorSpace colorspace;
     DFBSurfaceCapabilities caps = DSCAPS_NONE;
     CoreSurface *surface = NULL;
     unsigned long resource_id = 0;
     DFBDisplayLayerConfig  config;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (data->primary.context)
          dfb_layer_context_get_configuration( data->primary.context, &config );
     else if (data->context)
          dfb_layer_context_get_configuration( data->context, &config );
     else {
          config.width       = 512;
          config.height      = 512;
          config.pixelformat = DSPF_ARGB;
          config.colorspace  = DSCS_RGB;
     }

     if (desc->flags & DSDESC_HINTS && desc->hints & DSHF_FONT) {
          format = dfb_config->font_format;
          colorspace = DFB_COLORSPACE_DEFAULT(format);

          if (dfb_config->font_premult)
               caps = DSCAPS_PREMULTIPLIED;
     }
     else {
          format = config.pixelformat;
          colorspace = config.colorspace;
     }

     if (!desc || !interface)
          return DFB_INVARG;

     D_DEBUG_AT( IDFB, "  -> flags  0x%08x\n", desc->flags );

     if (desc->flags & DSDESC_WIDTH) {
          D_DEBUG_AT( IDFB, "  -> width  %d\n", desc->width );

          width = desc->width;
          if (width < 1 || width > 20480)
               return DFB_INVARG;
     }

     if (desc->flags & DSDESC_HEIGHT) {
          D_DEBUG_AT( IDFB, "  -> height %d\n", desc->height );

          height = desc->height;
          if (height < 1 || height > 20480)
               return DFB_INVARG;
     }

     if (desc->flags & DSDESC_PALETTE) {
          D_DEBUG_AT( IDFB, "  -> PALETTE\n" );

          if (!desc->palette.entries) {
               D_DEBUG_AT( IDFB, "  -> no entries!\n" );
               return DFB_INVARG;
          }

          if (!desc->palette.size) {
               D_DEBUG_AT( IDFB, "  -> no size!\n" );
               return DFB_INVARG;
          }
     }

     if (desc->flags & DSDESC_CAPS) {
          D_DEBUG_AT( IDFB, "  -> caps   0x%08x\n", desc->caps );

          caps = desc->caps;
     }

     if (desc->flags & DSDESC_PIXELFORMAT) {
          D_DEBUG_AT( IDFB, "  -> format %s\n", dfb_pixelformat_name(desc->pixelformat) );

          format = desc->pixelformat;
          colorspace = DFB_COLORSPACE_DEFAULT(format);
     }

     if (desc->flags & DSDESC_COLORSPACE) {
          D_DEBUG_AT( IDFB, "  -> colorspace %s\n", dfb_colorspace_name(desc->pixelformat) );

          if (!DFB_COLORSPACE_IS_COMPATIBLE(desc->colorspace, format)) {
               D_DEBUG_AT( IDFB, "  -> incompatible color space!\n" );
               return DFB_INVARG;
          }

          colorspace = desc->colorspace;
     }

     if (desc->flags & DSDESC_RESOURCE_ID)
          resource_id = desc->resource_id;

     switch (format) {
          case DSPF_A1:
          case DSPF_A1_LSB:
          case DSPF_A4:
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_ARGB8565:
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_ARGB1666:
          case DSPF_ARGB6666:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_RGBA4444:
          case DSPF_AYUV:
          case DSPF_AVYU:
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
          case DSPF_YV16:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
          case DSPF_VYU:
          case DSPF_RGB444:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGBAF88871:
          case DSPF_YUV444P:
               break;

          default:
               D_DEBUG_AT( IDFB, "  -> invalid pixelformat 0x%08x\n", format );
               return DFB_INVARG;
     }

     if (caps & DSCAPS_PRIMARY) {
          if (dfb_config->primary_id) {
               ret = CoreDFB_GetSurface( data->core, dfb_config->primary_id, &surface );
               if (ret)
                    return ret;

               DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

               ret = IDirectFBSurface_Construct( iface, NULL, NULL, NULL, NULL, surface, DSCAPS_PRIMARY, data->core, thiz );
               if (ret) {
                    dfb_surface_unref( surface );
                    return ret;
               }

               init_palette( surface, desc );

               dfb_surface_unref( surface );

               *interface = iface;

               return ret;
          }

          if (desc->flags & DSDESC_PREALLOCATED) {
               D_DEBUG_AT( IDFB, "  -> cannot make preallocated primary!\n" );
               return DFB_INVARG;
          }

          if (desc->flags & DSDESC_PIXELFORMAT)   // FIXME COLORSPACE
               format = desc->pixelformat;
          else if (data->primary.format) {
               format = data->primary.format;
               colorspace = data->primary.colorspace;
          }
          else if (dfb_config->mode.format) {
               format = dfb_config->mode.format;
               colorspace = DFB_COLORSPACE_DEFAULT(format);
          }
          else {
               format = config.pixelformat;
               colorspace = config.colorspace;
          }

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
                                                          format, colorspace, caps,
                                                          CSTF_SHARED, resource_id,
                                                          NULL, &surface );
                         if (ret)
                              return ret;

                         surface->notifications |= CSNF_FLIP;

                         init_palette( surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                         ret = IDirectFBSurface_Construct( iface, NULL,
                                                           NULL, NULL, NULL, surface, caps, data->core, thiz );
                         if (ret == DFB_OK) {
                              CoreWindowStack_BackgroundSetImage( data->stack, surface );
                              CoreWindowStack_BackgroundSetMode( data->stack, DLBM_IMAGE );
                         }

                         dfb_surface_unref( surface );

                         if (!ret)
                              *interface = iface;

                         return ret;
                    }
                    else {
                         CoreWindow           *window;
                         DFBWindowDescription  wd;

                         memset( &wd, 0, sizeof(wd) );

                         wd.flags = DWDESC_POSX | DWDESC_POSY | DWDESC_WIDTH | DWDESC_HEIGHT |
                                    DWDESC_PIXELFORMAT | DWDESC_COLORSPACE | DWDESC_SURFACE_CAPS | DWDESC_CAPS | DWDESC_RESOURCE_ID;

                         if (dfb_config->scaled.width && dfb_config->scaled.height) {
                              wd.posx = (config.width  - dfb_config->scaled.width)  / 2;
                              wd.posy = (config.height - dfb_config->scaled.height) / 2;
                         }
                         else {
                              wd.posx = (config.width  - width)  / 2;
                              wd.posy = (config.height - height) / 2;
                         }

                         if (!(caps & (DSCAPS_VIDEOONLY | DSCAPS_SYSTEMONLY))) {
                              if (dfb_config->window_policy == CSP_SYSTEMONLY)
                                   caps |= DSCAPS_SYSTEMONLY;
                              else if (dfb_config->window_policy == CSP_VIDEOONLY)
                                   caps |= DSCAPS_VIDEOONLY;
                         }

                         wd.width        = width;
                         wd.height       = height;
                         wd.pixelformat  = format;
                         wd.colorspace   = colorspace;
                         wd.surface_caps = caps;
                         wd.resource_id  = resource_id;

                         switch (format) {
                              case DSPF_ARGB8565:
                              case DSPF_ARGB4444:
                              case DSPF_RGBA4444:
                              case DSPF_ARGB2554:
                              case DSPF_ARGB1555:
                              case DSPF_RGBA5551:
                              case DSPF_ARGB:
                              case DSPF_ABGR:
                              case DSPF_AYUV:
                              case DSPF_AVYU:
                              case DSPF_AiRGB:
                              case DSPF_RGBAF88871:
                                   wd.caps |= DWCAPS_ALPHACHANNEL;
                                   break;

                              default:
                                   break;
                         }

                         if ((caps & DSCAPS_FLIPPING) == DSCAPS_DOUBLE)
                              wd.caps |= DWCAPS_DOUBLEBUFFER;

                         if (caps & DSCAPS_STEREO)
                              wd.caps |= DWCAPS_STEREO;

                         ret = CoreLayerContext_CreateWindow( data->context, &wd, &window );
                         if (ret)
                              return ret;

                         drop_window( data, true );

                         data->primary.window = window;

                         dfb_window_attach( window, focus_listener,
                                            data, &data->primary.reaction );

                         CoreWindow_ChangeOptions( window, DWOP_NONE, DWOP_SCALE );

                         CoreWindow_AllowFocus( window );

                         if (dfb_config->scaled.width && dfb_config->scaled.height)
                              CoreWindow_Resize( window, dfb_config->scaled.width,
                                                         dfb_config->scaled.height );

                         init_palette( window->surface, desc );

                         DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                         ret = IDirectFBSurface_Window_Construct( iface, NULL,
                                                                  NULL, NULL, window,
                                                                  caps, data->core, thiz );

                         if (!ret)
                              *interface = iface;

                         return ret;
                    }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE: {
                    CoreLayerRegion  *region;
                    CoreLayerContext *context = data->primary.context;

                    config.flags |= DLCONF_BUFFERMODE | DLCONF_PIXELFORMAT | DLCONF_COLORSPACE |
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

                    if (caps & DSCAPS_STEREO) {
                         config.flags   |= DLCONF_OPTIONS;
                         config.options  = DLOP_STEREO;
                    }

                    config.pixelformat = format;
                    config.colorspace  = colorspace;
                    config.width       = width;
                    config.height      = height;

                    ret = CoreLayerContext_SetConfiguration( context, &config );
                    if (ret) {
                         if (!(caps & (DSCAPS_SYSTEMONLY | DSCAPS_VIDEOONLY)) &&
                             config.buffermode == DLBM_BACKVIDEO) {
                              config.buffermode = DLBM_BACKSYSTEM;

                              ret = CoreLayerContext_SetConfiguration( context, &config );
                              if (ret)
                                   return ret;
                         }
                         else
                              return ret;
                    }

                    ret = CoreLayerContext_GetPrimaryRegion( context, true, &region );
                    if (ret)
                         return ret;

                    ret = CoreLayerRegion_GetSurface( region, &surface );
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

                    /* Make a single buffered primary display layer visible
                     * since the IDirectFB::GetDisplayLayer automatic flip
                     * defect fix prevents flipping when the region is frozen.
                     */
                    if (config.buffermode != DLBM_BACKVIDEO && 
                        config.buffermode != DLBM_TRIPLE) {
                         /* If a window stack is available, give it the
                          * opportunity to render the background (optionally
                          * based on configuration) and flip the display layer
                          * so it is visible.  Otherwise, just directly flip
                          * the display layer and make it visible.
                          */
                         D_ASSERT( region->context );
                         if (region->context->stack) {
                              CoreWindowStack_RepaintAll( region->context->stack );
                         }
                         else {
                              CoreLayerRegion_FlipUpdate( region, NULL, DSFLIP_NONE );
                         }
                    }

                    DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

                    ret = IDirectFBSurface_Layer_Construct( iface, NULL,
                                                            NULL, NULL, region, caps, data->core, thiz );

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

     if (desc->flags & DSDESC_PREALLOCATED) {
          int               min_pitch;
          CoreSurfaceConfig config;
          int               i, num = 1;

          min_pitch = DFB_BYTES_PER_LINE(format, width);

          if (caps & DSCAPS_DOUBLE)
               num = 2;
          else if (caps & DSCAPS_TRIPLE)
               num = 3;

          D_DEBUG_AT( IDFB, "  -> %d buffers, min pitch %d\n", num, min_pitch );

          for (i=0; i<num; i++) {
               if (!desc->preallocated[i].data) {
                    D_DEBUG_AT( IDFB, "  -> no data in preallocated [%d]\n", i );
                    return DFB_INVARG;
               }

               if (desc->preallocated[i].pitch < min_pitch) {
                    D_DEBUG_AT( IDFB, "  -> wrong pitch (%d) in preallocated [%d]\n", desc->preallocated[i].pitch, i );
                    return DFB_INVARG;
               }
          }

          config.flags      = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_COLORSPACE | CSCONF_CAPS | CSCONF_PREALLOCATED;
          config.size.w     = width;
          config.size.h     = height;
          config.format     = format;
          config.colorspace = colorspace;
          config.caps       = caps;

          ret = dfb_surface_pools_prealloc( desc, &config );
          if (ret) {
               D_DERROR( ret, "IDirectFB::CreateSurface: Preallocation failed!\n" );
               return ret;
          }

          ret = CoreDFB_CreateSurface( data->core, &config, CSTF_PREALLOCATED, resource_id, NULL, &surface );
          if (ret)
               return ret;
     }
     else {
          CoreSurfaceConfig config;

          config.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_COLORSPACE | CSCONF_CAPS;
          config.size.w       = width;
          config.size.h       = height;
          config.format       = format;
          config.colorspace   = colorspace;
          config.caps         = caps;

          ret = CoreDFB_CreateSurface( data->core, &config, CSTF_NONE, resource_id, NULL, &surface );
          if (ret)
               return ret;
     }

     init_palette( surface, desc );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

     ret = IDirectFBSurface_Construct( iface, NULL,
                                       NULL, NULL, NULL, surface, caps, data->core, thiz );

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

     ret = CoreDFB_CreatePalette( data->core, size, &palette );
     if (ret)
          return ret;

     if (desc && desc->flags & DPDESC_ENTRIES)
          CorePalette_SetEntries( palette, desc->entries, size, 0 );
     else
          dfb_palette_generate_rgb332_map( palette );

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBPalette );

     ret = IDirectFBPalette_Construct( iface, palette, data->core );

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
     context.idirectfb = thiz;

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

#ifndef DIRECTFB_DISABLE_DEPRECATED
     dfb_input_enumerate_devices( EnumInputDevices_Callback, &context, DICAPS_ALL );
#else
     dfb_input_enumerate_devices( EnumInputDevices_Callback, &context, DIDCAPS_ALL );
#endif

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

#ifndef DIRECTFB_DISABLE_DEPRECATED
     dfb_input_enumerate_devices( GetInputDevice_Callback, &context, DICAPS_ALL );
#else
     dfb_input_enumerate_devices( GetInputDevice_Callback, &context, DIDCAPS_ALL );
#endif

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

     /* Store the context of input event buffer for input device hotplug support. */
     containers_add_input_eventbuffer(&context);

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
     ret = IDirectFBImageProvider_CreateFromBuffer( databuffer, data->core, thiz, &iface );

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
     DFBDataBufferDescription    dbdesc;
     IDirectFBDataBuffer        *databuffer;
     IDirectFBFont              *font;

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

     /* Create a data buffer. */
     dbdesc.flags = DBDESC_FILE;
     dbdesc.file  = filename;

     ret = thiz->CreateDataBuffer( thiz, &dbdesc, &databuffer );
     if (ret)
          return ret;

     /* Create (probing) the font. */
     ret = IDirectFBFont_CreateFromBuffer( databuffer, data->core, desc, &font );

     /* We don't need it anymore, font has its own reference. */
     databuffer->Release( databuffer );

     if (!ret)
          *interface = font;

     return ret;
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

          ret = IDirectFBDataBuffer_Streamed_Construct( iface, data->core, thiz );
     }
     else if (desc->flags & DBDESC_FILE) {
          if (!desc->file)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_File_Construct( iface, desc->file, data->core, thiz );
     }
     else if (desc->flags & DBDESC_MEMORY) {
          if (!desc->memory.data || !desc->memory.length)
               return DFB_INVARG;

          DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBDataBuffer );

          ret = IDirectFBDataBuffer_Memory_Construct( iface,
                                                      desc->memory.data,
                                                      desc->memory.length,
                                                      data->core, thiz );
     }
     else
          return DFB_INVARG;

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
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!mime_type || !data || !size)
          return DFB_INVARG;

     return CoreDFB_ClipboardSet( data->core, mime_type, strlen(mime_type) + 1, clip_data,
                                  size, timestamp->tv_sec * 1000000 + timestamp->tv_usec );
}

static DFBResult
IDirectFB_GetClipboardData( IDirectFB     *thiz,
                            char         **mime_type,
                            void         **clip_data,
                            unsigned int  *size )
{
     DFBResult ret;
     char      tmp_mime_type[MAX_CLIPBOARD_MIME_TYPE_SIZE];
     u32       tmp_mime_type_size;
     char      tmp_data[MAX_CLIPBOARD_DATA_SIZE];
     u32       tmp_data_size;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!mime_type && !data && !size)
          return DFB_INVARG;

     ret = CoreDFB_ClipboardGet( data->core, tmp_mime_type, &tmp_mime_type_size, tmp_data, &tmp_data_size );
     if (ret)
          return ret;

     *mime_type = strdup( tmp_mime_type );
     if (!*mime_type)
          return D_OOM();

     *clip_data = malloc( tmp_data_size );
     if (!*clip_data) {
          free( *mime_type );
          return D_OOM();
     }

     direct_memcpy( *clip_data, tmp_data, tmp_data_size );

     *size = tmp_data_size;

     return DFB_OK;
}

static DFBResult
IDirectFB_GetClipboardTimeStamp( IDirectFB      *thiz,
                                 struct timeval *timestamp )
{
     DFBResult ret;
     u64       ts;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     if (!timestamp)
          return DFB_INVARG;

     ret = CoreDFB_ClipboardGetTimestamp( data->core, &ts );
     if (ret)
          return ret;

     timestamp->tv_sec  = ts / 1000000;
     timestamp->tv_usec = ts % 1000000;

     return ret;
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

     return CoreDFB_WaitIdle( data->core );
}

static DFBResult
IDirectFB_WaitForSync( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     CoreLayer_WaitVSync( data->layer );

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

static DFBResult
IDirectFB_GetSurface( IDirectFB         *thiz,
                      DFBSurfaceID       surface_id,
                      IDirectFBSurface **ret_interface )
{
     DFBResult         ret;
     CoreSurface      *surface;
     IDirectFBSurface *iface;

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p, %u )\n", __FUNCTION__, thiz, surface_id );

     ret = CoreDFB_GetSurface( data->core, surface_id, &surface );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( iface, IDirectFBSurface );

     ret = IDirectFBSurface_Construct( iface, NULL, NULL, NULL, NULL, surface, surface->config.caps, data->core, thiz );

     dfb_surface_unref( surface );

     if (!ret)
          *ret_interface = iface;

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

     CoreWindowStack_BackgroundSetImage( stack, image_data->surface );

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

DFBResult
IDirectFB_InitLayers( IDirectFB *thiz )
{
     DFBResult ret;
     int       i;
     int       num = dfb_layer_num();

     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     for (i=0; i<num; i++) {
          CoreLayer      *layer = dfb_layer_at_translated( i );
          DFBConfigLayer *conf  = &dfb_config->layers[i];

          if (conf->init) {
               CoreLayerContext           *context;
               CoreWindowStack            *stack;
               CardCapabilities            caps;
               DFBDisplayLayerConfigFlags  fail;
               DFBColorKey                 key;

               ret = CoreLayer_GetPrimaryContext( layer, false, &context );
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

               if (CoreLayerContext_TestConfiguration( context, &conf->config, &fail )) {
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

                         if (CoreLayerContext_TestConfiguration( context, &conf->config, &fail )) {
                              D_ERROR( "DirectFB/DirectFBCreate: "
                                       "Setting system memory desktop back buffer failed!\n"
                                       "     -> Using front buffer only mode.\n" );

                              conf->config.flags &= ~DLCONF_BUFFERMODE;
                         }
                    }
               }

               if (conf->config.flags) {
                    ret = CoreLayerContext_SetConfiguration( context, &conf->config );
                    if (ret) {
                         D_DERROR( ret, "InitLayers: Could not set configuration for layer %d!\n", i );
                         dfb_layer_context_unref( context );
                         goto error;
                    }
               }

               ret = dfb_layer_context_get_configuration( context, &conf->config );
               D_ASSERT( ret == DFB_OK );

               ret = CoreLayerContext_GetPrimaryRegion( context, true, &data->layers[i].region );
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
                    conf->src_key.r = conf->palette[conf->src_key_index].r;
                    conf->src_key.g = conf->palette[conf->src_key_index].g;
                    conf->src_key.b = conf->palette[conf->src_key_index].b;
               }

               key.r     = conf->src_key.r;
               key.g     = conf->src_key.g;
               key.b     = conf->src_key.b;
               key.index = conf->src_key_index;

               CoreLayerContext_SetSrcColorKey( context, &key );

               switch (conf->background.mode) {
                    case DLBM_COLOR:
                         CoreWindowStack_BackgroundSetColor( stack, &conf->background.color );
                         CoreWindowStack_BackgroundSetColorIndex( stack, conf->background.color_index );
                         break;

                    case DLBM_IMAGE:
                    case DLBM_TILE:
                         LoadBackgroundImage( thiz, stack, conf );
                         break;

                    default:
                         break;
               }

               CoreWindowStack_BackgroundSetMode( stack, conf->background.mode );

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



static void
InitIDirectFB_Async( void *ctx,
                     void *ctx2 )
{
     DFBResult       ret;
     IDirectFB      *thiz = ctx;
     IDirectFB_data *data = ctx2;

     D_DEBUG_AT( IDFB, "%s( %p, %p )\n", __FUNCTION__, thiz, data );

     ret = CoreLayer_GetPrimaryContext( data->layer, true, &data->context );
     if (ret) {
          D_ERROR( "%s: Could not get default context of primary layer!\n", __FUNCTION__ );
          return;
     }

     data->stack = dfb_layer_context_windowstack( data->context );

     if (dfb_core_is_master( data->core )) {
          ret = IDirectFB_InitLayers( thiz );
          if (ret)
               return;

          /* not fatal */
          ret = dfb_wm_post_init( data->core );
          if (ret)
               D_DERROR( ret, "DirectFBCreate: Post initialization of WM failed!\n" );

          dfb_core_activate( data->core );
     }

     direct_mutex_lock( &data->init_lock );

     data->init_done = true;

     direct_waitqueue_broadcast( &data->init_wq );

     direct_mutex_unlock( &data->init_lock );
}


/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
DFBResult
IDirectFB_Construct( IDirectFB *thiz, CoreDFB *core )
{
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
     thiz->GetSurface = IDirectFB_GetSurface;

     direct_mutex_init( &data->init_lock );
     direct_waitqueue_init( &data->init_wq );

     if (dfb_config->call_nodirect && dfb_core_is_master( core ))
          Core_AsyncCall( InitIDirectFB_Async, thiz, data );
     else
          InitIDirectFB_Async( thiz, data );

     return DFB_OK;
}

DFBResult
IDirectFB_WaitInitialised( IDirectFB *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFB)

     D_DEBUG_AT( IDFB, "%s( %p )\n", __FUNCTION__, thiz );

     direct_mutex_lock( &data->init_lock );

     while (!data->init_done)
          direct_waitqueue_wait( &data->init_wq, &data->init_lock );

     direct_mutex_unlock( &data->init_lock );

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

     context->ret = IDirectFBDisplayLayer_Construct( *context->interface, layer, context->core, context->idirectfb );

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

          if (dfb_config->cursor_automation) {
               switch (event->type) {
                    case DIET_BUTTONPRESS:
                         if (data->primary.window)
                              CoreWindowStack_CursorEnable( data->stack, false );
                         break;
                    case DIET_KEYPRESS:
                         if (data->primary.window)
                              CoreWindowStack_CursorEnable( data->stack,
                                                             (event->key_symbol ==
                                                              DIKS_ESCAPE) ||
                                                             (event->modifiers &
                                                              DIMM_META) );
                         break;
                    default:
                         break;
               }
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
drop_window( IDirectFB_data *data, bool enable_cursor )
{
     if (!data->primary.window)
          return;

     dfb_window_detach( data->primary.window, &data->primary.reaction );
     dfb_window_unref( data->primary.window );

     data->primary.window  = NULL;
     data->primary.focused = false;

     if (dfb_config->cursor_automation)
          CoreWindowStack_CursorEnable( data->stack, enable_cursor );
}

