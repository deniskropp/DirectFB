/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
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
#include <directfb_internals.h>

#include <media/idirectfbvideoprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <misc/mem.h>
#include <misc/util.h>

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, AviFile )

}

/*
 * This is only for C++ to use our C constructor.
 */
static class Foo { public: Foo() {} } foo;

#include <aviplay.h>
#include <fourcc.h>

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
     CoreSurface      *source;
} IDirectFBVideoProvider_AviFile_data;


static void IDirectFBVideoProvider_AviFile_Destruct(
                                                 IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_AviFile_data *data;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (data->player->IsPlaying())
          data->player->Stop();

     delete data->player;

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     dfb_surface_unref( data->source );

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult IDirectFBVideoProvider_AviFile_AddRef(
                                                 IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     data->ref++;

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_Release(
                                                 IDirectFBVideoProvider *thiz )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (--data->ref == 0) {
          IDirectFBVideoProvider_AviFile_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetCapabilities(
                                           IDirectFBVideoProvider       *thiz,
                                           DFBVideoProviderCapabilities *caps )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!caps)
          return DFB_INVARG;

     *caps = (DFBVideoProviderCapabilities) ( DVCAPS_BASIC |
                                              DVCAPS_SCALE |
                                              DVCAPS_SEEK );

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetSurfaceDescription(
                                                 IDirectFBVideoProvider *thiz,
                                                 DFBSurfaceDescription  *desc )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!desc)
          return DFB_INVARG;

     desc->flags = (DFBSurfaceDescriptionFlags)
          (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc->width  = data->player->GetWidth();
     desc->height = data->player->GetHeight();
     desc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_PlayTo(
                                           IDirectFBVideoProvider *thiz,
                                           IDirectFBSurface       *destination,
                                           const DFBRectangle     *dstrect,
                                           DVFrameCallback         callback,
                                           void                   *ctx )
{
     DFBRectangle           rect;
     IDirectFBSurface_data *dst_data;

     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!destination)
        return DFB_INVARG;

     dst_data = (IDirectFBSurface_data*)destination->priv;

     if (!dst_data)
          return DFB_DEAD;

     /* URGENT: keep in sync with DrawCallback */

     switch (dst_data->surface->format) {
          case DSPF_YUY2:
               data->source->format = DSPF_YUY2;
               if (data->player->SetColorSpace( fccYUY2, 0 ))
                    return DFB_UNSUPPORTED;
               break;
          case DSPF_UYVY:
               data->source->format = DSPF_UYVY;
               if (data->player->SetColorSpace( fccUYVY, 0 ))
                    return DFB_UNSUPPORTED;
               break;
          default:
               data->source->format = DSPF_RGB16;
               data->player->SetColorSpace( 0, 0 );
               break;
     }

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
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

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
     double curpos;

     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

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
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!seconds)
        return DFB_INVARG;

     *seconds = data->player->GetPos();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetLength(
     IDirectFBVideoProvider *thiz,
     double                 *seconds)
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!seconds)
        return DFB_INVARG;

     *seconds = data->player->GetVideoLength();

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_GetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!adj)
          return DFB_INVARG;

     return DFB_UNIMPLEMENTED;
}

static DFBResult IDirectFBVideoProvider_AviFile_SetColorAdjustment(
                                                  IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     INTERFACE_GET_DATA(IDirectFBVideoProvider_AviFile)

     if (!adj)
          return DFB_INVARG;

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

     data->source->front_buffer->system.addr   = (void*)(image->Data());
     data->source->front_buffer->system.pitch  = image->Bpl();


     DFBRectangle rect = { 0, 0, image->Width(), image->Height() };

     if (rect.w == data->dest_rect.w  &&  rect.h == data->dest_rect.h) {
          dfb_gfxcard_blit( &rect, data->dest_rect.x,
                            data->dest_rect.y, &data->state );
     }
     else {
          DFBRectangle drect = data->dest_rect;

          dfb_gfxcard_stretchblit( &rect, &drect, &data->state );
     }

     if (data->callback)
          data->callback( data->ctx );
}


/* exported symbols */

extern "C" {

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if (strstr( ctx->filename, ".avi" ) ||
         strstr( ctx->filename, ".AVI" ) ||
         strstr( ctx->filename, ".asf" ) ||
         strstr( ctx->filename, ".ASF" ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     DFBResult ret;

     DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_AviFile)

     data->ref = 1;

     try {
          data->player = CreateAviPlayer( filename, 16 /* FIXME */ );

          data->player->SetDrawCallback2( AviFile_DrawCallback, data );
          data->player->SetKillHandler( AviFile_KillCallback, data );
     }
     catch (FatalError e) {
          ERRORMSG( "DirectFB/AviFile: CreateAviPlayer failed: %s\n",
                    e.GetDesc() );
          DFB_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     ret = dfb_surface_create_preallocated( data->player->GetWidth(),
                                            data->player->GetHeight(),
                                            DSPF_RGB16, CSP_SYSTEMONLY,
                                            DSCAPS_SYSTEMONLY, NULL,
                                            NULL, NULL, 0, 0, &data->source );
     if (ret) {
          delete data->player;
          DFB_DEALLOCATE_INTERFACE(thiz);
          return ret;
     }

     data->state.source   = data->source;
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

