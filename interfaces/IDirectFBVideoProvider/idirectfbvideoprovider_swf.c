/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.
              
   The SWF Provider is written by Joachim Steiger <roh@hyte.de>.

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

#include <sys/time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <malloc.h>

#include <pthread.h>

#include <flash.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <media/idirectfbvideoprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>
#include <misc/mem.h>

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, SWF )

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int                  ref;       /* reference counter */
     FlashHandle          flashHandle;
     struct FlashInfo     flashInfo;
     struct FlashDisplay  flashDisplay;
     pthread_t            thread;

     IDirectFBSurface    *destination;
     DFBRectangle         dest_rect;

     DVFrameCallback      callback;
     void                *ctx;

     CardState            state;
     CoreSurface         *source;
} IDirectFBVideoProvider_Swf_data;


/* ------------------------------------ */
int
readFile (const char *filename, char **buffer, long *size)
{
     FILE *in;
     char *buf;
     long length;

     in = fopen (filename, "r");
     if (in == 0) {
          perror (filename);
          return -1;
     }
     fseek (in, 0, SEEK_END);
     length = ftell (in);
     rewind (in);
     (int *) buf = DFBMALLOC(length);
     fread (buf, length, 1, in);
     fclose (in);
     *size = length;
     *buffer = buf;
     return length;
}

void
showUrl (char *url, char *target, void *client_data)
{
     printf ("SWF GetURL : %s\n", url);
}

void
getSwf (char *url, int level, void *client_data)
{
     FlashHandle flashHandle;
     char *buffer;
     long size;

     flashHandle = (FlashHandle) client_data;
     printf ("SWF LoadMovie: %s @ %d\n", url, level);
     if (readFile (url, &buffer, &size) > 0) {
          FlashParse (flashHandle, level, buffer, size);
     }
}


static void* FrameThread( void *ctx )
{
     IDirectFBVideoProvider_Swf_data *data = (IDirectFBVideoProvider_Swf_data*)ctx;
     struct timeval wd2,now,tv;
     long           cmd;
     long           wakeUp;
     long delay = 0;

     cmd = FLASH_WAKEUP;
     wakeUp = FlashExec (data->flashHandle, cmd, 0, &wd2);

     while (1) {

          pthread_testcancel();


          gettimeofday (&now, 0);
          delay = (wd2.tv_sec - now.tv_sec) * 1000 + (wd2.tv_usec - now.tv_usec) / 1000;

          if (delay < 0)
               delay = 20;

          if (data->flashDisplay.flash_refresh) {
               DFBRectangle   rect, drect;

               rect.x=0;
               rect.y=0;
               rect.w=(int) data->flashInfo.frameWidth / 20;
               rect.h=(int) data->flashInfo.frameHeight / 20;

               drect = data->dest_rect;

               dfb_gfxcard_stretchblit( &rect, &drect, &data->state );
               data->flashDisplay.flash_refresh = 0;

               if (data->callback)
                    data->callback (data->ctx);
          }

          if (wakeUp) {
               tv.tv_sec = 0;
               tv.tv_usec = delay * 1000;
               select( 0, 0, 0, 0, &tv );

               cmd = FLASH_WAKEUP;
               wakeUp = FlashExec (data->flashHandle, cmd, 0, &wd2);
          }
          else
               return NULL;
     }
}

/* ------------------------------------------ */
static 
void IDirectFBVideoProvider_Swf_Destruct(IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swf_data *data;

     data = (IDirectFBVideoProvider_Swf_data*)thiz->priv;

     thiz->Stop( thiz );

     FlashClose(data->flashHandle);

     dfb_surface_unref( data->source );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBVideoProvider_Swf_AddRef(IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_Release(IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (--data->ref == 0) {
          IDirectFBVideoProvider_Swf_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult 
IDirectFBVideoProvider_Swf_GetCapabilities(
                                          IDirectFBVideoProvider       *thiz,
                                          DFBVideoProviderCapabilities *caps )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (!caps)
          return DFB_INVARG;

     *caps = DVCAPS_BASIC | DVCAPS_SCALE;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_GetSurfaceDescription(
                                                                 IDirectFBVideoProvider *thiz,
                                                                 DFBSurfaceDescription  *desc )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (!desc)
          return DFB_INVARG;

     memset( desc, 0, sizeof(DFBSurfaceDescription) );
     desc->flags = (DFBSurfaceDescriptionFlags)
                   (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);

     desc->width  = (int) data->flashInfo.frameWidth / 20;
     desc->height = (int) data->flashInfo.frameHeight / 20;
     desc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Swf_PlayTo(
                                                  IDirectFBVideoProvider *thiz,
                                                  IDirectFBSurface       *destination,
                                                  const DFBRectangle     *dstrect,
                                                  DVFrameCallback         callback,
                                                  void                   *ctx )
{
     DFBRectangle           rect;
     IDirectFBSurface_data *dst_data;

     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (!destination)
          return DFB_INVARG;

     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!dst_data)
          return DFB_DEAD;

     /* build the destination rectangle */
     if (dstrect) {
          if (dstrect->w < 1  ||  dstrect->h < 1)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else
          rect = dst_data->area.wanted;

     /* save for later blitting operation */
     data->dest_rect = rect;

     /* build the clip rectangle */
     if (!dfb_rectangle_intersect( &rect, &dst_data->area.current ))
          return DFB_INVARG;

     /* put the destination clip into the state */
     data->state.clip.x1 = rect.x;
     data->state.clip.y1 = rect.y;
     data->state.clip.x2 = rect.x + rect.w - 1;
     data->state.clip.y2 = rect.y + rect.h - 1;
     data->state.destination = dst_data->surface;
     data->state.modified = (StateModificationFlags)
                            (data->state.modified | SMF_CLIP | SMF_DESTINATION);

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     destination->AddRef( destination );
     data->destination = destination;   /* FIXME: install listener */

     data->callback = callback;
     data->ctx = ctx;

     if (data->thread == -1)
          pthread_create( &data->thread, NULL, FrameThread, data );

     return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Swf_Stop(IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (data->thread != -1) {
          pthread_cancel( data->thread );
          pthread_join( data->thread, NULL );
          data->thread = -1;
     }

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_Swf_SeekTo(
                                                  IDirectFBVideoProvider *thiz,
                                                  double                  seconds )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_GetPos(
                                                  IDirectFBVideoProvider *thiz,
                                                  double                 *seconds )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_GetLength(
                                                     IDirectFBVideoProvider *thiz,
                                                     double                 *seconds )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     *seconds = 0.0;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_GetColorAdjustment(
                                                              IDirectFBVideoProvider *thiz,
                                                              DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (!adj)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_Swf_SetColorAdjustment(
                                                              IDirectFBVideoProvider *thiz,
                                                              DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_Swf)

     if (!adj)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}


/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (strstr( ctx->filename, ".swf" ) ||
         strstr( ctx->filename, ".SWF" ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     char *buffer;
     long size;
     int status;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_Swf)

     data->ref = 1;

     if (readFile (filename, &buffer, &size) < 0) {
          printf( "DirectFB/Swf: Loading Swf file failed.\n");
          DFBFREE( data );
          DFB_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     data->flashHandle = FlashNew();
     if (data->flashHandle == 0) {
          printf( "DirectFB/Swf: Creation of Swfplayer failed.\n");
          DFBFREE( data );
          DFB_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     do {
          status = FlashParse (data->flashHandle, 0, buffer, size);
     }
     while (status & FLASH_PARSE_NEED_DATA);
     DFBFREE(buffer);

     FlashGetInfo (data->flashHandle, &data->flashInfo);

     dfb_surface_create( (int) data->flashInfo.frameWidth  / 20,
                         (int) data->flashInfo.frameHeight / 20,
                         DSPF_RGB16, CSP_SYSTEMONLY, DSCAPS_SYSTEMONLY, NULL,
                         &(data->source));

     data->flashDisplay.pixels = data->source->back_buffer->system.addr;
     data->flashDisplay.bpl    = data->source->back_buffer->system.pitch;
     data->flashDisplay.width = data->source->width;
     data->flashDisplay.height = data->source->height;
     data->flashDisplay.depth = 16;
     data->flashDisplay.bpp = 2;

     data->thread = -1;     

/*
     pthread_mutex_init( &data->source.front_lock, NULL );
     pthread_mutex_init( &data->source.back_lock, NULL );
     pthread_mutex_init( &data->source.listeners_mutex, NULL );
*/
     data->state.source   = data->source;
     data->state.modified = SMF_ALL;

     FlashGraphicInit (data->flashHandle, &data->flashDisplay);
//     FlashSoundInit(data->flashHandle, "/dev/dsp");
     FlashSetGetUrlMethod (data->flashHandle, showUrl, 0);
     FlashSetGetSwfMethod (data->flashHandle, getSwf, 
                           (void *) data->flashHandle);


     thiz->AddRef    = IDirectFBVideoProvider_Swf_AddRef;
     thiz->Release   = IDirectFBVideoProvider_Swf_Release;
     thiz->GetCapabilities = IDirectFBVideoProvider_Swf_GetCapabilities;
     thiz->GetSurfaceDescription = 
     IDirectFBVideoProvider_Swf_GetSurfaceDescription;
     thiz->PlayTo    = IDirectFBVideoProvider_Swf_PlayTo;
     thiz->Stop      = IDirectFBVideoProvider_Swf_Stop;
     thiz->SeekTo    = IDirectFBVideoProvider_Swf_SeekTo;
     thiz->GetPos    = IDirectFBVideoProvider_Swf_GetPos;
     thiz->GetLength = IDirectFBVideoProvider_Swf_GetLength;
     thiz->GetColorAdjustment = 
     IDirectFBVideoProvider_Swf_GetColorAdjustment;
     thiz->SetColorAdjustment = 
     IDirectFBVideoProvider_Swf_SetColorAdjustment;

     return DFB_OK;
}
