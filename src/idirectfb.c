/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <malloc.h>
#include <string.h>

#include <directfb.h>
#include <directfb_version.h>
#include <directfb_internals.h>

#include <core/core.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/input.h>
#include <core/layers.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/windows.h>

#include <core/fbdev/fbdev.h>

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
#include <misc/mem.h>
#include <misc/util.h>

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                  ref;      /* reference counter */
     DFBCooperativeLevel  level;    /* current cooperative level */

     DisplayLayer        *layer;    /* primary display layer */

     struct {
          int             width;    /* IDirectFB stores window width    */
          int             height;   /* and height and the pixel depth   */
          int             bpp;      /* from SetVideoMode() parameters.  */
     } primary;                     /* Used for DFSCL_NORMAL's primary. */
} IDirectFB_data;

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


static DFBEnumerationResult EnumDisplayLayers_Callback( DisplayLayer *layer,
                                                        void         *ctx );
static DFBEnumerationResult GetDisplayLayer_Callback  ( DisplayLayer *layer,
                                                        void         *ctx );
static DFBEnumerationResult EnumInputDevices_Callback ( InputDevice  *device,
                                                        void         *ctx );
static DFBEnumerationResult GetInputDevice_Callback   ( InputDevice  *device,
                                                        void         *ctx );
static DFBEnumerationResult CreateEventBuffer_Callback( InputDevice  *device,
                                                        void         *ctx );

/*
 * Destructor
 *
 * Free data structure and set the pointer to NULL,
 * to indicate the dead interface.
 */
static void
IDirectFB_Destruct( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (data->level != DFSCL_NORMAL)
          dfb_layer_release( data->layer, true );

     dfb_core_unref();     /* TODO: where should we place this call? */

     idirectfb_singleton = NULL;

     DFB_DEALLOCATE_INTERFACE( thiz );
}


static DFBResult
IDirectFB_AddRef( IDirectFB *thiz )
{
     INTERFACE_GET_DATA(IDirectFB)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFB_Release( IDirectFB *thiz )
{
     INTERFACE_GET_DATA(IDirectFB)

     if (--data->ref == 0)
          IDirectFB_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFB_SetCooperativeLevel( IDirectFB           *thiz,
                               DFBCooperativeLevel  level )
{
     INTERFACE_GET_DATA(IDirectFB)

     if (level == data->level)
          return DFB_OK;

     switch (level) {
          case DFSCL_NORMAL:
               dfb_layer_release( data->layer, true );
               break;
          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE:
               if (dfb_config->force_windowed)
                    return DFB_ACCESSDENIED;

               if (data->level == DFSCL_NORMAL) {
                    DFBResult ret = dfb_layer_purchase( data->layer );
                    if (ret)
                         return ret;
               }
               break;
          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

static DFBResult
IDirectFB_GetCardCapabilities( IDirectFB           *thiz,
                               DFBCardCapabilities *caps )
{
     CardCapabilities card_caps;

     INTERFACE_GET_DATA(IDirectFB)

     if (!caps)
          return DFB_INVARG;

     card_caps = dfb_gfxcard_capabilities();

     caps->acceleration_mask = card_caps.accel;
     caps->blitting_flags    = card_caps.blitting;
     caps->drawing_flags     = card_caps.drawing;
     caps->video_memory      = dfb_gfxcard_memory_length();

     return DFB_OK;
}

static DFBResult
IDirectFB_EnumVideoModes( IDirectFB            *thiz,
                          DFBVideoModeCallback  callbackfunc,
                          void                 *callbackdata )
{
     VideoMode *m;

     INTERFACE_GET_DATA(IDirectFB)

     if (!callbackfunc)
          return DFB_INVARG;

     m = dfb_fbdev_modes();
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
                        unsigned int  width,
                        unsigned int  height,
                        unsigned int  bpp )
{
     INTERFACE_GET_DATA(IDirectFB)

     if (!width || !height || !bpp)
          return DFB_INVARG;

     switch (data->level) {
          case DFSCL_NORMAL:
               /* FIXME: resize window if already existent */
               break;

          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE: {
               DFBResult ret;
               DFBDisplayLayerConfig config;

               config.width       = width;
               config.height      = height;
               config.pixelformat = dfb_pixelformat_for_depth( bpp );

               if (config.pixelformat == DSPF_UNKNOWN)
                    return DFB_INVARG;

               config.flags = DLCONF_WIDTH | DLCONF_HEIGHT | DLCONF_PIXELFORMAT;

               ret = dfb_layer_set_configuration( data->layer, &config );
               if (ret)
                    return ret;

               break;
          }
     }

     data->primary.width  = width;
     data->primary.height = height;
     data->primary.bpp    = bpp;

     return DFB_OK;
}

static void
init_palette( CoreSurface *surface, DFBSurfaceDescription *desc )
{
     int          num;
     CorePalette *palette = surface->palette;

     if (!palette || !(desc->flags & DSDESC_PALETTE))
          return;

     num = MIN( desc->palette.size, palette->num_entries );

     dfb_memcpy( palette->entries,
                 desc->palette.entries, num * sizeof(DFBColor));

     dfb_palette_update( palette, 0, num - 1 );
}

static DFBResult
IDirectFB_CreateSurface( IDirectFB              *thiz,
                         DFBSurfaceDescription  *desc,
                         IDirectFBSurface      **interface )
{
     DFBResult ret;
     unsigned int width = 256;
     unsigned int height = 256;
     int policy = CSP_VIDEOLOW;
     DFBSurfacePixelFormat format;
     DFBSurfaceCapabilities caps = 0;
     DFBDisplayLayerConfig  config;
     CoreSurface *surface = NULL;

     INTERFACE_GET_DATA(IDirectFB)

     dfb_layer_get_configuration( data->layer, &config );

     format = config.pixelformat;

     if (!desc || !interface)
          return DFB_INVARG;

     if (desc->flags & DSDESC_WIDTH) {
          width = desc->width;
          if (!width)
               return DFB_INVARG;
     }
     if (desc->flags & DSDESC_HEIGHT) {
          height = desc->height;
          if (!height)
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
          case DSPF_A8:
          case DSPF_ARGB:
          case DSPF_I420:
          case DSPF_LUT8:
          case DSPF_RGB15:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
#ifdef SUPPORT_RGB332
          case DSPF_RGB332:
#endif
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
               break;

          default:
               return DFB_INVARG;
     }

     if (caps & DSCAPS_PRIMARY) {
          if (desc->flags & DSDESC_PREALLOCATED)
               return DFB_INVARG;

          /* FIXME: should we allow to create more primaries in windowed mode?
                    should the primary surface be a singleton?
                    or should we return an error? */
          switch (data->level) {
               case DFSCL_NORMAL: {
                    int                    x, y;
                    CoreWindow            *window;
                    DFBWindowCapabilities  window_caps = DWCAPS_NONE;

                    width  = data->primary.width;
                    height = data->primary.height;

                    x = ((int)config.width  - (int)width)  / 2;
                    y = ((int)config.height - (int)height) / 2;

                    if ((desc->flags & DSDESC_PIXELFORMAT)
                        && desc->pixelformat == DSPF_ARGB)
                         window_caps |= DWCAPS_ALPHACHANNEL;

                    if (caps & DSCAPS_FLIPPING)
                         window_caps |= DWCAPS_DOUBLEBUFFER;

                    ret = dfb_layer_create_window( data->layer, x, y,
                                                   width, height, window_caps,
                                                   caps, format, &window );
                    if (ret)
                         return ret;

                    dfb_window_init( window );

                    dfb_window_set_opacity( window, 0xFF );

                    init_palette( window->surface, desc );

                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    return IDirectFBSurface_Window_Construct( *interface, NULL,
                                                              NULL, window,
                                                              caps );
               }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE:
                    if (format != config.pixelformat) {
                         config.pixelformat = format;

                         ret = dfb_layer_set_configuration( data->layer,
                                                            &config );
                         if (ret)
                              return ret;
                    }

                    init_palette( dfb_layer_surface( data->layer ), desc );
                    
                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );
                    
                    return IDirectFBSurface_Layer_Construct( *interface, NULL,
                                                             NULL, data->layer,
                                                             caps );
          }
     }


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

          if ((caps & DSCAPS_FLIPPING) &&
              (!desc->preallocated[1].data ||
                desc->preallocated[1].pitch < min_pitch))
          {
               return DFB_INVARG;
          }

          ret = dfb_surface_create_preallocated( width, height,
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
          ret = dfb_surface_create( width, height, format,
                                    policy, caps, NULL, &surface );
          if (ret)
               return ret;
     }

     init_palette( surface, desc );
     
     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

     ret = IDirectFBSurface_Construct( *interface, NULL, NULL, surface, caps );

     dfb_surface_unref( surface );

     return ret;
}

static DFBResult
IDirectFB_EnumDisplayLayers( IDirectFB               *thiz,
                             DFBDisplayLayerCallback  callbackfunc,
                             void                    *callbackdata )
{
     EnumDisplayLayers_Context context;

     INTERFACE_GET_DATA(IDirectFB)

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

     INTERFACE_GET_DATA(IDirectFB)

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

     INTERFACE_GET_DATA(IDirectFB)

     if (!callbackfunc)
          return DFB_INVARG;

     context.callback     = callbackfunc;
     context.callback_ctx = callbackdata;

     dfb_input_enumerate_devices( EnumInputDevices_Callback, &context );

     return DFB_OK;
}

static DFBResult
IDirectFB_GetInputDevice( IDirectFB             *thiz,
                          DFBInputDeviceID       id,
                          IDirectFBInputDevice **interface )
{
     GetInputDevice_Context context;

     INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     context.interface = interface;
     context.id        = id;

     dfb_input_enumerate_devices( GetInputDevice_Callback, &context );

     return (*interface) ? DFB_OK : DFB_IDNOTFOUND;
}

static DFBResult
IDirectFB_CreateEventBuffer( IDirectFB                   *thiz,
                             DFBInputDeviceCapabilities   caps,
                             IDirectFBEventBuffer       **interface)
{
     CreateEventBuffer_Context context;

     INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBEventBuffer );
     IDirectFBEventBuffer_Construct( *interface );

     context.caps      = caps;
     context.interface = interface;

     dfb_input_enumerate_devices( CreateEventBuffer_Callback, &context );

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

     INTERFACE_GET_DATA(IDirectFB)

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
     DFBResult                            ret;
     DFBInterfaceFuncs                   *funcs = NULL;
     IDirectFBVideoProvider              *videoprovider;
     IDirectFBVideoProvider_ProbeContext  ctx;

     INTERFACE_GET_DATA(IDirectFB)

     /* Check arguments */
     if (!interface || !filename)
          return DFB_INVARG;

     if (access( filename, R_OK ) != 0)
          return errno2dfb( errno );

     /* Fill out probe context */
     ctx.filename = filename;
     
     /* Find a suitable implemenation */
     ret = DFBGetInterface( &funcs,
                            "IDirectFBVideoProvider", NULL,
                            DFBProbeInterface, &ctx );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( videoprovider, IDirectFBVideoProvider );

     /* Construct the interface */
     ret = funcs->Construct( videoprovider, filename );
     if (ret)
          return ret;

     *interface = videoprovider;

     return DFB_OK;
}

static DFBResult
IDirectFB_CreateFont( IDirectFB           *thiz,
                      const char          *filename,
                      DFBFontDescription  *desc,
                      IDirectFBFont      **interface )
{
     DFBResult                   ret;
     DFBInterfaceFuncs          *funcs = NULL;
     IDirectFBFont              *font;
     IDirectFBFont_ProbeContext  ctx;

     INTERFACE_GET_DATA(IDirectFB)

     /* Check arguments */
     if (!interface)
          return DFB_INVARG;

     if (filename) {
          if (!desc)
               return DFB_INVARG;
     
          if (access( filename, R_OK ) != 0)
               return errno2dfb( errno );
     }

     /* Fill out probe context */
     ctx.filename = filename;
     
     /* Find a suitable implemenation */
     ret = DFBGetInterface( &funcs,
                            "IDirectFBFont", NULL,
                            DFBProbeInterface, &ctx );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( font, IDirectFBFont );

     /* Construct the interface */
     ret = funcs->Construct( font, filename, desc );
     if (ret)
          return ret;

     *interface = font;
     
     return DFB_OK;
}

static DFBResult
IDirectFB_CreateDataBuffer( IDirectFB                 *thiz,
                            DFBDataBufferDescription  *desc,
                            IDirectFBDataBuffer      **interface )
{
     INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     if (!desc) {
          DFB_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_Streamed_Construct( *interface );
     }
          
     if (desc->flags & DBDESC_FILE) {
          if (!desc->file)
               return DFB_INVARG;

          DFB_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_File_Construct( *interface,
                                                     desc->file );
     }
     
     if (desc->flags & DBDESC_MEMORY) {
          if (!desc->memory.data || !desc->memory.length)
               return DFB_INVARG;

          DFB_ALLOCATE_INTERFACE( *interface, IDirectFBDataBuffer );

          return IDirectFBDataBuffer_Memory_Construct( *interface,
                                                       desc->memory.data,
                                                       desc->memory.length );
     }

     return DFB_INVARG;
}

static DFBResult
IDirectFB_Suspend( IDirectFB *thiz )
{
     return dfb_core_suspend();
}

static DFBResult
IDirectFB_Resume( IDirectFB *thiz )
{
     return dfb_core_resume();
}

static DFBResult
IDirectFB_WaitIdle( IDirectFB *thiz )
{
     INTERFACE_GET_DATA(IDirectFB)

     dfb_gfxcard_sync();

     return DFB_OK;
}

static DFBResult
IDirectFB_WaitForSync( IDirectFB *thiz )
{
     INTERFACE_GET_DATA(IDirectFB)

     dfb_fbdev_wait_vsync();

     return DFB_OK;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
DFBResult
IDirectFB_Construct( IDirectFB *thiz )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFB)

     data->ref = 1;

     data->level = DFSCL_NORMAL;

     data->primary.width  = 640;
     data->primary.height = 480;

     data->layer = dfb_layer_at( DLID_PRIMARY );

     thiz->AddRef = IDirectFB_AddRef;
     thiz->Release = IDirectFB_Release;
     thiz->SetCooperativeLevel = IDirectFB_SetCooperativeLevel;
     thiz->GetCardCapabilities = IDirectFB_GetCardCapabilities;
     thiz->EnumVideoModes = IDirectFB_EnumVideoModes;
     thiz->SetVideoMode = IDirectFB_SetVideoMode;
     thiz->CreateSurface = IDirectFB_CreateSurface;
     thiz->EnumDisplayLayers = IDirectFB_EnumDisplayLayers;
     thiz->GetDisplayLayer = IDirectFB_GetDisplayLayer;
     thiz->EnumInputDevices = IDirectFB_EnumInputDevices;
     thiz->GetInputDevice = IDirectFB_GetInputDevice;
     thiz->CreateEventBuffer = IDirectFB_CreateEventBuffer;
     thiz->CreateImageProvider = IDirectFB_CreateImageProvider;
     thiz->CreateVideoProvider = IDirectFB_CreateVideoProvider;
     thiz->CreateFont = IDirectFB_CreateFont;
     thiz->CreateDataBuffer = IDirectFB_CreateDataBuffer;
     thiz->Suspend = IDirectFB_Suspend;
     thiz->Resume = IDirectFB_Resume;
     thiz->WaitIdle = IDirectFB_WaitIdle;
     thiz->WaitForSync = IDirectFB_WaitForSync;

     return DFB_OK;
}


/*
 * internal functions
 */

static DFBEnumerationResult
EnumDisplayLayers_Callback( DisplayLayer *layer, void *ctx )
{
     DFBDisplayLayerDescription  desc;
     EnumDisplayLayers_Context  *context = (EnumDisplayLayers_Context*) ctx;

     dfb_layer_description( layer, &desc );

     return context->callback( dfb_layer_id( layer ), desc,
                               context->callback_ctx );
}

static DFBEnumerationResult
GetDisplayLayer_Callback( DisplayLayer *layer, void *ctx )
{
     GetDisplayLayer_Context *context = (GetDisplayLayer_Context*) ctx;

     if (dfb_layer_id( layer ) != context->id)
          return DFENUM_OK;

     if ((context->ret = dfb_layer_enable( layer )) == DFB_OK) {
          DFB_ALLOCATE_INTERFACE( *context->interface, IDirectFBDisplayLayer );

          IDirectFBDisplayLayer_Construct( *context->interface, layer );
     }

     return DFENUM_CANCEL;
}

static DFBEnumerationResult
EnumInputDevices_Callback( InputDevice *device, void *ctx )
{
     DFBInputDeviceDescription  desc;
     EnumInputDevices_Context  *context = (EnumInputDevices_Context*) ctx;

     dfb_input_device_description( device, &desc );

     return context->callback( dfb_input_device_id( device ), desc,
                               context->callback_ctx );
}

static DFBEnumerationResult
GetInputDevice_Callback( InputDevice *device, void *ctx )
{
     GetInputDevice_Context *context = (GetInputDevice_Context*) ctx;

     if (dfb_input_device_id( device ) != context->id)
          return DFENUM_OK;

     DFB_ALLOCATE_INTERFACE( *context->interface, IDirectFBInputDevice );

     IDirectFBInputDevice_Construct( *context->interface, device );

     return DFENUM_CANCEL;
}

static DFBEnumerationResult
CreateEventBuffer_Callback( InputDevice *device, void *ctx )
{
     DFBInputDeviceDescription   desc;
     CreateEventBuffer_Context  *context = (CreateEventBuffer_Context*) ctx;

     dfb_input_device_description( device, &desc );

     if (! (desc.caps & context->caps))
          return DFENUM_OK;

     IDirectFBEventBuffer_AttachInputDevice( *context->interface, device );

     return DFENUM_OK;
}

