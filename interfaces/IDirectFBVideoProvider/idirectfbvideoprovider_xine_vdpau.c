/*
 * Copyright (C) 2004-2007 Claudio Ciccani <klan@users.sf.net>
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

#define DIRECT_ENABLE_DEBUG

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <pthread.h>

#include <directfb.h>

#include <idirectfb.h>

#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surface.h>
#include <core/system.h>

#include <gfx/util.h>

#include <display/idirectfbsurface.h>

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/interface.h>
#include <direct/thread.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>
#include <xine/video_out_vdpau.h>

#include <x11vdpau/x11.h>

/* use new speed parameter if available */
#ifdef XINE_PARAM_FINE_SPEED
# undef  XINE_PARAM_SPEED
# undef  XINE_SPEED_NORMAL
# define XINE_PARAM_SPEED  XINE_PARAM_FINE_SPEED
# define XINE_SPEED_NORMAL XINE_FINE_SPEED_NORMAL
#endif


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer,
           CoreDFB                *core );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Xine )


D_DEBUG_DOMAIN( XineDFB_VDPAU, "XineDFB/VDPAU", "Xine DFB VDPAU" );

/************************** Driver Specific Data ******************************/


typedef void (*DVOutputCallback) ( void                  *cdata,
                                   int                    width,
                                   int                    height,
                                   double                 ratio,
                                   DFBSurfacePixelFormat  format,
                                   DFBRectangle          *dest_rect );

typedef struct {
     IDirectFBSurface *destination;
     IDirectFBSurface *subpicture;

     DVOutputCallback  output_cb;
     void             *output_cdata;

     DVFrameCallback  frame_cb;
     void            *frame_cdata;
} dfb_visual_t;


/**************************** VideoProvider Data ******************************/

typedef struct {
     int                            ref;

     CoreDFB                       *core;
     
     pthread_mutex_t                lock;

     char                          *mrl;
     int                            mrl_changed;
     
     char                          *cfg;
     char                          *pipe;

     xine_t                        *xine;
     xine_video_port_t             *vo;
     xine_audio_port_t             *ao;
     xine_post_t                   *post;
     xine_stream_t                 *stream;
     xine_event_queue_t            *queue;
     int                            start_time;
     int                            speed;

     IDirectFBSurface              *destination;
     x11_visual_vdpau_t             visual;
    
     DFBSurfacePixelFormat          format; // video format
     int                            width;  // video width
     int                            height; // video height
     int                            length; // duration

     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     
     bool                           full_area;
     DFBRectangle                   dest_rect;
     CoreSurface                   *dest_surface;
   
     int                            mouse_x;
     int                            mouse_y;
     
     IDirectFBDataBuffer           *buffer;
     DirectThread                  *buffer_thread;

     IDirectFBEventBuffer          *events;

     DVFrameCallback                callback;
     void                          *callback_ctx;
} IDirectFBVideoProvider_Xine_data;


/***************************** Private Functions ******************************/

static DFBResult
get_stream_error( IDirectFBVideoProvider_Xine_data *data );

static void
send_videoprovider_event( IDirectFBVideoProvider_Xine_data *data,
                          DFBVideoProviderEventType         type );

static void
event_listener( void *cdata, const xine_event_t *event );

static DFBResult
make_pipe( char **ret_path );

static void
XineDFB_VDPAU_init( x11_visual_vdpau_t *vdpau,
                    void               *data );

/***************************** DataBuffer Thread ******************************/

void*
BufferThread( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_Xine_data *data   = arg;
     IDirectFBDataBuffer              *buffer = data->buffer;
     int                               fd;

     fd = open( data->pipe, O_WRONLY );
     if (fd < 0) {
          D_PERROR( "IDirectFBVideoProvider_Xine: "
                    "failed to open fifo '%s'\n", data->pipe );
          return (void*)1;
     }

     while (!direct_thread_is_canceled( self )) {
          DFBResult     ret;
          char          buf[4096];
          unsigned int  len = 0;

          buffer->WaitForDataWithTimeout( buffer, sizeof(buf), 0, 1 );
          ret = buffer->GetData( buffer, sizeof(buf), buf, &len );
          if (ret == DFB_OK && len)
               write( fd, buf, len );

          if (ret == DFB_EOF)
               break;
     }

     close( fd );

     return (void*)0;
}     

/******************************* Public Methods *******************************/

static void
IDirectFBVideoProvider_Xine_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Xine_data *data = thiz->priv;

     if (data->xine) {
          if (data->stream) {              
               xine_stop( data->stream );

               xine_close( data->stream );

               if (data->queue)
                    xine_event_dispose_queue( data->queue );
               
               xine_dispose( data->stream );
          }

          if (data->post)
               xine_post_dispose( data->xine, data->post );

          if (data->vo)
               xine_close_video_driver( data->xine, data->vo );

          if (data->ao)
               xine_close_audio_driver( data->xine, data->ao );

          if (data->cfg) {
               xine_config_save( data->xine, data->cfg );
               D_FREE( data->cfg );
          }

          xine_exit( data->xine );
     }

     if (data->buffer_thread) {
          direct_thread_cancel( data->buffer_thread );
          direct_thread_join( data->buffer_thread );
          direct_thread_destroy( data->buffer_thread );
     }
 
     if (data->buffer)
          data->buffer->Release( data->buffer );

     if (data->pipe) {
          unlink( data->pipe );
          D_FREE( data->pipe );
     }

     if (data->events)
          data->events->Release( data->events );

     D_FREE( data->mrl );

     pthread_mutex_destroy( &data->lock );
     
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_Xine_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     pthread_mutex_lock( &data->lock );

     data->ref++;

     pthread_mutex_unlock( &data->lock );

     return DR_OK;
}

static DirectResult
IDirectFBVideoProvider_Xine_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     pthread_mutex_lock( &data->lock );

     if (--data->ref == 0)
          IDirectFBVideoProvider_Xine_Destruct( thiz );
     else
          pthread_mutex_unlock( &data->lock );

     return DR_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_CreateEventBuffer( IDirectFBVideoProvider  *thiz,
                                                 IDirectFBEventBuffer   **ret_buffer )
{
     IDirectFBEventBuffer *buffer;
     DFBResult             ret;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
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
IDirectFBVideoProvider_Xine_AttachEventBuffer( IDirectFBVideoProvider *thiz,
                                               IDirectFBEventBuffer   *events )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     pthread_mutex_lock( &data->lock );

     if (data->events) {
          pthread_mutex_unlock( &data->lock );
          return DFB_BUSY;
     }

     ret = events->AddRef( events );
     if (ret == DFB_OK)
          data->events = events;

     pthread_mutex_unlock( &data->lock );

     return ret;
}

static DFBResult
IDirectFBVideoProvider_Xine_DetachEventBuffer( IDirectFBVideoProvider *thiz,
                                               IDirectFBEventBuffer   *events )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     pthread_mutex_lock( &data->lock );

     if (!data->events) {
          pthread_mutex_unlock( &data->lock );
          return DFB_BUFFEREMPTY;
     }

     if (data->events != events) {
          pthread_mutex_unlock( &data->lock );
          return DFB_INVARG;
     }

     data->events = NULL;

     events->Release( events );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetCapabilities( IDirectFBVideoProvider       *thiz,
                                             DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (!caps)
          return DFB_INVARG;

     *caps = DVCAPS_BASIC       | DVCAPS_SCALE    | DVCAPS_SPEED      |
             DVCAPS_BRIGHTNESS  | DVCAPS_CONTRAST | DVCAPS_SATURATION |
             DVCAPS_INTERACTIVE | DVCAPS_VOLUME   | DVCAPS_EVENT;

     if (xine_get_stream_info( data->stream, XINE_STREAM_INFO_SEEKABLE ))
          *caps |= DVCAPS_SEEK;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetSurfaceDescription( IDirectFBVideoProvider *thiz,
                                                   DFBSurfaceDescription  *desc )
{
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!desc)
          return DFB_INVARG;
     
     if (!data->width || !data->height) {
          data->width  = xine_get_stream_info( data->stream,
                                               XINE_STREAM_INFO_VIDEO_WIDTH );
          data->height = xine_get_stream_info( data->stream,
                                               XINE_STREAM_INFO_VIDEO_HEIGHT );

          if (data->width < 1 || data->height < 1) {
               data->width  = 320;
               data->height = 240;
          }
     }

     desc->flags       = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT | DSDESC_CAPS);
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = data->format;
     desc->caps        = DSCAPS_PREMULTIPLIED;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetStreamDescription( IDirectFBVideoProvider *thiz,
                                                  DFBStreamDescription   *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!desc)
          return DFB_INVARG;

     desc->caps = DVSCAPS_NONE;

     if (xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_VIDEO )) {
          desc->caps |= DVSCAPS_VIDEO;
     
          direct_snputs( desc->video.encoding,
                         xine_get_meta_info( data->stream, XINE_META_INFO_VIDEOCODEC ) ?:"",
                         DFB_STREAM_DESC_ENCODING_LENGTH );
          desc->video.framerate = xine_get_stream_info( data->stream, 
                                                        XINE_STREAM_INFO_FRAME_DURATION );
          if (desc->video.framerate)
               desc->video.framerate = 90000.0 / desc->video.framerate;
          desc->video.aspect    = xine_get_stream_info( data->stream,
                                                        XINE_STREAM_INFO_VIDEO_RATIO ) / 10000.0;
          desc->video.bitrate   = xine_get_stream_info( data->stream,
                                                        XINE_STREAM_INFO_VIDEO_BITRATE );
     }

     if (xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_AUDIO )) {
          desc->caps |= DVSCAPS_AUDIO;

          direct_snputs( desc->audio.encoding,
                         xine_get_meta_info( data->stream, XINE_META_INFO_AUDIOCODEC ) ?:"",
                         DFB_STREAM_DESC_ENCODING_LENGTH );
          desc->audio.samplerate = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_SAMPLERATE );
          desc->audio.channels   = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_CHANNELS );
          desc->audio.bitrate    = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_BITRATE );
     }
               
     direct_snputs( desc->title,
                    xine_get_meta_info( data->stream, XINE_META_INFO_TITLE ) ?:"",
                    DFB_STREAM_DESC_TITLE_LENGTH );
     direct_snputs( desc->author,
                    xine_get_meta_info( data->stream, XINE_META_INFO_ARTIST ) ?:"",
                    DFB_STREAM_DESC_AUTHOR_LENGTH );
     direct_snputs( desc->album,
                    xine_get_meta_info( data->stream, XINE_META_INFO_ALBUM ) ?:"",
                    DFB_STREAM_DESC_ALBUM_LENGTH );
     direct_snputs( desc->genre,
                    xine_get_meta_info( data->stream, XINE_META_INFO_GENRE ) ?:"",
                    DFB_STREAM_DESC_GENRE_LENGTH );
     direct_snputs( desc->comment,
                    xine_get_meta_info( data->stream, XINE_META_INFO_COMMENT ) ?:"",
                    DFB_STREAM_DESC_COMMENT_LENGTH );
     desc->year = atoi( xine_get_meta_info( data->stream, XINE_META_INFO_YEAR ) ?:"" );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_PlayTo( IDirectFBVideoProvider *thiz,
                                    IDirectFBSurface       *dest,
                                    const DFBRectangle     *dest_rect,
                                    DVFrameCallback         callback,
                                    void                   *ctx )
{
     DFBResult              ret;
     IDirectFBSurface_data *dest_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!dest)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM( dest, dest_data, IDirectFBSurface );

     if (!dest_data->surface)
          return DFB_DESTROYED;

     if (dest_rect) {
          D_DEBUG_AT( XineDFB_VDPAU, "%s( data %p, dest %p, %d,%d-%dx%d )\n", __FUNCTION__,
                      data, dest, DFB_RECTANGLE_VALS( dest_rect ) );

          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
     }
     else {
          int width, height;

          dest->GetSize( dest, &width, &height );

          D_DEBUG_AT( XineDFB_VDPAU, "%s( data %p, dest %p [%dx%d] )\n", __FUNCTION__,
                      data, dest, width, height );
     }


     pthread_mutex_lock( &data->lock );


     if (data->dest_surface) {
          dfb_surface_unref( data->dest_surface );
          data->dest_surface = NULL;
     }

     ret = dfb_surface_ref( dest_data->surface );
     if (ret)
          goto error;

     data->dest_surface = dest_data->surface;


     if (dest_rect) {
          data->dest_rect = *dest_rect;
          data->full_area = false;
     } else {
          data->full_area = true;

          data->dest_rect.x = 0;
          data->dest_rect.y = 0;
          data->dest_rect.w = data->dest_surface->config.size.w;
          data->dest_rect.h = data->dest_surface->config.size.h;
     }

     data->callback     = callback;
     data->callback_ctx = ctx;

#if 0
     /* update visual */
     data->visual.destination = dest;
     data->visual.frame_cb    = callback;
     data->visual.frame_cdata = ctx;

     /* notify visual changes */
     if (!xine_port_send_gui_data( data->vo, 
                                   XINE_GUI_SEND_SELECT_VISUAL,
                                   (void*) &data->visual ))
     {
          pthread_mutex_unlock( &data->lock );
          return DFB_UNSUPPORTED;
     }
#endif

     if (data->status != DVSTATE_PLAY) {
          pthread_mutex_unlock( &data->lock );

          /* Holding lock here causes lock failures (avoided dead lock) in callbacks. */
          if (!xine_play( data->stream, 0, data->start_time ))
               return get_stream_error( data );

          pthread_mutex_lock( &data->lock );

          xine_set_param( data->stream, XINE_PARAM_SPEED, data->speed );
          usleep( 100 );
               
          xine_get_pos_length( data->stream,
                               NULL, NULL, &data->length );
                               
          data->status = DVSTATE_PLAY;

          send_videoprovider_event( data, DVPET_STARTED );
     }

error:
     pthread_mutex_unlock( &data->lock );

     return ret;
}

static DFBResult
IDirectFBVideoProvider_Xine_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     pthread_mutex_lock( &data->lock );

     if (data->status == DVSTATE_STOP) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

     if (data->status == DVSTATE_PLAY) {
          data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );

          xine_get_pos_length( data->stream, NULL, &data->start_time, NULL );
          xine_stop( data->stream );

          usleep( 50 );
     }

     data->status = DVSTATE_STOP;

     send_videoprovider_event( data, DVPET_STOPPED );

     dfb_surface_unref( data->dest_surface );
     data->dest_surface = NULL;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetStatus( IDirectFBVideoProvider *thiz,
                                       DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!status)
          return DFB_INVARG;

     *status = data->status;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_SeekTo( IDirectFBVideoProvider *thiz,
                                    double                  seconds )
{
     int offset;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
         
     if (seconds < 0.0)
          return DFB_INVARG;
          
     pthread_mutex_lock( &data->lock );

     if (!xine_get_stream_info( data->stream, XINE_STREAM_INFO_SEEKABLE )) {
          pthread_mutex_unlock( &data->lock );
          return DFB_UNSUPPORTED;
     }

     offset = (int) (seconds * 1000.0);
     if (data->length > 0 && offset > data->length) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

     if (data->status == DVSTATE_PLAY) {
          data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );

          pthread_mutex_unlock( &data->lock );

          if (!xine_play( data->stream, 0, offset ))
               return get_stream_error( data );

          pthread_mutex_lock( &data->lock );

          xine_set_param( data->stream, XINE_PARAM_SPEED, data->speed );

          usleep( 100 );
     }
     
     data->start_time = offset;

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetPos( IDirectFBVideoProvider *thiz,
                                    double                 *seconds )
{
     int pos = 0;
     int i;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
          
     if (!seconds)
          return DFB_INVARG;

     for (i = 5; i--;) {
          if (xine_get_pos_length( data->stream, NULL, &pos, NULL ))
               break;
               
          usleep( 1000 );
     }

     *seconds = (double)pos / 1000.0;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetLength( IDirectFBVideoProvider *thiz,
                                       double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
          
     if (!seconds)
          return DFB_INVARG;

     xine_get_pos_length( data->stream, NULL, NULL, &data->length );
          
     *seconds = (double)data->length / 1000.0;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetColorAdjustment( IDirectFBVideoProvider *thiz,
                                                DFBColorAdjustment     *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!adj)
          return DFB_INVARG;

     adj->flags      = (DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION);
     adj->brightness = xine_get_param( data->stream,
                                       XINE_PARAM_VO_BRIGHTNESS );
     adj->contrast   = xine_get_param( data->stream,
                                       XINE_PARAM_VO_CONTRAST );
     adj->saturation = xine_get_param( data->stream,
                                       XINE_PARAM_VO_SATURATION );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_SetColorAdjustment( IDirectFBVideoProvider   *thiz,
                                                const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!adj)
          return DFB_INVARG;

     if (adj->flags & DCAF_BRIGHTNESS)
          xine_set_param( data->stream,
                          XINE_PARAM_VO_BRIGHTNESS,
                          adj->brightness );

     if (adj->flags & DCAF_CONTRAST)
          xine_set_param( data->stream,
                          XINE_PARAM_VO_CONTRAST,
                          adj->contrast );

     if (adj->flags & DCAF_SATURATION)
          xine_set_param( data->stream,
                          XINE_PARAM_VO_SATURATION,
                          adj->saturation );

     return DFB_OK;
}

static int
translate_key( DFBInputDeviceKeySymbol key )
{
     switch (key) {
          case DIKS_F1:
               return XINE_EVENT_INPUT_MENU1;
          case DIKS_F2:
               return XINE_EVENT_INPUT_MENU2;
          case DIKS_F3:
               return XINE_EVENT_INPUT_MENU3;
          case DIKS_F4:
               return XINE_EVENT_INPUT_MENU4;
          case DIKS_F5:
               return XINE_EVENT_INPUT_MENU5;
          case DIKS_F6:
               return XINE_EVENT_INPUT_MENU6;
          case DIKS_F7:
               return XINE_EVENT_INPUT_MENU7;
          case DIKS_CURSOR_UP:
               return XINE_EVENT_INPUT_UP;
          case DIKS_CURSOR_DOWN:
               return XINE_EVENT_INPUT_DOWN;
          case DIKS_CURSOR_LEFT:
               return XINE_EVENT_INPUT_LEFT;
          case DIKS_CURSOR_RIGHT:
               return XINE_EVENT_INPUT_RIGHT;
          case DIKS_ENTER:
               return XINE_EVENT_INPUT_SELECT;
          case DIKS_PAGE_DOWN:
               return XINE_EVENT_INPUT_NEXT;
          case DIKS_PAGE_UP:
               return XINE_EVENT_INPUT_PREVIOUS;
          case DIKS_END:
               return XINE_EVENT_INPUT_ANGLE_NEXT;
          case DIKS_HOME:
               return XINE_EVENT_INPUT_ANGLE_PREVIOUS;
          case DIKS_BACKSPACE:
               return XINE_EVENT_INPUT_BUTTON_FORCE;
          case DIKS_0:
               return XINE_EVENT_INPUT_NUMBER_0;
          case DIKS_1:
               return XINE_EVENT_INPUT_NUMBER_1;
          case DIKS_2:
               return XINE_EVENT_INPUT_NUMBER_2;
          case DIKS_3:
               return XINE_EVENT_INPUT_NUMBER_3;
          case DIKS_4:
               return XINE_EVENT_INPUT_NUMBER_4;
          case DIKS_5:
               return XINE_EVENT_INPUT_NUMBER_5;
          case DIKS_6:
               return XINE_EVENT_INPUT_NUMBER_6;
          case DIKS_7:
               return XINE_EVENT_INPUT_NUMBER_7;
          case DIKS_8:
               return XINE_EVENT_INPUT_NUMBER_8;
          case DIKS_9:
               return XINE_EVENT_INPUT_NUMBER_9;
          case DIKS_PLUS_SIGN:
               return XINE_EVENT_INPUT_NUMBER_10_ADD;
          default:
               break;
     }
          
     return 0;
}

static DFBResult
IDirectFBVideoProvider_Xine_SendEvent( IDirectFBVideoProvider *thiz,
                                       const DFBEvent         *evt )
{
     xine_input_data_t  i;
     xine_event_t      *e = &i.event;
     int                dw, dh;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (!evt)
          return DFB_INVARG;

     pthread_mutex_lock( &data->lock );

     if (data->status == DVSTATE_FINISHED) {
          pthread_mutex_unlock( &data->lock );
          return DFB_OK;
     }

// FIXME     if (data->full_area) {
//          IDirectFBSurface *s = data->visual.destination;
//          s->GetSize( s, &dw, &dh );
//     } else {
          dw = data->dest_rect.w;
          dh = data->dest_rect.h;
//     }    

     pthread_mutex_unlock( &data->lock );

     e->type = 0;

     switch (evt->clazz) {
          case DFEC_INPUT:
               switch (evt->input.type) {
                    case DIET_KEYPRESS:
                         e->type = translate_key( evt->input.key_symbol );
                         break;
                    case DIET_BUTTONPRESS:
                         e->type = XINE_EVENT_INPUT_MOUSE_BUTTON;
                         switch (evt->input.button) {
                              case DIBI_LEFT:
                                   i.button = 1;
                                   break;
                              case DIBI_MIDDLE:
                                   i.button = 2;
                                   break;
                              case DIBI_RIGHT:
                                   i.button = 3;
                                   break;
                              default:
                                   e->type = 0;
                                   break;
                         }
                         i.x = data->mouse_x;
                         i.y = data->mouse_y;
                         break;
                    case DIET_AXISMOTION: 
                         e->type = XINE_EVENT_INPUT_MOUSE_MOVE;
                         switch (evt->input.axis) {
                              case DIAI_X:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_x += evt->input.axisrel *
                                                         data->width / dw;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_x = evt->input.axisabs *
                                                         data->width / dw;
                                   break;
                              case DIAI_Y:
                                   if (evt->input.flags & DIEF_AXISREL)
                                        data->mouse_y += evt->input.axisabs *
                                                         data->height / dh;
                                   if (evt->input.flags & DIEF_AXISABS)
                                        data->mouse_y = evt->input.axisabs *
                                                         data->height / dh;
                                   break;
                              default:
                                   e->type = 0;
                                   break;
                         }
                         i.x = data->mouse_x;
                         i.y = data->mouse_y;
                         break;
                    default:
                         break;
               }
               break;

          case DFEC_WINDOW:
               switch (evt->window.type) {
                    case DWET_KEYDOWN:
                         e->type = translate_key( evt->input.key_symbol );
                         break;
                    case DWET_BUTTONDOWN:
                         e->type = XINE_EVENT_INPUT_MOUSE_BUTTON;
                         switch (evt->window.button) {
                              case DIBI_LEFT:
                                   i.button = 1;
                                   break;
                              case DIBI_MIDDLE:
                                   i.button = 2;
                                   break;
                              case DIBI_RIGHT:
                                   i.button = 3;
                                   break;
                              default:
                                   e->type = 0;
                                   break;
                         }
                         i.x = evt->window.x * data->width  / dw;
                         i.y = evt->window.y * data->height / dh;
                         break;
                    case DWET_MOTION:
                         e->type = XINE_EVENT_INPUT_MOUSE_MOVE;
                         i.x = evt->window.x * data->width  / dw;
                         i.y = evt->window.y * data->height / dh;
                         break;
                    default:
                         break;
               }
               break;

          default:
               break;
     }
   
     if (e->type) {
          e->stream      = data->stream;
          e->data        = NULL;
          e->data_length = 0;
          gettimeofday( &e->tv, NULL );

          if (e->type == XINE_EVENT_INPUT_MOUSE_MOVE  ||
              e->type == XINE_EVENT_INPUT_MOUSE_BUTTON) {
               e->data        = (void*) e;
               e->data_length = sizeof(xine_input_data_t);
          }

          xine_event_send( data->stream, e );
     }

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_SetPlaybackFlags( IDirectFBVideoProvider        *thiz,
                                              DFBVideoProviderPlaybackFlags  flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (flags & ~DVPLAY_LOOPING)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_SetSpeed( IDirectFBVideoProvider *thiz,
                                      double                  multiplier )
{    
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (multiplier < 0.0)
          return DFB_INVARG;
          
     if (multiplier > 32.0)
          return DFB_UNSUPPORTED;
     
     xine_set_param( data->stream, XINE_PARAM_SPEED,
                     (multiplier*XINE_SPEED_NORMAL+.5) );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetSpeed( IDirectFBVideoProvider *thiz,
                                      double                 *ret_multiplier )
{
     int speed;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (!ret_multiplier)
          return DFB_INVARG;
          
     speed = xine_get_param( data->stream, XINE_PARAM_SPEED );

     *ret_multiplier = (double)speed / (double)XINE_SPEED_NORMAL;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_SetVolume( IDirectFBVideoProvider *thiz,
                                       float                   level )
{
     int vol, amp;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (level < 0.0)
          return DFB_INVARG;
          
     if (level > 2.0)
          return DFB_UNSUPPORTED;
          
     if (level > 1.0) {
          vol = 100;
          amp = (level*100.0);
     }
     else {
          vol = (level*100.0);
          amp = 100;
     }
     
     xine_set_param( data->stream, XINE_PARAM_AUDIO_VOLUME, vol );
     xine_set_param( data->stream, XINE_PARAM_AUDIO_AMP_LEVEL, amp );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetVolume( IDirectFBVideoProvider *thiz,
                                       float                  *ret_level )
{
     int vol, amp;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (!ret_level)
          return DFB_INVARG;

     vol = xine_get_param( data->stream, XINE_PARAM_AUDIO_VOLUME );
     amp = xine_get_param( data->stream, XINE_PARAM_AUDIO_AMP_LEVEL );

     *ret_level = (float)vol/100.0 * (float)amp/100.0;
     
     return DFB_OK;
}
     
/****************************** Exported Symbols ******************************/
     
static void
dummy_frame_output_cb( void *user_data,
                       int video_width, int video_height,
                       double video_pixel_aspect,
                       int *dest_x, int *dest_y,
                       int *dest_width, int *dest_height,
                       double *dest_pixel_aspect,
                       int *win_x, int *win_y )
{
     IDirectFBVideoProvider_Xine_data *data = user_data;

     D_DEBUG_AT( XineDFB_VDPAU, "%s( data %p, video %dx%d, aspect %.2f )\n", __FUNCTION__,
                 data, video_width, video_height, video_pixel_aspect );

     if (!data)
          return;

     pthread_mutex_lock( &data->lock );

     if (!data->dest_surface) {
          pthread_mutex_unlock( &data->lock );
          return;
     }

     D_MAGIC_ASSERT( data->dest_surface, CoreSurface );

     if (/*data->format != format ||*/ data->width != video_width || data->height != video_height) {
          //data->format = format;
          data->width  = video_width;
          data->height = video_height;

          send_videoprovider_event( data, DVPET_SURFACECHANGE );
     }

     if (data->full_area) {
          data->dest_rect.w = data->dest_surface->config.size.w;
          data->dest_rect.h = data->dest_surface->config.size.h;
     }

     *dest_x      = data->dest_rect.x;
     *dest_y      = data->dest_rect.y;
     *dest_width  = data->dest_rect.w;
     *dest_height = data->dest_rect.h;

     *dest_pixel_aspect = 1.0f; // FIXME
     *win_x             = 0;
     *win_y             = 0;

     pthread_mutex_unlock( &data->lock );
}

static void
dest_size_cb( void *user_data,
              int video_width, int video_height,
              double video_pixel_aspect,
              int *dest_width, int *dest_height,
              double *dest_pixel_aspect )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( data %p, video %dx%d, aspect %.2f )\n", __FUNCTION__,
                 user_data, video_width, video_height, video_pixel_aspect );
}

/****************************** Exported Symbols ******************************/

static char *filename_to_mrl( const char *filename )
{
     struct stat st;
     
     if (!filename || !strncmp( filename, "stdin:", 6 ))
          return NULL; /* force data buffer */
          
     if (stat( filename, &st ) == 0 && S_ISFIFO( st.st_mode ))
          return NULL; /* force data buffer */
        
     if (!strcmp ( filename, "/dev/cdrom" )     ||
         !strncmp( filename, "/dev/cdroms/", 12 ))
          return D_STRDUP( "cdda:/1" );

     if (!strcmp( filename, "/dev/dvd" ))
          return D_STRDUP( "dvd:/" );
          
     if (!strcmp( filename, "/dev/vcd" ))
          return D_STRDUP( "vcd:/" );
        
     return D_STRDUP( filename );
}

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     char              *mrl;
     char              *xinerc;
     xine_t            *xine;
     xine_video_port_t *vo;
     xine_audio_port_t *ao;
     xine_stream_t     *stream;
     DFBResult          result;
     
     if (dfb_system_type() != CORE_X11VDPAU)
          return DFB_UNSUPPORTED;

     mrl = filename_to_mrl( ctx->filename );
     if (!mrl)
          return DFB_OK; /* avoid probe in this case */
          
     /* Ignore GIFs */
     if (!strcmp( strrchr( mrl, '.' ) ? : "", ".gif" )) {
          D_FREE( mrl );
          return DFB_UNSUPPORTED;
     }
     
     xine = xine_new();
     if (!xine) {
          D_FREE( mrl );
          return DFB_INIT;
     }

     xinerc = getenv( "XINERC" );
     if (!xinerc || !*xinerc) {
          xinerc = alloca( 2048 );
          snprintf( xinerc, 2048, 
                    "%s/.xine/config", xine_get_homedir() );
     }
          
     xine_config_load( xine, xinerc );

     xine_init( xine );

     vo = xine_open_video_driver( xine, "none", XINE_VISUAL_TYPE_NONE, NULL );
     if (!vo) {
          xine_exit( xine );
          D_FREE( mrl );
          return DFB_INIT;
     }

     ao = xine_open_audio_driver( xine, "none", NULL );
     if (!ao) {
          xine_close_video_driver( xine, vo );
          xine_exit( xine );
          D_FREE( mrl );
          return DFB_INIT;
     }
     
     stream = xine_stream_new( xine, ao, vo );
     if (!stream) {
          xine_close_audio_driver( xine, ao );
          xine_close_video_driver( xine, vo );
          xine_exit( xine );
          D_FREE( mrl );
          return DFB_INIT;
     }
          
     result = xine_open( stream, mrl ) ? DFB_OK : DFB_UNSUPPORTED;

     xine_close( stream );
     xine_dispose( stream );
     xine_close_video_driver( xine, vo );
     xine_close_audio_driver( xine, ao );
     xine_exit( xine );
     D_FREE( mrl );

     return result;
}

static DFBResult make_pipe( char **ret_path )
{
     char path[512];
     int  i, len;

     len = snprintf( path, sizeof(path), 
                     "%s/xine-vp-", getenv("TEMP") ? : "/tmp" );

     for (i = 0; i <= 0xffff; i++) {
          snprintf( path+len, sizeof(path)-len, "%04x", i );

          if (mkfifo( path, 0600 ) < 0) {
               if (errno == EEXIST)
                    continue;
               return errno2result( errno );
          }

          if (ret_path)
               *ret_path = D_STRDUP( path );
          
          return DFB_OK;
     }

     return DFB_FAILURE;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer,
           CoreDFB                *core )
{
     const char               *xinerc;
     int                       verbosity;
     IDirectFBDataBuffer_data *buffer_data;
     DFBX11                   *x11 = dfb_system_data();
     
     D_DEBUG_AT( XineDFB_VDPAU, "%s( buffer %p, core %p )\n", __FUNCTION__, buffer, core );

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Xine );
          
     data->ref    = 1;
     data->core   = core;
     data->speed  = XINE_SPEED_NORMAL;
     data->status = DVSTATE_STOP;
     data->format = DSPF_ARGB;

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;

     data->mrl = filename_to_mrl( buffer_data->filename );
     if (!data->mrl) { /* data buffer mode */
          DFBResult ret;

          ret = make_pipe( &data->pipe );
          if (ret)
               return ret;

          buffer->AddRef( buffer );
          
          data->buffer = buffer;
          data->buffer_thread = direct_thread_create( DTT_DEFAULT,
                                                      BufferThread, data, "Xine Input" );
               
          data->mrl = D_MALLOC( strlen( data->pipe ) + 7 );
          sprintf( data->mrl, "fifo:/%s", data->pipe );
     }
     
     data->xine = xine_new();
     if (!data->xine) {
          D_ERROR( "DirectFB/VideoProvider_Xine: xine_new() failed.\n" );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return DFB_INIT;
     }

     xinerc = getenv( "XINERC" );
     if (!xinerc || !*xinerc) {
          const char *home = xine_get_homedir();
          char        path[2048];
          
          snprintf( path, sizeof(path), "%s/.xine", home );
          mkdir( path , 0755 );
          snprintf( path, sizeof(path), "%s/.xine/config", home );
          data->cfg = D_STRDUP( path );
     } 
     else {
          data->cfg = D_STRDUP( xinerc );
     }

     if (data->cfg)
          xine_config_load( data->xine, data->cfg );

     xine_init( data->xine );

     if (direct_config->quiet) {
          verbosity = XINE_VERBOSITY_NONE;
     }
     else if (direct_config->debug) {
          verbosity = XINE_VERBOSITY_DEBUG;
     }
     else {
          verbosity = XINE_VERBOSITY_LOG;
     }
     
     xine_engine_set_param( data->xine,
                            XINE_ENGINE_PARAM_VERBOSITY,
                            verbosity );

     /* prepare the visual */
     data->visual.x11.display         = x11->display;
     data->visual.x11.screen          = x11->screennum;
     data->visual.x11.frame_output_cb = dummy_frame_output_cb;
     data->visual.x11.dest_size_cb    = dest_size_cb;
//     data->visual.x11.d               = x11->vdp_drawable;
     data->visual.x11.user_data       = data;

     data->visual.device               = x11->vdp.device;
     data->visual.vdp_get_proc_address = x11->vdp.GetProcAddress;

     XineDFB_VDPAU_init( &data->visual, data );

     /* open the video driver */
     data->vo = xine_open_video_driver( data->xine, "vdpau",
                                        XINE_VISUAL_TYPE_X11,
                                        (void*) &data->visual );
     if (!data->vo) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to load video driver 'vdpau'.\n" );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return DFB_FAILURE;
     }

     /* open audio driver */
     data->ao = xine_open_audio_driver( data->xine, NULL, NULL );
     if (!data->ao) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to load audio driver.\n" );
          data->ao = xine_open_audio_driver( data->xine, "none", NULL );
     }
     
     /* create a new stream */
     data->stream = xine_stream_new( data->xine, data->ao, data->vo );
     if (!data->stream) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to create a new stream.\n" );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return DFB_FAILURE;
     }


     xine_osd_new(data->stream, 0, 0, 900, 500);


     xine_set_param( data->stream, 
                     XINE_PARAM_VERBOSITY,
                     verbosity );
     xine_set_param( data->stream,
                     XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
                     -1 );

     xine_set_param( data->stream,
                     XINE_PARAM_VO_ASPECT_RATIO,
                     XINE_VO_ASPECT_AUTO );

     direct_util_recursive_pthread_mutex_init( &data->lock );

     pthread_mutex_lock( &data->lock );

     /* create a event queue for end-of-playback notification */
     data->queue = xine_event_new_queue( data->stream );
     if (data->queue)
          xine_event_create_listener_thread( data->queue,
                                             event_listener, (void*) data );

     /* open the MRL */
     if (!xine_open( data->stream, data->mrl )) {
          DFBResult ret = get_stream_error( data );
          pthread_mutex_unlock( &data->lock );
          pthread_mutex_destroy( &data->lock );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return ret;
     }

     xine_get_pos_length( data->stream, NULL, NULL, &data->length );

     /* init a post plugin if no video */
     if (!xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_VIDEO ) &&
          xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_AUDIO )) {
          const char* const *post_list;
          const char        *post_plugin;
          xine_post_out_t   *audio_source;

          post_list = xine_list_post_plugins_typed( data->xine,
                                        XINE_POST_TYPE_AUDIO_VISUALIZATION );

          post_plugin = xine_config_register_string( data->xine,
                                        "gui.post_audio_plugin", post_list[0],
                                        "Audio visualization plugin",
                                        NULL, 0, NULL, NULL );

          data->post = xine_post_init( data->xine, post_plugin,
                                       0, &data->ao, &data->vo );
          if (data->post) {
               audio_source = xine_get_audio_source( data->stream );
               xine_post_wire_audio_port( audio_source,
                                          data->post->audio_input[0] );
          }
     }

     pthread_mutex_unlock( &data->lock );

     thiz->AddRef                = IDirectFBVideoProvider_Xine_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Xine_Release;
     thiz->CreateEventBuffer     = IDirectFBVideoProvider_Xine_CreateEventBuffer;
     thiz->AttachEventBuffer     = IDirectFBVideoProvider_Xine_AttachEventBuffer;
     thiz->DetachEventBuffer     = IDirectFBVideoProvider_Xine_DetachEventBuffer;
     thiz->GetCapabilities       = IDirectFBVideoProvider_Xine_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_Xine_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_Xine_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_Xine_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Xine_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_Xine_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_Xine_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Xine_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Xine_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_Xine_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_Xine_SetColorAdjustment;
     thiz->SendEvent             = IDirectFBVideoProvider_Xine_SendEvent;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_Xine_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_Xine_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_Xine_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_Xine_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_Xine_GetVolume;
     
     return DFB_OK;
}


/***************************** Private Functions ******************************/

static DFBResult
get_stream_error( IDirectFBVideoProvider_Xine_data *data )
{
     DFBResult ret;
     int       err = 0;

     if (data->stream)     
          err = xine_get_error( data->stream );

     switch (err) {
          case XINE_ERROR_NO_INPUT_PLUGIN:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "there is no input plugin to handle '%s'.\n",
                        data->mrl );
               ret = DFB_UNSUPPORTED;
               break;

          case XINE_ERROR_NO_DEMUX_PLUGIN:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "there is no demuxer plugin to decode '%s'.\n",
                        data->mrl );
               ret = DFB_UNSUPPORTED;
               break;

          case XINE_ERROR_DEMUX_FAILED:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "demuxer plugin failed; probably '%s' is corrupted.\n",
                        data->mrl );
               ret = DFB_FAILURE;
               break;

          case XINE_ERROR_MALFORMED_MRL:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "mrl '%s' is corrupted.\n",
                        data->mrl );
               ret = DFB_FAILURE;
               break;

          default:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "xine engine generic error !!\n" );
               ret = DFB_FAILURE;
               break;
     }
     
     return ret;
}

static void
send_videoprovider_event( IDirectFBVideoProvider_Xine_data *data,
                          DFBVideoProviderEventType         type )
{
     DFBEvent event = { videoprovider: { DFEC_VIDEOPROVIDER, type } };

     if (data->events)
          data->events->PostEvent( data->events, &event );
}

static void
event_listener( void *cdata, const xine_event_t *event )
{
     int                               lock = 10;
     IDirectFBVideoProvider_Xine_data *data = cdata;

     if (!data)
          return;

     switch (event->type) {
          case XINE_EVENT_MRL_REFERENCE:
          case XINE_EVENT_UI_PLAYBACK_FINISHED:
               break;

          default:
               return;
     }

     while (pthread_mutex_trylock( &data->lock )) {
          if (!--lock) {
               D_WARN( "could not lock provider data" );
               break;
          }

          usleep( 1000 );
     }

     switch (event->type) {
          case XINE_EVENT_MRL_REFERENCE:
               if (!data->mrl_changed) {
                    xine_mrl_reference_data_t *ref = event->data;
                    
                    D_FREE( data->mrl );
                    data->mrl = D_STRDUP( ref->mrl );
                    data->mrl_changed = 1;
               }
               break;
               
          case XINE_EVENT_UI_PLAYBACK_FINISHED:
               data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );

               if (data->mrl_changed) {
                    data->mrl_changed = 0;

                    send_videoprovider_event( data, DVPET_STREAMCHANGE );

                    if (!xine_open( data->stream, data->mrl )) {
                         data->status = DVSTATE_FINISHED;

                         send_videoprovider_event( data, DVPET_FATALERROR );
                         break;
                    }
                    if (data->status == DVSTATE_PLAY) {
                         pthread_mutex_unlock( &data->lock );

                         if (!xine_play( data->stream, 0, data->start_time )) {
                              pthread_mutex_lock( &data->lock );

                              data->status = DVSTATE_STOP;

                              send_videoprovider_event( data, DVPET_FATALERROR );
                              break;
                         }

                         pthread_mutex_lock( &data->lock );

                         xine_set_param( data->stream, 
                                         XINE_PARAM_SPEED, data->speed );

                         send_videoprovider_event( data, DVPET_STARTED );
                    }
               }
               else {
                    if (data->flags & DVPLAY_LOOPING) {
                         xine_play( data->stream, 0, 0 );
                         xine_set_param( data->stream, 
                                         XINE_PARAM_SPEED, data->speed );

                         send_videoprovider_event( data, DVPET_STARTED );
                    }
                    else {
                         xine_stop( data->stream );
                         data->status = DVSTATE_FINISHED;

                         send_videoprovider_event( data, DVPET_FINISHED );
                    }
                    data->start_time = 0;
               }
               break;
        
          case XINE_EVENT_FRAME_FORMAT_CHANGE: /* aspect ratio */
               send_videoprovider_event( data, DVPET_STREAMCHANGE );
               break;

          default:
               break;
     }

     if (lock)
          pthread_mutex_unlock( &data->lock );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static DirectOnce   queues_once;
static DirectMutex  queues_lock;
static DirectHash   queues_hash;
static uint32_t     queues_count;
static void        *queues_data;

static void
queues_init( void )
{
     direct_mutex_init( &queues_lock );
     direct_hash_init( &queues_hash, 7 );
}

/**********************************************************************************************************************/

static VdpStatus
XineDFB_VDPAU_PresentationQueueTargetCreateX11( VdpDevice                   device,
                                                Drawable                    drawable,
                                                /* output parameters follow */
                                                VdpPresentationQueueTarget * target )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, device );

     *target = 1;

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueTargetDestroy( VdpPresentationQueueTarget presentation_queue_target )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue_target );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueCreate( VdpDevice                  device,
                                       VdpPresentationQueueTarget presentation_queue_target,
                                       /* output parameters follow */
                                       VdpPresentationQueue *     presentation_queue )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, device );

     direct_mutex_lock( &queues_lock );

     *presentation_queue = ++queues_count;

     direct_hash_insert( &queues_hash, queues_count, queues_data );

     direct_mutex_unlock( &queues_lock );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueDestroy( VdpPresentationQueue presentation_queue )
{
     IDirectFBVideoProvider_Xine_data *data;

     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue );

     direct_mutex_lock( &queues_lock );

     data = direct_hash_lookup( &queues_hash, presentation_queue );
     if (data)
          direct_hash_remove( &queues_hash, presentation_queue );

     direct_mutex_unlock( &queues_lock );

     if (!data)
          return VDP_STATUS_INVALID_HANDLE;

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueDisplay( VdpPresentationQueue presentation_queue,
                                        VdpOutputSurface     output_surface,
                                        uint32_t             clip_width,
                                        uint32_t             clip_height,
                                        VdpTime              earliest_presentation_time )
{
     DFBX11                           *x11 = dfb_system_data();
     IDirectFBVideoProvider_Xine_data *data;

     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u, surface %u, clip %ux%u, time %zu )\n", __FUNCTION__,
                 presentation_queue, output_surface, clip_width, clip_height, earliest_presentation_time );

     direct_mutex_lock( &queues_lock );

     data = direct_hash_lookup( &queues_hash, presentation_queue );

     direct_mutex_unlock( &queues_lock );

     if (!data) {
          D_DEBUG_AT( XineDFB_VDPAU, "  -> hash lookup failed\n" );
          return VDP_STATUS_INVALID_HANDLE;
     }


     pthread_mutex_lock( &data->lock );

     if (data->dest_surface) {
          DFBResult          ret;
          CoreSurfaceConfig  config;
          CoreSurface       *surface;
     
          config.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS | CSCONF_PREALLOCATED;
          config.size.w = data->width;
          config.size.h = data->height;
          config.format = DSPF_ARGB;
          config.caps   = DSCAPS_VIDEOONLY;
          config.preallocated[0].addr   = NULL;
          config.preallocated[0].handle = output_surface;
          config.preallocated[0].pitch  = data->width * 4;

          ret = dfb_surface_create( data->core, &config, CSTF_EXTERNAL | CSTF_PREALLOCATED, output_surface, NULL, &surface );
          if (ret) {
               D_DERROR( ret, "DirectFB/Xine/VDPAU: Could not create preallocated output surface!\n" );
               pthread_mutex_unlock( &data->lock );
               return VDP_STATUS_ERROR;
          }
     
          XUnlockDisplay( x11->display );
     
          dfb_gfx_stretch_to( surface, data->dest_surface, NULL, &data->dest_rect, false );
     
     
          dfb_surface_unref( surface );


          if (data->callback)
               data->callback( data->callback_ctx );

          XLockDisplay( x11->display );
     }

     pthread_mutex_unlock( &data->lock );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueBlockUntilSurfaceIdle( VdpPresentationQueue presentation_queue,
                                                      VdpOutputSurface     surface,
                                                      /* output parameters follow */
                                                      VdpTime *            first_presentation_time )
{
//     DFBX11                   *x11 = dfb_system_data();

     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue );

//     x11->vdp.PresentationQueueBlockUntilSurfaceIdle( x11->vdp_queue, surface, first_presentation_time );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueSetBackgroundColor( VdpPresentationQueue presentation_queue,
                                                   VdpColor * const     background_color )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueGetTime( VdpPresentationQueue presentation_queue,
                                        /* output parameters follow */
                                        VdpTime *            current_time )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PresentationQueueQuerySurfaceStatus( VdpPresentationQueue         presentation_queue,
                                                   VdpOutputSurface             surface,
                                                   /* output parameters follow */
                                                   VdpPresentationQueueStatus * status,
                                                   VdpTime *                    first_presentation_time )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, presentation_queue );

     return VDP_STATUS_OK;
}

static VdpStatus
XineDFB_VDPAU_PreemptionCallbackRegister( VdpDevice             device,
                                          VdpPreemptionCallback callback,
                                          void *                context )
{
     D_DEBUG_AT( XineDFB_VDPAU, "%s( %u )\n", __FUNCTION__, device );

     return VDP_STATUS_OK;
}

static void
XineDFB_VDPAU_init( x11_visual_vdpau_t *vdpau,
                    void               *data )
{
     direct_once( &queues_once, queues_init );

     vdpau->vdp_queue_target_create_x11 = XineDFB_VDPAU_PresentationQueueTargetCreateX11;
     vdpau->vdp_queue_target_destroy = XineDFB_VDPAU_PresentationQueueTargetDestroy;
     vdpau->vdp_queue_create = XineDFB_VDPAU_PresentationQueueCreate;
     vdpau->vdp_queue_destroy = XineDFB_VDPAU_PresentationQueueDestroy;
     vdpau->vdp_queue_display = XineDFB_VDPAU_PresentationQueueDisplay;
     vdpau->vdp_queue_block = XineDFB_VDPAU_PresentationQueueBlockUntilSurfaceIdle;
     vdpau->vdp_queue_set_background_color = XineDFB_VDPAU_PresentationQueueSetBackgroundColor;
     vdpau->vdp_queue_get_time = XineDFB_VDPAU_PresentationQueueGetTime;
     vdpau->vdp_queue_query_surface_status = XineDFB_VDPAU_PresentationQueueQuerySurfaceStatus;
     vdpau->vdp_preemption_callback_register = XineDFB_VDPAU_PreemptionCallbackRegister;

     queues_data = data;
}

