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

#include <stdio.h>
#include <string.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <core/coredefs.h>

#include <direct/mem.h>
#include <direct/thread.h>
#include <direct/util.h>

#include "config.h"

#include "timidity/timidity.h"

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Timidity )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int              ref;       /* reference counter */
     char            *filename;

     void            *buf;
     int              playing;
     DirectThread    *thread;
     pthread_mutex_t  lock;

     int              length;

     MidiSong        *song;

     IFusionSoundStream *stream;
     IFusionSoundBuffer *buffer;

     FMBufferCallback    callback;
     void               *ctx;
} IFusionSoundMusicProvider_Timidity_data;

/* interface implementation */

static void
IFusionSoundMusicProvider_Timidity_Destruct( IFusionSoundMusicProvider *thiz )
{
    IFusionSoundMusicProvider_Timidity_data *data =
         (IFusionSoundMusicProvider_Timidity_data*)thiz->priv;

    thiz->Stop( thiz );

    D_FREE( data->filename );

    pthread_mutex_destroy( &data->lock );

    DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Timidity_AddRef( IFusionSoundMusicProvider *thiz )
{
    DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    data->ref++;

    return DFB_OK;
}


static DFBResult
IFusionSoundMusicProvider_Timidity_Release( IFusionSoundMusicProvider *thiz )
{
    DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    if (--data->ref == 0) {
        IFusionSoundMusicProvider_Timidity_Destruct( thiz );
    }

    return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetCapabilities(
                                            IFusionSoundMusicProvider   *thiz,
                                            FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!caps)
          return DFB_INVARG;

     *caps = FMCAPS_BASIC | FMCAPS_RESAMPLE;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_EnumTracks(
                                        IFusionSoundMusicProvider *thiz,
                                        FSTrackCallback            callback,
                                        void                      *callbackdata )
{
     FSTrackDescription desc;
     
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!callback)
          return DFB_INVARG;

     memset( &desc, 0, sizeof(FSTrackDescription) );
     snprintf( desc.encoding,
               FS_TRACK_DESC_ENCODING_LENGTH, "Midi" );

     callback( 0, desc, callbackdata );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetTrackID( 
                                        IFusionSoundMusicProvider *thiz,
                                        FSTrackID                 *ret_track_id )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!ret_track_id)
          return DFB_INVARG;

     *ret_track_id = 0;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetTrackDescription(
                                              IFusionSoundMusicProvider *thiz,
                                              FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!desc)
          return DFB_INVARG;

     memset( desc, 0, sizeof(FSTrackDescription) );
     snprintf( desc->encoding,
               FS_TRACK_DESC_ENCODING_LENGTH, "Midi" );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetStreamDescription(
                                              IFusionSoundMusicProvider *thiz,
                                              FSStreamDescription       *desc )
{
    DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    if (!desc)
        return DFB_INVARG;

    desc->flags        = FSSDF_SAMPLERATE | FSSDF_CHANNELS |
                         FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
    desc->samplerate   = 44100;
    desc->channels     = 1;
    desc->sampleformat = FSSF_S16;
    desc->buffersize   = 11025;

    return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetBufferDescription(
                                              IFusionSoundMusicProvider *thiz,
                                              FSBufferDescription       *desc )
{
    DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    if (!desc)
        return DFB_INVARG;

    desc->flags        = FSBDF_SAMPLERATE | FSBDF_CHANNELS |
                         FSBDF_SAMPLEFORMAT | FSBDF_LENGTH;
    desc->samplerate   = 44100;
    desc->channels     = 1;
    desc->sampleformat = FSSF_S16;
    desc->length       = 11025;

    return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_SelectTrack( 
                                        IFusionSoundMusicProvider *thiz,
                                        FSTrackID                  track_id )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (track_id != 0)
          return DFB_INVARG;

     return DFB_OK;
}

static void*
TimidityStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Timidity_data *data =
          (IFusionSoundMusicProvider_Timidity_data*) ctx;

     while (data->playing) {
          pthread_mutex_lock( &data->lock );

          if (!data->playing || !Timidity_Active()) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          Timidity_PlaySome( data->buf, data->length );

          pthread_mutex_unlock( &data->lock );

          data->stream->Write( data->stream, data->buf, data->length );
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_PlayToStream(
                                       IFusionSoundMusicProvider *thiz,
                                       IFusionSoundStream        *destination )
{
     FSStreamDescription desc;

     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_S16:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = 0;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;

          Timidity_Stop();
          Timidity_FreeSong( data->song );
     }

     /* free buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }

     /* release previous destination buffer */
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }

     data->length = desc.buffersize / 2;

     if (Timidity_Init( desc.samplerate,
                        desc.channels,
                        data->length )) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }

     /* load song */
     data->song = Timidity_LoadSong( data->filename );
     if (!data->song) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     Timidity_Start( data->song );

     /* reference destination stream */
     destination->AddRef( destination );
     data->stream = destination;

     data->playing = 1;
     data->buf     = D_MALLOC( data->length * desc.channels * 2 );

     /* start thread */
     data->thread = direct_thread_create( DTT_DEFAULT,
                                          TimidityStreamThread, data, "Timidity" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
TimidityBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Timidity_data *data =
          (IFusionSoundMusicProvider_Timidity_data*) ctx;

     while (data->playing) {
          void *buf;

          if (data->buffer->Lock( data->buffer, &buf ) != DFB_OK)
               break;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing || !Timidity_Active()) {
               pthread_mutex_unlock( &data->lock );
               data->buffer->Unlock( data->buffer );
               break;
          }

          Timidity_PlaySome( buf, data->length );

          pthread_mutex_unlock( &data->lock );

          data->buffer->Unlock( data->buffer );

          if (data->callback)
               data->callback( data->length, data->ctx );
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_PlayToBuffer(
                                       IFusionSoundMusicProvider *thiz,
                                       IFusionSoundBuffer        *destination,
                                       FMBufferCallback           callback, 
                                       void                      *ctx )
{
     FSBufferDescription desc;

     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_S16:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = 0;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;

          Timidity_Stop();
          Timidity_FreeSong( data->song );
     }

     /* free buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }

     /* release previous destination buffer */
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }

     data->length = desc.length / 2;

     if (Timidity_Init( desc.samplerate,
                        desc.channels,
                        data->length )) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }

     /* load song */
     data->song = Timidity_LoadSong( data->filename );
     if (!data->song) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     Timidity_Start( data->song );

     /* reference destination buffer */
     destination->AddRef( destination );
     data->buffer = destination;

     data->callback = callback;
     data->ctx      = ctx;

     data->playing = 1;
     
     /* start thread */
     data->thread = direct_thread_create( DTT_DEFAULT,
                                          TimidityBufferThread, data, "Timidity" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}


static DFBResult
IFusionSoundMusicProvider_Timidity_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = 0;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;

          Timidity_Stop();
          Timidity_FreeSong( data->song );
     }

     /* free buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->stream) {
          data->stream->Release( data->stream );
          data->stream = NULL;
     }

     /* release previous destination buffer */
     if (data->buffer) {
          data->buffer->Release( data->buffer );
          data->buffer = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}


static DFBResult IFusionSoundMusicProvider_Timidity_SeekTo(
                                           IFusionSoundMusicProvider *thiz,
                                           double                     seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     return DFB_UNIMPLEMENTED;
}


static DFBResult IFusionSoundMusicProvider_Timidity_GetPos(
                                           IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!Timidity_Active())
          return DFB_EOF;
     
     return DFB_UNIMPLEMENTED;
}


static DFBResult IFusionSoundMusicProvider_Timidity_GetLength(
                                           IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     return DFB_UNIMPLEMENTED;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     char buf[5];
     FILE *file;

     file = fopen( ctx->filename, "rb" );
     if (!file)
          return DFB_UNSUPPORTED;

     memset( buf, 0, 5 );
     if (fread( buf, 1, 4, file ) != 4) {
          fclose( file );
          return DFB_UNSUPPORTED;
     }
     if (!strstr( buf, "MThd" )) {
          fclose( file );
          return DFB_UNSUPPORTED;
     }

     fclose( file );

     return DFB_OK;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundMusicProvider_Timidity)

     /* initialize private data */
     data->ref = 1;
     data->filename = D_STRDUP( filename );
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Timidity_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Timidity_Release;
     thiz->GetCapabilities      =
          IFusionSoundMusicProvider_Timidity_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_Timidity_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_Timidity_GetTrackID;
     thiz->GetTrackDescription  = 
          IFusionSoundMusicProvider_Timidity_GetTrackDescription;
     thiz->GetStreamDescription =
          IFusionSoundMusicProvider_Timidity_GetStreamDescription;
     thiz->GetBufferDescription = 
          IFusionSoundMusicProvider_Timidity_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_Timidity_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Timidity_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Timidity_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Timidity_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Timidity_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Timidity_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Timidity_GetLength;

     return DFB_OK;
}
