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

     void                *buffer;
     
     IFusionSoundStream  *dest;
     FSStreamDescription  desc;
} IFusionSoundMusicProvider_Vorbis_data;


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

     *caps = FMCAPS_BASIC | ((ov_seekable( &data->vf )) ? FMCAPS_SEEK : 0);

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

static void*
VorbisThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Vorbis_data *data = 
          (IFusionSoundMusicProvider_Vorbis_data*) ctx;
     
     float **src; // src[0] = first channel, src[1] = second channel, ...
     char   *dst     = data->buffer;
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
                               data->desc.buffersize, &section );

          pthread_mutex_unlock( &data->lock );
          
          if (len > 0) {
               int d_n = data->desc.channels;
               int s_n = data->info->channels;
               int i, j;
               
               switch (data->desc.sampleformat) {
                    case FSSF_U8:
                         /* Convert to unsigned 8 */
                         for (i = 0; i < s_n; i++) {
                              float *s = src[i];
                              int   *d = (int*)src[i];
                              
                              for (j = 0; j < len; j++) {
                                   int c;
                                   c    = s[j] * 128.f;
                                   c   += 128;
                                   d[j] = CLAMP( c, 0, 255 );
                              }
                         }
                         /* Copy/Interleave channels */
                         if (s_n == d_n) {
                              for (i = 0; i < s_n; i++) {
                                   int  *s = (int*)src[i];
                                   __u8 *d = (__u8*)&dst[i];

                                   for (j = 0; j < len; j++) {
                                       *d  = s[j];
                                        d += d_n;
                                   }
                              }
                         }
                         /* Upmix mono to stereo */
                         else if (s_n < d_n) {
                              int  *s = (int*)src[0];
                              __u8 *d = (__u8*)&dst[0];

                              for (i = 0; i < len; i++)
                                   d[i*2+0] = d[i*2+1] = s[i];
                         }
                         /* Downmix stereo to mono */
                         else if (s_n > d_n) {
                              int  *s0 = (int*)src[0];
                              int  *s1 = (int*)src[1];
                              __u8 *d  = (__u8*)&dst[0];

                              for (i = 0; i < len; i++)
                                   d[i] = (s0[i] + s1[i]) >> 1;
                         }
                         break;
                         
                    case FSSF_S16:
                         /* Convert to signed 16 */
                         for (i = 0; i < s_n; i++) {
                              float *s = src[i];
                              int   *d = (int*)src[i];
                              
                              for (j = 0; j < len; j++) {
                                   int c;
                                   c    = s[j] * 32768.f;
                                   d[j] = CLAMP( c, -32768, 32767 );
                              }
                         }
                         /* Copy/Interleave channels */
                         if (s_n == d_n) {
                              for (i = 0; i < s_n; i++) {
                                   int   *s = (int*)src[i];
                                   __s16 *d = (__s16*)&dst[i*2];

                                   for (j = 0; j < len; j++) {
                                       *d  = s[j];
                                        d += d_n;
                                   }
                              }
                         }
                         /* Upmix mono to stereo */
                         else if (s_n < d_n) {
                              int   *s = (int*)src[0];
                              __s16 *d = (__s16*)&dst[0];

                              for (i = 0; i < len; i++)
                                   d[i*2+0] = d[i*2+1] = s[i];
                         }
                         /* Downmix stereo to mono */
                         else if (s_n > d_n) {
                              int   *s0 = (int*)src[0];
                              int   *s1 = (int*)src[1];
                              __s16 *d  = (__s16*)&dst[0];

                              for (i = 0; i < len; i++)
                                   d[i] = (s0[i] + s1[i]) >> 1;
                         }
                         break;
                         
                    default:
                         D_BUG( "unexpected sample format" );
                         break;
               }
                                  
               data->dest->Write( data->dest, dst, len );
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
IFusionSoundMusicProvider_Vorbis_PlayTo( IFusionSoundMusicProvider *thiz,
                                         IFusionSoundStream        *destination )
{
     FSStreamDescription desc;

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
          case FSSF_S16:
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
     if (data->buffer) {
          D_FREE( data->buffer );
          data->buffer = NULL;
     }

     /* release previous destination stream */
     if (data->dest) {
          data->dest->Release( data->dest );
          data->dest = NULL;
     }

     /* allocate buffer */
     data->buffer = D_MALLOC( desc.buffersize * desc.channels *
                              ((desc.sampleformat == FSSF_U8) ? 1 : 2) );
     if (!data->buffer) {
          pthread_mutex_unlock( &data->lock );
          return D_OOM();
     }
     
     /* reference destination stream */
     destination->AddRef( destination );
     data->dest = destination;
     data->desc = desc;
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_OUTPUT, VorbisThread, data, "Vorbis" );

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
     if (data->buffer) {
          D_FREE( data->buffer );
          data->buffer = NULL;
     }

     /* release destination stream */
     if (data->dest) {
          data->dest->Release( data->dest );
          data->dest = NULL;
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

     if (data->finished)
          *seconds = ov_time_total( &data->vf, -1 );
     else
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
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Vorbis_GetStreamDescription;
     thiz->PlayTo               = IFusionSoundMusicProvider_Vorbis_PlayTo;
     thiz->Stop                 = IFusionSoundMusicProvider_Vorbis_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Vorbis_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Vorbis_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Vorbis_GetLength;

     return DFB_OK;
}

