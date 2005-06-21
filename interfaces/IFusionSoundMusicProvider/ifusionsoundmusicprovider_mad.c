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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <direct/types.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <mad.h>


static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Mad )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int                       ref;       /* reference counter */
     
     int                       fd;

     struct mad_synth          synth;
     struct mad_stream         stream;
     struct mad_frame          frame;   
     
     long                      size;
     double                    length;
     int                       samplerate;
     int                       channels;
     FSTrackDescription        desc;
     
     DirectThread             *thread;
     pthread_mutex_t           lock; 
     bool                      playing;
     bool                      finished;

     struct {
          IFusionSoundStream  *stream; 
          IFusionSoundBuffer  *buffer;
          int                  format;
          int                  channels;
          int                  length;
     } dest;

     FMBufferCallback          callback;
     void                     *ctx;
} IFusionSoundMusicProvider_Mad_data;

#define INPUT_BUFFER_SIZE  8192

#define XING_MAGIC         (('X' << 24) | ('i' << 16) | ('n' << 8) | 'g')


struct id3_tag {
     __s8 tag[3];
     __s8 title[30];
     __s8 artist[30];
     __s8 album[30];
     __s8 year[4];
     __s8 comment[30];
     __u8 genre;
};

static const char *id3_genres[] = {
     "Blues", "Classic Rock", "Country", "Dance", "Disco", "Funk", "Grunge", 
     "Hip-Hop", "Jazz", "Metal", "New Age", "Oldies", "Other", "Pop", "R&B", 
     "Rap", "Reggae", "Rock", "Techno", "Industrial", "Alternative", "Ska", 
     "Death Metal", "Pranks", "Soundtrack", "Euro-Techno", "Ambient", 
     "Trip-Hop", "Vocal", "Jazz+Funk", "Fusion", "Trance", "Classical", 
     "Instrumental", "Acid", "House", "Game", "Sound Clip", "Gospel", "Noise", 
     "AlternRock", "Bass", "Soul", "Punk", "Space", "Meditative", 
     "Instrumental Pop", "Instrumental Rock", "Ethnic", "Gothic", "Darkwave", 
     "Techno-Industrial", "Electronic", "Pop-Folk", "Eurodance", "Dream", 
     "Southern Rock", "Comedy", "Cult", "Gangsta Rap", "Top 40", 
     "Christian Rap", "Pop/Funk", "Jungle", "Native American", "Cabaret", 
     "New Wave", "Psychedelic", "Rave", "Showtunes", "Trailer", "Lo-Fi", 
     "Tribal", "Acid Punk", "Acid Jazz", "Polka", "Retro", "Musical", 
     "Rock & Roll", "Hard Rock", "Folk", "Folk/Rock", "National Folk", "Swing", 
     "Fast-Fusion", "Bebob", "Latin", "Revival", "Celtic", "Bluegrass", 
     "Avantgarde", "Gothic Rock", "Progressive Rock", "Psychedelic Rock", 
     "Symphonic Rock", "Slow Rock", "Big Band", "Chorus", "Easy Listening", 
     "Acoustic", "Humour", "Speech", "Chanson", "Opera", "Chamber Music", 
     "Sonata", "Symphony", "Booty Bass", "Primus", "Porn Groove", "Satire", 
     "Slow Jam", "Club", "Tango", "Samba", "Folklore", "Ballad", 
     "Power Ballad", "Rhythmic Soul", "Freestyle", "Duet", "Punk Rock", 
     "Drum Solo", "A Cappella", "Euro-House", "Dance Hall", "Goa", 
     "Drum & Bass", "Club-House", "Hardcore", "Terror", "Indie", "BritPop", 
     "Negerpunk", "Polsk Punk", "Beat", "Christian Gangsta Rap", "Heavy Metal", 
     "Black Metal", "Crossover", "Contemporary Christian", "Christian Rock", 
     "Merengue", "Salsa", "Thrash Metal", "Anime", "JPop", "Synthpop"
};



static inline int
FtoU8( mad_fixed_t sample )
{
     /* round */
     sample += (1L << (MAD_F_FRACBITS - 8));

     /* clip */
     if (sample >= MAD_F_ONE)
          sample = MAD_F_ONE - 1;
     else if (sample < -MAD_F_ONE)
          sample = -MAD_F_ONE;

     /* quantize */
     return (sample >> (MAD_F_FRACBITS + 1 - 8)) + 128;
}

static inline int
FtoS16( mad_fixed_t sample )
{
     /* round */
     sample += (1L << (MAD_F_FRACBITS - 16));

     /* clip */
     if (sample >= MAD_F_ONE)
          sample = MAD_F_ONE - 1;
     else if (sample < -MAD_F_ONE)
          sample = -MAD_F_ONE;

     /* quantize */
     return sample >> (MAD_F_FRACBITS + 1 - 16);
}

static void
mad_mix_audio( mad_fixed_t const *left, mad_fixed_t const *right,
               char *dst, int len, int format, int src_channels, int dst_channels )
{ 
     int s_n = src_channels;
     int d_n = dst_channels;
     int i;
               
     switch (format) {
          case 8:
               /* Copy/Interleave channels */
               if (s_n == d_n) {
                    __u8 *d = (__u8*)&dst[0];
                    
                    if (s_n == 2) {
                         for (i = 0; i < len; i++) {
                              d[i*2+0] = FtoU8(left[i]);
                              d[i*2+1] = FtoU8(right[i]);
                         }
                    } else {
                         for (i = 0; i < len; i++)
                              d[i] = FtoU8(left[i]);
                    }
               }
               /* Upmix mono to stereo */
               else if (s_n < d_n) {
                    __u8 *d = (__u8*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i*2+0] = d[i*2+1] = FtoU8(left[i]);
               }
               /* Downmix stereo to mono */
               else if (s_n > d_n) {
                    __u8 *d  = (__u8*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i] = (FtoU8(left[i]) + FtoU8(right[i])) >> 1;
               }
               break;
                         
          case 16:
               /* Copy/Interleave channels */
               if (s_n == d_n) {
                    __s16 *d = (__s16*)&dst[0];
                    
                    if (s_n == 2) {
                         for (i = 0; i < len; i++) {
                              d[i*2+0] = FtoS16(left[i]);
                              d[i*2+1] = FtoS16(right[i]);
                         }
                    } else {
                         for (i = 0; i < len; i++)
                              d[i] = FtoS16(left[i]);
                    }
               }
               /* Upmix mono to stereo */
               else if (s_n < d_n) {
                    __s16 *d = (__s16*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i*2+0] = d[i*2+1] = FtoS16(left[i]);
               }
               /* Downmix stereo to mono */
               else if (s_n > d_n) {
                    __s16 *d = (__s16*)&dst[0];

                    for (i = 0; i < len; i++)
                         d[i] = (FtoS16(left[i]) + FtoS16(right[i])) >> 1;
               }
               break;
                         
          default:
               D_BUG( "unexpected sample format" );
               break;
     }
}


static void
IFusionSoundMusicProvider_Mad_Destruct( IFusionSoundMusicProvider *thiz )
{
     IFusionSoundMusicProvider_Mad_data *data =
         (IFusionSoundMusicProvider_Mad_data*)thiz->priv;

     thiz->Stop( thiz );

     mad_synth_finish( &data->synth );
     mad_frame_finish( &data->frame );
     mad_stream_finish( &data->stream );

     pthread_mutex_destroy( &data->lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Mad_AddRef( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_Release( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (--data->ref == 0)
          IFusionSoundMusicProvider_Mad_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                               FSMusicProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!caps)
          return DFB_INVARG;

     *caps = FMCAPS_BASIC | FMCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_EnumTracks( IFusionSoundMusicProvider *thiz,
                                          FSTrackCallback            callback,
                                          void                      *callbackdata )
{
     FSTrackDescription desc;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!callback)
          return DFB_INVARG;

     desc = data->desc;     
     callback( 0, desc, callbackdata );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_GetTrackID( IFusionSoundMusicProvider *thiz,
                                          FSTrackID                 *ret_track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!ret_track_id)
          return DFB_INVARG;

     *ret_track_id = 0;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                                   FSTrackDescription        *desc )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!desc)
          return DFB_INVARG;

     *desc = data->desc;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                    FSStreamDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSSDF_SAMPLERATE   | FSSDF_CHANNELS  |
                          FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
     desc->samplerate   = data->samplerate;
     desc->channels     = data->channels;
     desc->sampleformat = FSSF_S16;
     desc->buffersize   = (desc->samplerate / desc->channels) >> 1;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                    FSBufferDescription       *desc )
{     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!desc)
          return DFB_INVARG;

     desc->flags        = FSBDF_SAMPLERATE   | FSBDF_CHANNELS  |
                          FSBDF_SAMPLEFORMAT | FSBDF_LENGTH;
     desc->samplerate   = data->samplerate;
     desc->channels     = data->channels;
     desc->sampleformat = FSSF_S16;
     desc->length       = (desc->samplerate / desc->channels) >> 1;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_SelectTrack( IFusionSoundMusicProvider *thiz,
                                           FSTrackID                  track_id )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (track_id != 0)
          return DFB_INVARG;

     return DFB_OK;
}

static void*
MadStreamThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Mad_data *data   = ctx;
     IFusionSoundStream                 *stream = data->dest.stream;
     
     char inbuf[INPUT_BUFFER_SIZE];
     char outbuf[1152 * data->dest.channels * data->dest.format >> 3];
     
     data->finished = false;
     
     data->stream.next_frame = NULL;
     
     while (data->playing) {
          ssize_t len;
          int     offset = 0;
                
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->stream.next_frame) {
               offset = data->stream.bufend - data->stream.next_frame;
               direct_memmove( &inbuf[0], data->stream.next_frame, offset );
          }
               
          len = read( data->fd, &inbuf[offset], sizeof(inbuf)-offset );   
      
          pthread_mutex_unlock( &data->lock );

          if (len > 0) {
               len += offset;
               if (len < sizeof(inbuf))
                    memset( &inbuf[len], 0, sizeof(inbuf)-len );
          } else {
               data->finished = true;
               break;
          }
          
          mad_stream_buffer( &data->stream, &inbuf[0], sizeof(inbuf) );

          do {
               struct mad_pcm *pcm = &data->synth.pcm;
               
               if (mad_frame_decode( &data->frame, &data->stream ) == -1) {           
                    if (!MAD_RECOVERABLE(data->stream.error))
                         break;
                    continue;
               }
                 
               mad_synth_frame( &data->synth, &data->frame );

               mad_mix_audio( pcm->samples[0], pcm->samples[1], outbuf,
                              pcm->length, data->dest.format,
                              pcm->channels, data->dest.channels );

               stream->Write( stream, outbuf, pcm->length );
          } while (data->playing);
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Mad_PlayToStream( IFusionSoundMusicProvider *thiz,
                                            IFusionSoundStream        *destination )
{
     FSStreamDescription desc;
     int                 dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;
         
     /* check if number of channels is supported */
     if (desc.channels > 2)
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
     data->dest.stream   = destination;
     data->dest.format   = dst_format;
     data->dest.channels = desc.channels;
     data->dest.length   = desc.buffersize;
     
     /* start thread */
     data->playing = true;
     data->thread  = direct_thread_create( DTT_DEFAULT, 
                                           MadStreamThread, data, "Mad" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static void*
MadBufferThread( DirectThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Mad_data *data   = ctx;
     IFusionSoundBuffer                 *buffer = data->dest.buffer; 
     
     char inbuf[INPUT_BUFFER_SIZE];
     int  blocksize = data->dest.channels * data->dest.format >> 3;
     int  written   = 0;
     
     data->finished = false;
     
     data->stream.next_frame = NULL;
     
     while (data->playing) {
          ssize_t len;
          int     offset = 0;
                
          pthread_mutex_lock( &data->lock );

          if (!data->playing) {
               pthread_mutex_unlock( &data->lock );
               break;
          }
          
          if (data->stream.next_frame) {
               offset = data->stream.bufend - data->stream.next_frame;
               direct_memmove( &inbuf[0], data->stream.next_frame, offset );
          }
               
          len = read( data->fd, &inbuf[offset], sizeof(inbuf)-offset );   
      
          pthread_mutex_unlock( &data->lock );

          if (len > 0) {
               len += offset;
               if (len < sizeof(inbuf))
                    memset( &inbuf[len], 0, sizeof(inbuf)-len );
          } else {
               data->finished = true;
               break;
          }
          
          mad_stream_buffer( &data->stream, &inbuf[0], sizeof(inbuf) );

          do {
               struct mad_pcm *pcm   = &data->synth.pcm;
               mad_fixed_t    *left  = (mad_fixed_t*)pcm->samples[0];
               mad_fixed_t    *right = (mad_fixed_t*)pcm->samples[1];
               char           *dst;
               int             len, n;
               
               if (mad_frame_decode( &data->frame, &data->stream ) == -1) {           
                    if (!MAD_RECOVERABLE(data->stream.error))
                         break;
                    continue;
               }
                 
               mad_synth_frame( &data->synth, &data->frame );
               len = pcm->length;
               
               if (buffer->Lock( buffer, (void*)&dst ) != DFB_OK) {
                    D_ERROR( "IFusionSoundMusicProvider_Mad: "
                             "Couldn't lock buffer!\n" );
                    break;
               }
               
               do {
                    n = MIN( data->dest.length-written, len );
                    
                    mad_mix_audio( left, right, &dst[written*blocksize], n,
                                   data->dest.format, pcm->channels, data->dest.channels );
                    left    += n;
                    right   += n;
                    len     -= n;
                    written += n;
                    
                    if (written >= data->dest.length) {
                         if (data->callback) {
                              buffer->Unlock( buffer );
                              data->callback( written, data->ctx );
                              buffer->Lock( buffer, (void*)&dst );
                         }
                         written = 0;
                    }
               } while (len > 0);

               buffer->Unlock( buffer );
          } while (data->playing);
     }
     
     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Mad_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                            IFusionSoundBuffer        *destination,
                                            FMBufferCallback           callback,
                                            void                      *ctx )
{
     FSBufferDescription desc;
     int                 dst_format = 0;

     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination samplerate is supported */
     if (desc.samplerate != data->samplerate)
          return DFB_UNSUPPORTED;
    
     /* check if number of channels is supported */
     if (desc.channels > 2)
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
     data->thread  = direct_thread_create( DTT_DEFAULT,
                                           MadBufferThread, data, "Mad" );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Mad_Stop( IFusionSoundMusicProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

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
IFusionSoundMusicProvider_Mad_SeekTo( IFusionSoundMusicProvider *thiz,
                                      double                     seconds )
{
     off_t pos;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     pos = seconds * (double)(data->desc.bitrate >> 3);
     pos = (pos > data->size) ? data->size : pos;
     
     pthread_mutex_lock( &data->lock );   
     lseek( data->fd, pos, SEEK_SET );    
     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_Mad_GetPos( IFusionSoundMusicProvider *thiz,
                                      double                    *seconds )
{
     off_t pos;
     
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )

     if (!seconds)
          return DFB_INVARG;

     if (data->finished) {
          *seconds = data->length;
          return DFB_EOF;
     }
     
     pos = lseek( data->fd, 0, SEEK_CUR );
     if (data->playing && data->stream.this_frame) {
          pos -= data->stream.bufend - data->stream.this_frame;
          pos  = (pos < 0) ? 0 : pos;
     }
                
     *seconds = (double)pos / (double)(data->desc.bitrate >> 3);
          
     return DFB_OK;
}

static DFBResult 
IFusionSoundMusicProvider_Mad_GetLength( IFusionSoundMusicProvider *thiz,
                                         double                    *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IFusionSoundMusicProvider_Mad )
     
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = data->length;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     /* FIXME: detect by contents */
     if (!strcasecmp( strrchr( ctx->filename, '.' ), ".mp3" ))
          return DFB_OK;
          
     return DFB_UNSUPPORTED;
}   

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     char               buf[16384];
     struct stat        st;
     struct mad_header  header;
     unsigned int       mpeg_id = 3;
     unsigned long      frames  = 0;
     const char        *version;
     struct id3_tag     id3;
     int                i, error;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSoundMusicProvider_Mad )

     data->ref = 1;
     
     data->fd = open( filename, O_RDONLY );
     if (data->fd < 0)
          return DFB_IO;
          
     fstat( data->fd, &st );
     data->size = st.st_size;
          
     if (read( data->fd, buf, sizeof(buf) ) < sizeof(buf)) {
          close( data->fd );
          return DFB_IO;
     }

     mad_stream_init( &data->stream );
     mad_frame_init( &data->frame );
     mad_synth_init( &data->synth );
     mad_stream_options( &data->stream, MAD_OPTION_IGNORECRC );
    
     mad_stream_buffer( &data->stream, buf, sizeof(buf) );
     
     /* find first valid frame */
     for (i = 0; i < 10; i++) {
          error = mad_frame_decode( &data->frame, &data->stream );
          if (!error) {
               /* get MPEG version */
               mpeg_id = (mad_bit_read( &data->stream.ptr, 32 ) >> 11) & 3;
               /* get number of frames from Xing headers */
               if (data->stream.anc_bitlen >= 128 &&
                   mad_bit_read( &data->stream.anc_ptr, 32 ) == XING_MAGIC)
               {
                    D_DEBUG( "IFusionSoundMusicProvider_Mad: Found Xing header.\n" );
                    if (mad_bit_read( &data->stream.anc_ptr, 32 ) & 1)
                         frames = mad_bit_read( &data->stream.anc_ptr, 32 );
               }
               break;
          } 
          else {
               if (!MAD_RECOVERABLE(data->stream.error))
                    break;
          }
     }
     
     if (error) {
          D_DEBUG( "IFusionSoundMusicProvider_Mad: Couldn't find a valid frame!\n" );
          mad_synth_finish( &data->synth );
          mad_frame_finish( &data->frame );
          mad_stream_finish( &data->stream );
          close( data->fd );
          return DFB_FAILURE;
     }
     
     header           = data->frame.header;   
     data->samplerate = header.samplerate;
     data->channels   = MAD_NCHANNELS( &header );
     
     /* get ID3 tag */    
     lseek( data->fd, -sizeof(id3), SEEK_END );
     read( data->fd, &id3, sizeof(id3) );
     
     if (!strncmp( id3.tag, "TAG", 3 )) {
          data->size -= sizeof(id3);
            
          strncpy( data->desc.artist, id3.artist, 
                   MIN( FS_TRACK_DESC_ARTIST_LENGTH-1, sizeof(id3.artist) ) );
          strncpy( data->desc.title, id3.title, 
                   MIN( FS_TRACK_DESC_TITLE_LENGTH-1, sizeof(id3.title) ) );
          strncpy( data->desc.album, id3.album, 
                   MIN( FS_TRACK_DESC_ALBUM_LENGTH-1, sizeof(id3.album) ) );
          data->desc.year = strtol( id3.year, NULL, 10 );
             
          if (id3.genre < sizeof(id3_genres)/sizeof(id3_genres[0])) {
               const char *genre = id3_genres[(int)id3.genre];
               strncpy( data->desc.genre, genre,
                        MIN( FS_TRACK_DESC_GENRE_LENGTH-1, strlen(genre) ) );
          }
     }
     
     switch (mpeg_id) {
          case 0:
               version = "2.5";
               break;
          case 2:
               version = "2";
               break;
          case 3:
          default:
               version = "1";
               break;
     }       
     
     if (frames) {
          /* compute avarage bitrate for VBR stream */
          switch (header.layer) {
               case MAD_LAYER_I:
                    frames *= 384;
                    break;
               case MAD_LAYER_II:
                    frames *= 1152;
                    break;
               case MAD_LAYER_III:
               default:
                    if (header.flags & (MAD_FLAG_LSF_EXT | MAD_FLAG_MPEG_2_5_EXT))
                         frames *= 576;
                    else
                         frames *= 1152;
                    break;
          }
          
          data->length = (double)frames / (double)header.samplerate;
          data->desc.bitrate = (double)(data->size << 3) / data->length;         
          
          snprintf( data->desc.encoding, 
                    FS_TRACK_DESC_ENCODING_LENGTH,
                    "MPEG-%s Layer %d (VBR)", version, header.layer );
     }
     else {
          if (header.bitrate < 8000)
               header.bitrate = 8000;
          
          data->length = (double)data->size / (double)(header.bitrate >> 3);
          data->desc.bitrate = header.bitrate;
              
          snprintf( data->desc.encoding, 
                    FS_TRACK_DESC_ENCODING_LENGTH,
                    "MPEG-%s Layer %d", version, header.layer );
     }
     
     lseek( data->fd, 0, SEEK_SET );
     
     direct_util_recursive_pthread_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Mad_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Mad_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_Mad_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_Mad_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_Mad_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_Mad_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_Mad_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_Mad_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_Mad_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_Mad_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_Mad_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Mad_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Mad_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Mad_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Mad_GetLength;

     return DFB_OK;
}

