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
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <directfb.h>

#include <media/idirectfbvideoprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <direct/conf.h>
#include <direct/interface.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>


static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, Xine )


/************************** Driver Specific Data ******************************/

typedef void (*DVOutputCallback) ( void         *cdata,
                                   int           width,
                                   int           height,
                                   double        ratio,
                                   DFBRectangle *dest_rect );

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
     int                    ref;
     DFBResult              err;
     
     char                  *mrl;
     char                  *cfg;

     xine_t                *xine;
     xine_video_port_t     *vo;
     xine_audio_port_t     *ao;
     xine_post_t           *post;
     xine_stream_t         *stream;
     xine_event_queue_t    *queue;

     dfb_visual_t           visual;
     
     int                    width;  /* video width */
     int                    height; /* video height */
     int                    length; /* duration */

     bool                   is_playing;
     bool                   is_paused;
     bool                   is_finished;
     
     bool                   full_area;
     DFBRectangle           dest_rect;
     
} IDirectFBVideoProvider_Xine_data;


/***************************** Private Functions ******************************/

static void
get_stream_error( IDirectFBVideoProvider_Xine_data *data );

static void
frame_output( void *cdata, int width, int height,
              double ratio, DFBRectangle *dest_rect );

static void
event_listner( void *cdata, const xine_event_t *event );


/******************************* Public Methods *******************************/

static void
IDirectFBVideoProvider_Xine_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_Xine_data *data;

     data = (IDirectFBVideoProvider_Xine_data*) thiz->priv;

     if (data->xine) {
          if (data->stream) {
               xine_set_param( data->stream,
                               XINE_PARAM_AUDIO_MUTE,
                               0 );
               
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
               free( data->cfg );
          }

          xine_exit( data->xine );
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

     *caps = DVCAPS_BASIC      | DVCAPS_SCALE    |
             DVCAPS_BRIGHTNESS | DVCAPS_CONTRAST |
             DVCAPS_SATURATION;

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
     desc->pixelformat = dfb_primary_layer_pixelformat();

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

     if (data->is_paused) {
          xine_set_param( data->stream,
                          XINE_PARAM_AUDIO_MUTE,
                          0 );
          xine_set_param( data->stream,
                          XINE_PARAM_SPEED,
                          XINE_SPEED_NORMAL );
          
          data->is_paused = false;
     } else 
     if (!data->is_playing) {     
          xine_set_param( data->stream,
                          XINE_PARAM_AUDIO_MUTE,
                          0 );
          
          if(!xine_play( data->stream, 0, 0 )) {
               get_stream_error( data );
               return data->err;
          }

          xine_get_pos_length( data->stream, NULL, 
                               NULL, &data->length );
          
          data->is_playing  = true;
          data->is_finished = false;
     }
     
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_Xine_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )

     if (data->is_playing && !data->is_paused) {
          xine_set_param( data->stream,
                          XINE_PARAM_AUDIO_MUTE,
                          1 );
          xine_set_param( data->stream,
                          XINE_PARAM_SPEED,
                          XINE_SPEED_PAUSE );
          
          data->is_paused = true;
          usleep( 800 );

          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_Xine_SeekTo( IDirectFBVideoProvider *thiz,
                                    double                  seconds )
{     
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
          
     if (data->is_playing) {      
          int offset;

          if (!xine_get_stream_info( data->stream,
                                     XINE_STREAM_INFO_SEEKABLE ))
               return DFB_UNSUPPORTED;

          offset = (int) (seconds * 1000.0);

          if (offset < 0)
               offset = 0;
          else
          if (data->length > 0 && offset > data->length)
               offset = data->length;

          xine_play( data->stream, 0, offset );
          
          if (data->is_paused)
               xine_set_param( data->stream,
                               XINE_PARAM_SPEED,
                               XINE_SPEED_PAUSE );
          
          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetPos( IDirectFBVideoProvider *thiz,
                                    double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
          
     if (!seconds)
          return DFB_INVARG;

     if (data->is_playing) {
          int pos = 0;
          int try;

          for (try = 5; try--; ) {
               if (xine_get_pos_length( data->stream, NULL,
                                        &pos, NULL ))
                    break;
               
               usleep( 1000 );
          }

          *seconds = (double) pos / 1000.0;
          
          return DFB_OK;
     } else
     if (data->is_finished) {
          *seconds = (double) data->length / 1000.0;
          
          return DFB_OK;
     }

     *seconds = 0.0;
          
     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_Xine_GetLength( IDirectFBVideoProvider *thiz,
                                       double                 *seconds )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_Xine )
          
     if (!seconds)
          return DFB_INVARG;

     if (xine_get_pos_length( data->stream, NULL,
                              NULL, &data->length )) {
          *seconds = (double) data->length / 1000.0;
          return DFB_OK;
     }

     *seconds = 0.0;
          
     return DFB_UNSUPPORTED;
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


/****************************** Exported Symbols ******************************/

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     char              *xinerc;
     xine_t            *xine;
     xine_video_port_t *vo;
     xine_audio_port_t *ao;
     xine_stream_t     *stream;
     dfb_visual_t       visual;
     DFBResult          result;
     
     xine = xine_new();
     if (!xine)
          return DFB_INIT;

     xinerc = getenv( "XINERC" );

     if (!xinerc || !*xinerc) {
          asprintf( &xinerc, "%s/.xine/config", xine_get_homedir() );
          
          if(xinerc) {
               xine_config_load( xine, xinerc );
               free( xinerc );
          }
     } else
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
          
     if (xine_open( stream, ctx->filename ))
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
Construct( IDirectFBVideoProvider *thiz,
           const char             *filename )
{
     const char        *xinerc;
     int                verbosity;
     const char* const *ao_list;
     const char        *ao_driver;
     
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_Xine )

     data->ref = 1;
     data->err = DFB_FAILURE;
     data->mrl = D_STRDUP( filename );
     
     data->xine = xine_new();
     if (!data->xine) {
          D_ERROR( "DirectFB/VideoProvider_Xine: xine_new() failed.\n" );
          return DFB_INIT;
     }

     xinerc = getenv( "XINERC" );
     
     if (!xinerc || !*xinerc) {
          char *xined;
          asprintf( &xined, "%s/.xine", xine_get_homedir() );
          mkdir( xined , 755 );
          asprintf( &data->cfg, "%s/config", xined );
          free( xined );
     } else
          data->cfg = strdup( xinerc );

     if (data->cfg)
          xine_config_load( data->xine, data->cfg );

     xine_init( data->xine );

     if(direct_config->quiet)
          verbosity = XINE_VERBOSITY_NONE;
     else
     if(direct_config->debug)
          verbosity = XINE_VERBOSITY_DEBUG;
     else
          verbosity = XINE_VERBOSITY_LOG;
     
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
          xine_exit( data->xine );
          return data->err;
     }

     /* get available audio plugins */
     ao_list = xine_list_audio_output_plugins( data->xine );

     /* register config entry */
     ao_driver = xine_config_register_string( data->xine, "audio.driver",
                                        ao_list[0], "Audio driver to use",
                                        NULL, 0, NULL, NULL );
     
     /* open audio driver */
     data->ao = xine_open_audio_driver( data->xine, ao_driver, NULL );
     if (!data->ao) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to load audio driver '%s'.\n", ao_driver );
          xine_close_video_driver( data->xine, data->vo );
          xine_exit( data->xine );
          return data->err;
     }
     
     /* create a new stream */
     data->stream = xine_stream_new( data->xine, data->ao, data->vo );
     if (!data->stream) {
          D_ERROR( "DirectFB/VideoProvider_Xine: "
                   "failed to create a new stream.\n" );
          xine_close_video_driver( data->xine, data->vo );
          xine_close_audio_driver( data->xine, data->ao );
          xine_exit( data->xine );
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
          if (data->queue)
               xine_event_dispose_queue( data->queue );
          xine_dispose( data->stream );
          xine_close_video_driver( data->xine, data->vo );
          xine_close_audio_driver( data->xine, data->ao );
          xine_exit( data->xine );
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
     thiz->PlayTo                = IDirectFBVideoProvider_Xine_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_Xine_Stop;
     thiz->SeekTo                = IDirectFBVideoProvider_Xine_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_Xine_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_Xine_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_Xine_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_Xine_SetColorAdjustment;

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
frame_output( void *cdata, int width, int height,
          double ratio, DFBRectangle *dest_rect )
{
     IDirectFBVideoProvider_Xine_data *data;
     IDirectFBSurface                 *surface;

     data = (IDirectFBVideoProvider_Xine_data*) cdata;

     if (!data)
          return;

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
               xine_stop( data->stream );
               data->is_playing  = false;
               data->is_paused   = false;
               data->is_finished = true;
               break;

          default:
               break;
     }
}

