/*
   (c) Copyright 2001  Kim JeongHoe <king@mizi.com>
   All rights reserved.

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

#include <directfb.h>

#include "misc/util.h"

#include "core/core.h"
#include "core/coredefs.h"
#include "core/layers.h"
#include "core/gfxcard.h"

#include "display/idirectfbsurface.h"

#include <libmpeg3.h>


/*
 * private data struct of IDirectFBVideoProvider
 */
typedef struct {
    int               ref;       /* reference counter */
    mpeg3_t          *stream;
    __u8            **scans;

    pthread_t         thread;

    IDirectFBSurface *destination;
    DFBRectangle      dest_rect;
    DVFrameCallback   callback;
    void             *ctx;

    CardState            state;
    CoreSurface         *source;
} IDirectFBVideoProvider_Libmpeg3_data;


static void
IDirectFBVideoProvider_Libmpeg3_Destruct( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;
     
    if (data->thread != -1)
    {
        pthread_cancel( data->thread );
        pthread_join( data->thread, NULL );
        data->thread = -1;
    }

    mpeg3_close( data->stream );
    free( data->scans );

    surface_destroy( data->source );
    
    free( thiz->priv );
    thiz->priv = NULL;

#ifndef DFB_DEBUG
    free( thiz );
#endif
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_AddRef( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    data->ref++;

    return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_Libmpeg3_Release( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    if (--data->ref == 0) {
        IDirectFBVideoProvider_Libmpeg3_Destruct( thiz );
    }

    return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Libmpeg3_GetSurfaceDescription(
    IDirectFBVideoProvider *thiz, DFBSurfaceDescription  *desc )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    if (!thiz || !desc)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data *) thiz->priv;

    if (!data)
        return DFB_DEAD;

    memset( desc, 0, sizeof(DFBSurfaceDescription) );
    desc->flags = (DFBSurfaceDescriptionFlags)
        (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);

    desc->width       = mpeg3_video_width( data->stream, 0 );
    desc->height      = mpeg3_video_height( data->stream, 0 );
    desc->pixelformat = layers->surface->format;

    return DFB_OK;
}


static void*
MpegThread( void *ctx )
{
    IDirectFBVideoProvider_Libmpeg3_data *data =
        (IDirectFBVideoProvider_Libmpeg3_data*) ctx;
    struct timeval start, now, after;
    long frame_delay;
    long delay = -1;
    double rate;
    DFBRectangle rect, drect, srect;
    int drop = 0;
    long frame;

    rate = mpeg3_frame_rate( data->stream, 0 ) / 1000.0;
    frame_delay = (long) (1000 / mpeg3_frame_rate( data->stream, 0 ));
    rect.x = 0;
    rect.y = 0;
    rect.w = mpeg3_video_width( data->stream, 0 );
    rect.h = mpeg3_video_height( data->stream, 0 );

    gettimeofday(&start, 0);

    while ( 1 ) {
        pthread_testcancel();
        gettimeofday (&now, 0);
        if ( drop )
        {
            mpeg3_drop_frames( data->stream, drop, 0 );
            drop = 0;
        }

        if ( mpeg3_read_frame( data->stream, data->scans, 0, 0, 
                               rect.w, rect.h, rect.w, rect.h,
                               MPEG3_RGB565, 0 ) )
            break;

        drect = data->dest_rect;
        if (drect.w != rect.w  ||  drect.h != rect.h)
            gfxcard_stretchblit( &rect, &drect, &data->state );
        else {
            srect = rect;
            gfxcard_blit( &srect, drect.x, drect.y, &data->state );
        }

        if (data->callback)
            data->callback (data->ctx);

        frame = mpeg3_get_frame( data->stream, 0 );
        gettimeofday (&after, 0);
        delay = (after.tv_sec - start.tv_sec) * 1000 +
            (after.tv_usec - start.tv_usec) / 1000;
        {
            long cframe = (long) (delay * rate);

            if ( frame < cframe )
            {
                drop = 1;
                continue;
            }
            else if ( frame == cframe )
                delay = ((long) ((frame + 1) / rate)) - delay;
            else
                delay = frame_delay;
        }
        after.tv_sec = 0;
        after.tv_usec = delay * 1000;
        select( 0, 0, 0, 0, &after );
    }

    return NULL;
}


static DFBResult
IDirectFBVideoProvider_Libmpeg3_PlayTo( IDirectFBVideoProvider *thiz,
                                        IDirectFBSurface       *destination,
                                        DFBRectangle           *dstrect,
                                        DVFrameCallback         callback,
                                        void                   *ctx )
{
    DFBRectangle                                rect;
    IDirectFBVideoProvider_Libmpeg3_data       *data;
    IDirectFBSurface_data                      *dst_data;

    if (!thiz || !destination)
        return DFB_INVARG;
     
    data = (IDirectFBVideoProvider_Libmpeg3_data *) thiz->priv;
    dst_data = (IDirectFBSurface_data*)destination->priv;

    if (!data || !dst_data)
        return DFB_DEAD;

        /* build the destination rectangle */
    if (dstrect) {
        if (dstrect->w < 1  ||  dstrect->h < 1)
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

    if (data->destination) {
        data->destination->Release( data->destination );
        data->destination = NULL;     /* FIXME: remove listener */
    }
     
    destination->AddRef( destination );
    data->destination = destination;   /* FIXME: install listener */

    data->callback = callback;
    data->ctx = ctx;

    if (data->thread == -1)
        pthread_create( &data->thread, NULL, MpegThread, data );
    
     
    return DFB_OK;
}


static DFBResult
IDirectFBVideoProvider_Libmpeg3_Stop( IDirectFBVideoProvider *thiz )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    if (data->thread != -1)
    {
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


static DFBResult IDirectFBVideoProvider_Libmpeg3_SeekTo(
                                                  IDirectFBVideoProvider *thiz,
                                                  double               seconds )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;
    double rate;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    rate = mpeg3_frame_rate( data->stream, 0 );
    mpeg3_set_frame( data->stream, (long) (seconds * rate), 0 );

    return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Libmpeg3_GetPos(
                                                  IDirectFBVideoProvider *thiz,
                                                  double              *seconds )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;
    double rate;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    rate = mpeg3_frame_rate( data->stream, 0 );
    *seconds = mpeg3_get_frame( data->stream, 0 ) / rate;

    return DFB_OK;
}


static DFBResult IDirectFBVideoProvider_Libmpeg3_GetLength(
                                                  IDirectFBVideoProvider *thiz,
                                                  double              *seconds )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;
    double rate;
    long frames;

    if (!thiz)
        return DFB_INVARG;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)thiz->priv;

    if (!data)
        return DFB_DEAD;

    frames = mpeg3_video_frames( data->stream, 0 );
    if (frames <= 1)
         return DFB_UNSUPPORTED;
    
    rate = mpeg3_frame_rate( data->stream, 0 );
    *seconds = frames / rate;
    
    return DFB_OK;
}





/* exported symbols */

const char *get_type()
{
     return "IDirectFBVideoProvider";
}

const char *get_implementation()
{
     return "Libmpeg3";
}

DFBResult Probe( const char *filename )
{
    if ( mpeg3_check_sig( (char *) filename) )
        return DFB_OK;
     
    return DFB_UNSUPPORTED;
}

DFBResult Construct( IDirectFBVideoProvider *thiz, const char *filename )
{
    IDirectFBVideoProvider_Libmpeg3_data *data;
    int height, width;

    data = (IDirectFBVideoProvider_Libmpeg3_data*)
        malloc( sizeof(IDirectFBVideoProvider_Libmpeg3_data) );
    memset( data, 0, sizeof(IDirectFBVideoProvider_Libmpeg3_data) );
    thiz->priv = data;

    data->ref = 1;

    data->stream = mpeg3_open( (char *) filename);
    if ( data->stream == NULL )
    {
        free( data );
        return DFB_FAILURE;
    }

    if ( !mpeg3_has_video( data->stream ) )
    {
        free( data );
        return DFB_FAILURE;
    }

    mpeg3_set_mmx( data->stream, 1 );

    width = mpeg3_video_width( data->stream, 0 );
    height = mpeg3_video_height( data->stream, 0 );
    surface_create( width, height, DSPF_RGB16, CSP_SYSTEMONLY,
                    DSCAPS_SYSTEMONLY, &(data->source));

    data->state.source   = data->source;
    data->state.modified = SMF_ALL;

    {
        int i, pitch;
        unsigned char * buf = data->source->back_buffer->system.addr;
 
        pitch = data->source->back_buffer->system.pitch;
        data->scans = (unsigned char **) malloc( sizeof(unsigned char *) * height );

        for ( i = 0; i < height; i ++ )
        {
            data->scans[i] = buf;
            buf += pitch;
        }
    }

/*     
    pthread_mutex_init( &data->source.front_lock, NULL );
    pthread_mutex_init( &data->source.back_lock, NULL );
    pthread_mutex_init( &data->source.listeners_mutex, NULL );
*/   
    data->thread = -1;

    thiz->AddRef = IDirectFBVideoProvider_Libmpeg3_AddRef;
    thiz->Release = IDirectFBVideoProvider_Libmpeg3_Release;
    thiz->GetSurfaceDescription =
        IDirectFBVideoProvider_Libmpeg3_GetSurfaceDescription;
    thiz->PlayTo = IDirectFBVideoProvider_Libmpeg3_PlayTo;
    thiz->Stop = IDirectFBVideoProvider_Libmpeg3_Stop;
    thiz->SeekTo = IDirectFBVideoProvider_Libmpeg3_SeekTo;
    thiz->GetPos = IDirectFBVideoProvider_Libmpeg3_GetPos;
    thiz->GetLength = IDirectFBVideoProvider_Libmpeg3_GetLength;

    return DFB_OK;
}
