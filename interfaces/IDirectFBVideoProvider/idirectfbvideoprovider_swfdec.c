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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <pthread.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <directfb.h>

#include <media/idirectfbvideoprovider.h>

#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <gfx/convert.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
#endif

#include <swfdec.h>
#include <swfdec_buffer.h>
#include <swfdec_render.h>


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Swfdec )


typedef struct {
     int                    ref;

     SwfdecDecoder         *decoder;

     int                    width;
     int                    height;
     int                    frames;
     double                 rate;
     double                 length;
     long                   interval;
     
     DirectThread          *thread;
     pthread_mutex_t        mutex;
     pthread_cond_t         cond;

     bool                   seeking;
     bool                   stopped;
     bool                   finished;

#ifdef HAVE_FUSIONSOUND
     IFusionSound          *sound;
     IFusionSoundStream    *stream;
#endif

     IDirectFBSurface      *dest;
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect;
     
     DVFrameCallback        callback;
     void                  *ctx;
} IDirectFBVideoProvider_Swfdec_data;



static inline long long
microsec( void )
{
     struct timeval t;
     gettimeofday( &t, NULL );
     return (t.tv_sec * 1000000ll + t.tv_usec);
}


static void
SwfPutImage( CoreSurface  *surface,
             DFBRectangle *rect,
             SwfdecBuffer *source )
{
     __u8  *D;
     __u32 *S;
     char  *dst, *src;
     int    pitch;
     int    w, h, n;
    
     if (dfb_surface_soft_lock( surface, DSLF_WRITE,
                                (void*)&dst, &pitch, 0 ) != DFB_OK)
          return;
   
     dst += pitch * rect->y +
            DFB_BYTES_PER_LINE( surface->format, rect->x );
     src  = source->data;
            
     for (h = rect->h; h--;) {
          D = (__u8 *) dst;
          S = (__u32*) src;
          w = rect->w;
          
          switch (surface->format) {
               case DSPF_RGB332:
                    while (w--) {
                         *D++ = RGB32_TO_RGB332( *S );
                         S++;
                    }
                    break;
               case DSPF_ARGB4444:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB4444( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB4444( *(S+0) );
                         p1 = RGB32_TO_ARGB4444( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB4444( *S );
                    break;
               case DSPF_ARGB2554: 
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB2554( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB2554( *(S+0) );
                         p1 = RGB32_TO_ARGB2554( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB2554( *S );
                    break;
               case DSPF_ARGB1555:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_ARGB1555( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_ARGB1555( *(S+0) );
                         p1 = RGB32_TO_ARGB1555( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_ARGB1555( *S );
                    break;
               case DSPF_RGB16:
                    if ((__u32)D & 2) {
                         *((__u16*)D) = RGB32_TO_RGB16( *S );
                         D += 2; S++;
                         w--;
                    }

                    for (n = w/2; n--;) {
                         __u32 p0, p1;
                         p0 = RGB32_TO_RGB16( *(S+0) );
                         p1 = RGB32_TO_RGB16( *(S+1) );
#ifdef WORDS_BIGENDIAN
                         *((__u32*)D) = (p0 << 16) | p1;
#else
                         *((__u32*)D) = p0 | (p1 << 16);
#endif
                         D += 4; S += 2;
                    }

                    if (w & 1)
                         *((__u16*)D) = RGB32_TO_RGB16( *S );
                    break;
               case DSPF_RGB24:
                    while (w--) {
                         *(D+0) = (*S      ) & 0xff;
                         *(D+1) = (*S >>  8) & 0xff;
                         *(D+2) = (*S >> 16) & 0xff;
                         D += 3; S++;
                    }
                    break;
               case DSPF_RGB32:
               case DSPF_AiRGB:
                    direct_memcpy( dst, src, w*4 );
                    break;
               case DSPF_ARGB:
                    while (w--) {
                         *((__u32*)D) = *S++ | 0xff000000;
                         D += 4;
                    }
                    break;
               default:
                    break;
          }
          
          dst += pitch;
          src += rect->w*4;
     }
     
     dfb_surface_unlock( surface, 0 );
}

static void*
SwfPlayback( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data   = arg;
     long                                adjust = 0;
     int                                 w      = 0;
     int                                 h      = 0;
   
     pthread_setcanceltype( PTHREAD_CANCEL_ASYNCHRONOUS, NULL );

     while (1) {
          SwfdecBuffer *video_buffer;
          SwfdecBuffer *audio_buffer;
          DFBRectangle  rect;
          long long     start;
          int           pos;
          
          direct_thread_testcancel( self );
          
          if (data->finished || data->stopped) {
               pthread_mutex_lock( &data->mutex );
               pthread_cond_wait( &data->cond, &data->mutex );
               pthread_mutex_unlock( &data->mutex );
               adjust = 0;
          }
          
          start = microsec();
          
          pthread_mutex_lock( &data->mutex );
          
          rect = (data->rect.w == 0)
                  ? data->dest_data->area.wanted : data->rect;     
          if (rect.w != w || rect.h != h) {
               w = rect.w;
               h = rect.h;
               swfdec_decoder_set_image_size( data->decoder, w, h );
          }
    
          swfdec_render_iterate( data->decoder ); 
          pos = swfdec_render_get_frame_index( data->decoder ) + 1;
          if (pos >= data->frames)
               data->finished = true;
          
          if (adjust < data->interval) {
               video_buffer = swfdec_render_get_image( data->decoder ); 
               audio_buffer = swfdec_render_get_audio( data->decoder );
          
               if (video_buffer) {
                    SwfPutImage( data->dest_data->surface,
                                 &rect, video_buffer );
                    swfdec_buffer_unref( video_buffer );
               }
          
               if (audio_buffer) {
#ifdef HAVE_FUSIONSOUND
                    if (data->stream) {
                         if (!data->seeking) {
                              data->stream->Write( data->stream,
                                                   audio_buffer->data,
                                                   audio_buffer->length/4 );
                         } else
                              data->stream->Wait( data->stream, 0 );
                    }
#endif
                    swfdec_buffer_unref( audio_buffer );
               }
           
               if (data->callback)
                    data->callback( data->ctx );
          } else 
               D_INFO( "IDirectFBVideoProvider_Swfdec: "
                       "discarding frame %i.\n", pos );

          data->seeking = false;

          if (!data->stopped && !data->finished) {
               adjust += (long) (microsec() - start);
               if (adjust < data->interval) {
                    struct timeval  t0;
                    struct timespec t1;
                    long            s;

                    s = data->interval - adjust;
                    gettimeofday( &t0, NULL );
                    t1.tv_sec  = t0.tv_sec + s/1000000;
                    t1.tv_nsec = (t0.tv_usec + (s%1000000)) * 1000;
               
                    pthread_cond_timedwait( &data->cond,
                                            &data->mutex, &t1 );
                    adjust = 0;
               } else
                    adjust -= data->interval;
          }

          pthread_mutex_unlock( &data->mutex );
     }
  
     return 0;
}


static void
IDirectFBVideoProvider_Swfdec_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swfdec_data *data = thiz->priv;
     
     if (data->thread) {
          direct_thread_cancel( data->thread );
          direct_thread_join( data->thread );
          direct_thread_destroy( data->thread );
     }
     
     if (data->decoder)
          swfdec_decoder_free( data->decoder );
          
     if (data->dest)
          data->dest->Release( data->dest );

#ifdef HAVE_FUSIONSOUND
     if (data->stream) {
          data->stream->Flush( data->stream );
          data->stream->Release( data->stream );
     }

     if (data->sound)
          data->sound->Release( data->sound );
#endif

     pthread_cond_destroy( &data->cond );
     pthread_mutex_destroy( &data->mutex );     
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBVideoProvider_Swfdec_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (--data->ref == 0)
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                               DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!caps)
          return DFB_INVARG;
          
     *caps = DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_SEEK;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                     DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!desc)
          return DFB_INVARG;
          
     desc->flags       = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = DSPF_RGB32;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_PlayTo( IDirectFBVideoProvider *thiz,
                                      IDirectFBSurface       *dest,
                                      const DFBRectangle     *dest_rect,
                                      DVFrameCallback         callback,
                                      void                   *ctx )
{
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect      = { 0, 0, 0, 0 };
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;
          
     dest_data = dest->priv;
     
     switch (dest_data->surface->format) {
          case DSPF_RGB332:
          case DSPF_ARGB4444:
          case DSPF_ARGB2554:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_AiRGB:
               break;
          default:
               D_DEBUG( "IDirectFBVideoProvider_Swf: "
                        "unsupported destination pixelformat.\n" );
               return DFB_UNSUPPORTED;
     }
     
     if (dest_rect) {
          if (dest_rect->x < 1 || dest_rect->y < 1)
               return DFB_INVARG;
         
          rect    = *dest_rect;
          rect.x += dest_data->area.wanted.x;
          rect.y += dest_data->area.wanted.y;
          
          if (!dfb_rectangle_intersect( &rect,
                                        &dest_data->area.wanted ))
               return DFB_INVARG;
     }

     pthread_mutex_lock( &data->mutex );
     
     if (data->dest)
          data->dest->Release( data->dest );
     dest->AddRef( dest );
     
     data->seeking   = false;
     data->dest      = dest;
     data->dest_data = dest_data;
     data->rect      = rect;
     data->callback  = callback;
     data->ctx       = ctx;
     
     if (data->stopped || data->finished) {
          if (data->finished)
               swfdec_render_seek( data->decoder, 0 );
          data->stopped  = false;
          data->finished = false;
          pthread_cond_signal( &data->cond );
     }

     pthread_mutex_unlock( &data->mutex );
     
     if (!data->thread) {
          data->thread = direct_thread_create( DTT_OUTPUT, SwfPlayback,
                                               (void*)data, "Swf Playback" );
          if (!data->thread) {
               D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                        "direct_thread_create() failed!\n" );
               return DFB_FAILURE;
          }
     }
          
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
 
     if (!data->stopped && !data->finished) {
          pthread_mutex_lock( &data->mutex );
          
          data->stopped = true;
          pthread_cond_signal( &data->cond );
          
          pthread_mutex_unlock( &data->mutex );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     int position;
          
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (data->finished)
          return DFB_UNSUPPORTED;

     position = (seconds * data->rate);
     position = CLAMP( position, 0, data->frames-1 );

     pthread_mutex_lock( &data->mutex );

     swfdec_render_seek( data->decoder, position );
     data->seeking = true;
     pthread_cond_signal( &data->cond );
     
     pthread_mutex_unlock( &data->mutex );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     int position;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!seconds)
          return DFB_INVARG;
          
     position = swfdec_render_get_frame_index( data->decoder ) + 1;
     *seconds = (double)position / data->rate;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
          
     if (!seconds)
          return DFB_INVARG;
          
     *seconds = data->length;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                                  DFBColorAdjustment     *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!adj)
          return DFB_INVARG;
          
     adj->flags = DCAF_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                  const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!adj)
          return DFB_INVARG;
     
     return DFB_UNIMPLEMENTED;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     SwfdecDecoder *decoder;
     char*          buf;
     int            fd, err;

     decoder = swfdec_decoder_new();
     if (!decoder)
          return DFB_INIT;
          
     fd = open( ctx->filename, O_RDONLY );
     if (fd < 0) {
          swfdec_decoder_free( decoder );
          return DFB_FAILURE;
     }

     buf = calloc( 1, 64 );
     if (!buf) {
          swfdec_decoder_free( decoder );
          close( fd );
          return DFB_NOSYSTEMMEMORY;
     }
     
     read( fd, buf, 64 );
     close( fd );

     swfdec_decoder_add_data( decoder, buf, 64 );
     err = swfdec_decoder_parse( decoder );
     swfdec_decoder_free( decoder );
   
     return (err == SWF_ERROR) ? DFB_UNSUPPORTED : DFB_OK;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename )
{
     int          fd;
     struct stat  st;
     char        *buffer;
     int          i;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Swfdec )
     
     data->ref = 1;
     
     swfdec_init();
     
     data->decoder = swfdec_decoder_new();
     if (!data->decoder) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "couldn't create swf decoder.\n" );
          return DFB_INIT;
     }

     fd = open( filename, O_RDONLY );
     if (fd < 0) {
          D_PERROR( "IDirectFBVideoProvider_Swfdec: "
                    "couldn't open file '%s'", filename );
          swfdec_decoder_free( data->decoder );
          return DFB_IO;
     }
     
     fstat( fd, &st );
     buffer = calloc( 1, st.st_size );
     if (!buffer) {
          swfdec_decoder_free( data->decoder );
          close( fd );
          return D_OOM();
     }

     if (read( fd, buffer, st.st_size ) < 0) { 
          D_PERROR( "IDirectFBVideoProvider_Swfdec: "
                    "error reading from '%s'", filename );
          swfdec_decoder_free( data->decoder ); 
          free( buffer );
          close( fd );
          return DFB_IO;
     }

     close( fd );

     swfdec_decoder_add_data( data->decoder, buffer, st.st_size );  
     swfdec_decoder_eof( data->decoder );

     swfdec_decoder_set_colorspace( data->decoder, SWF_COLORSPACE_RGB888 );
    
     for (i = 0; i < 4; i++) {
          if (swfdec_decoder_parse( data->decoder ) == SWF_ERROR) { 
               D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                        "swfdec_decoder_parse() failed at stage %i!\n", i+1 );
               swfdec_decoder_free( data->decoder );
               return DFB_FAILURE;
          }
     }
     
     swfdec_decoder_get_image_size( data->decoder,
                                    &data->width, &data->height );
     swfdec_decoder_get_n_frames( data->decoder, &data->frames );
     swfdec_decoder_get_rate( data->decoder, &data->rate );
     
     data->width    = data->width  ? : 1;
     data->height   = data->height ? : 1;
     data->frames   = data->frames ? : 1;
     data->rate     = data->rate   ? : 1;
     data->interval = (1000000.0 / data->rate + 0.555);
     data->length   = (double)data->frames / data->rate;

#ifdef HAVE_FUSIONSOUND
     if (FusionSoundCreate( &data->sound ) == DFB_OK) {
          FSStreamDescription dsc;

          dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                             FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.buffersize   = 4096;
          dsc.channels     = 2;
          dsc.samplerate   = 44100;
          dsc.sampleformat = FSSF_S16;

          if (data->sound->CreateStream( data->sound, &dsc,
                                         &data->stream ) != DFB_OK) {
               D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                        "IFusionSound::CreateStream() failed!\n" );
               data->sound->Release( data->sound );
               data->sound = NULL;
          }
     } else
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "FusionSoundCreate() failed!\n" );
#endif     

     direct_util_recursive_pthread_mutex_init( &data->mutex );
     pthread_cond_init( &data->cond, NULL );
     
     thiz->AddRef                = IDirectFBVideoProvider_Swfdec_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Swfdec_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_Swfdec_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_Swfdec_GetSurfaceDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_Swfdec_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Swfdec_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_Swfdec_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Swfdec_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Swfdec_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_Swfdec_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_Swfdec_SetColorAdjustment;

     return DFB_OK;
}

