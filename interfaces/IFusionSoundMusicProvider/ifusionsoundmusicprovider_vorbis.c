/*
 * Copyright (C) 2005 Claudio Ciccani <klan@users.sf.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <vorbis/codec.h>
#include <vorbis/vorbisfile.h>

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Vorbis )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                  ref;       /* reference counter */
     
     OggVorbis_File       vf;
     vorbis_info         *info;
     
     DirectThread        *thread;
     pthread_mutex_t      lock; 
     bool                 playing;
     bool                 finished;

     void                *buf;

     struct {
          IFusionSoundStream  *stream; 
          IFusionSoundBuffer  *buffer;
          int                  format;
          int                  channels;
          int                  length;
     } dest;

     FMBufferCallback     callback;
     void                *ctx;
} IFusionSoundMusicProvider_Vorbis_data;


static __inline__ int
FtoU8( float s )
{
     int d;
     d  = s * 128.f;
     d += 128;
     return CLAMP( d, 0, 255 );
}

static __inline__ int
FtoS16( float s )
{
     int d;
     d = s * 32768.f;
     return CLAMP( d, -32768, 32767 );
}

static void
vorbis_mix_audio( float **src, char *dst, int len,
                  int format, int src_channels, int dst_channels )
{
     int s_n = src_channels;
     int d_n = dst_channels;
     int i, j;
               
     switch (format) {
          case 8:
               /* Copy/Interleave channels */
               if (s_n == d_n) {
                    for (i = 0; i < s_n; i++) {
                         float *s = src[i];
                         __u8  *d = (__u8*)&dst[i];

                         for (j = 0; j < len; j++) {
                              *d  = FtoU8(s[j]);
                               d += d_n;
                         }
                    }
               }
               /* Upmix mono to stereo */
               else if (s_n < d_n) {
                    float *s = src[0];
                    __u8  *d = (__u8*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i*2+0] = d[i*2+1] = FtoU8(s[i]);
               }
               /* Downmix stereo to mono */
               else if (s_n > d_n) {
                    float *s0 = src[0];
                    float *s1 = src[1];
                    __u8  *d  = (__u8*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i] = (FtoU8(s0[i]) + FtoU8(s1[i])) >> 1;
               }
               break;
                         
          case 16:
               /* Copy/Interleave channels */
               if (s_n == d_n) {
                    for (i = 0; i < s_n; i++) {
                         float *s = src[i];
                         __s16 *d = (__s16*)&dst[i*2];

                         for (j = 0; j < len; j++) {
                              *d  = FtoS16(s[j]);
                               d += d_n;
                         }
                    }
               }
               /* Upmix mono to stereo */
               else if (s_n < d_n) {
                    float *s = src[0];
                    __s16 *d = (__s16*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i*2+0] = d[i*2+1] = FtoS16(s[i]);
               }
               /* Downmix stereo to mono */
               else if (s_n > d_n) {
                    float *s0 = src[0];
                    float *s1 = src[1];
                    __s16 *d  = (__s16*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i] = (FtoS16(s0[i]) + FtoS16(s1[i])) >> 1;
               }
               break;
                         
          default:
               D_BUG( "unexpected sample format" );
               break;
     }
}


static void
IFusionSoundMusicProvider_Vorbis_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Vorbis_data *data =
         (IFusionSoundMusicProvider_Vorbis_data*)thiz->priv;

     thiz->Stop( thiz );
    
     ov_clear( &data->vf );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (--data->ref == 0)
          IFusionSoundMusicProvider_Vorbis_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                                  FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!caps)
          return DFB_INVARG;

     if (ov_seekable( &data->vf ))
          *caps = FMCAPS_BASIC | FMCAPS_SEEK;
     else
          *caps = FMCAPS_BASIC;

     return DFB_OK;
}

static void
vorbis_get_metadata( vorbis_info        *vi,
                     vorbis_comment     *vc, 
                     FSTrackDescription *desc )
{
     char **ptr = vc->user_comments;

     memset( desc, 0, sizeof(FSTrackDescription) );
     
     while (*ptr) {
          char *comment = *ptr;

          if (!strncasecmp( comment, "ARTIST=", sizeof("ARTIST=")-1 )) {
               strncpy( desc->artist,
                        comment + sizeof("ARTIST=") - 1,
                        FS_TRACK_DESC_ARTIST_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "TITLE=", sizeof("TITLE=")-1 )) {
               strncpy( desc->title,
                        comment + sizeof("TITLE=") - 1,
                        FS_TRACK_DESC_TITLE_LENGTH - 1 );
          } 
          else if (!strncasecmp( comment, "ALBUM=", sizeof("ALBUM=")-1 )) {
               strncpy( desc->album,
                        comment + sizeof("ALBUM=") - 1,
                        FS_TRACK_DESC_ALBUM_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "GENRE=", sizeof("GENRE=")-1 )) {
               strncpy( desc->genre,
                        comment + sizeof("GENRE=") - 1,
                        FS_TRACK_DESC_GENRE_LENGTH - 1 );
          }
          else if (!strncasecmp( comment, "DATE=", sizeof("DATE=")-1 )) {
               desc->year = strtol( comment + sizeof("DATE=") - 1, NULL, 10 );
          }

          ptr++;
     }

     snprintf( desc->encoding, 
               FS_TRACK_DESC_ENCODING_LENGTH, "Vorbis" );
     
     desc->bitrate = vi->bitrate_nominal ? : 128000;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_EnumTracks( IFusionSoundMusicProvider *thiz,
                                             FSTrackCallback            callback,
                                             void                      *callbackdata )
{
     FSTrackDescription desc;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!callback)
          return DFB_INVARG;

     vorbis_get_metadata( data->info, ov_comment( &data->vf, -1 ), &desc );
     callback( 0, desc, callbackdata );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetTrackID( IFusionSoundMusicProvider *thiz,
                                             FSTrackID                 *ret_track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!ret_track_id)
          return DFB_INVARG;

     *ret_track_id = 0;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                      FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     vorbis_get_metadata( data->info, ov_comment( &data->vf, -1 ), desc );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                       FSStreamDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = data->info->rate;
     desc->channels     = data->info->channels;
     desc->sampleformat = FSSF_S16;
     desc->buffersize   = (data->info->rate / data->info->channels) >> 1;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                       FSBufferDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSBDF_SAMPLERATE   | FSBDF_CHANNELS  |
                          FSBDF_SAMPLEFORMAT | FSBDF_LENGTH;
     desc->samplerate   = data->info->rate;
     desc->channels     = data->info->channels;
     desc->sampleformat = FSSF_S16;
     desc->length       = (data->info->rate / data->info->channels) >> 1;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_SelectTrack( IFusionSoundMusicProvider *thiz,
                                              FSTrackID                  track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (track_id != 0)
          return DFB_INVARG;

     return DFB_OK;
}

static void*
VorbisStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Vorbis_data *data = 
          (IFusionSoundMusicProvider_Vorbis_data*) ctx;
     
     float **src; // src[0] = first channel, src[1] = second channel, ...
     char   *dst     = data->buf;
     int     section = 0; 

     data->finished = false;
     
     while (data->playing) {
          long len;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          len = ov_read_float( &data->vf, &src,
                               data->dest.length, &section );

          pthread_mutex_unlock( &data->lock );
          
          if (len > 0) {
               vorbis_mix_audio( src, dst, len, data->dest.format,
                                 data->info->channels, data->dest.channels );
                                  
               data->dest.stream->Write( data->dest.stream, dst, len );
          }
          else if (len == 0) {
               D_DEBUG( "IFusionSoundMusicProvider_Vorbis: End of stream.\n" );
               data->finished = true;
               break;
          }
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_PlayToStream( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundStream        *destination )
{
     FSStreamDescription desc;
     int                 dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->info->rate)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels != data->info->channels &&
        (desc.channels > 2 || data->info->channels > 2))
          return DFB_UNSUPPORTED;
     
     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
               dst_format = 8;
               break;
          case FSSF_S16:
               dst_format = 16;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     /* release buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }

     /* allocate buffer */
     data->buf = D_MALLOC( desc.buffersize * desc.channels * dst_format >> 3 );
     if (!data->buf) {
          pthread_mutex_unlock( &data->lock );
          return D_OOM();
     }
     
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.stream   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.buffersize;
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_OUTPUT, 
                                           VorbisStreamThread, data, "Vorbis" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
VorbisBufferThread( DirectThread *thread, void *ctx )
{ 
     IFusionSoundMusicProvider_Vorbis_data *data = 
          (IFusionSoundMusicProvider_Vorbis_data*) ctx;
     
     IFusionSoundBuffer *buffer  = data->dest.buffer;
     int                 section = 0;

     data->finished = false;
     
     while (data->playing && !data->finished) {
          float **src;
          char   *dst;
          long    len;
          int     pos = 0;
          
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          if (buffer->Lock( buffer, (void*)&dst ) != DFB_OK) {
               D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                        "Couldn't lock buffer!\n" );
               pthread_mutex_unlock( &data->lock );
               data->finished = true;
               break;
          }

          do {
               len = ov_read_float( &data->vf, &src,
                                    data->dest.length - pos, &section );

               if (len > 0) {
                    vorbis_mix_audio( src, dst, len, data->dest.format,
                                      data->info->channels, data->dest.channels );
               
                    pos += len;
                    dst += len * data->dest.channels * data->dest.format >> 3;     
               }
               else if (len == 0) {
                    D_DEBUG( "IFusionSoundMusicProvider_Vorbis: "
                             "End of stream.\n" );
                    data->finished = true;
                    break;
               }
          } while (pos < data->dest.length);

          buffer->Unlock( buffer );

          pthread_mutex_unlock( &data->lock );

          if (data->callback && pos)
               data->callback( pos, data->ctx );
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                               IFusionSoundBuffer        *destination,
                                               FMBufferCallback           callback,
                                               void                      *ctx )
{
     FSBufferDescription desc;
     int                 dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->info->rate)
          return DFB_UNSUPPORTED;
     
     /* check if number of channels is supported */
     if (desc.channels != data->info->channels &&
        (desc.channels > 2 || data->info->channels > 2))
          return DFB_UNSUPPORTED;
     
     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_U8:
               dst_format = 8;
               break;
          case FSSF_S16:
               dst_format = 16;
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     /* release buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }
     
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest.buffer   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.length;

     /* register new callback */
     data->callback = callback;
     data->ctx      = ctx;
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_OUTPUT,
                                           VorbisBufferThread, data, "Vorbis" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Vorbis_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = false;
          pthread_mutex_unlock( &data->lock );
          direct_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          direct_thread_destroy( data->thread );
          data->thread = NULL;
     }

     /* release buffer */
     if (data->buf) {
          D_FREE( data->buf );
          data->buf = NULL;
     }

     /* release previous destination stream */
     if (data->dest.stream) {
          data->dest.stream->Release( data->dest.stream );
          data->dest.stream = NULL;
     }

     /* release previous destination buffer */
     if (data->dest.buffer) {
          data->dest.buffer->Release( data->dest.buffer );
          data->dest.buffer = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_Vorbis_SeekTo( IFusionSoundMusicProvider *thiz,
                                         double                     seconds )
{
     int ret;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     pthread_mutex_lock( &data->lock );
     ret = ov_time_seek( &data->vf, seconds );
     pthread_mutex_unlock( &data->lock );
     
     return (ret == 0) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult 
IFusionSoundMusicProvider_Vorbis_GetPos( IFusionSoundMusicProvider *thiz,
                                         double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )

     if (!seconds)
          return DFB_INVARG;

     if (data->finished) {
          *seconds = ov_time_total( &data->vf, -1 );
          return DFB_EOF;
     }
          
     *seconds = ov_time_tell( &data->vf );
          
     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_Vorbis_GetLength( IFusionSoundMusicProvider *thiz,
                                            double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Vorbis )
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = ov_time_total( &data->vf, -1 );

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     FILE           *fp;
     OggVorbis_File  vf;
     long            ret;

     fp = fopen( ctx->filename, "rb" );
     if (!fp)
          return DFB_UNSUPPORTED;
          
     ret = ov_test( fp, &vf, NULL, 0 );
     ov_clear( &vf );

     return (ret == 0) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     FILE *fp;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Vorbis )

     data->ref = 1;
     
     fp = fopen( filename, "rb" );
     if (!fp)
          return DFB_IO;
          
     if (ov_open( fp, &data->vf, NULL, 0 ) < 0) {
          D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                   "Error opening stream!\n" );
          fclose( fp );
          return DFB_FAILURE;
     }
     
     data->info = ov_info( &data->vf, -1 );
     if (!data->info) {
          D_ERROR( "IFusionSoundMusicProvider_Vorbis: "
                   "Error getting stream informations!\n" );
          ov_clear( &data->vf );
          return DFB_FAILURE;
     }
     
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Vorbis_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Vorbis_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Vorbis_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_Vorbis_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_Vorbis_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Vorbis_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Vorbis_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Vorbis_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_Vorbis_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Vorbis_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Vorbis_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Vorbis_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Vorbis_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Vorbis_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Vorbis_GetLength;

     return DFB_OK;
}

