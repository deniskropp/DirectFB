/*
   Copyright (C) 2005-2008 Claudio Ciccani <klan@users.sf.net>

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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
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
#include <media/idirectfbdatabuffer.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
#endif

#include <libswfdec/swfdec.h>

#include <cairo.h>
#include <cairo-features.h>
#ifdef CAIRO_HAS_DIRECTFB_SURFACE
# include <cairo-directfb.h>
#endif



static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer  );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Swfdec )


typedef struct {
     DirectLink   link;
     
     void        *data;
} Link;

typedef struct {
     int                        ref;
     
     pthread_mutex_t            lock;
     
     SwfdecPlayer              *player;
     pthread_mutex_t            player_lock;

     int                        width;
     int                        height;
     double                     rate;
     double                     speed;
     float                      volume;
     
     DFBVideoProviderStatus     status;
     
     struct {
          DirectThread         *thread;
          pthread_mutex_t       lock;
          pthread_cond_t        cond;
     
          long                  pos;
          
          long                  seek;
          
          IDirectFBSurface     *dest;
#ifndef CAIRO_HAS_DIRECTFB_SURFACE
          IDirectFBSurface     *buffer;
#endif
          cairo_surface_t      *surface;
     } video;
     
     struct {
          DirectThread         *thread;
          pthread_mutex_t       lock;
          pthread_cond_t        cond;
          
          DirectLink           *streams;
          unsigned int          offset;
          
#ifdef HAVE_FUSIONSOUND
          IFusionSound         *sound;
          IFusionSoundStream   *stream;
          IFusionSoundPlayback *playback;
#endif
     } audio;

     int                        mouse_x;
     int                        mouse_y;
     int                        mouse_btn;
     
     DVFrameCallback            callback;
     void                      *ctx;
     
     DirectLink                *events;
     DFBVideoProviderEventType  events_mask;
     pthread_mutex_t            events_lock;
} IDirectFBVideoProvider_Swfdec_data;

/*****************************************************************************/

typedef struct _DataBufferLoader {
     SwfdecLoader         loader;
     
     IDirectFBDataBuffer *buffer;
     
     DirectThread        *thread;
} DataBufferLoader;

typedef struct _DataBufferLoaderClass {
     SwfdecLoaderClass    class;
} DataBufferLoaderClass;

G_DEFINE_TYPE (DataBufferLoader, databuffer_loader, SWFDEC_TYPE_LOADER)


static void*
SwfLoader( DirectThread *self, void *arg )
{
      DataBufferLoader *d = arg;
      char              buf[4096];
      unsigned int      len;
      
      while (1) {
          SwfdecBuffer *b;
          
          d->buffer->WaitForData( d->buffer, sizeof(buf) );
          if (d->buffer->GetData( d->buffer, sizeof(buf), buf, &len ))
               break;
               
          b = swfdec_buffer_new_and_alloc( len );
          memcpy( b->data, buf, len );
          swfdec_loader_push( &d->loader, b );
     }
     
     swfdec_loader_eof( &d->loader );
     
     return NULL;
}      

static void
databuffer_loader_load( SwfdecLoader *loader, SwfdecLoader *parent, 
                        SwfdecLoaderRequest request, const char *data, gsize data_len)
{
     DataBufferLoader *d = (DataBufferLoader*)loader;
     SwfdecBuffer     *b;
     unsigned int      length;

     if (request == SWFDEC_LOADER_REQUEST_POST) {
          D_WARN( "SWFDEC_LOADER_REQUEST_POST not supported" );
          return;
     }

     D_ASSUME( d->buffer != NULL );
     if (!d->buffer) {
          DFBDataBufferDescription dsc;
          DFBResult                ret;
          
          dsc.flags = DBDESC_FILE;
          dsc.file  = swfdec_url_get_url( swfdec_loader_get_url(loader) );
          
          ret = idirectfb_singleton->CreateDataBuffer( idirectfb_singleton, &dsc, &d->buffer );
          if (ret) {
               D_DERROR( ret, "IDirectFBVideoProvider_Swfdec: CreateDataBuffer() failed!\n" );
               return;
          }
     }

     d->buffer->GetLength( d->buffer, &length );
     if (length)
          swfdec_loader_set_size( loader, length );
          
     swfdec_loader_open( loader, NULL );
          
     if (((IDirectFBDataBuffer_data*)d->buffer->priv)->is_memory) {
          IDirectFBDataBuffer_Memory_data *m = d->buffer->priv;

          b = swfdec_buffer_new_for_data( (void*)m->buffer, m->length );
          swfdec_loader_push( loader, b );
          swfdec_loader_eof( loader );
     }
     else {
          char         buf[4096];
          unsigned int len;
          
          d->buffer->WaitForData( d->buffer, sizeof(buf) );
          if (d->buffer->GetData( d->buffer, sizeof(buf), buf, &len )) {
               D_ERROR( "IDirectFBVideoProvider_Swfdec: GetData() failed!\n" );
               return;
          }
          
          b = swfdec_buffer_new_and_alloc( len );       
          memcpy( b->data, buf, len );
          swfdec_loader_push( &d->loader, b );
          
          if (!length || length > len)
               d->thread = direct_thread_create( DTT_DEFAULT, SwfLoader, d, "Swf Loader" );
          else
               swfdec_loader_eof( &d->loader );
     }
}

static void
databuffer_loader_close( SwfdecLoader *loader )
{
     DataBufferLoader *d = (DataBufferLoader*)loader;
     
     if (d->thread) {
          direct_thread_cancel( d->thread );
          direct_thread_join( d->thread );
          direct_thread_destroy( d->thread );
          d->thread = NULL;
     }

     if (d->buffer) {
          d->buffer->Release( d->buffer );
          d->buffer = NULL;
     }
}

static void
databuffer_loader_class_init( DataBufferLoaderClass *klass )
{
     SwfdecLoaderClass *loader_class = SWFDEC_LOADER_CLASS (klass);

     loader_class->load = databuffer_loader_load;
     loader_class->close = databuffer_loader_close;
}

static void
databuffer_loader_init( DataBufferLoader *loader )
{
}

SwfdecLoader*
databuffer_loader_new( IDirectFBDataBuffer *buffer )
{
     SwfdecLoader *loader;
     SwfdecURL    *url = NULL;
     char         *file;
     
     file = ((IDirectFBDataBuffer_data*)buffer->priv)->filename;
     
     if (file && access( file, F_OK ) == 0) {
          char *uri;
          if (*file != '/') {
               char buf[4096];
               uri = g_strconcat( "file://", getcwd( buf, sizeof(buf) ), "/", file, NULL );
          } else {
               uri = g_strconcat( "file://", file, NULL );
          }
          url = swfdec_url_new( uri );
          g_free( uri );
     }
     else {
          /* XXX: Swfdec crashes with non-standard URIs. */
          if (!file || (strncmp( file, "file://", 7 ) && strncmp( file, "http://", 7 ))) {
               char *uri = g_strconcat( "file://", file, NULL );
               url = swfdec_url_new( uri );
               g_free( uri );
          }
          else {
               url = swfdec_url_new( file );
          }
     }

     loader = g_object_new (databuffer_loader_get_type(), "url", url, NULL);
     swfdec_url_free( url );
     
     buffer->AddRef( buffer );
     ((DataBufferLoader*)loader)->buffer = buffer;
     
     databuffer_loader_load( loader, NULL, SWFDEC_LOADER_REQUEST_DEFAULT, NULL, 0 );
  
     return loader;
}

/*****************************************************************************/

static void
send_event( IDirectFBVideoProvider_Swfdec_data *data,
            DFBVideoProviderEventType           type )
{
     Link                  *link;
     DFBVideoProviderEvent  event;
     
     if (!data->events || !(data->events_mask & type))
          return;
         
     event.clazz = DFEC_VIDEOPROVIDER;
     event.type  = type;
     
     pthread_mutex_lock( &data->events_lock );
     
     direct_list_foreach (link, data->events) {
          IDirectFBEventBuffer *buffer = link->data;
          buffer->PostEvent( buffer, DFB_EVENT(&event) );
     }
     
     pthread_mutex_unlock( &data->events_lock );
}

/*****************************************************************************/

static inline void
getclock( struct timespec *ret )
{
#ifdef _POSIX_TIMERS
     clock_gettime( CLOCK_REALTIME, ret );
#else
     struct timeval t;
     gettimeofday( &t, NULL );
     ret->tv_sec  = t.tv_sec;
     ret->tv_nsec = t.tv_usec * 1000;
#endif
}

static void*
SwfVideo( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data = arg;
     long                                next = 0;
     
     send_event( data, DVPET_STARTED );
     
     while (data->status != DVSTATE_STOP) {
          struct timespec  s, t;
          cairo_t         *cr;
          u32              bg;
          int              w, h;
          
          getclock( &s );
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->video.lock );
          
          if (data->video.seek > data->video.pos) {
               next = data->video.seek - data->video.pos;
               data->video.seek = 0;
          }
          
          pthread_mutex_lock( &data->player_lock );
          swfdec_player_advance( data->player, next );
          next = swfdec_player_get_next_event( data->player );
          pthread_mutex_unlock( &data->player_lock );
          
          bg = swfdec_player_get_background_color( data->player );   
#ifndef CAIRO_HAS_DIRECTFB_SURFACE
          data->video.buffer->Clear( data->video.buffer, bg >> 16, bg >> 8, bg, bg >> 24 );
          w = data->width; h = data->height;
#else
          data->video.dest->SetClip( data->video.dest, NULL );
          data->video.dest->SetDrawingFlags( data->video.dest, DSDRAW_NOFX );
          data->video.dest->Clear( data->video.dest, bg >> 16, bg >> 8, bg, bg >> 24 );
          data->video.dest->GetSize( data->video.dest, &w, &h );
#endif
    
          cr = cairo_create( data->video.surface );
          swfdec_player_render( data->player, cr, 0, 0, w, h );
          cairo_destroy( cr );
          
#ifndef CAIRO_HAS_DIRECTFB_SURFACE
          data->video.dest->StretchBlit( data->video.dest,
                                         data->video.buffer, NULL, NULL );
#endif
          
          data->video.pos += next;
          
          if (data->callback)
               data->callback( data->ctx );
          
          if (next < 0) {
               data->status = DVSTATE_FINISHED;
               send_event( data, DVPET_FINISHED );
               pthread_cond_wait( &data->video.cond, &data->video.lock );
               next = 0;
          }
          else {
               if (!data->speed) {
                    pthread_cond_wait( &data->video.cond, &data->video.lock );
               }
               else {     
                    if (data->speed != 1.0)
                         next = (double)next / data->speed;
                     
                    t.tv_nsec = s.tv_nsec + (next % 1000) * 1000000;
                    t.tv_sec  = s.tv_sec + next / 1000 + t.tv_nsec / 1000000000;
                    t.tv_nsec %= 1000000000;
               
                    pthread_cond_timedwait( &data->video.cond, &data->video.lock, &t );
                    
                    getclock( &t );
                    next = (t.tv_sec - s.tv_sec) * 1000 + 
                           (t.tv_nsec - s.tv_nsec + 500000) / 1000000;
                    if (data->speed != 1.0)
                         next = (double)next * data->speed;
               }
          }
          
          pthread_mutex_unlock( &data->video.lock );
     }
     
     return NULL;
}

#ifdef HAVE_FUSIONSOUND
static void
audio_advance( SwfdecPlayer *player, guint msecs, guint samples,
               IDirectFBVideoProvider_Swfdec_data *data )
{
     if (samples >= data->audio.offset)
          data->audio.offset = 0;
     else
          data->audio.offset -= samples;
}

static void
audio_added( SwfdecPlayer *player, SwfdecAudio *audio,
             IDirectFBVideoProvider_Swfdec_data *data )
{
     Link *link;
     
     link = D_MALLOC( sizeof(Link) );
     if (!link) {
          D_OOM();
          return;
     }
     
     g_object_ref( audio );
     link->data = audio;
     
     pthread_mutex_lock( &data->audio.lock );
     
     direct_list_append( &data->audio.streams, &link->link );
     pthread_cond_signal( &data->audio.cond );
     
     pthread_mutex_unlock( &data->audio.lock );
}

static void
audio_removed (SwfdecPlayer *player, SwfdecAudio *audio,
               IDirectFBVideoProvider_Swfdec_data *data )
{
     Link *link;
     
     pthread_mutex_lock( &data->audio.lock );
     
     direct_list_foreach (link, data->audio.streams) {
          if (link->data == (void*)audio) {
               direct_list_remove( &data->audio.streams, &link->link );
               g_object_unref( link->data );
               D_FREE( link );
               break;
          }
     }
     
     pthread_mutex_unlock( &data->audio.lock );
}

static void*
SwfAudio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Swfdec_data *data = arg;
     s16                                 buf[1152*2];
     
     while (data->status != DVSTATE_STOP) {
          Link         *link;
          unsigned int  offset;
          
          direct_thread_testcancel( self );
          
          pthread_mutex_lock( &data->audio.lock );
          
          if (!data->speed) {
               pthread_cond_wait( &data->audio.cond, &data->audio.lock );
               pthread_mutex_unlock( &data->audio.lock );
               continue;
          }
          
          if (!data->audio.streams) {
               pthread_cond_wait( &data->audio.cond, &data->audio.lock );
               pthread_mutex_unlock( &data->audio.lock );
               continue;
          }
          
          memset( buf, 0, sizeof(buf) );
          
          offset = data->audio.offset;
          direct_list_foreach (link, data->audio.streams)
               swfdec_audio_render( (SwfdecAudio*)link->data, buf, offset, sizeof(buf)/4 );
          data->audio.offset += sizeof(buf)/4;

          pthread_mutex_unlock( &data->audio.lock );
        
          data->audio.stream->Write( data->audio.stream, buf, sizeof(buf)/4 );
     }
     
     return NULL;
}
#endif

/*****************************************************************************/

static void
IDirectFBVideoProvider_Swfdec_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Swfdec_data *data = thiz->priv;
     
     if (data->video.thread) {
          direct_thread_cancel( data->video.thread );
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
     }
     
     if (data->audio.thread) {
          direct_thread_cancel( data->audio.thread );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
     }
       
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback)
          data->audio.playback->Release( data->audio.playback );
     
     if (data->audio.stream)
          data->audio.stream->Release( data->audio.stream );
     
     if (data->audio.sound)
          data->audio.sound->Release( data->audio.sound );
    
     while (data->audio.streams) {
          Link *link = (Link*)data->audio.streams; 
          direct_list_remove( &data->audio.streams, &link->link );
          g_object_unref( link->data );
          D_FREE( link );
     }
#endif

     if (data->player)
          g_object_unref( data->player );
     
     if (data->video.surface)
          cairo_surface_destroy( data->video.surface );
          
#ifndef CAIRO_HAS_DIRECTFB_SURFACE
     if (data->video.buffer)
          data->video.buffer->Release( data->video.buffer );
#endif
          
     if (data->video.dest)
          data->video.dest->Release( data->video.dest );
          
     while (data->events) {
          Link                 *link   = (Link*)data->events;
          IDirectFBEventBuffer *buffer = link->data;
          direct_list_remove( &data->events, &link->link );
          buffer->Release( buffer );
          D_FREE( link );
     }          

     pthread_mutex_destroy( &data->lock );
     pthread_mutex_destroy( &data->player_lock );
     pthread_mutex_destroy( &data->video.lock );
     pthread_mutex_destroy( &data->audio.lock );
     pthread_mutex_destroy( &data->events_lock );
     pthread_cond_destroy( &data->video.cond );
     pthread_cond_destroy( &data->audio.cond );
     
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
          
     *caps = DVCAPS_BASIC | DVCAPS_SEEK | DVCAPS_SCALE | DVCAPS_SPEED | DVCAPS_INTERACTIVE;
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback)
          *caps |= DVCAPS_VOLUME;
#endif
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                     DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!desc)
          return DFB_INVARG;
          
     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = DSPF_ARGB;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                    DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!desc)
          return DFB_INVARG;
     
     desc->caps = DVSCAPS_VIDEO | DVSCAPS_AUDIO; /* assuming audio */

     snprintf( desc->video.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "Shockwave Flash" );
     desc->video.framerate = data->rate;
     desc->video.aspect    = 0;
     desc->video.bitrate   = 0;

     snprintf( desc->audio.encoding,
               DFB_STREAM_DESC_ENCODING_LENGTH, "MP3" );
     desc->audio.samplerate = 44100;
     desc->audio.channels   = 2;
     desc->audio.bitrate    = 0;

     desc->title[0] = desc->author[0] = desc->album[0]   =
     desc->year     = desc->genre[0]  = desc->comment[0] = 0;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_PlayTo( IDirectFBVideoProvider *thiz,
                                      IDirectFBSurface       *dest,
                                      const DFBRectangle     *dest_rect,
                                      DVFrameCallback         callback,
                                      void                   *ctx )
{
     DFBResult         ret;
     IDirectFBSurface *dst;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!dest)
          return DFB_INVARG;

     ret = dest->GetSubSurface( dest, dest_rect, &dst );
     if (ret)
          return ret;
          
     pthread_mutex_lock( &data->lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
 
     if (data->video.dest)
          data->video.dest->Release( data->video.dest );
     data->video.dest = dst;
     
#ifdef CAIRO_HAS_DIRECTFB_SURFACE
     if (data->video.surface)
          cairo_surface_destroy( data->video.surface );
     data->video.surface = cairo_directfb_surface_create( idirectfb_singleton, dst );
#else 
     if (!data->video.surface) {
          DFBSurfaceDescription  dsc;
          void                  *ptr;
          int                    pitch;
          
          dsc.flags = DSDESC_CAPS | DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc.caps = DSCAPS_SYSTEMONLY;
          dsc.width = data->width;
          dsc.height = data->height;
          dsc.pixelformat = DSPF_ARGB;
          
          ret = idirectfb_singleton->CreateSurface( idirectfb_singleton, &dsc, &data->video.buffer );
          if (ret) {
               pthread_mutex_unlock( &data->video.lock );
               pthread_mutex_unlock( &data->audio.lock );
               pthread_mutex_unlock( &data->lock );
               return ret;
          }
          
          data->video.buffer->Lock( data->video.buffer, DSLF_WRITE, &ptr, &pitch );
          data->video.buffer->Unlock( data->video.buffer );
          
          data->video.surface = cairo_image_surface_create_for_data( ptr, CAIRO_FORMAT_ARGB32, 
                                                                     data->width, data->height, pitch );
     }
#endif

     data->callback = callback;
     data->ctx      = ctx;
     
     data->status = DVSTATE_PLAY;
     
     if (!data->video.thread)
          data->video.thread = direct_thread_create( DTT_DEFAULT, SwfVideo, (void*)data, "Swf Video" );
          
#ifdef HAVE_FUSIONSOUND
     if (!data->audio.thread && data->audio.stream)
         data->audio.thread = direct_thread_create( DTT_DEFAULT, SwfAudio, (void*)data, "Swf Audio" );
#endif

     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->lock );
          
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     pthread_mutex_lock( &data->lock );
     
     if (data->status == DVSTATE_STOP) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }
 
     data->status = DVSTATE_STOP;
     
     if (data->video.thread) {
          pthread_cond_signal( &data->video.cond );
          direct_thread_join( data->video.thread );
          direct_thread_destroy( data->video.thread );
          data->video.thread = NULL;
     }
          
     if (data->audio.thread) {
          pthread_cond_signal( &data->audio.cond );
          direct_thread_join( data->audio.thread );
          direct_thread_destroy( data->audio.thread );
          data->video.thread = NULL;
     }
       
     if (data->video.surface) {
          cairo_surface_destroy( data->video.surface );
          data->video.surface = NULL;
     }
     
#ifndef CAIRO_HAS_DIRECTFB_SURFACE
     if (data->video.buffer) {
          data->video.buffer->Release( data->video.buffer );
          data->video.buffer = NULL;
     }
#endif
     
     if (data->video.dest) {
          data->video.dest->Release( data->video.dest );
          data->video.dest = NULL;
     }
     
     send_event( data, DVPET_STOPPED );
     
     pthread_mutex_unlock( &data->lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetStatus( IDirectFBVideoProvider *thiz,
                                         DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!status)
          return DFB_INVARG;

     *status = data->status;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SeekTo( IDirectFBVideoProvider *thiz,
                                      double                  seconds )
{
     long msecs;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (seconds < 0.0)
          return DFB_INVARG;
          
     msecs = seconds * 1000.0;
        
     pthread_mutex_lock( &data->video.lock );
     
     if (data->video.pos > msecs) {
          pthread_mutex_unlock( &data->video.lock );
          return DFB_UNSUPPORTED;
     }
     
     data->video.seek = msecs;
     
     pthread_cond_signal( &data->video.cond );
     
     pthread_mutex_unlock( &data->video.lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetPos( IDirectFBVideoProvider *thiz,
                                      double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!seconds)
          return DFB_INVARG;

     *seconds = (double)data->video.pos / 1000.0;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetLength( IDirectFBVideoProvider *thiz,
                                         double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
          
     if (!seconds)
          return DFB_INVARG;
     
     *seconds = 0;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SendEvent( IDirectFBVideoProvider *thiz,
                                         const DFBEvent         *evt )
{
     int    w, h;
     double x, y;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )

     if (!evt)
          return DFB_INVARG;
          
     pthread_mutex_lock( &data->lock );
     
     if (!data->video.dest) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }
     
     data->video.dest->GetSize( data->video.dest, &w, &h );
     x = (double)data->width / w;
     y = (double)data->height / h;
          
     switch (evt->clazz) {
          case DFEC_INPUT:
               switch (evt->input.type) {
                    case DIET_BUTTONPRESS:
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_press( data->player, 
                                                    data->mouse_x*x, data->mouse_y*y,
                                                    evt->input.button+1 );
                         pthread_mutex_unlock( &data->player_lock );
                         break; 
                    case DIET_BUTTONRELEASE:
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_release( data->player,
                                                      data->mouse_x*x, data->mouse_y*y,
                                                      evt->input.button+1 );
                         pthread_mutex_unlock( &data->player_lock );
                         break;
                    case DIET_AXISMOTION:
                         switch (evt->input.axis) {
                              case DIAI_X:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_x += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_x = evt->input.axisabs;
                                   pthread_mutex_lock( &data->player_lock );
                                   swfdec_player_mouse_move( data->player, 
                                                             data->mouse_x*x, data->mouse_y*y );
                                   pthread_mutex_unlock( &data->player_lock );
                                   break;
                              case DIAI_Y:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_y += evt->input.axisrel;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_y = evt->input.axisabs;
                                   pthread_mutex_lock( &data->player_lock );
                                   swfdec_player_mouse_move( data->player, 
                                                             data->mouse_x*x, data->mouse_y*y );
                                   pthread_mutex_unlock( &data->player_lock );
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
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_press( data->player, 
                                                    evt->window.x*x, evt->window.y*y,
                                                    evt->window.button+1 );
                         pthread_mutex_unlock( &data->player_lock );
                         break;
                    case DWET_BUTTONUP:
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_release( data->player, 
                                                      evt->window.x*x, evt->window.y*y,
                                                      evt->window.button+1 );
                         pthread_mutex_unlock( &data->player_lock );
                         break;
                    case DWET_ENTER:
                    case DWET_MOTION:
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_move( data->player, 
                                                   evt->window.x*x, evt->window.y*y );
                         pthread_mutex_unlock( &data->player_lock );
                         break;
                    case DWET_LEAVE:
                         pthread_mutex_lock( &data->player_lock );
                         swfdec_player_mouse_move( data->player, -1, -1 );
                         pthread_mutex_unlock( &data->player_lock );
                         break;
                    default:
                         break;
               }
               break;

          default:
               break;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetSpeed( IDirectFBVideoProvider *thiz,
                                        double                  multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
          
     if (multiplier > 64.0)
          return DFB_UNSUPPORTED;
          
     pthread_mutex_lock( &data->lock );
     pthread_mutex_lock( &data->video.lock );
     pthread_mutex_lock( &data->audio.lock );
     
     if (multiplier && multiplier < 0.01)
          multiplier = 0.01;
     
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback)
          data->audio.playback->SetPitch( data->audio.playback, multiplier );
#endif

     if (multiplier > data->speed) {
          pthread_cond_signal( &data->video.cond );
          pthread_cond_signal( &data->audio.cond );
     }
          
     data->speed = multiplier;
     
     send_event( data, DVPET_SPEEDCHANGE );
     
     pthread_mutex_unlock( &data->video.lock );
     pthread_mutex_unlock( &data->audio.lock );
     pthread_mutex_unlock( &data->lock );  
     
     return DFB_OK;
}

static DFBResult 
IDirectFBVideoProvider_Swfdec_GetSpeed( IDirectFBVideoProvider *thiz,
                                        double                 *ret_multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!ret_multiplier)
          return DFB_INVARG;
          
     *ret_multiplier = data->speed;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_SetVolume( IDirectFBVideoProvider *thiz,
                                         float                   level )
{
     DFBResult ret = DFB_UNSUPPORTED;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (level < 0.0)
          return DFB_INVARG;
          
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback) {
          ret = data->audio.playback->SetVolume( data->audio.playback, level );
          if (ret == DFB_OK)
               data->volume = level;
     }
#endif

     return ret;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_GetVolume( IDirectFBVideoProvider *thiz,
                                         float                  *ret_level )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!ret_level)
          return DFB_INVARG;

     *ret_level = data->volume;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_CreateEventBuffer( IDirectFBVideoProvider  *thiz,
                                                 IDirectFBEventBuffer   **ret_buffer )
{
     IDirectFBEventBuffer *buffer;
     DFBResult             ret;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!ret_buffer)
          return DFB_INVARG;
          
     ret = idirectfb_singleton->CreateEventBuffer( idirectfb_singleton, &buffer );
     if (ret)
          return ret;
          
     ret = thiz->AttachEventBuffer( thiz, buffer );
     
     buffer->Release( buffer );
     
     *ret_buffer = (ret == DFB_OK) ? buffer : NULL;
     
     return ret;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_AttachEventBuffer( IDirectFBVideoProvider *thiz,
                                                 IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret;
     Link      *link;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!buffer)
          return DFB_INVARG;
     
     ret = buffer->AddRef( buffer );
     if (ret)
          return ret;
     
     link = D_MALLOC( sizeof(Link) );
     if (!link) {
          buffer->Release( buffer );
          return D_OOM();
     }
     
     link->data = buffer;
     
     pthread_mutex_lock( &data->events_lock );
     direct_list_append( &data->events, &link->link );
     pthread_mutex_unlock( &data->events_lock );
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_DetachEventBuffer( IDirectFBVideoProvider *thiz,
                                                 IDirectFBEventBuffer   *buffer )
{
     DFBResult  ret = DFB_ITEMNOTFOUND;
     Link      *link;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (!buffer)
          return DFB_INVARG;
     
     pthread_mutex_lock( &data->events_lock );
     
     direct_list_foreach (link, data->events) {
          if (link->data == (void*)buffer) {
               IDirectFBEventBuffer *buffer = link->data;
               direct_list_remove( &data->events, &link->link );
               buffer->Release( buffer );
               D_FREE( link );
               ret = DFB_OK;
               break;
          }
     }

     pthread_mutex_unlock( &data->events_lock );
     
     return ret;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_EnableEvents( IDirectFBVideoProvider    *thiz,
                                            DFBVideoProviderEventType  mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (mask & ~DVPET_ALL)
          return DFB_INVARG;
          
     data->events_mask |= mask;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Swfdec_DisableEvents( IDirectFBVideoProvider    *thiz,
                                             DFBVideoProviderEventType  mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Swfdec )
     
     if (mask & ~DVPET_ALL)
          return DFB_INVARG;
          
     data->events_mask &= ~mask;
     
     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     if ((ctx->header[0] == 'F' || ctx->header[0] == 'C') &&
         (ctx->header[1] == 'W' && ctx->header[2] == 'S'))
          return DFB_OK;
    
     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     SwfdecLoader *loader;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Swfdec )
     
     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->speed  = 1.0;
     data->volume = 1.0;
     
     if (!g_thread_supported())
          g_thread_init( NULL );
     
     swfdec_init();
     
     loader = databuffer_loader_new( buffer );   
     if (!loader) {
        D_ERROR( "IDirectFBVideoProvider_Swfdec: Couldn't initialize loader!\n" );
        IDirectFBVideoProvider_Swfdec_Destruct( thiz );
        return DFB_FAILURE;
     }
     
     data->player = swfdec_player_new( NULL );
     if (!data->player) {
        D_ERROR( "IDirectFBVideoProvider_Swfdec: Couldn't initialize player!\n" );
        g_object_unref( loader );
        IDirectFBVideoProvider_Swfdec_Destruct( thiz );
        return DFB_FAILURE;
     }
     
     swfdec_player_set_loader( data->player, loader );

#ifdef HAVE_FUSIONSOUND
     if (idirectfb_singleton->GetInterface( idirectfb_singleton,
                                            "IFusionSound", 0, 0,
                                            (void**)&data->audio.sound ) == DFB_OK)
     {
          FSStreamDescription dsc;

          dsc.flags        = FSSDF_BUFFERSIZE   | FSSDF_CHANNELS  |
                             FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.buffersize   = 4410;
          dsc.channels     = 2;
          dsc.samplerate   = 44100;
          dsc.sampleformat = FSSF_S16;

          if (data->audio.sound->CreateStream( data->audio.sound, &dsc,
                                               &data->audio.stream ) == DFB_OK) {
               data->audio.stream->GetPlayback( data->audio.stream, 
                                                &data->audio.playback );
               
               g_signal_connect( data->player, "advance", G_CALLBACK(audio_advance), data );
               g_signal_connect( data->player, "audio-added", G_CALLBACK(audio_added), data );
               g_signal_connect( data->player, "audio-removed", G_CALLBACK(audio_removed), data );
          }
          else {
               D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                        "IFusionSound::CreateStream() failed!\n" );
               data->audio.sound->Release( data->audio.sound );
               data->audio.sound = NULL;
          }
     }
     else {
          D_ERROR( "IDirectFBVideoProvider_Swfdec: "
                   "failed to get FusionSound interface!\n" );
     }
#endif     
 
     direct_util_recursive_pthread_mutex_init( &data->lock );
     direct_util_recursive_pthread_mutex_init( &data->player_lock );
     direct_util_recursive_pthread_mutex_init( &data->video.lock );
     direct_util_recursive_pthread_mutex_init( &data->audio.lock );
     direct_util_recursive_pthread_mutex_init( &data->events_lock );
     pthread_cond_init( &data->video.cond, NULL );
     pthread_cond_init( &data->audio.cond, NULL );
     
     swfdec_player_advance( data->player, 0 );
     if (!swfdec_player_is_initialized( data->player )) {
        D_ERROR( "IDirectFBVideoProvider_Swfdec: Player didn't initialize itself!\n" );
        IDirectFBVideoProvider_Swfdec_Destruct( thiz );
        return DFB_FAILURE;
     }
     
     data->rate = swfdec_player_get_rate( data->player );
     swfdec_player_get_default_size( data->player, 
                                    (guint*)&data->width, (guint*)&data->height );
     data->width  = data->width  ? : 1;
     data->height = data->height ? : 1;

     D_DEBUG( "IDirectFBVideoProvider_Swfdec: %dx%d at %.3ffps.\n",
              data->width, data->height, data->rate );
     
     thiz->AddRef                = IDirectFBVideoProvider_Swfdec_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Swfdec_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_Swfdec_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_Swfdec_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_Swfdec_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_Swfdec_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Swfdec_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_Swfdec_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_Swfdec_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Swfdec_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Swfdec_GetLength;
     thiz->SendEvent             = IDirectFBVideoProvider_Swfdec_SendEvent;
     thiz->SetSpeed              = IDirectFBVideoProvider_Swfdec_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_Swfdec_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_Swfdec_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_Swfdec_GetVolume;
     thiz->CreateEventBuffer     = IDirectFBVideoProvider_Swfdec_CreateEventBuffer;
     thiz->AttachEventBuffer     = IDirectFBVideoProvider_Swfdec_AttachEventBuffer;
     thiz->DetachEventBuffer     = IDirectFBVideoProvider_Swfdec_DetachEventBuffer;
     thiz->EnableEvents          = IDirectFBVideoProvider_Swfdec_EnableEvents;
     thiz->DisableEvents         = IDirectFBVideoProvider_Swfdec_DisableEvents;
     
     return DFB_OK;
}

