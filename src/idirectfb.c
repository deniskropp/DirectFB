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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <malloc.h>
#include <string.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/surfacemanager.h>
#include <core/input.h>
#include <core/layers.h>
#include <core/fbdev.h>
#include <core/v4l.h>

#include <directfb_internals.h>

#include <display/idirectfbsurface.h>
#include <display/idirectfbsurface_layer.h>
#include <display/idirectfbsurface_window.h>
#include <display/idirectfbdisplaylayer.h>
#include <input/idirectfbinputdevice.h>
#include <media/idirectfbfont.h>

#include "directfb_version.h"

#include "idirectfb.h"

/*
 * Destructor
 *
 * Free data structure and set the pointer to NULL,
 * to indicate the dead interface.
 */
void IDirectFB_Destruct( IDirectFB *thiz )
{
     core_deinit();     /* TODO: where should we place this call? */

     free( thiz->priv );
     thiz->priv = NULL;
}

DFBResult IDirectFB_AddRef( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

DFBResult IDirectFB_Release( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0)
          IDirectFB_Destruct( thiz );

     return DFB_OK;
}

DFBResult IDirectFB_SetCooperativeLevel( IDirectFB *thiz,
                                         DFBCooperativeLevel level )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (level == data->level)
          return DFB_OK;

     switch (level) {
          case DFSCL_NORMAL:
               layer_unlock( layers );
               break;
          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE:
               if (data->level == DFSCL_NORMAL)
                    if (layer_lock( layers ) != DFB_OK)
                         return DFB_LOCKED;
               break;
          default:
               return DFB_INVARG;
     }

     data->level = level;

     return DFB_OK;
}

DFBResult IDirectFB_GetCardCapabilities( IDirectFB               *thiz,
                                         DFBCardCapabilities     *caps )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!caps)
          return DFB_INVARG;

     caps->acceleration_mask = card->caps.accel;
     caps->blitting_flags    = card->caps.blitting;
     caps->drawing_flags     = card->caps.drawing;

     return DFB_OK;
}

DFBResult IDirectFB_EnumVideoModes( IDirectFB *thiz,
                                    DFBVideoModeCallback callbackfunc,
                                    void *callbackdata )
{
     VideoMode *m = display->modes;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;
     
     if (!data)
          return DFB_DEAD;

     if (!callbackfunc)
          return DFB_INVARG;

     while (m) {
          callbackfunc( m->xres, m->yres, m->bpp, callbackdata );

          m = m->next;
     }

     return DFB_OK;
}

DFBResult IDirectFB_SetVideoMode( IDirectFB *thiz,
                                  int width, int height, int bpp )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     switch (data->level) {
          case DFSCL_NORMAL:
               data->primary.width = width;
               data->primary.height = height;
               data->primary.bpp = bpp;
               break;
          case DFSCL_FULLSCREEN:
          case DFSCL_EXCLUSIVE: {
               DFBResult ret = layers->SetMode( layers, width, height, bpp );
               if (ret)
                    return ret;
               break;
          }
     }

     return DFB_OK;
}

DFBResult IDirectFB_CreateSurface( IDirectFB *thiz, DFBSurfaceDescription *desc,
                                   IDirectFBSurface **interface )
{
     DFBResult ret;
     int width = 400;
     int height = 300;
     int format = layers->surface->format;
     int policy = CSP_VIDEOLOW;
     DFBSurfaceCapabilities caps = 0;
     CoreSurface *surface = NULL;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

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

     if (desc->flags & DSDESC_CAPS)
          caps = desc->caps;
     
     
     if (caps & DSCAPS_PRIMARY) {

          /* FIXME: singleton? */
          switch (data->level) {
               case DFSCL_NORMAL: {
                    CoreWindow *window;

                    if (!data->primary.width  ||  !data->primary.height) {
                         data->primary.width = width;
                         data->primary.height = height;
                    }

                    if (caps & DSCAPS_ALPHA)
                         window = window_create( layers->windowstack, 0, 0,
                                                 data->primary.width,
                                                 data->primary.height,
                                                 DWCAPS_ALPHACHANNEL );
                    else
                         window = window_create( layers->windowstack, 0, 0,
                                                 data->primary.width,
                                                 data->primary.height, 0 );

                    window_set_opacity( window, 0xFF );

                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    return IDirectFBSurface_Window_Construct( *interface, NULL,
                                                              NULL, window,
                                                              caps );
               }
               case DFSCL_FULLSCREEN:
               case DFSCL_EXCLUSIVE:
                    DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

                    return IDirectFBSurface_Layer_Construct( *interface, NULL,
                                                             NULL, layers,
                                                             caps );
          }
     }

     if (desc->flags & DSDESC_BPP) {
          switch (desc->bpp) {
               case  1:
                    format = DSPF_A1;
                    break;
               case  8:
                    format = DSPF_A8;
                    break;
               case 15:
                    format = DSPF_RGB15;
                    break;
               case 16:
                    format = DSPF_RGB16;
                    break;
               case 24:
                    format = DSPF_RGB24;
                    break;
               case 32:
                    if (caps & DSCAPS_ALPHA)
                         format = DSPF_ARGB;
                    else
                         format = DSPF_RGB32;
                    break;
               default:
                    return DFB_INVARG;
          }
     }

     if (caps & DSCAPS_VIDEOONLY)
          policy = CSP_VIDEOONLY;
     else if (caps & DSCAPS_SYSTEMONLY)
          policy = CSP_SYSTEMONLY;

     ret = surface_create( width, height, format, policy, caps, &surface );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBSurface );

     return IDirectFBSurface_Construct( *interface, NULL, NULL, surface, caps );
}

DFBResult IDirectFB_EnumDisplayLayers( IDirectFB *thiz,
                                       DFBDisplayLayerCallback callbackfunc,
                                       void *callbackdata )
{
     DisplayLayer *dl = layers;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!callbackfunc)
          return DFB_INVARG;

     while (dl) {
          if (callbackfunc( dl->id, dl->caps, callbackdata ))
	       return DFB_OK;

          dl = dl->next;
     }

     return DFB_OK;
}

DFBResult IDirectFB_GetDisplayLayer( IDirectFB *thiz, unsigned int id,
                                     IDirectFBDisplayLayer **layer )
{
     DisplayLayer *dl = layers;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!layer)
          return DFB_INVARG;

     while (dl) {
          if (dl->id == id) {
               dl->Enable( dl );
               
               /* FIXME: singleton? */
               DFB_ALLOCATE_INTERFACE( *layer, IDirectFBDisplayLayer );

               return IDirectFBDisplayLayer_Construct( *layer, dl );
          }
          dl = dl->next;
     }

     return DFB_INVARG;
}

DFBResult IDirectFB_EnumInputDevices( IDirectFB *thiz,
                                      DFBInputDeviceCallback callbackfunc,
                                      void *callbackdata )
{
     InputDevice *d = inputdevices;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!callbackfunc)
          return DFB_INVARG;

     while (d) {
          callbackfunc( d->id, d->desc, callbackdata );
          d = d->next;
     }

     return DFB_OK;
}

DFBResult IDirectFB_GetInputDevice( IDirectFB *thiz, unsigned int id,
                                    IDirectFBInputDevice **device )
{
     InputDevice *d = inputdevices;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (!device)
          return DFB_INVARG;

     while (d) {
          if (d->id == id) {
               /* FIXME: singleton? */
               DFB_ALLOCATE_INTERFACE( *device, IDirectFBInputDevice );

               return IDirectFBInputDevice_Construct( *device, d );
          }
          d = d->next;
     }

     return DFB_INVARG;
}

static int image_probe( DFBInterfaceImplementation *impl, void *ctx )
{
     if (impl->Probe((char *) ctx) == DFB_OK)
          return 1;

     return 0;
}

DFBResult IDirectFB_CreateImageProvider( IDirectFB *thiz, const char *filename,
                                         IDirectFBImageProvider **interface )
{
     DFBResult ret;
     IDirectFB_data             *data = (IDirectFB_data*)thiz->priv;
     DFBInterfaceImplementation *impl = NULL;
     void *ctx;
     int   fd;

     if (!data)
          return DFB_DEAD;

     if (!filename || !interface)
          return DFB_INVARG;

     /*  read the first 32 bytes  */
     fd = open (filename, O_RDONLY);
     if (fd == -1)
          return DFB_FILENOTFOUND;
     ctx = malloc (32);
     if (read (fd, ctx, 32) < 32) {
          free (ctx);
          close (fd);
          return DFB_IO;
     }
     close (fd);
     
     ret = DFBGetInterface( &impl, "IDirectFBImageProvider", NULL,
                            image_probe, ctx );
     
     free (ctx);

     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBImageProvider );

     return impl->Construct( *interface, filename );
}


static int video_probe( DFBInterfaceImplementation *impl, void *ctx )
{
     if (impl->Probe( (char*)ctx ) == DFB_OK)
          return 1;

     return 0;
}

DFBResult IDirectFB_CreateVideoProvider( IDirectFB               *thiz,
                                         const char              *filename,
                                         IDirectFBVideoProvider **interface )
{
     DFBResult ret;
     IDirectFB_data *data;
     DFBInterfaceImplementation *impl = NULL;

     if (!thiz || !interface)
          return DFB_INVARG;

     data = (IDirectFB_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;


     if (filename) {
          ret = DFBGetInterface( &impl,
                                 "IDirectFBVideoProvider", NULL,
                                 video_probe, (void*)filename );
     }
     else {
          ret = DFBGetInterface( &impl,
                                 "IDirectFBVideoProvider", "V4L",
                                 NULL, NULL );
     }

     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBVideoProvider );

     return impl->Construct( *interface, filename );
}

DFBResult IDirectFB_CreateFont( IDirectFB *thiz, const char *filename,
                                DFBFontDescription *desc,
                                IDirectFBFont **interface )
{
     DFBResult ret;
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;
     DFBInterfaceImplementation *impl = NULL;

     if (!data)
          return DFB_DEAD;

     if (!desc || !filename || !interface)
          return DFB_INVARG;

     /* The only supported font format yet. */
     ret = DFBGetInterface( &impl,
                            "IDirectFBFont", "FT2",
                            NULL, NULL );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IDirectFBFont );

     return impl->Construct( *interface, filename, desc );
}

DFBResult IDirectFB_Suspend( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;
     
     if (!data)
          return DFB_DEAD;

     input_suspend();
     layers_suspend();
     surfacemanager_suspend();

     return DFB_OK;
}

DFBResult IDirectFB_Resume( IDirectFB *thiz )
{
     IDirectFB_data *data = (IDirectFB_data*)thiz->priv;
     
     if (!data)
          return DFB_DEAD;

     layers_resume();
     input_resume();

     return DFB_OK;
}

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
DFBResult IDirectFB_Construct( IDirectFB *thiz )
{
     IDirectFB_data *data;

     data = (IDirectFB_data*)malloc( sizeof(IDirectFB_data) );
     memset( data, 0, sizeof(IDirectFB_data) );
     thiz->priv = data;

     data->ref = 1;

     data->level = DFSCL_NORMAL;


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
     thiz->CreateImageProvider = IDirectFB_CreateImageProvider;
     thiz->CreateVideoProvider = IDirectFB_CreateVideoProvider;
     thiz->CreateFont = IDirectFB_CreateFont;
     thiz->Suspend = IDirectFB_Suspend;
     thiz->Resume = IDirectFB_Resume;

     layer_cursor_enable( layers, 1 );

     return DFB_OK;
}

