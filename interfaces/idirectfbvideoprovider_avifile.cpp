/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.



   The contents of this software are proprietary and confidential to
   convergence integrated media GmbH. Use of this information is
   to be in accordance with the terms of the license agreement you
   entered into with convergence integrated media GmbH. Individuals
   having access to this software are responsible for maintaining the
   confidentiality of the content and for keeping the software secure
   when not in use. Transfer to any party is strictly forbidden other
   than as expressly permitted in writing by convergence integrated
   media GmbH.  Unauthorized transfer to or possession by any
   unauthorized party may be a criminal offense.

   convergence integrated media GmbH MAKES NO WARRANTIES EITHER
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
   WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE,
   OR NON-INFRINGEMENT.
   convergence integrated media GmbH SHALL NOT BE LIABLE FOR ANY
   DAMAGES SUFFERED BY LICENSEE AS A RESULT OF USING, MODIFYING OR
   DISTRIBUTING THIS SOFTWARE OR ITS DERIVATIVES.

   convergence integrated media GmbH
   Rosenthaler Str.51
   D-10178 Berlin / Germany
*/

extern "C" {
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <directfb.h>

#include <misc/util.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/layers.h>
#include <core/gfxcard.h>

#include <display/idirectfbsurface.h>
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

     free( thiz->priv );
     thiz->priv = NULL;
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

     memset( desc, 0, sizeof(DFBSurfaceDescription) );
     desc->flags = (DFBSurfaceDescriptionFlags)
                             (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_BPP);
     desc->width = data->player->Width();
     desc->height = data->player->Height();
     desc->bpp = BYTES_PER_PIXEL(layers->surface->format)*8;

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

     thiz->Stop( thiz );


     /* build the destination rectangle */
     if (dstrect) {
          if (dstrect->w < 0  ||  dstrect->h < 0)
               return DFB_INVARG;

          rect = *dstrect;

          rect.x += dst_data->req_rect.x;
          rect.y += dst_data->req_rect.y;
     }
     else
          rect = dst_data->req_rect;

     /* save for later blitting operation */
     data->dest_rect = rect;


     /* build the clip rectangle */
     if (!rectangle_intersect( &rect, &dst_data->clip_rect ))
          return DFB_INVARG;

     /* put the destination clip into the state */
     data->state.clip.x1 = rect.x;
     data->state.clip.y1 = rect.y;
     data->state.clip.x2 = rect.x + rect.w - 1;
     data->state.clip.y2 = rect.y + rect.h - 1;
     data->state.destination = dst_data->surface;
     data->state.modified = (StateModificationFlags)
                            (data->state.modified | SMF_CLIP | SMF_DESTINATION);


     destination->AddRef( destination );
     data->destination = destination;   /* FIXME: install listener */

     data->callback = callback;
     data->ctx = ctx;

     data->player->Start();

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
          data->player->Stop();

     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;     /* FIXME: remove listener */
     }

     return DFB_OK;
}

static DFBResult IDirectFBVideoProvider_AviFile_SeekTo(
                                                  IDirectFBVideoProvider *thiz,
                                                  double               seconds )
{
     IDirectFBVideoProvider_AviFile_data *data;

     if (!thiz)
        return DFB_INVARG;

     data = (IDirectFBVideoProvider_AviFile_data*)thiz->priv;

     if (!data)
          return DFB_DEAD;

     data->player->Reseek( seconds );

     return DFB_OK;
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
         strstr( filename, ".AVI" ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
     IDirectFBVideoProvider_AviFile_data *data;

     data = (IDirectFBVideoProvider_AviFile_data*)
                         malloc( sizeof(IDirectFBVideoProvider_AviFile_data) );
     memset( data, 0, sizeof(IDirectFBVideoProvider_AviFile_data) );
     thiz->priv = data;

     data->ref = 1;

     try {
          data->player = CreateAviPlayer( filename, 16 /* FIXME */ );

          data->player->SetDrawCallback2( AviFile_DrawCallback, data );
     }
     catch (FatalError e) {
          ERRORMSG( "DirectFB/AviFile: CreateAviPlayer failed: %s\n",
                    e.GetDesc() );
          free( data );
          return DFB_FAILURE;
     }


#warning If the two lines below fail to compile change them to "GetWidth, GetHeight"
     data->source.width  = data->player->Width();
     data->source.height = data->player->Height();
     data->source.format = DSPF_RGB16;
     
     data->source.front_buffer = (SurfaceBuffer*)malloc( sizeof(SurfaceBuffer) );
     memset( data->source.front_buffer, 0, sizeof(SurfaceBuffer) );
     
     data->source.front_buffer->policy = CSP_SYSTEMONLY;
     data->source.front_buffer->system.health = CSH_STORED;

     data->source.back_buffer = data->source.front_buffer;
     
     pthread_mutex_init( &data->source.front_lock, NULL );
     pthread_mutex_init( &data->source.back_lock, NULL );
     pthread_mutex_init( &data->source.listeners_mutex, NULL );
     
     data->state.source   = &data->source;
     data->state.modified = SMF_ALL;


     thiz->AddRef = IDirectFBVideoProvider_AviFile_AddRef;
     thiz->Release = IDirectFBVideoProvider_AviFile_Release;
     thiz->GetSurfaceDescription =
          IDirectFBVideoProvider_AviFile_GetSurfaceDescription;
     thiz->PlayTo = IDirectFBVideoProvider_AviFile_PlayTo;
     thiz->Stop = IDirectFBVideoProvider_AviFile_Stop;
     thiz->SeekTo = IDirectFBVideoProvider_AviFile_SeekTo;

     return DFB_OK;
}

}

