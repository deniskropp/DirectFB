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

#include "directfb.h"
#include "directfb_version.h"
#include "directfb_internals.h"

#include "core/core.h"
#include "core/coretypes.h"

#include "core/fbdev.h"
#include "core/state.h"
#include "core/gfxcard.h"
#include "core/input.h"
#include "core/layers.h"
#include "core/surfaces.h"
#include "core/surfacemanager.h"
#include "core/windows.h"

#include "display/idirectfbsurface.h"
#include "display/idirectfbsurface_layer.h"
#include "display/idirectfbsurface_window.h"
#include "display/idirectfbdisplaylayer.h"
#include "input/idirectfbinputbuffer.h"
#include "input/idirectfbinputdevice.h"

#include "idirectfb.h"

#include "misc/conf.h"
#include "misc/mem.h"
#include "misc/util.h"

/*
 * private data struct of IDirectFB
 */
typedef struct {
     int                 ref;      /* reference counter */
     DFBCooperativeLevel level;    /* current cooperative level */

     struct {
          int            width;    /* IDirectFB stores window width    */
          int            height;   /* and height and the pixel depth   */
          int            bpp;      /* from SetVideoMode() parameters.  */
     } primary;                    /* Used for DFSCL_NORMAL's primary. */
} IDirectFB_data;

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

typedef struct {
     char        header[32];
     const char *filename;
} CreateImageProvider_Context;

static DFBEnumerationResult EnumInputDevices_Callback ( InputDevice *device,
                                                        void        *ctx );
static DFBEnumerationResult GetInputDevice_Callback   ( InputDevice *device,
                                                        void        *ctx );
static DFBEnumerationResult CreateEventBuffer_Callback( InputDevice *device,
                                                        void        *ctx );

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
          dfb_layer_unlock( dfb_layers );

     dfb_core_unref();     /* TODO: where should we place this call? */

     DFBFREE( thiz->priv );
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     DFBFREE( thiz );
#endif
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
               dfb_layer_unlock( dfb_layers );
               break;
          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE:
               if (dfb_config->force_windowed)
                    return DFB_ACCESSDENIED;

               if (data->level == DFSCL_NORMAL) {
                    DFBResult ret = dfb_layer_lock( dfb_layers );
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

               switch (bpp) {
                    case 15:
                         config.pixelformat = DSPF_RGB15;
                         break;
                    case 16:
                         config.pixelformat = DSPF_RGB16;
                         break;
                    case 24:
                         config.pixelformat = DSPF_RGB24;
                         break;
                    case 32:
                         config.pixelformat = DSPF_RGB32;
                         break;
                    default:
                         return DFB_INVARG;
               }

               config.width      = width;
               config.height     = height;
               config.buffermode = DLBM_FRONTONLY;
               config.options    = 0;

               config.flags = DLCONF_WIDTH | DLCONF_HEIGHT | /*DLCONF_BUFFERMODE |*/
                              DLCONF_PIXELFORMAT | DLCONF_OPTIONS;

               ret = dfb_layers->SetConfiguration( dfb_layers, &config );
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

static DFBResult
IDirectFB_CreateSurface( IDirectFB              *thiz,
                         DFBSurfaceDescription  *desc,
                         IDirectFBSurface      **interface )
{
     DFBResult ret;
     unsigned int width = 256;
     unsigned int height = 256;
     int policy = CSP_VIDEOLOW;
     DFBSurfacePixelFormat format = dfb_layers->shared->surface->format;
     DFBSurfaceCapabilities caps = 0;
     CoreSurface *surface = NULL;

     INTERFACE_GET_DATA(IDirectFB)


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

     if (desc->flags & DSDESC_CAPS)
          caps = desc->caps;

     if (desc->flags & DSDESC_PIXELFORMAT)
          format = desc->pixelformat;


     if (caps & DSCAPS_PRIMARY) {
          if (desc->flags & DSDESC_PREALLOCATED)
               return DFB_INVARG;

          /* FIXME: should we allow to create more primaries in windowed mode?
                    should the primary surface be a singleton?
                    or should we return an error? */
          switch (data->level) {
               case DFSCL_NORMAL: {
                    int         x, y;
                    CoreWindow *window;

                    width  = data->primary.width;
                    height = data->primary.height;

                    x = (dfb_layers->shared->width  - width)  / 2;
                    y = (dfb_layers->shared->height - height) / 2;

                    if ((desc->flags & DSDESC_PIXELFORMAT)
                        && desc->pixelformat == DSPF_ARGB)
                    {
                         window = dfb_window_create( dfb_layers->shared->windowstack,
                                                     x, y,
                                                     data->primary.width,
                                                     data->primary.height,
                                                     DWCAPS_ALPHACHANNEL |
                                                     DWCAPS_DOUBLEBUFFER,
                                                     DSPF_UNKNOWN );
                    }
                    else
                         window = dfb_window_create( dfb_layers->shared->windowstack,
                                                     x, y,
                                                     data->primary.width,
                                                     data->primary.height,
                                                     (caps & DSCAPS_FLIPPING) ?
                                                     DWCAPS_DOUBLEBUFFER : 0,
                                                     DSPF_UNKNOWN );

                    if (!window)
                         return DFB_FAILURE;

                    dfb_window_init( window );

                    dfb_window_set_opacity( window, 0xFF );

                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    return IDirectFBSurface_Window_Construct( *interface, NULL,
                                                              NULL, window,
                                                              caps );
               }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE:
                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    return IDirectFBSurface_Layer_Construct( *interface, NULL,
                                                             NULL, dfb_layers,
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
                                                 format, policy, caps,
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
                                    policy, caps, &surface );
          if (ret)
               return ret;
     }

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

     return IDirectFBSurface_Construct( *interface, NULL, NULL, surface, caps );
}

static DFBResult
IDirectFB_EnumDisplayLayers( IDirectFB               *thiz,
                             DFBDisplayLayerCallback  callbackfunc,
                             void                    *callbackdata )
{
     DisplayLayer *dl = dfb_layers;

     INTERFACE_GET_DATA(IDirectFB)

     if (!callbackfunc)
          return DFB_INVARG;

     while (dl) {
          if (callbackfunc( dl->shared->id, dl->shared->caps, callbackdata ) == DFENUM_CANCEL)
               break;

          dl = dl->next;
     }

     return DFB_OK;
}

static DFBResult
IDirectFB_GetDisplayLayer( IDirectFB              *thiz,
                           DFBDisplayLayerID       id,
                           IDirectFBDisplayLayer **layer )
{
     DisplayLayer *dl = dfb_layers;

     INTERFACE_GET_DATA(IDirectFB)

     if (!layer)
          return DFB_INVARG;

     while (dl) {
          if (dl->shared->id == id) {
               DFBResult ret;

               ret = dfb_layer_enable( dl );
               if (ret)
                    return ret;

               DFB_ALLOCATE_INTERFACE( *layer, IDirectFBDisplayLayer );

               return IDirectFBDisplayLayer_Construct( *layer, dl );
          }
          dl = dl->next;
     }

     return DFB_IDNOTFOUND;
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

     *interface = NULL;

     context.caps      = caps;
     context.interface = interface;

     dfb_input_enumerate_devices( CreateEventBuffer_Callback, &context );

     return (*interface) ? DFB_OK : DFB_IDNOTFOUND;
}

static int
image_probe( DFBInterfaceFuncs *funcs, void *ctx )
{
     CreateImageProvider_Context *context = (CreateImageProvider_Context*) ctx;

     if (funcs->Probe( context->header, context->filename ) == DFB_OK)
          return 1;

     return 0;
}

static DFBResult
IDirectFB_CreateImageProvider( IDirectFB               *thiz,
                               const char              *filename,
                               IDirectFBImageProvider **interface )
{
     int                          fd;
     DFBResult                    ret;
     CreateImageProvider_Context  ctx;
     DFBInterfaceFuncs           *funcs = NULL;

     INTERFACE_GET_DATA(IDirectFB)

     if (!filename || !interface)
          return DFB_INVARG;

     /*  read the first 32 bytes  */
     fd = open( filename, O_RDONLY | O_NONBLOCK );
     if (fd == -1)
          return errno2dfb( errno );

     if (read( fd, ctx.header, 32 ) < 32) {
          close( fd );
          return DFB_IO;
     }
     close( fd );

     ctx.filename = filename;

     ret = DFBGetInterface( &funcs, "IDirectFBImageProvider", NULL,
                            image_probe, (void*)&ctx );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBImageProvider );

     ret = funcs->Construct( *interface, filename );

     if (ret) {
        free( *interface );
        *interface = NULL;
     }

     return ret;
}


static int
video_probe( DFBInterfaceFuncs *funcs, void *ctx )
{
     if (funcs->Probe( (char*)ctx ) == DFB_OK)
          return 1;

     return 0;
}

static DFBResult
IDirectFB_CreateVideoProvider( IDirectFB               *thiz,
                               const char              *filename,
                               IDirectFBVideoProvider **interface )
{
     DFBResult          ret;
     DFBInterfaceFuncs *funcs = NULL;

     INTERFACE_GET_DATA(IDirectFB)

     if (!interface || !filename)
          return DFB_INVARG;

     if (access( filename, R_OK ) != 0)
          return errno2dfb( errno );

     ret = DFBGetInterface( &funcs,
                            "IDirectFBVideoProvider", NULL,
                            video_probe, (void*)filename );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBVideoProvider );

     ret = funcs->Construct( *interface, filename );

     if (ret) {
        free(*interface);
        *interface = NULL;
     }

     return ret;
}

static DFBResult
IDirectFB_CreateFont( IDirectFB           *thiz,
                      const char          *filename,
                      DFBFontDescription  *desc,
                      IDirectFBFont      **interface )
{
     DFBResult          ret;
     DFBInterfaceFuncs *funcs = NULL;

     INTERFACE_GET_DATA(IDirectFB)

     if (!interface)
          return DFB_INVARG;

     /* TODO: FIXME: Probe Font Loaders! */
     if (filename) {
          if (!desc)
               return DFB_INVARG;

          if (access( filename, R_OK ) != 0)
               return errno2dfb( errno );

          /* the only supported real font format yet. */
          ret = DFBGetInterface( &funcs,
                                 "IDirectFBFont", "FT2",
                                 NULL, NULL );
     }
     else {
          /* use the default bitmap font */
          ret = DFBGetInterface( &funcs,
                                 "IDirectFBFont", "Default",
                                 NULL, NULL );
     }

     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBFont );

     ret = funcs->Construct( *interface, filename, desc );

     if (ret) {
          free(*interface);
          *interface = NULL;
     }

     return ret;
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
     IDirectFB_data *data;

     data = (IDirectFB_data*)DFBCALLOC( 1, sizeof(IDirectFB_data) );
     thiz->priv = data;

     data->ref = 1;

     data->level = DFSCL_NORMAL;

     data->primary.width  = 500;
     data->primary.height = 500;


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
EnumInputDevices_Callback( InputDevice *device, void *ctx )
{
     EnumInputDevices_Context *context = (EnumInputDevices_Context*) ctx;

     return context->callback( dfb_input_device_id( device ),
                               dfb_input_device_description( device ),
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

     return DFENUM_OK;
}

static DFBEnumerationResult
CreateEventBuffer_Callback( InputDevice *device, void *ctx )
{
     CreateEventBuffer_Context  *context = (CreateEventBuffer_Context*) ctx;
     DFBInputDeviceDescription   desc    = dfb_input_device_description( device );

     if (! (desc.caps & context->caps))
          return DFENUM_OK;

     if (! *context->interface) {
          DFB_ALLOCATE_INTERFACE( *context->interface, IDirectFBEventBuffer );

          IDirectFBEventBuffer_Construct( *context->interface );
     }

     IDirectFBEventBuffer_AttachInputDevice( *context->interface, device );

     return DFENUM_OK;
}

