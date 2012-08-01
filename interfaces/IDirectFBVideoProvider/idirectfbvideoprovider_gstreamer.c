/*
   Copyright (C) 2006-2008 Claudio Ciccani <klan@users.sf.net>

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
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <fcntl.h>
#include <unistd.h>

#include <math.h>

#include <pthread.h>

#include <direct/types.h>
#include <direct/list.h>
#include <direct/messages.h>
#include <direct/memcpy.h>
#include <direct/thread.h>
#include <direct/util.h>

#include <directfb.h>

#include <idirectfb.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbvideoprovider.h>

#include <core/surface.h>
#include <core/layers.h>

#include <display/idirectfbsurface.h>

#include <gfx/clip.h>

#ifdef HAVE_FUSIONSOUND
# include <fusionsound.h>
# include <fusionsound_limits.h>
#endif

#undef __GNUC__
#define __GNUC__ 1

#include <gst/gst.h>
#include <glib.h>
#include <gst/app/gstappsink.h>


static DFBResult Probe( IDirectFBVideoProvider_ProbeContext *ctx );
static DFBResult Construct( IDirectFBVideoProvider *thiz, IDirectFBDataBuffer *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, GSTREAMER )

D_DEBUG_DOMAIN( GST, "VideoProvider/GST", "DirectFB VideoProvider Gstreamer" );

typedef struct {
     DirectLink            link;
     IDirectFBEventBuffer *buffer;
} EventLink;

typedef struct {
     int                            ref;

     DirectThread                  *audio_thread;
     DirectMutex                    audio_lock;
     DirectWaitQueue                audio_cond;

     DirectThread                  *video_thread;
     DirectMutex                    video_lock;
     DirectWaitQueue                video_cond;

     DFBVideoProviderStatus         status;
     DFBVideoProviderPlaybackFlags  flags;
     double                         speed;
     float                          volume;
     
     IDirectFBDataBuffer           *buffer;
     bool                           interlaced;
     bool                           seekable;

     GstElement                    *pipeline;
     GstBus                        *bus;
     GstElement                    *decode;
     GstElement                    *decode_audio;
     GstElement                    *convert_audio;
     GstElement                    *resample_audio;
     GstElement                    *filter_video;
     GstElement                    *scale_video;
     GstElement                    *decode_video;
     GstElement                    *queue_audio;
     GstElement                    *queue_video;
     GstElement                    *appsink_audio;
     GstElement                    *appsink_video;

     int                            width;
     int                            height;
     int                            format;
     int                            error;
     int                            parsed_audio;
     int                            parsed_video;

     IDirectFBSurface              *dest;
     void                          *ctx;
     DVFrameCallback                callback;

     EventLink                     *events;
     DFBVideoProviderEventType      events_mask;
     DirectMutex                    events_lock;
} IDirectFBVideoProvider_GSTREAMER_data;

/*****************************************************************************/

static void
dispatch_event( IDirectFBVideoProvider_GSTREAMER_data *data, DFBVideoProviderEventType type )
{
     EventLink             *link;
     DFBVideoProviderEvent  event;

     if (!data->events || !(data->events_mask & type))
          return;

     event.clazz = DFEC_VIDEOPROVIDER;
     event.type  = type;

     direct_mutex_lock( &data->events_lock );

     direct_list_foreach (link, data->events)
          link->buffer->PostEvent( link->buffer, DFB_EVENT(&event) );

     direct_mutex_unlock( &data->events_lock );
}

static void
release_events( IDirectFBVideoProvider_GSTREAMER_data *data )
{
     EventLink *link;
     EventLink *tmp;

     direct_list_foreach_safe (link, tmp, data->events) {
          direct_list_remove( (DirectLink**)&data->events, &link->link );
          link->buffer->Release( link->buffer );
          D_FREE( link );
     }
}

/*****************************************************************************/

static gboolean
pipeline_bus_call( GstBus *bus, GstMessage *msg, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;

     switch (GST_MESSAGE_TYPE (msg)) {
          case GST_MESSAGE_EOS:
               D_DEBUG_AT( GST, "BUS: end of stream\n" );
               break;
          case GST_MESSAGE_ERROR: {
               gchar  *debug;
               GError *error;

               gst_message_parse_error( msg, &error, &debug );

               D_DEBUG_AT( GST, "BUS: Error: %s\n", error->message );
               g_free( debug ); 
               g_error_free( error );

               direct_mutex_lock( &data->video_lock );
               data->error = 1;
               direct_waitqueue_signal( &data->video_cond );
               direct_mutex_unlock( &data->video_lock );
               break;
          }
          case GST_MESSAGE_WARNING: {
               gchar  *debug;
               GError *error;

               gst_message_parse_warning( msg, &error, &debug );
               D_DEBUG_AT( GST, "BUS: Warning: %s\n", error->message );
               g_free( debug ); 
               g_error_free( error );
               break;
          }
          case GST_MESSAGE_INFO: {
               gchar  *debug;
               GError *error;

               gst_message_parse_info( msg, &error, &debug );
               D_DEBUG_AT( GST, "BUS: Info: %s\n", error->message );
               g_free( debug ); 
               g_error_free( error );
               break;
          }
          default:
               D_DEBUG_AT( GST, "BUS: msg type '%d' name '%s'\n", GST_MESSAGE_TYPE(msg), GST_MESSAGE_TYPE_NAME(msg) );
               break;
     }

     return TRUE;
}

static void
decode_video_unknown_type( GstBin *bin, GstPad *pad, GstCaps *caps, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;

     D_DEBUG_AT( GST, "decode_video_unknown_type: type %s caps %s\n", GST_PAD_NAME(pad), gst_caps_to_string(caps) );
}

static void
decode_audio_unknown_type( GstBin *bin, GstPad *pad, GstCaps *caps, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;

     D_DEBUG_AT( GST, "decode_audio_unknown_type: type %s caps %s\n", GST_PAD_NAME(pad), gst_caps_to_string(caps) );
}

static void
decode_pad_added( GstElement *element, GstPad *srcpad, gpointer ptr )
{
     GstPadLinkReturn                       ret;
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;
     GstPad                                *sink = NULL;
     char                                  *type = GST_PAD_NAME( srcpad );
     char                                  *caps = gst_caps_to_string( gst_pad_get_caps(srcpad) );
     int                                    err  = 1;

     D_DEBUG_AT( GST, "decode_pad_added: type %s\n", GST_PAD_NAME(srcpad) );

     if (strstr( caps, "video" ) || g_strrstr( caps, "image" )) {
          D_DEBUG_AT( GST, "decode_pad_added: linking video pad '%s' caps '%s'\n", type, caps );
          sink = gst_element_get_pad( data->filter_video, "sink" );
          ret = gst_pad_link( srcpad, sink );
          gst_object_unref( sink );
     }
     else if (strstr( caps, "audio" )) {
          D_DEBUG_AT( GST, "decode_pad_added: linking audio pad '%s' caps '%s'\n", type, caps );
          sink = gst_element_get_pad( data->decode_audio, "sink" );
          ret = gst_pad_link( srcpad, sink );
          gst_object_unref( sink );
     }
     else {
          D_DEBUG_AT( GST, "decode_pad_added: unhandled pad '%s' caps '%s'\n", type, caps );
          return;
     }

     switch (ret) {
          case GST_PAD_LINK_OK:
               D_DEBUG_AT( GST, "gst_pad_link --> OK\n" );
               err = 0;
               break;
          case GST_PAD_LINK_WRONG_HIERARCHY:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_HIERARCHY\n" );
               break;
          case GST_PAD_LINK_WAS_LINKED:
               D_DEBUG_AT( GST, "gst_pad_link --> WAS_LINKED\n" );
               break;
          case GST_PAD_LINK_WRONG_DIRECTION:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_DIRECTION\n" );
               break;
          case GST_PAD_LINK_NOFORMAT:
               D_DEBUG_AT( GST, "gst_pad_link --> NOFORMAT\n" );
               break;
          case GST_PAD_LINK_NOSCHED:
               D_DEBUG_AT( GST, "gst_pad_link --> NOSCHED\n" );
               break;
          case GST_PAD_LINK_REFUSED:
               D_DEBUG_AT( GST, "gst_pad_link --> REFUSED\n" );
               break;
          default:
               D_DEBUG_AT( GST, "gst_pad_link --> '%d'\n", ret );
               break;
     }

     if (err) {
          direct_mutex_lock( &data->video_lock );
          data->error = 1;
          direct_waitqueue_signal( &data->video_cond );
          direct_mutex_unlock( &data->video_lock );
     }
}

static void
decode_audio_pad_added( GstElement *element, GstPad *pad, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;
     GstPadLinkReturn                       ret;
     GstPad                                *sink;
     int                                    err  = 1;

     D_DEBUG_AT( GST, "decode_audio_pad_added: type %s caps %s\n", GST_PAD_NAME(pad), gst_caps_to_string(gst_pad_get_caps(pad)));

     sink = gst_element_get_pad( data->queue_audio, "sink" );
     ret  = gst_pad_link( pad, sink );
     switch (ret) {
          case GST_PAD_LINK_OK:
               D_DEBUG_AT( GST, "gst_pad_link --> OK\n" );
               err = 0;
               break;
          case GST_PAD_LINK_WRONG_HIERARCHY:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_HIERARCHY\n" );
               break;
          case GST_PAD_LINK_WAS_LINKED:
               D_DEBUG_AT( GST, "gst_pad_link --> WAS_LINKED\n" );
               break;
          case GST_PAD_LINK_WRONG_DIRECTION:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_DIRECTION\n" );
               break;
          case GST_PAD_LINK_NOFORMAT:
               D_DEBUG_AT( GST, "gst_pad_link --> NOFORMAT\n" );
               break;
          case GST_PAD_LINK_NOSCHED:
               D_DEBUG_AT( GST, "gst_pad_link --> NOSCHED\n" );
               break;
          case GST_PAD_LINK_REFUSED:
               D_DEBUG_AT( GST, "gst_pad_link --> REFUSED\n" );
               break;
          default:
               D_DEBUG_AT( GST, "gst_pad_link --> '%d'\n", ret );
               break;
     }

     gst_object_unref( sink );

     direct_mutex_lock( &data->video_lock );
     if (err)
          data->error = 1;
     else
          data->parsed_audio = 1;
     direct_waitqueue_signal( &data->video_cond );
     direct_mutex_unlock( &data->video_lock );
}

static void
decode_video_pad_added(GstElement *element, GstPad *pad, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;
     GstPadLinkReturn                       ret;
     GstPad                                *sink;
     int                                    i;
     int                                    width;
     int                                    height;
     int                                    err  = 1;
     GstCaps                               *caps = gst_pad_get_caps( pad );

     D_DEBUG_AT( GST, "decode_video_pad_added: type %s caps %s\n", GST_PAD_NAME(pad), gst_caps_to_string(caps) );

     for (i = 0; i < gst_caps_get_size(caps); i++) {
          const GstStructure *str = gst_caps_get_structure( caps, i );
          if (gst_structure_get_int( str, "width", &width ) && gst_structure_get_int( str, "height", &height )) {
               data->width  = width;
               data->height = height;
          }
     }

     sink = gst_element_get_pad( data->queue_video, "sink" );
     ret  = gst_pad_link( pad, sink );
     switch (ret) {
          case GST_PAD_LINK_OK:
               D_DEBUG_AT( GST, "gst_pad_link --> OK\n" );
               err = 0;
               break;
          case GST_PAD_LINK_WRONG_HIERARCHY:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_HIERARCHY\n" );
               break;
          case GST_PAD_LINK_WAS_LINKED:
               D_DEBUG_AT( GST, "gst_pad_link --> WAS_LINKED\n" );
               break;
          case GST_PAD_LINK_WRONG_DIRECTION:
               D_DEBUG_AT( GST, "gst_pad_link --> WRONG_DIRECTION\n" );
               break;
          case GST_PAD_LINK_NOFORMAT:
               D_DEBUG_AT( GST, "gst_pad_link --> NOFORMAT\n" );
               break;
          case GST_PAD_LINK_NOSCHED:
               D_DEBUG_AT( GST, "gst_pad_link --> NOSCHED\n" );
               break;
          case GST_PAD_LINK_REFUSED:
               D_DEBUG_AT( GST, "gst_pad_link --> REFUSED\n" );
               break;
          default:
               D_DEBUG_AT( GST, "gst_pad_link --> '%d'\n", ret );
               break;
     }

     gst_object_unref( sink );
     direct_mutex_lock( &data->video_lock );
     if (err)
          data->error = 1;
     else
          data->parsed_video = 1;
     direct_waitqueue_signal( &data->video_cond );
     direct_mutex_unlock( &data->video_lock );
}

static int
prepare( IDirectFBVideoProvider_GSTREAMER_data *data, int argc, char *argv[], char *filename )
{
     int max_signals = 5;

     direct_mutex_init( &data->audio_lock );
     direct_mutex_init( &data->video_lock );
     direct_waitqueue_init( &data->audio_cond );
     direct_waitqueue_init( &data->video_cond );

     gst_init( &argc, &argv );

     data->pipeline       = gst_pipeline_new( "uri-decode-pipeline" );
     data->decode         = gst_element_factory_make( "uridecodebin", "uri-decode-bin" );
     data->decode_audio   = gst_element_factory_make( "decodebin2", "decode-audio" );
     data->convert_audio  = gst_element_factory_make ("audioconvert", "convert-audio");
     data->resample_audio = gst_element_factory_make ("audioresample", "resample-audio");
     data->filter_video   = gst_element_factory_make( "ffmpegcolorspace", "filter-video" );
     data->scale_video    = gst_element_factory_make ("videoscale", "scale-video");
     data->decode_video   = gst_element_factory_make( "decodebin2", "decode-video" );
     data->queue_audio    = gst_element_factory_make( "queue", "queue-audio" );
     data->queue_video    = gst_element_factory_make( "queue", "queue-video" );
     data->appsink_audio  = gst_element_factory_make( "appsink", "sink-buffer-audio" );
     data->appsink_video  = gst_element_factory_make( "appsink", "sink-buffer-video" );

     if (!data->pipeline || !data->decode || !data->filter_video || !data->scale_video || !data->decode_audio || !data->decode_video || !data->queue_audio || !data->queue_video || !data->appsink_audio || !data->appsink_video) {
          D_DEBUG_AT( GST, "error: failed to create some gstreamer elements\n" );
          return 0;
     }

     g_signal_connect( data->decode, "pad-added", G_CALLBACK(decode_pad_added), data );
     g_signal_connect( data->decode_audio, "unknown-type", G_CALLBACK(decode_audio_unknown_type), data );
     g_signal_connect( data->decode_audio, "pad-added", G_CALLBACK(decode_audio_pad_added), data );
     g_signal_connect( data->decode_video, "unknown-type", G_CALLBACK(decode_video_unknown_type), data );
     g_signal_connect( data->decode_video, "pad-added", G_CALLBACK(decode_video_pad_added), data );

     GstCaps *caps = gst_caps_new_simple ("video/x-raw-rgb",
     //"bpp", G_TYPE_INT, "32",
     //"depth", G_TYPE_INT, "32",             
     //"framerate", GST_TYPE_FRACTION, 25, 1,
     //"pixel-aspect-ratio", GST_TYPE_FRACTION, 1, 1,
     //"width", G_TYPE_INT, 320,
     //"height", G_TYPE_INT, 240,
     NULL);

     g_object_set( G_OBJECT(data->decode), "uri", filename, NULL );

     gst_bin_add_many( GST_BIN(data->pipeline), data->decode, data->filter_video, data->scale_video, data->decode_audio, data->decode_video, data->queue_audio, data->queue_video, data->appsink_audio, data->appsink_video, NULL );

     //gst_element_link( data->queue_audio, data->convert_audio );
     //gst_element_link( data->convert_audio, data->resample_audio );
     //gst_element_link( data->resample_audio, data->appsink_audio );
     gst_element_link( data->queue_audio, data->appsink_audio );
     gst_element_link( data->queue_video, data->appsink_video );

     //gst_element_link_filtered( data->filter_video, data->scale_video, caps );
     //gst_element_link( data->scale_video, data->decode_video );
     gst_element_link_filtered( data->filter_video, data->decode_video, caps );

     data->bus = gst_pipeline_get_bus( GST_PIPELINE(data->pipeline) );

     direct_mutex_lock( &data->video_lock );

     gst_element_set_state( GST_ELEMENT(data->pipeline), GST_STATE_PAUSED );
     gst_bus_add_watch( data->bus, pipeline_bus_call, data );

     while (!data->parsed_video && !data->error && --max_signals)
          direct_waitqueue_wait_timeout( &data->video_cond, &data->video_lock, 1000000 );

     direct_mutex_unlock( &data->video_lock );

     if (data->error || data->width < 1 || data->height < 1)
          return 0;

     return 1;
}

static void *
process_audio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = arg;
     GstAppSink                            *sink = (GstAppSink *)data->appsink_audio;
     GstBuffer                             *buffer;
     IDirectFBSurface_data                 *dst_data;
     CoreSurfaceBufferLock                  lock;
     DFBResult                              ret;

     while (!data->error) {
          buffer = gst_app_sink_pull_buffer( sink );
          if (!buffer)
               break;

          D_DEBUG_AT( GST, "appsink_audio_new_buffer: len %d caps '%s'\n", GST_BUFFER_SIZE(buffer), gst_caps_to_string(gst_buffer_get_caps(buffer)) );

          gst_buffer_unref( buffer );
     }

     return 0;
}

static void *
process_video( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = arg;
     GstAppSink                            *sink = (GstAppSink *)data->appsink_video;
     GstBuffer                             *buffer;
     IDirectFBSurface_data                 *dst_data;
     CoreSurfaceBufferLock                  lock;
     DFBResult                              ret;

     DIRECT_INTERFACE_GET_DATA_FROM( data->dest, dst_data, IDirectFBSurface );

     while (!data->error) {
          buffer = gst_app_sink_pull_buffer( sink );
          if (!buffer)
               break;

          D_DEBUG_AT( GST, "appsink_video_new_buffer: len %d caps '%s'\n", GST_BUFFER_SIZE(buffer), gst_caps_to_string(gst_buffer_get_caps(buffer)) );

          ret = dfb_surface_lock_buffer( dst_data->surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
          if (ret)
               break;

          direct_memcpy(lock.addr, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer));

          gst_buffer_unref( buffer );

          dfb_surface_unlock_buffer( dst_data->surface, &lock );

          if (data->callback)
               data->callback( data->ctx );
     }

     return 0;
}

/*****************************************************************************/

static void
IDirectFBVideoProvider_GSTREAMER_Destruct( IDirectFBVideoProvider *thiz )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = thiz->priv;

     release_events( data );
     direct_mutex_deinit( &data->events_lock );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBVideoProvider_GSTREAMER_AddRef( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     data->ref++;

     return DR_OK;
}

static DirectResult
IDirectFBVideoProvider_GSTREAMER_Release( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     if (--data->ref == 0)
          IDirectFBVideoProvider_GSTREAMER_Destruct( thiz );

     return DR_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetCapabilities( IDirectFBVideoProvider *thiz, DFBVideoProviderCapabilities *caps )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetCapabilitiesn" );

     if (!caps)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     *caps = DVCAPS_BASIC | DVCAPS_SCALE | DVCAPS_SPEED;

     if (data->seekable)
          *caps |= DVCAPS_SEEK;

     if (data->interlaced)
          *caps |= DVCAPS_INTERLACED;     

     if (data->audio_thread)
          *caps |= DVCAPS_VOLUME;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetSurfaceDescription( IDirectFBVideoProvider *thiz, DFBSurfaceDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetSurfaceDescription\n" );

     if (!desc)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     desc->flags = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;

     if (data->interlaced) {
          desc->flags |= DSDESC_CAPS;
          desc->caps = DSCAPS_INTERLACED;
     }

     desc->width  = data->width;
     desc->height = data->height;
     desc->pixelformat = DSPF_RGB24;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetStreamDescription( IDirectFBVideoProvider *thiz, DFBStreamDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetStreamDescription\n" );

     if (!desc)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     desc->caps = DVSCAPS_VIDEO;

     desc->video.aspect = 0.0;

     if (data->audio_thread) {
          desc->caps |= DVSCAPS_AUDIO;

          desc->audio.samplerate = 0;
          desc->audio.channels   = 0;
          desc->audio.bitrate    = 0;
     }

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_PlayTo( IDirectFBVideoProvider *thiz, IDirectFBSurface *dest, const DFBRectangle *dest_rect, DVFrameCallback callback, void *ctx )
{
     IDirectFBSurface_data *dest_data;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_PlayTo\n" );

     if (!dest)
          return DFB_INVARG;

     if (!dest->priv)
          return DFB_DESTROYED;

     dest_data = dest->priv;

     direct_mutex_lock( &data->video_lock );

     data->dest     = dest;
     data->ctx      = ctx;
     data->callback = callback;

     if (data->parsed_audio) {
          data->audio_thread = direct_thread_create( DTT_DEFAULT, process_audio, (void*)data, "Gstreamer Audio" );
     } else {
          gst_bin_remove_many( GST_BIN(data->pipeline), data->decode_audio, data->queue_audio, data->appsink_audio, NULL );
          data->audio_thread = 0;
     }

     data->video_thread = direct_thread_create( DTT_DEFAULT, process_video, (void*)data, "Gstreamer Video" );

     gst_element_set_state( GST_ELEMENT(data->pipeline), GST_STATE_PLAYING );

     data->status = DVSTATE_PLAY;

     data->speed = 1.0f;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_Stop( IDirectFBVideoProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_Stop\n" );

     direct_mutex_lock( &data->video_lock );

     gst_element_set_state( data->pipeline, GST_STATE_NULL );

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetStatus( IDirectFBVideoProvider *thiz, DFBVideoProviderStatus *status )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )
     
     D_DEBUG_AT( GST, "GSTREAMER_GetStatus\n" );

     if (!status)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     *status = data->status;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SeekTo( IDirectFBVideoProvider *thiz, double seconds )
{
     s64    time;
     double pos = 0.0;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SeekTo\n" );

     if (seconds < 0.0)
          return DFB_INVARG;

     if (!data->seekable)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetPos( IDirectFBVideoProvider *thiz, double *seconds )
{
     s64 position;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetPos\n" );

     if (!seconds)
          return DFB_INVARG;
/*
     position = get_stream_clock( data ) - data->start_time;
     *seconds = (position < 0) ? 0.0 : ((double)position/AV_TIME_BASE);
*/   
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetLength( IDirectFBVideoProvider *thiz, double *seconds )
{    
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetLength\n" );

     if (!seconds)
          return DFB_INVARG;
/*
     if (data->context->duration != AV_NOPTS_VALUE) {
          *seconds = (double)data->context->duration/AV_TIME_BASE;
          return DFB_OK;
     }
*/  
     *seconds = 9.0;

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetColorAdjustment( IDirectFBVideoProvider *thiz, DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetColorAdjustment\n" );
     if (!adj)
          return DFB_INVARG;

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SetColorAdjustment( IDirectFBVideoProvider *thiz, const DFBColorAdjustment *adj )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SetColorAdjustment\n" );

     if (!adj)
          return DFB_INVARG;

     return DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SetPlaybackFlags( IDirectFBVideoProvider *thiz, DFBVideoProviderPlaybackFlags flags )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SetPlaybackFlags\n" );

     if (flags & ~DVPLAY_LOOPING)
          return DFB_UNSUPPORTED;

     if (flags & DVPLAY_LOOPING && !data->seekable)
          return DFB_UNSUPPORTED;
          
     data->flags = flags;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SetSpeed( IDirectFBVideoProvider *thiz, double multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SetSpeed\n" );

     if (multiplier != 0.0 || multiplier != 1.0)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     data->speed = multiplier;
     gst_element_set_state( GST_ELEMENT(data->pipeline), multiplier == 0.0f ? GST_STATE_PAUSED : GST_STATE_PLAYING);

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult 
IDirectFBVideoProvider_GSTREAMER_GetSpeed( IDirectFBVideoProvider *thiz, double *ret_multiplier )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetSpeed\n" );

     if (!ret_multiplier)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     *ret_multiplier = data->speed;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SetVolume( IDirectFBVideoProvider *thiz, float level )
{
     DFBResult ret = DFB_UNSUPPORTED;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SetVolume\n" );

     if (level < 0.0)
          return DFB_INVARG;
/*     
#ifdef HAVE_FUSIONSOUND
     if (data->audio.playback) {
          ret = data->audio.playback->SetVolume( data->audio.playback, level );
          if (ret == DFB_OK)
               data->volume = level;
     }
#endif
*/
     return ret;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetVolume( IDirectFBVideoProvider *thiz, float *ret_level )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetVolume\n" );

     if (!ret_level)
          return DFB_INVARG;
/*
     *ret_level = data->volume;
*/
     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_CreateEventBuffer( IDirectFBVideoProvider *thiz, IDirectFBEventBuffer **ret_buffer )
{
     IDirectFBEventBuffer *buffer;
     DFBResult             ret;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_CreateEventBuffer\n" );

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
IDirectFBVideoProvider_GSTREAMER_AttachEventBuffer( IDirectFBVideoProvider *thiz, IDirectFBEventBuffer *buffer )
{
     DFBResult  ret;
     EventLink *link;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_AttachEventBuffer\n" );

     if (!buffer)
          return DFB_INVARG;

     ret = buffer->AddRef( buffer );
     if (ret)
          return ret;

     link = D_MALLOC( sizeof(EventLink) );
     if (!link) {
          buffer->Release( buffer );
          return D_OOM();
     }

     link->buffer = buffer;

     direct_mutex_lock( &data->events_lock );
     direct_list_append( (DirectLink**)&data->events, &link->link );
     direct_mutex_unlock( &data->events_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_DetachEventBuffer( IDirectFBVideoProvider *thiz, IDirectFBEventBuffer *buffer )
{
     DFBResult  ret = DFB_ITEMNOTFOUND;
     EventLink *link;

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_DetachEventBuffer\n" );

     if (!buffer)
          return DFB_INVARG;

     direct_mutex_lock( &data->events_lock );

     direct_list_foreach (link, data->events) {
          if (link->buffer == buffer) {
               direct_list_remove( (DirectLink**)&data->events, &link->link );
               link->buffer->Release( link->buffer );
               D_FREE( link );
               ret = DFB_OK;
               break;
          }
     }

     direct_mutex_unlock( &data->events_lock );

     return ret;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_EnableEvents( IDirectFBVideoProvider *thiz, DFBVideoProviderEventType mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_EnableEvents\n" );

     if (mask & ~DVPET_ALL)
          return DFB_INVARG;

     data->events_mask |= mask;

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_DisableEvents( IDirectFBVideoProvider *thiz, DFBVideoProviderEventType mask )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_DisableEvents\n" );

     if (mask & ~DVPET_ALL)
          return DFB_INVARG;

     data->events_mask &= ~mask;

     return DFB_OK;
}

static DFBResult
Probe( IDirectFBVideoProvider_ProbeContext *ctx )
{
     return DFB_OK;
}

static DFBResult
Construct( IDirectFBVideoProvider *thiz, IDirectFBDataBuffer *buffer )
{
     if (!((IDirectFBDataBuffer_data*)buffer->priv)->filename) {
          return DFB_UNSUPPORTED;
     }

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBVideoProvider_GSTREAMER )

     data->ref    = 1;
     data->status = DVSTATE_STOP;
     data->buffer = buffer;
     data->speed  = 0.0;
     data->volume = 0.0;

     data->events_mask = DVPET_NONE;

     if (!prepare( data, 0, NULL, ((IDirectFBDataBuffer_data*)buffer->priv)->filename )) {
          return DFB_UNSUPPORTED;
     }

     thiz->AddRef                = IDirectFBVideoProvider_GSTREAMER_AddRef;
     thiz->Release               = IDirectFBVideoProvider_GSTREAMER_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_GSTREAMER_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_GSTREAMER_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_GSTREAMER_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_GSTREAMER_PlayTo;
     thiz->Stop                  = IDirectFBVideoProvider_GSTREAMER_Stop;
     thiz->GetStatus             = IDirectFBVideoProvider_GSTREAMER_GetStatus;
     thiz->SeekTo                = IDirectFBVideoProvider_GSTREAMER_SeekTo;
     thiz->GetPos                = IDirectFBVideoProvider_GSTREAMER_GetPos;
     thiz->GetLength             = IDirectFBVideoProvider_GSTREAMER_GetLength;
     thiz->GetColorAdjustment    = IDirectFBVideoProvider_GSTREAMER_GetColorAdjustment;
     thiz->SetColorAdjustment    = IDirectFBVideoProvider_GSTREAMER_SetColorAdjustment;
     thiz->SetPlaybackFlags      = IDirectFBVideoProvider_GSTREAMER_SetPlaybackFlags;
     thiz->SetSpeed              = IDirectFBVideoProvider_GSTREAMER_SetSpeed;
     thiz->GetSpeed              = IDirectFBVideoProvider_GSTREAMER_GetSpeed;
     thiz->SetVolume             = IDirectFBVideoProvider_GSTREAMER_SetVolume;
     thiz->GetVolume             = IDirectFBVideoProvider_GSTREAMER_GetVolume;
     thiz->CreateEventBuffer     = IDirectFBVideoProvider_GSTREAMER_CreateEventBuffer;
     thiz->AttachEventBuffer     = IDirectFBVideoProvider_GSTREAMER_AttachEventBuffer;
     thiz->DetachEventBuffer     = IDirectFBVideoProvider_GSTREAMER_DetachEventBuffer;
     thiz->EnableEvents          = IDirectFBVideoProvider_GSTREAMER_EnableEvents;
     thiz->DisableEvents         = IDirectFBVideoProvider_GSTREAMER_DisableEvents;

     return DFB_OK;
}

