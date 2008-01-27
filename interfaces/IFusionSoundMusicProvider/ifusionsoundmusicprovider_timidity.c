/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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
#include <string.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/mem.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <timidity.h>

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename,
           DirectStream              *stream );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Timidity )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                           ref;       /* reference counter */
     
     DirectStream                 *st;

     MidSong                      *song;
     unsigned int                  song_pos;
     unsigned int                  song_length;
     
     void                         *buf;
     
     DirectThread                 *thread;
     pthread_mutex_t               lock;
     int                           seeked;
     
     FSMusicProviderStatus         status;
     FSMusicProviderPlaybackFlags  flags;

     IFusionSoundStream           *stream;
     IFusionSoundBuffer           *buffer;

     FMBufferCallback              callback;
     void                         *ctx;
} IFusionSoundMusicProvider_Timidity_data;

static int             timidity_refs = 0;
static pthread_mutex_t timidity_lock = PTHREAD_MUTEX_INITIALIZER;

/* MidIStream callbacks */

static size_t
read_callback( void *ctx, void *ptr, size_t size, size_t num )
{
     IFusionSoundMusicProvider_Timidity_data *data = ctx;
     size_t                                   pos  = 0;
     
     num *= size;
     while (pos < num) {
          unsigned int len = 0;
          direct_stream_wait( data->st, num-pos, NULL );
          if (direct_stream_read( data->st, num-pos, ptr+pos, &len ))
               break;
          pos += len;
     }
     
     return pos/size;
}

static int
close_callback( void *ctx )
{
     return 0;
}

/* interface implementation */

static void
IFusionSoundMusicProvider_Timidity_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Timidity_data *data =
         (IFusionSoundMusicProvider_Timidity_data*)thiz->priv;

     thiz->Stop( thiz );

     if (data->st)
          direct_stream_destroy( data->st );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
     
     pthread_mutex_lock( &timidity_lock );
     if (--timidity_refs == 0)
          mid_exit();
     pthread_mutex_unlock( &timidity_lock );
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

     if (--data->ref == 0)
          IFusionSoundMusicProvider_Timidity_Destruct( thiz );

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

     *caps = FMCAPS_BASIC | FMCAPS_SEEK | FMCAPS_RESAMPLE;

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
     snprintf( desc->encoding, FS_TRACK_DESC_ENCODING_LENGTH, "MIDI" );

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
    desc->length       = (data->song_length) ? (data->song_length*441/10) : 44100;

    return DFB_OK;
}

static void*
TimidityStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Timidity_data *data = ctx;
     FSStreamDescription                      dsc;
     
     data->stream->GetDescription( data->stream, &dsc );

     while (data->status == FMSTATE_PLAY) {
          size_t size;
          
          pthread_mutex_lock( &data->lock );

          if (data->status != FMSTATE_PLAY) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               mid_song_seek( data->song, data->song_pos );
               data->stream->Flush( data->stream );
               data->seeked = false;
          }
          
          data->song_pos = mid_song_get_time( data->song );
          
          size = mid_song_read_wave( data->song, data->buf, dsc.buffersize );
          if (!size) {
               if (data->flags & FMPLAY_LOOPING)
                    mid_song_start( data->song );
               else
                    data->status = FMSTATE_FINISHED;
          }
          
          pthread_mutex_unlock( &data->lock );
          
          data->stream->Write( data->stream, data->buf,
                               size/(FS_BYTES_PER_SAMPLE(dsc.sampleformat)*dsc.channels) );
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_PlayToStream(
                                       IFusionSoundMusicProvider *thiz,
                                       IFusionSoundStream        *destination )
{
     FSStreamDescription  desc;
     MidIStream          *stream;
     MidSongOptions       options;

     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     thiz->Stop( thiz );

     pthread_mutex_lock( &data->lock );
     
     direct_stream_seek( data->st, 0 );
     stream = mid_istream_open_callbacks( read_callback, close_callback, data );
     if (!stream) {
          D_ERROR( "IFusionSoundMusicProvider_Timidity: couldn't open input stream!\n" );
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     
     options.rate = desc.samplerate;
     options.format = (desc.sampleformat == FSSF_S16) ? MID_AUDIO_S16 : MID_AUDIO_U8;
     options.channels = desc.channels;
     options.buffer_size = desc.buffersize;
     
     data->song = mid_song_load( stream, &options );
     
     mid_istream_close( stream );
     
     if (!data->song) {
          D_ERROR( "IFusionSoundMusicProvider_Timidity: couldn't load song!\n" );
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     
     data->song_length = mid_song_get_total_time( data->song );
     
     mid_song_start( data->song );
     
     if (data->status == FMSTATE_FINISHED && !data->seeked)
          data->song_pos = 0;
     mid_song_seek( data->song, data->song_pos );

     /* reference destination stream */
     destination->AddRef( destination );
     data->stream = destination;
     
     data->buf = D_MALLOC( desc.buffersize*desc.channels*FS_BYTES_PER_SAMPLE(desc.sampleformat) );
     
     data->status = FMSTATE_PLAY;

     /* start thread */
     data->thread = direct_thread_create( DTT_DEFAULT,
                                          TimidityStreamThread, data, "Timidity" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
TimidityBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Timidity_data *data = ctx;
     FSBufferDescription                      dsc;
     
     data->buffer->GetDescription( data->buffer, &dsc );

     while (data->status == FMSTATE_PLAY) {
          void  *ptr;
          int    len;
          size_t size;
          
          pthread_mutex_lock( &data->lock );

          if (data->status != FMSTATE_PLAY) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->seeked) {
               mid_song_seek( data->song, data->song_pos );
               data->stream->Flush( data->stream );
               data->seeked = false;
          }

          data->song_pos = mid_song_get_time( data->song );
          
          if (data->buffer->Lock( data->buffer, &ptr, &len, 0 ) != DFB_OK) {
               D_ERROR( "IFusionSoundMusicProvider_Timidity: couldn't lock buffer!\n" );
               data->status = FMSTATE_FINISHED;
               pthread_mutex_unlock( &data->lock );
               break;
          }

          size = mid_song_read_wave( data->song, ptr, len );
          if (!size) {
               if (data->flags & FMPLAY_LOOPING)
                    mid_song_start( data->song );
               else
                    data->status = FMSTATE_FINISHED;
          }
          
          data->buffer->Unlock( data->buffer );

          pthread_mutex_unlock( &data->lock );

          if (data->callback && size) {
               size /= FS_BYTES_PER_SAMPLE(dsc.sampleformat) * dsc.channels;
               if (data->callback( size, data->ctx ))
                    data->status = FMSTATE_STOP;
          }
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
     FSBufferDescription  desc;
     MidIStream          *stream;
     MidSongOptions       options;

     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     if (desc.channels > 2)
          return DFB_UNSUPPORTED;

     switch (desc.sampleformat) {
          case FSSF_U8:
          case FSSF_S16:
               break;
          default:
               return DFB_UNSUPPORTED;
     }
     
     thiz->Stop( thiz );

     pthread_mutex_lock( &data->lock );

     direct_stream_seek( data->st, 0 );
     stream = mid_istream_open_callbacks( read_callback, close_callback, data );
     if (!stream) {
          D_ERROR( "IFusionSoundMusicProvider_Timidity: couldn't open input stream!\n" );
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     
     options.rate = desc.samplerate;
     options.format = (desc.sampleformat == FSSF_S16) ? MID_AUDIO_S16 : MID_AUDIO_U8;
     options.channels = desc.channels;
     options.buffer_size = desc.length;
     
     data->song = mid_song_load( stream, &options );
     
     mid_istream_close( stream );
     
     if (!data->song) {
          D_ERROR( "IFusionSoundMusicProvider_Timidity: couldn't load song!\n" );
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     
     data->song_length = mid_song_get_total_time( data->song );   
     
     mid_song_start( data->song );
     
     if (data->status == FMSTATE_FINISHED && !data->seeked)
          data->song_pos = 0;
     mid_song_seek( data->song, data->song_pos );

     /* reference destination buffer */
     destination->AddRef( destination );
     data->buffer = destination;

     data->callback = callback;
     data->ctx      = ctx;

     data->status = FMSTATE_PLAY;
     
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
     
     data->status = FMSTATE_STOP;

     /* stop thread */
     if (data->thread) {
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }
     
     /* free song */
     if (data->song) {
          mid_song_free( data->song );
          data->song = NULL;
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

static DFBResult
IFusionSoundMusicProvider_Timidity_GetStatus( IFusionSoundMusicProvider *thiz,
                                              FSMusicProviderStatus     *status )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)
     
     if (!status)
          return DFB_INVARG;
          
     *status = data->status;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_SeekTo( IFusionSoundMusicProvider *thiz,
                                           double                     seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)
     
     if (seconds < 0.0)
          return DFB_INVARG;
          
     pthread_mutex_lock( &data->lock );
     data->song_pos = seconds * 1000.0;
     data->seeked   = true;
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetPos( IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = (double)data->song_pos / 1000.0;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetLength( IFusionSoundMusicProvider *thiz,
                                              double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = (double)data->song_length / 1000.0;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_SetPlaybackFlags(
                                             IFusionSoundMusicProvider    *thiz,
                                             FSMusicProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)
     
     if (flags & ~FMPLAY_LOOPING)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;
     
     return DFB_OK;
}


/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header, "MThd", 4 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, 
           const char                *filename,
           DirectStream              *stream )
{
     pthread_mutex_lock( &timidity_lock );
     if (timidity_refs == 0) {
          if (mid_init( NULL ) < 0) {
               D_ERROR( "IFsusionSoundMusicProvider_Timidity: couldn't initialize TiMidity!\n" );
               pthread_mutex_unlock( &timidity_lock );
               return DFB_INIT;
          }
     }
     timidity_refs++;
     pthread_mutex_unlock( &timidity_lock );
     
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundMusicProvider_Timidity)

     /* initialize private data */
     data->ref = 1;
     data->st = direct_stream_dup( stream );
     data->status = FMSTATE_STOP;
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Timidity_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Timidity_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Timidity_GetCapabilities;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Timidity_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Timidity_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Timidity_GetBufferDescription;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Timidity_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Timidity_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Timidity_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_Timidity_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_Timidity_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Timidity_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Timidity_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_Timidity_SetPlaybackFlags;

     return DFB_OK;
}
