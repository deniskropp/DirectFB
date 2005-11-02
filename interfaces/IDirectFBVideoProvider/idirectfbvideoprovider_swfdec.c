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
#include <direct/list.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <directfb.h>

#include <idirectfb.h>

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
           IDirectFBDataBuffer    *buffer  );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Swfdec )

typedef struct {
     DirectLink    link;

     SwfdecBuffer *buffer;
} AudioElement;

typedef struct {
     int                    ref;

     IDirectFBDataBuffer   *buffer;
     
     SwfdecDecoder         *decoder;

     int                    width;
     int                    height;
     double                 rate;
     long                   interval;
     
     bool                   seeked;
     bool                   stopped;
     
     DirectThread          *input_thread;
     pthread_mutex_t        input_lock;
     
     DirectThread          *video_thread;
     pthread_mutex_t        video_lock;
     pthread_cond_t         video_cond;
     
     IDirectFBSurface      *dest;
     IDirectFBSurface_data *dest_data;
     DFBRectangle           rect;
     
     DirectThread          *audio_thread;
     pthread_mutex_t        audio_lock;
     AudioElement          *audio_queue;

#ifdef HAVE_FUSIONSOUND
     IFusionSound          *sound;
     IFusionSoundStream    *stream;
#endif     

     int                    mouse_x;
     int                    mouse_y;
     int                    mouse_btn;
     
     DVFrameCallback        callback;
     void                  *ctx;
} IDirectFBVideoProvider_Swfdec_data;


#ifdef HAVE_FUSIONSOUND
static inline void
add_audio_buffer( IDirectFBVideoProvider_Swfdec_data *data, SwfdecBuffer *buffer )
{
     AudioElement *e;
     
     e = D_CALLOC( 1, sizeof(AudioElement) );
     if (!e) {
          D_OOM();
          return;
     }
     
     e->buffer = buffer;
          
     direct_list_append( (DirectLink**)&data->audio_queue, &e->link );
}

static inline SwfdecBuffer*
get_audio_buffer( IDirectFBVideoProvider_Swfdec_data *data )
{
     AudioElement *e;
     SwfdecBuffer *buffer = NULL;

     e = data->audio_queue;
     if (e) { 
          buffer = e->buffer;
          direct_list_remove( (DirectLink**)&data->audio_queue, &e->link );
          D_FREE( e );
     }
     
     return buffer;
}

static inline void
free_audio_buffers( IDirectFBVideoProvider_Swfdec_data *data )
{
     AudioElement *e;
     
     if (data->audio_queue) {
          direct_list_foreach( e, data->audio_queue ) {
               direct_list_remove( (DirectLink**)&data->audio_queue, &e->link );
               swfdec_buffer_unref( e->buffer );
               D_FREE( e );
          }
     }
}
#endif


static void*
SwfInput( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data   = arg;
     IDirectFBDataBuffer                *buffer = data->buffer;
     
     while (!direct_thread_is_canceled( self )) {
          DFBResult     ret;
          unsigned char tmp[1024];
          unsigned int  len = 0;
          
          ret = buffer->WaitForDataWithTimeout( buffer, sizeof(tmp), 0, 1 );
          if (ret == DFB_TIMEOUT)
               continue;
          
          while ((ret = buffer->GetData( buffer,
                                         sizeof(tmp), tmp, &len )) == DFB_OK) {
               void *buf;

               buf = malloc( len );
               if (!buf) {
                    D_OOM();
                    return (void*)1;
               }

               direct_memcpy( buf, tmp, len );

               pthread_mutex_lock( &data->input_lock );
          
               swfdec_decoder_add_data( data->decoder, buf, len );
               if (swfdec_decoder_parse( data->decoder ) == SWF_EOF) {
                    pthread_mutex_unlock( &data->input_lock );
                    ret = DFB_EOF;
                    break;
               }
          
               pthread_mutex_unlock( &data->input_lock );

               direct_thread_testcancel( self );
          }

          if (ret == DFB_EOF) {
               swfdec_decoder_eof( data->decoder );
               break;
          }
     }
     
     return (void*)0;
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
                    direct_memcpy( dst, src, w*4 );
                    break;
               case DSPF_ARGB:
                    while (w--) {
                         *((__u32*)D) = *S++ | 0xff000000;
                         D += 4;
                    }
                    break;
               case DSPF_AiRGB:
                    while (w--) {
                         *((__u32*)D) = *S++ & 0x00ffffff;
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
SwfVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data  = arg;
     long                                delay = 0;
     int                                 w     = 0;
     int                                 h     = 0;

     swfdec_decoder_get_image_size( data->decoder, &w, &h );     

     while (!direct_thread_is_canceled( self )) {
          struct timeval start;
          
          gettimeofday( &start, NULL );
          
          if (data->stopped) {
               pthread_mutex_lock( &data->video_lock );
               pthread_cond_wait( &data->video_cond, &data->video_lock );
               pthread_mutex_unlock( &data->video_lock );
               gettimeofday( &start, NULL );
               delay = 0;
          }

          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video_lock );
    
          pthread_mutex_lock( &data->input_lock );
          swfdec_render_iterate( data->decoder );
          pthread_mutex_unlock( &data->input_lock );

#ifdef HAVE_FUSIONSOUND
          if (data->audio_thread) {
               SwfdecBuffer *audio_buffer = NULL;
               
               if (!data->stopped)
                    audio_buffer = swfdec_render_get_audio( data->decoder );

               pthread_mutex_lock( &data->audio_lock );
               
               if (data->seeked)
                    free_audio_buffers( data );
                    
               if (audio_buffer)
                    add_audio_buffer( data, audio_buffer );
                    
               pthread_mutex_unlock( &data->audio_lock );
          }
#endif              
          
          if (delay < data->interval) {
               SwfdecBuffer *video_buffer;
               DFBRectangle  rect;
               
               rect = (data->rect.w == 0)
                      ? data->dest_data->area.wanted : data->rect;     
               if (rect.w != w || rect.h != h) {
                    w = rect.w;
                    h = rect.h;
                    swfdec_decoder_set_image_size( data->decoder, w, h );
               }
               
               video_buffer = swfdec_render_get_image( data->decoder );
               if (video_buffer) {
                    SwfPutImage( data->dest_data->surface,
                                 &rect, video_buffer );
                                 
                    swfdec_buffer_unref( video_buffer );
                    
                    if (data->callback)
                         data->callback( data->ctx );
               }
          }
          else {
               D_INFO( "IDirectFBVideoProvider_Swfdec: "
                       "discarding video frame %d.\n",
                        swfdec_render_get_frame_index( data->decoder ) );
          }

          data->seeked = false;

          if (!data->stopped) {
               struct timeval now;

               gettimeofday( &now, NULL );
               delay += (now.tv_sec  - start.tv_sec) * 1000000 + 
                        (now.tv_usec - start.tv_usec);
               
               if (delay < data->interval) {
                    struct timespec t;

                    t.tv_sec  =  start.tv_sec;
                    t.tv_nsec = (start.tv_usec + data->interval) * 1000;
               
                    pthread_cond_timedwait( &data->video_cond,
                                            &data->video_lock, &t );
                    delay = 0;
               } else {
                    delay -= data->interval;
               }
          }

          pthread_mutex_unlock( &data->video_lock );
     }
  
     return (void*)0;
}

#ifdef HAVE_FUSIONSOUND
static void*
SwfAudio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data = arg;
     
     data->stream->Wait( data->stream, 0 );

     while (!direct_thread_is_canceled( self )) {
          SwfdecBuffer *audio_buffer;
          
          pthread_mutex_lock( &data->audio_lock );
          audio_buffer = get_audio_buffer( data );
          pthread_mutex_unlock( &data->audio_lock );

          direct_thread_testcancel( self );

          if (data->seeked)
               data->stream->Wait( data->stream, 0 );

          if (audio_buffer) {
               data->stream->Write( data->stream,
                                    audio_buffer->data,
                                    audio_buffer->length/4 );
               swfdec_buffer_unref( audio_buffer );
          } else {
               usleep( 10 );
          }
     }

     return (void*)0;
}
#endif


static void
IDirectFBVideoProvider_Swfdec_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swfdec_data *data = thiz->priv;
     
     if (data->input_thread) {
          direct_thread_cancel( data->input_thread );
          direct_thread_join( data->input_thread );
          direct_thread_destroy( data->input_thread );
     }
     
     if (data->video_thread) { 
          direct_thread_cancel( data->video_thread );
          pthread_cond_signal( &data->video_cond );
          direct_thread_join( data->video_thread );
          direct_thread_destroy( data->video_thread );
     }
     
#ifdef HAVE_FUSIONSOUND
     if (data->audio_thread) {
          direct_thread_cancel( data->audio_thread );
          direct_thread_join( data->audio_thread );
          direct_thread_destroy( data->audio_thread );
          free_audio_buffers( data );
     }
        
     if (data->stream) {
          data->stream->Flush( data->stream );
          data->stream->Release( data->stream );
     }

     if (data->sound)
          data->sound->Release( data->sound );
#endif

     if (data->decoder)
          swfdec_decoder_free( data->decoder );
          
     if (data->dest)
          data->dest->Release( data->dest );

     if (data->buffer)
          data->buffer->Release( data->buffer );

     pthread_cond_destroy( &data->video_cond );
     pthread_mutex_destroy( &data->video_lock );
     pthread_mutex_destroy( &data->audio_lock );
     pthread_mutex_destroy( &data->input_lock );
     
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
          
     *caps = DVCAPS_BASIC | DVCAPS_SCALE      |
             DVCAPS_SEEK  | DVCAPS_INTERACTIVE;
     
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

     pthread_mutex_lock( &data->video_lock );
     pthread_mutex_lock( &data->audio_lock );
     
     if (data->dest)
          data->dest->Release( data->dest );
     dest->AddRef( dest );
     
     data->seeked    = false;
     data->dest      = dest;
     data->dest_data = dest_data;
     data->rect      = rect;
     data->callback  = callback;
     data->ctx       = ctx;

     if (data->stopped) {
          data->stopped  = false;
          pthread_cond_signal( &data->video_cond );
     }
     
     if (!data->video_thread) {
          data->video_thread = direct_thread_create( DTT_DEFAULT, SwfVideo,
                                                    (void*)data, "Swf Video" );
     }

#ifdef HAVE_FUSIONSOUND
     if (!data->audio_thread && data->stream) {
          data->audio_thread = direct_thread_create( DTT_DEFAULT, SwfAudio,
                                                     (void*)data, "Swf Audio" );
     }
#endif
 
     pthread_mutex_unlock( &data->video_lock );
     pthread_mutex_unlock( &data->audio_lock );
          
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
 
     if (!data->stopped) {
          pthread_mutex_lock( &data->video_lock );
          
          data->stopped = true;
          pthread_cond_signal( &data->video_cond );
          
          pthread_mutex_unlock( &data->video_lock );
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (seconds < 0.0)
          return DFB_INVARG;
          
     pthread_mutex_lock( &data->video_lock );
     pthread_mutex_lock( &data->audio_lock );

     swfdec_render_seek( data->decoder, seconds*data->rate );
     data->seeked = true;
     
     pthread_cond_signal( &data->video_cond );
     
     pthread_mutex_unlock( &data->video_lock );
     pthread_mutex_unlock( &data->audio_lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     int frame = 0;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!seconds)
          return DFB_INVARG;
     
     frame = swfdec_render_get_frame_index( data->decoder ) + 1;
     *seconds = (double)frame / data->rate;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{
     int frames = 0;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
          
     if (!seconds)
          return DFB_INVARG;
     
     swfdec_decoder_get_n_frames( data->decoder, &frames );
     *seconds = (double)frames / data->rate;
     
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

static DFBResult
IDirectFBVideoProvider_Swfdec_SendEvent( IDirectFBVideoProvider *thiz,
                                         const DFBEvent         *evt )
{
     bool mouse_event = false;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!evt)
          return DFB_INVARG;

     switch (evt->clazz) {
          case DFEC_INPUT:
               switch (evt->input.type) {
                    case DIET_BUTTONPRESS:
                         switch (evt->input.button) {
                              case DIBI_LEFT:
                                   data->mouse_btn = 1;
                                   mouse_event = true;
                                   break;
                              case DIBI_MIDDLE:
                                   data->mouse_btn = 2;
                                   mouse_event = true;
                                   break;
                              case DIBI_RIGHT:
                                   data->mouse_btn = 3;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break; 
                    case DIET_BUTTONRELEASE:
                         switch (evt->input.button) {
                              case DIBI_LEFT:
                              case DIBI_MIDDLE:
                              case DIBI_RIGHT:
                                   data->mouse_btn = 0;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    case DIET_AXISMOTION:
                         switch (evt->input.axis) {
                              case DIAI_X:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_x += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_x = evt->input.axisabs;
                                   mouse_event = true;
                                   break;
                              case DIAI_Y:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_y += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_y = evt->input.axisabs;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    default:
                         break;
               }
               break;

          case DFEC_WINDOW:
               switch (evt->window.type) {
                    case DWET_BUTTONDOWN:
                         switch (evt->window.button) {
                              case DIBI_LEFT:
                                   data->mouse_btn = 1;
                                   mouse_event = true;
                                   break;
                              case DIBI_MIDDLE:
                                   data->mouse_btn = 2;
                                   mouse_event = true;
                                   break;
                              case DIBI_RIGHT:
                                   data->mouse_btn = 3;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         if (mouse_event) {
                              data->mouse_x = evt->window.x;
                              data->mouse_y = evt->window.y;
                         }
                         break;
                    case DWET_BUTTONUP:
                         switch (evt->window.button) {
                              case DIBI_LEFT:
                              case DIBI_MIDDLE:
                              case DIBI_RIGHT:
                                   data->mouse_x = evt->window.x;
                                   data->mouse_y = evt->window.y;
                                   data->mouse_btn = 0;
                                   mouse_event = true;
                                   break;
                              default:
                                   break;
                         }
                         break;
                    case DWET_ENTER:
                    case DWET_MOTION:
                         data->mouse_x = evt->window.x;
                         data->mouse_y = evt->window.y;
                         mouse_event = true;
                         break;
                    case DWET_LEAVE:
                         data->mouse_x = -1;
                         data->mouse_y = -1;
                         data->mouse_btn = 0;
                         mouse_event = true;
                         break;
                    default:
                         break;
               }
               break;

          default:
               break;
     }

     if (mouse_event) {
          swfdec_decoder_set_mouse( data->decoder,
                                    data->mouse_x,
                                    data->mouse_y, 
                                    data->mouse_btn );
     }

     return DFB_OK;
} 

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     SwfdecDecoder *decoder;
     char*          buf;
     int            err;
     
     decoder = swfdec_decoder_new();
     if (!decoder)
          return DFB_INIT;
          
     buf = malloc( sizeof(ctx->header) );
     if (!buf) {
          swfdec_decoder_free( decoder );
          return DFB_NOSYSTEMMEMORY;
     }
     
     memcpy( buf, ctx->header, sizeof(ctx->header) );

     swfdec_decoder_add_data( decoder, buf, sizeof(ctx->header) );
     err = swfdec_decoder_parse( decoder );
     swfdec_decoder_free( decoder );
     
     return (err == SWF_ERROR) ? DFB_UNSUPPORTED : DFB_OK;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     DFBResult     ret;
     unsigned int  len = 0;
     void         *buf;
      
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Swfdec )
     
     data->ref    = 1;
     data->buffer = buffer;
     
     buffer->AddRef( buffer );

     buffer->GetLength( buffer, &len );
     if (len)
          len /= 5;          /* preload 20% of the stream */
     len = MAX( len, 8192 ); /* at least 8Kb */
     
     buf = malloc( len );
     if (!buf) {
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return D_OOM();
     }

     buffer->WaitForData( buffer, len );
     ret = buffer->GetData( buffer, len, buf, &len );
     if (ret) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "error fetching %d bytes from buffer!", len );
          free( buf );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return ret;
     }

     swfdec_init();

     data->decoder = swfdec_decoder_new();
     if (!data->decoder) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "couldn't create swf decoder.\n" );
          free( buf );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return DFB_INIT;
     }

     swfdec_decoder_set_colorspace( data->decoder, SWF_COLORSPACE_RGB888 );
    
     swfdec_decoder_add_data( data->decoder, buf, len );  
     if (swfdec_decoder_parse( data->decoder ) == SWF_ERROR) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "swfdec_decoder_parse() failed!\n" );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     swfdec_decoder_get_image_size( data->decoder,
                                    &data->width, &data->height );
     swfdec_decoder_get_rate( data->decoder, &data->rate );
     
     data->width    = data->width  ? : 1;
     data->height   = data->height ? : 1;
     data->rate     = data->rate   ? : 1;
     data->interval = 1000000.0/data->rate;

     D_DEBUG( "IDirectFBVideoProvider_Swfdec: "
              "width:%d height:%d rate:%.4f.\n",
              data->width, data->height, data->rate );

#ifdef HAVE_FUSIONSOUND
     if (idirectfb_singleton->GetInterface( idirectfb_singleton,
                                            "IFusionSound", 0, 0,
                                            (void**)&data->sound ) == DFB_OK)
     {
          FSStreamDescription dsc;

          dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                             FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.buffersize   = 11025;
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
     }
     else {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "failed to get FusionSound interface!\n" );
     }
#endif     
 
     direct_util_recursive_pthread_mutex_init( &data->input_lock );
     direct_util_recursive_pthread_mutex_init( &data->audio_lock );
     direct_util_recursive_pthread_mutex_init( &data->video_lock );
     pthread_cond_init( &data->video_cond, NULL );

     data->input_thread = direct_thread_create( DTT_DEFAULT, SwfInput,
                                                (void*)data, "Swf Input" );
     if (!data->input_thread) {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "direct_thread_create() failed!\n" );
          IDirectFBVideoProvider_Swfdec_Destruct( thiz );
          return DFB_FAILURE;
     }
     
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
     thiz->SendEvent             = IDirectFBVideoProvider_Swfdec_SendEvent;
     
     return DFB_OK;
}

