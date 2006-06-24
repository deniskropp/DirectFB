/*
 * Copyright (C) 2004-2005 Claudio "KLaN" Ciccani <klan@users.sf.net>
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
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>

#include <pthread.h>

#include <directfb.h>

#include <media/idirectfbvideoprovider.h>
#include <media/idirectfbdatabuffer.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <direct/conf.h>
#include <direct/interface.h>
#include <direct/thread.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           IDirectFBDataBuffer    *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Xine )


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
     DFBResult                      err;
     
     char                          *mrl;
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

     dfb_visual_t                   visual;
    
     DFBSurfacePixelFormat          format; // video format
     int                            width;  // video width
     int                            height; // video height
     int                            length; // duration

     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     
     bool                           full_area;
     DFBRectangle                   dest_rect;
   
     int                            mouse_x;
     int                            mouse_y;
     
     IDirectFBDataBuffer           *buffer;
     DirectThread                  *buffer_thread;
} IDirectFBVideoProvider_Xine_data;


/***************************** Private Functions ******************************/

static void
get_stream_error( IDirectFBVideoProvider_Xine_data *data );

static void
frame_output( void *cdata, int width, int height, double ratio,
              DFBSurfacePixelFormat format, DFBRectangle *dest_rect );

static void
event_listner( void *cdata, const xine_event_t *event );

static DFBResult
make_pipe( char **ret_path );

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

          while ((ret = buffer->GetData( buffer,
                                  sizeof(buf), buf, &len )) == DFB_OK) {
               write( fd, buf, len );
               
               if (direct_thread_is_canceled( self ))
                    break;
          }

          if (ret == DFB_EOF)
               break;
               
          usleep( 100 );
     }

     close( fd );

     return (void*)0;
}     

/******************************* Public Methods *******************************/

static void
IDirectFBVideoProvider_Xine_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Xine_data *data;

     data = (IDirectFBVideoProvider_Xine_data*) thiz->priv;

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
     
     D_FREE( data->mrl );
     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBVideoProvider_Xine_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (--data->ref == 0)
          IDirectFBVideoProvider_Xine_Destruct( thiz );

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
             DVCAPS_INTERACTIVE | DVCAPS_VOLUME;

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

     desc->flags       = (DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT);
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = data->format;

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
     
          snprintf( desc->video.encoding,
                    DFB_STREAM_DESC_ENCODING_LENGTH,
                    xine_get_meta_info( data->stream, XINE_META_INFO_VIDEOCODEC ) ?:"" );
          desc->video.framerate = xine_get_stream_info( data->stream, 
                                                        XINE_STREAM_INFO_FRAME_DURATION );
          if (desc->video.framerate)
               desc->video.framerate = 90000.0 / desc->video.framerate;
          desc->video.aspect    = xine_get_stream_info( data->stream,
                                                        XINE_STREAM_INFO_VIDEO_RATIO ) / 10000.0;
          if (!desc->video.aspect)
               desc->video.aspect = 4.0/3.0;
          desc->video.bitrate   = xine_get_stream_info( data->stream,
                                                        XINE_STREAM_INFO_VIDEO_BITRATE );
     }

     if (xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_AUDIO )) {
          desc->caps |= DVSCAPS_AUDIO;

          snprintf( desc->audio.encoding,
                    DFB_STREAM_DESC_ENCODING_LENGTH,
                    xine_get_meta_info( data->stream, XINE_META_INFO_AUDIOCODEC ) ?:"" );
          desc->audio.samplerate = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_SAMPLERATE );
          desc->audio.channels   = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_CHANNELS );
          desc->audio.bitrate    = xine_get_stream_info( data->stream,
                                                         XINE_STREAM_INFO_AUDIO_BITRATE );
     }
               
     snprintf( desc->title,
               DFB_STREAM_DESC_TITLE_LENGTH,
               xine_get_meta_info( data->stream, XINE_META_INFO_TITLE ) ?:"" );
     snprintf( desc->author,
               DFB_STREAM_DESC_AUTHOR_LENGTH,
               xine_get_meta_info( data->stream, XINE_META_INFO_ARTIST ) ?:"" );
     snprintf( desc->album,
               DFB_STREAM_DESC_ALBUM_LENGTH,
               xine_get_meta_info( data->stream, XINE_META_INFO_ALBUM ) ?:"" );
     snprintf( desc->genre,
               DFB_STREAM_DESC_GENRE_LENGTH,
               xine_get_meta_info( data->stream, XINE_META_INFO_GENRE ) ?:"" );
     snprintf( desc->comment,
               DFB_STREAM_DESC_COMMENT_LENGTH,
               xine_get_meta_info( data->stream, XINE_META_INFO_COMMENT ) ?:"" );
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
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;
     
     memset( &data->dest_rect, 0, sizeof( DFBRectangle ) );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;

          data->dest_rect = *dest_rect;
          data->full_area = false;
     } else
          data->full_area = true;

     /* update visual */
     data->visual.destination = dest;
     data->visual.frame_cb    = callback;
     data->visual.frame_cdata = ctx;
     /* notify visual changes */
     if (!xine_port_send_gui_data( data->vo, 
                                   XINE_GUI_SEND_SELECT_VISUAL,
                                   (void*) &data->visual ))
          return DFB_UNSUPPORTED;
     
     if (data->status != DVSTATE_PLAY) {
          if (!xine_play( data->stream, 0, data->start_time )) {
               get_stream_error( data );
               return data->err;
          }

          xine_set_param( data->stream, XINE_PARAM_SPEED, data->speed );
          usleep( 100 );
               
          xine_get_pos_length( data->stream,
                               NULL, NULL, &data->length );
                               
          data->status = DVSTATE_PLAY;
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (data->status == DVSTATE_PLAY) {
          data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );
          xine_get_pos_length( data->stream, NULL, &data->start_time, NULL );
          xine_stop( data->stream );
          usleep( 50 );
          
          data->status = DVSTATE_STOP;
     }

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
          
     if (!xine_get_stream_info( data->stream,
                                XINE_STREAM_INFO_SEEKABLE ))
          return DFB_UNSUPPORTED;

     offset = (int) (seconds * 1000.0);
     if (data->length > 0 && offset > data->length)
          return DFB_OK;

     if (data->status == DVSTATE_PLAY) {
          data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );
          if (!xine_play( data->stream, 0, offset )) {
               get_stream_error( data );
               return data->err;
          }
          xine_set_param( data->stream, XINE_PARAM_SPEED, data->speed );
          usleep( 100 );
     }
     
     data->start_time = offset;

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

     if (data->status == DVSTATE_FINISHED)
          return DFB_OK;

     if (data->full_area) {
          IDirectFBSurface *s = data->visual.destination;
          s->GetSize( s, &dw, &dh );
     } else {
          dw = data->dest_rect.w;
          dh = data->dest_rect.h;
     }

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
     
     xine_set_param( data->stream, XINE_PARAM_SPEED,
                     multiplier*XINE_SPEED_NORMAL+.5 );
                     
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
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
     
     if (level < 0.0)
          return DFB_INVARG;
          
     if (level > 1.0)
          return DFB_UNSUPPORTED;
          
     xine_set_param( data->stream, XINE_PARAM_AUDIO_VOLUME, level*100.0 );
     
     return DFB_OK;
}          
     

/****************************** Exported Symbols ******************************/

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     const char        *mrl;
     char              *xinerc;
     xine_t            *xine;
     xine_video_port_t *vo;
     xine_audio_port_t *ao;
     xine_stream_t     *stream;
     dfb_visual_t       visual;
     DFBResult          result;
     
     /* Skip test in this case */
     if (!ctx->filename)
          return DFB_OK;
     
     if (!strcmp ( ctx->filename, "/dev/cdrom" )     ||
         !strncmp( ctx->filename, "/dev/cdroms/", 12 )) {
          mrl = "cdda:/1";
     }
     else if (!strcmp( ctx->filename, "/dev/dvd" )) {
          mrl = "dvd:/";
     }
     else if (!strcmp( ctx->filename, "/dev/vcd" )) {
          mrl = "vcd:/";
     }
     else {
          mrl = ctx->filename;
     }
     
     xine = xine_new();
     if (!xine)
          return DFB_INIT;

     xinerc = getenv( "XINERC" );
     if (!xinerc || !*xinerc) {
          xinerc = alloca( 2048 );
          snprintf( xinerc, 2048, 
                    "%s/.xine/config", xine_get_homedir() );
     }
          
     xine_config_load( xine, xinerc );

     xine_init( xine );

     memset( &visual, 0, sizeof(visual) );
     visual.output_cb = frame_output;
     
     vo = xine_open_video_driver( xine, "DFB",
                                  XINE_VISUAL_TYPE_DFB,
                                  (void*) &visual );
     if (!vo) {
          xine_exit( xine );
          return DFB_INIT;
     }

     ao = xine_open_audio_driver( xine, "none", NULL );
     if (!ao) {
          xine_close_video_driver( xine, vo );
          xine_exit( xine );
          return DFB_INIT;
     }
     
     stream = xine_stream_new( xine, ao, vo );
     if (!stream) {
          xine_close_audio_driver( xine, ao );
          xine_close_video_driver( xine, vo );
          xine_exit( xine );
          return DFB_INIT;
     }
          
     if (xine_open( stream, mrl ))
          result = DFB_OK;
     else
          result = DFB_UNSUPPORTED;
     
     xine_close( stream );
     xine_dispose( stream );
     xine_close_video_driver( xine, vo );
     xine_close_audio_driver( xine, ao );
     xine_exit( xine );

     return result;
}

static DFBResult
make_pipe( char **ret_path )
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
           IDirectFBDataBuffer    *buffer  )
{
     const char               *xinerc;
     int                       verbosity;
     char* const              *ao_list;
     const char               *ao_driver;
     IDirectFBDataBuffer_data *buffer_data;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Xine );
          
     data->ref    = 1;
     data->err    = DFB_FAILURE;
     data->speed  = XINE_SPEED_NORMAL;
     data->status = DVSTATE_STOP;
     data->format = DSPF_YUY2;

     buffer_data = (IDirectFBDataBuffer_data*) buffer->priv;

     if (!buffer_data->filename) {
          DFBResult ret;

          ret = make_pipe( &data->pipe );
          if (ret)
               return ret;

          buffer->AddRef( buffer );
          
          data->buffer = buffer;
          data->buffer_thread = direct_thread_create( DTT_DEFAULT,
                                                      BufferThread,
                                                      data,
                                                      "Xine DataBuffer Input" );
          if (!data->buffer_thread) {
               buffer->Release( buffer );
               D_FREE( data->pipe );
               return DFB_FAILURE;
          }               
     
          data->mrl = D_MALLOC( strlen( data->pipe ) + 7 );
          sprintf( data->mrl, "fifo:/%s", data->pipe );
     }
     else {
          if (!strcmp ( buffer_data->filename, "/dev/cdrom" )     ||
              !strncmp( buffer_data->filename, "/dev/cdroms/", 12 )) {
               data->mrl = D_STRDUP( "cdda:/1" );
          }
          else if (!strcmp( buffer_data->filename, "/dev/dvd" )) {
               data->mrl = D_STRDUP( "dvd:/" );
          }
          else if (!strcmp( buffer_data->filename, "/dev/vcd" )) {
               data->mrl = D_STRDUP( "vcd:/" );
          }
          else {
               data->mrl = D_STRDUP( buffer_data->filename );
          }
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
     data->visual.output_cb    = frame_output;
     data->visual.output_cdata = (void*) data;
     /* open the video driver */
     data->vo = xine_open_video_driver( data->xine, "DFB",
                                        XINE_VISUAL_TYPE_DFB,
                                        (void*) &data->visual );
     if (!data->vo) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to load video driver 'DFB'.\n" );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return data->err;
     }

     /* get available audio plugins */
     ao_list = (char* const*)xine_list_audio_output_plugins( data->xine );
     ao_driver = ao_list[0];
     
     /* serch for FusionSound plugin */
     for (; *ao_list; ao_list++) {
          if (!strcmp( *ao_list, "FusionSound" )) {
               ao_driver = *ao_list;
               break;
          }
     }

     /* register config entry */
     ao_driver = xine_config_register_string( data->xine, "audio.driver",
                                        ao_driver, "Audio driver to use",
                                        NULL, 0, NULL, NULL );
     
     /* open audio driver */
     data->ao = xine_open_audio_driver( data->xine, ao_driver, NULL );
     if (!data->ao) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to load audio driver '%s'.\n", ao_driver );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return data->err;
     }
     
     /* create a new stream */
     data->stream = xine_stream_new( data->xine, data->ao, data->vo );
     if (!data->stream) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to create a new stream.\n" );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return data->err;
     }

     xine_set_param( data->stream, 
                     XINE_PARAM_VERBOSITY,
                     verbosity );
     xine_set_param( data->stream,
                     XINE_PARAM_AUDIO_CHANNEL_LOGICAL,
                     -1 );

     /* create a event queue for end-of-playback notification */
     data->queue = xine_event_new_queue( data->stream );
     if (data->queue)
          xine_event_create_listener_thread( data->queue,
                                             event_listner, (void*) data );

     /* open the MRL */
     if (!xine_open( data->stream, data->mrl )) {
          get_stream_error( data );
          IDirectFBVideoProvider_Xine_Destruct( thiz );
          return data->err;
     }

     xine_get_pos_length( data->stream, NULL, NULL, &data->length );

     /* init a post plugin if no video */
     if (!xine_get_stream_info( data->stream, XINE_STREAM_INFO_HAS_VIDEO )) {
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

     thiz->AddRef                = IDirectFBVideoProvider_Xine_AddRef;
     thiz->Release               = IDirectFBVideoProvider_Xine_Release;
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
     
     return DFB_OK;
}


/***************************** Private Functions ******************************/

static void
get_stream_error( IDirectFBVideoProvider_Xine_data *data )
{
     int err = 0;

     if (data->stream)     
          err = xine_get_error( data->stream );

     switch (err) {
          case XINE_ERROR_NO_INPUT_PLUGIN:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "there is no input plugin to handle '%s'.\n",
                        data->mrl );
               data->err = DFB_UNSUPPORTED;
               break;

          case XINE_ERROR_NO_DEMUX_PLUGIN:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "there is no demuxer plugin to decode '%s'.\n",
                        data->mrl );
               data->err = DFB_UNSUPPORTED;
               break;

          case XINE_ERROR_DEMUX_FAILED:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "demuxer plugin failed; probably '%s' is corrupted.\n",
                        data->mrl );
               data->err = DFB_FAILURE;
               break;

          case XINE_ERROR_MALFORMED_MRL:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "mrl '%s' is corrupted.\n",
                        data->mrl );
               data->err = DFB_FAILURE;
               break;

          default:
               D_ERROR( "DirectFB/VideoProvider_Xine: "
                        "xine engine generic error !!\n" );
               data->err = DFB_FAILURE;
               break;
     }
}

static void
frame_output( void *cdata, int width, int height, double ratio,
              DFBSurfacePixelFormat format, DFBRectangle *dest_rect )
{
     IDirectFBVideoProvider_Xine_data *data;
     IDirectFBSurface                 *surface;

     data = (IDirectFBVideoProvider_Xine_data*) cdata;

     if (!data)
          return;

     data->format = format;
     data->width  = width;
     data->height = height;

     if (data->full_area) {
          surface = data->visual.destination;
          surface->GetSize( surface, &dest_rect->w, &dest_rect->h );
          dest_rect->x = 0;
          dest_rect->y = 0;
     } else
          *dest_rect = data->dest_rect;
}

static void
event_listner( void *cdata, const xine_event_t *event )
{
     IDirectFBVideoProvider_Xine_data *data;

     data = (IDirectFBVideoProvider_Xine_data*) cdata;

     if (!data)
          return;

     switch (event->type) {
          case XINE_EVENT_UI_PLAYBACK_FINISHED:
               data->speed = xine_get_param( data->stream, XINE_PARAM_SPEED );
               if (data->flags & DVPLAY_LOOPING) {
                    xine_play( data->stream, 0, 0 );
                    xine_set_param( data->stream, 
                                    XINE_PARAM_SPEED, data->speed );
               }
               else {
                    xine_stop( data->stream );
                    data->status = DVSTATE_FINISHED;
               }
               data->start_time = 0;
               break;

          default:
               break;
     }
}

