/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de>,   
              Andreas Hundt <andi@convergence.de> and
              Sven Neumann <sven@convergence.de>.

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

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>
}

#include <aviplay.h>

/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
     int               ref;       /* reference counter */
     IAviPlayer       *player;

     IDirectFBSurface *destination;
     DFBRectangle      dest_rect;
     DVFrameCallback   callback;
     void             *ctx;

     CardState         state;
     CoreSurface       source;
} IDirectFBVideoProvider_AviFile_data;


static void IDirectFBVideoProvider_AviFile_Destruct(
                                                 IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_AviFile_data *data;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (data->player->IsPlaying())
          data->player->Stop();

     delete data->player;

     reactor_free( data->source.reactor );

     free (thiz->priv);
     thiz->priv = NULL;

#ifndef DFB_DEBUG
     free( thiz );
#endif
}

static DFBResult IDirectFBVideoProvider_AviFile_AddRef(
                                                 IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_Release(
                                                 IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (--data->ref == 0) {
          IDirectFBVideoProvider_AviFile_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetCapabilities(
                                           IDirectFBVideoProvider       *thiz,
                                           DFBVideoProviderCapabilities *caps )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz || !caps)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *caps = (DFBVideoProviderCapabilities) ( DVCAPS_BASIC | 
                                              DVCAPS_SCALE | 
                                              DVCAPS_SEEK );

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetSurfaceDescription(
                                                 IDirectFBVideoProvider *thiz,
                                                 DFBSurfaceDescription  *desc )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz || !desc)
          return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     desc->flags = (DFBSurfaceDescriptionFlags)
          (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc->width  = data->player->GetWidth();
     desc->height = data->player->GetHeight();
     desc->pixelformat = layers->surface->format;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_PlayTo(
                                           IDirectFBVideoProvider *thiz,
                                           IDirectFBSurface       *destination,
                                           DFBRectangle           *dstrect,
                                           DVFrameCallback         callback,
                                           void                   *ctx )
{
     DFBRectangle                         rect;
     IDirectFBVideoProvider_AviFile_data *data;
     IDirectFBSurface_data               *dst_data;

     if (!thiz || !destination)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;
     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!data || !dst_data)
          return DFB_DEAD;

     /* URGENT: keep in sync with DrawCallback */

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
     if (!rectangle_intersect( &rect, &dst_data->area.current ))
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


     switch (data->player->GetState( NULL )) {
          case IAviPlayer::Playing:
               break;
          case IAviPlayer::Paused:
               data->player->Pause(false);
               break;
          default:
               data->player->Start();
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_Stop(
                                                 IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     if (data->player->IsPlaying())
          data->player->Pause(true);

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_SeekTo(
                                               IDirectFBVideoProvider *thiz,
                                               double                  seconds)
{
     IDirectFBVideoProvider_AviFile_data *data;
     double curpos;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     curpos = data->player->GetPos();
     data->player->Reseek( seconds );
     /* seeking forward for some small amount may actually bring us
      * _back_ to the last key frame -> compensate via PageUp()
      */
     if (seconds > curpos && curpos > data->player->GetPos())
       data->player->PageUp();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetPos(
     IDirectFBVideoProvider *thiz,
     double                 *seconds)
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = data->player->GetPos();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetLength(
     IDirectFBVideoProvider *thiz,
     double                 *seconds)
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     *seconds = data->player->GetVideoLength();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
    IDirectFBVideoProvider_AviFile_data *data;

    if (!thiz || !adj)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_AviFile_SetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
    IDirectFBVideoProvider_AviFile_data *data;

    if (!thiz || !adj)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    return DFB_UNIMPLEMENTED;
}

static void AviFile_KillCallback( int bogus, void *p )
{
  /* AviFile_KillCallback gets called when AviFile->Stop is called.
     At the moment we do nothing here...

     IDirectFBVideoProvider_AviFile_data *data =
          (IDirectFBVideoProvider_AviFile_data*)p;
   */
}

static void AviFile_DrawCallback( const CImage *image, void *p )
{
     IDirectFBVideoProvider_AviFile_data *data =
          (IDirectFBVideoProvider_AviFile_data*)p;

     data->source.front_buffer->system.addr   = (void*)(image->Data());
     data->source.front_buffer->system.pitch  = image->Bpl();

     
     DFBRectangle rect = { 0, 0, image->Width(), image->Height() };

     if (rect.w == data->dest_rect.w  &&  rect.h == data->dest_rect.h) {
          gfxcard_blit( &rect, data->dest_rect.x,
                        data->dest_rect.y, &data->state );
     }
     else {
          DFBRectangle drect = data->dest_rect;

          gfxcard_stretchblit( &rect, &drect, &data->state );
     }


     if (data->callback)
          data->callback( data->ctx );
}


/* exported symbols */

extern "C" {

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "AviFile";
}

DFBResult Probe( const char *filename )
{
     if (strstr( filename, ".avi" ) ||
         strstr( filename, ".AVI" ) ||
         strstr( filename, ".asf" ) ||
         strstr( filename, ".ASF" ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     IDirectFBVideoProvider_AviFile_data *data;

     data = (IDirectFBVideoProvider_AviFile_data*)
          calloc( 1, sizeof(IDirectFBVideoProvider_AviFile_data) );

     thiz->priv = data;

     data->ref = 1;

     try {
          data->player = CreateAviPlayer( filename, 16 /* FIXME */ );

          data->player->SetDrawCallback2( AviFile_DrawCallback, data );
          data->player->SetKillHandler( AviFile_KillCallback, data );
     }
     catch (FatalError e) {
          ERRORMSG( "DirectFB/AviFile: CreateAviPlayer failed: %s\n",
                    e.GetDesc() );
          free( data );
          return DFB_FAILURE;
     }


     data->source.width  = data->player->GetWidth();
     data->source.height = data->player->GetHeight();
     data->source.format = DSPF_RGB16;
     
     data->source.front_buffer = 
          (SurfaceBuffer*) calloc( 1, sizeof(SurfaceBuffer) );
     
     data->source.front_buffer->policy = CSP_SYSTEMONLY;
     data->source.front_buffer->system.health = CSH_STORED;

     data->source.back_buffer = data->source.front_buffer;
     
     pthread_mutex_init( &data->source.front_lock, NULL );
     pthread_mutex_init( &data->source.back_lock, NULL );
     
     data->source.reactor = reactor_new();
     
     data->state.source   = &data->source;
     data->state.modified = SMF_ALL;

     thiz->AddRef    = IDirectFBVideoProvider_AviFile_AddRef;
     thiz->Release   = IDirectFBVideoProvider_AviFile_Release;
     thiz->GetCapabilities = IDirectFBVideoProvider_AviFile_GetCapabilities;
     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_AviFile_GetSurfaceDescription;
     thiz->PlayTo    = IDirectFBVideoProvider_AviFile_PlayTo;
     thiz->Stop      = IDirectFBVideoProvider_AviFile_Stop;
     thiz->SeekTo    = IDirectFBVideoProvider_AviFile_SeekTo;
     thiz->GetPos    = IDirectFBVideoProvider_AviFile_GetPos;
     thiz->GetLength = IDirectFBVideoProvider_AviFile_GetLength;
     thiz->GetColorAdjustment = 
          IDirectFBVideoProvider_AviFile_GetColorAdjustment;
     thiz->SetColorAdjustment = 
          IDirectFBVideoProvider_AviFile_SetColorAdjustment;

     return DFB_OK;
}

}

