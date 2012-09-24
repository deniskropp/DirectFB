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

#include <gst/gst.h>
#include <gst/gstutils.h>
#include <glib.h>
#include <gst/app/gstappsink.h>

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


static DFBResult Probe( IDirectFBVideoProvider_ProbeContext *ctx );
static DFBResult Construct( IDirectFBVideoProvider *thiz, IDirectFBDataBuffer *buffer );


#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBVideoProvider, GSTREAMER )

D_DEBUG_DOMAIN( GST, "VideoProvider/GST", "DirectFB VideoProvider Gstreamer" );

#define AUDIO_BUFSIZE 4096

static DirectThread *gmain_thread;
static int           gmain_initialised;

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
     double                         secpos;
     double                         secdur;
     unsigned long long             bytepos;
     unsigned long long             bytedur;
     float                          volume;
     
     IDirectFBDataBuffer           *buffer;
     bool                           interlaced;
     bool                           seekable;
     bool                           playing;

     GstElement                    *pipeline;
     GstElement                    *decode;
     GstElement                    *convert_video;
     GstElement                    *queue_audio;
     GstElement                    *queue_video;
     GstElement                    *appsink_audio;
     GstElement                    *appsink_video;

#ifdef HAVE_FUSIONSOUND
     IFusionSound                  *audio_interface;
     IFusionSoundStream            *audio_stream;
     IFusionSoundPlayback          *audio_playback;
#endif

     int                            width;
     int                            height;
     int                            format;
     int                            audio_channels;
     int                            audio_rate;
     int                            audio_samplesize;
     int                            audio_format;
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

static void
update_status( IDirectFBVideoProvider_GSTREAMER_data *data, GstElement *elem, bool lock )
{
     GstFormat query = GST_FORMAT_TIME;
     gint64    val;

     if (lock) {
          direct_mutex_lock( &data->video_lock );
     }

#ifdef HAVE_GSTREAMER_1_0_API
     if (gst_element_query_position( elem, query, &val ))
#else
     if (gst_element_query_position( elem, &query, &val ))
#endif
          data->secpos = val / 1000000000.0;
#ifdef HAVE_GSTREAMER_1_0_API
     if (gst_element_query_duration( elem, query, &val ))
#else
     if (gst_element_query_duration( elem, &query, &val ))
#endif
          data->secdur = val / 1000000000.0;

     /*query = GST_FORMAT_BYTES;

     if (gst_element_query_position( elem, &query, &val ))
          data->bytepos = val;

     if (gst_element_query_duration( elem, &query, &val ))
          data->bytedur = val;*/

     D_DEBUG_AT( GST, "media seconds at %f/%f\n" , data->secpos, data->secdur);

     if (lock) {
          direct_mutex_unlock( &data->video_lock );
     }
}

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
decode_unknown_type( GstBin *bin, GstPad *pad, GstCaps *caps, gpointer ptr )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;

     D_DEBUG_AT( GST, "decode_unknown_type: type %s caps %s\n", GST_PAD_NAME(pad), gst_caps_to_string(caps) );
}

static void
decode_pad_added( GstElement *element, GstPad *srcpad, gpointer ptr )
{
     GstPadLinkReturn                       ret;
     IDirectFBVideoProvider_GSTREAMER_data *data = ptr;
     GstPad                                *sink = NULL;
     char                                  *type = GST_PAD_NAME( srcpad );
#ifdef HAVE_GSTREAMER_1_0_API
     GstCaps                               *pad_caps = gst_pad_query_caps( srcpad, NULL );
#else
     GstCaps                               *pad_caps = gst_pad_get_caps( srcpad );
#endif
     char                                  *caps = gst_caps_to_string( pad_caps );
     int                                    err  = 1;
     int                                    i;
     int                                    width;
     int                                    height;
     int                                    depth = 16;
     int                                    rate;
     int                                    channels;
     gboolean                               sign = 1;

     D_DEBUG_AT( GST, "decode_pad_added: type %s (caps %s)\n", GST_PAD_NAME(srcpad), caps );

     if (strstr( caps, "video" ) || strstr( caps, "image" )) {
          D_DEBUG_AT( GST, "decode_pad_added: linking video pad '%s' caps '%s'\n", type, caps );
          sink = gst_element_get_static_pad( data->convert_video, "sink" );
          ret = gst_pad_link( srcpad, sink );
          gst_object_unref( sink );

          for (i = 0; i < gst_caps_get_size(pad_caps); i++) {
               const GstStructure *str = gst_caps_get_structure( pad_caps, i );
               if (gst_structure_get_int( str, "width", &width ) && gst_structure_get_int( str, "height", &height )) {
                    data->width  = width;
                    data->height = height;
                    data->parsed_video = 1;
               }
          }
     }
#ifdef HAVE_FUSIONSOUND
     else if (strstr( caps, "audio" )) {
          D_DEBUG_AT( GST, "decode_pad_added: linking audio pad '%s' caps '%s'\n", type, caps );
          sink = gst_element_get_static_pad( data->queue_audio, "sink" );
          ret = gst_pad_link( srcpad, sink );
          gst_object_unref( sink );

          data->parsed_audio = 1;

          for (i = 0; i < gst_caps_get_size(pad_caps); i++) {
               const GstStructure *str = gst_caps_get_structure( pad_caps, i );

               gst_structure_get_int( str, "depth", &depth );
               gst_structure_get_boolean( str, "signed", &sign );

               if (gst_structure_get_int( str, "rate", &rate ) && gst_structure_get_int( str, "channels", &channels )) {
                    data->audio_rate     = rate;
                    data->audio_channels = channels;
                    data->audio_format   = FSSF_UNKNOWN;
     
                    switch (depth) {
                         case 8:
                              if (!sign) {
                                   data->audio_format = FSSF_U8;
                                   D_DEBUG_AT( GST, "decode_audio_pad_added: audio format is FSSF_U8.\n");
                              }
                              break;
                         case 16:
                              if (sign) {
                                   data->audio_format = FSSF_S16;
                                   D_DEBUG_AT( GST, "decode_audio_pad_added: audio format is FSSF_S16.\n");
                              }
                              break;
                         case 24:
                              if (sign) {
                                   data->audio_format = FSSF_S24;
                                   D_DEBUG_AT( GST, "decode_audio_pad_added: audio format is FSSF_S24.\n");
                              }
                              break;
                         case 32:
                              if (sign) {
                                   data->audio_format = FSSF_S32;
                                   D_DEBUG_AT( GST, "decode_audio_pad_added: audio format is FSSF_S32.\n");
                              }
                              break;
                         default:
                              break;
                    }
     
                    if (data->audio_format == FSSF_UNKNOWN)
                         D_DEBUG_AT( GST, "decode_audio_pad_added: audio format is UNKNOWN.\n" );
               }
          }
     }
#endif
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

     direct_mutex_lock( &data->video_lock );
     if (err)
          data->error = 1;
     direct_waitqueue_signal( &data->video_cond );
     direct_mutex_unlock( &data->video_lock );
}

static void *
gmain( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = arg;
     int                                    argc = 0;
     GMainLoop                             *loop;

     direct_mutex_lock( &data->video_lock );

     gst_init( &argc, NULL );

     loop = g_main_loop_new( NULL, FALSE );

     gmain_initialised = 1;

     direct_waitqueue_signal( &data->video_cond );
     direct_mutex_unlock( &data->video_lock );

     g_main_loop_run( loop );

     gmain_thread = 0;
     gmain_initialised = 0;

     return 0;
}

static int
prepare( IDirectFBVideoProvider_GSTREAMER_data *data, int argc, char *argv[], char *filename )
{
     GstBus *bus;
     int     max_signals = 5;

     direct_mutex_init( &data->audio_lock );
     direct_mutex_init( &data->video_lock );
     direct_waitqueue_init( &data->audio_cond );
     direct_waitqueue_init( &data->video_cond );

     if (!gmain_thread) {
          direct_mutex_lock( &data->video_lock );

          gmain_thread = direct_thread_create( DTT_DEFAULT, gmain, (void*)data, "Gmain" );

          if (!gmain_initialised )
               direct_waitqueue_wait( &data->video_cond, &data->video_lock );

          direct_mutex_unlock( &data->video_lock );
     }

     data->pipeline       = gst_pipeline_new( "uri-decode-pipeline" );
     data->decode         = gst_element_factory_make( "uridecodebin", "uri-decode-bin" );
#ifdef HAVE_GSTREAMER_1_0_API
     data->convert_video  = gst_element_factory_make( "videoconvert", "convert-video" );
#else
     data->convert_video  = gst_element_factory_make( "ffmpegcolorspace", "convert-video" );
#endif
     data->queue_audio    = gst_element_factory_make( "queue", "queue-audio" );
     data->queue_video    = gst_element_factory_make( "queue", "queue-video" );
     data->appsink_audio  = gst_element_factory_make( "appsink", "sink-buffer-audio" );
     data->appsink_video  = gst_element_factory_make( "appsink", "sink-buffer-video" );

     if (!data->pipeline || !data->decode || !data->queue_audio || !data->queue_video || !data->appsink_audio || !data->appsink_video) {
          D_DEBUG_AT( GST, "error: failed to create some gstreamer elements %p, %p, %p, %p, %p, %p\n", data->pipeline, data->decode, data->queue_audio, data->queue_video, data->appsink_audio, data->appsink_video );
          return 0;
     }

     g_signal_connect( data->decode, "pad-added", G_CALLBACK(decode_pad_added), data );
     g_signal_connect( data->decode, "unknown-type", G_CALLBACK(decode_unknown_type), data );
#ifdef HAVE_GSTREAMER_1_0_API
     GstCaps *caps = gst_caps_new_simple( "video/x-raw", "format", G_TYPE_STRING, "BGRA", NULL );
#else
     GstCaps *caps = gst_caps_new_simple( "video/x-raw-rgb", NULL );
#endif
     g_object_set( G_OBJECT(data->decode), "uri", filename, NULL );

#ifdef HAVE_FUSIONSOUND
     gst_bin_add_many( GST_BIN(data->pipeline), data->decode, data->convert_video, data->queue_video, data->appsink_video, data->queue_audio, data->appsink_audio, NULL );
     gst_element_link( data->queue_audio, data->appsink_audio );
#else
     gst_bin_add_many( GST_BIN(data->pipeline), data->decode, data->convert_video, data->queue_video, data->appsink_video, NULL );
#endif
     gst_element_link_filtered( data->convert_video, data->queue_video, caps );
     gst_element_link( data->queue_video, data->appsink_video );

     bus = gst_pipeline_get_bus( GST_PIPELINE(data->pipeline) );

     direct_mutex_lock( &data->video_lock );

     gst_element_set_state( GST_ELEMENT(data->pipeline), GST_STATE_PAUSED );
     gst_bus_add_watch( bus, pipeline_bus_call, data );
     gst_object_unref( bus );

     while (!data->parsed_video && !data->error && --max_signals)
          direct_waitqueue_wait_timeout( &data->video_cond, &data->video_lock, 1000000 );

     direct_mutex_unlock( &data->video_lock );

     if (data->error || data->width < 1 || data->height < 1) {
          D_DEBUG_AT( GST, "error: preparation was not successful\n" );
          return 0;
     }
sleep(1);
     update_status( data, data->pipeline, true );

     D_DEBUG_AT( GST, "prepare: parsed a/v = %d/%d\n", data->parsed_audio, data->parsed_video );

     return 1;
}

static void *
process_audio( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = arg;
     GstAppSink                            *sink = (GstAppSink *)data->appsink_audio;
#ifdef HAVE_GSTREAMER_1_0_API
     GstSample                             *sample;
#endif
     GstBuffer                             *buffer;
     char                                   buf[AUDIO_BUFSIZE];
     int                                    buflen;
     int                                    offset = 0;
     int                                    len;
     int                                    copied;
#ifdef HAVE_FUSIONSOUND
     while (!data->error) {
#ifdef HAVE_GSTREAMER_1_0_API
          sample = gst_app_sink_pull_sample( sink );
          buffer = gst_sample_get_buffer( sample );
          buflen = gst_buffer_get_size( buffer );
#else
          buffer = gst_app_sink_pull_buffer( sink );
#endif
          if (!buffer)
               break;

#ifdef HAVE_GSTREAMER_1_0_API
          D_DEBUG_AT( GST, "appsink_audio_new_buffer: len %d caps '%s'\n", (int)gst_buffer_get_size(buffer), gst_caps_to_string(gst_sample_get_caps(sample)) );
#else
          D_DEBUG_AT( GST, "appsink_audio_new_buffer: len %d caps '%s'\n", GST_BUFFER_SIZE(buffer), gst_caps_to_string(gst_buffer_get_caps(buffer)) );
#endif
#ifdef HAVE_GSTREAMER_1_0_API
          while (buflen > 0) {
               len = buflen > AUDIO_BUFSIZE ? AUDIO_BUFSIZE : buflen;
               copied = gst_buffer_extract( buffer, offset, buf, len );
               if (copied > 0 && copied < len)
                    len = copied;
               data->audio_stream->Write( data->audio_stream, buf, len / data->audio_samplesize );
               buflen -= len;
               offset += len;
          }
#else
          data->audio_stream->Write( data->audio_stream, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer) / data->audio_samplesize );
#endif
          gst_buffer_unref( buffer );
     }
#endif
     D_DEBUG_AT( GST, "Audio thread terminated.\n" );

     return 0;
}

static void *
process_video( DirectThread *self, void *arg )
{
     IDirectFBVideoProvider_GSTREAMER_data *data = arg;
     GstAppSink                            *sink = (GstAppSink *)data->appsink_video;
#ifdef HAVE_GSTREAMER_1_0_API
     GstSample                             *sample;
#endif
     GstBuffer                             *buffer;
     IDirectFBSurface_data                 *dst_data;
     CoreSurfaceBufferLock                  lock;
     DFBResult                              ret;

     DIRECT_INTERFACE_GET_DATA_FROM( data->dest, dst_data, IDirectFBSurface );

     while (!data->error) {
#ifdef HAVE_GSTREAMER_1_0_API
          sample = gst_app_sink_pull_sample( sink );
          buffer = gst_sample_get_buffer( sample );
#else
          buffer = gst_app_sink_pull_buffer( sink );
#endif
          if (!buffer)
               break;
#ifdef HAVE_GSTREAMER_1_0_API
          D_DEBUG_AT( GST, "appsink_video_new_buffer: len %d caps '%s'\n", (int)gst_buffer_get_size(buffer), gst_caps_to_string(gst_sample_get_caps(sample)) );
#else
          D_DEBUG_AT( GST, "appsink_video_new_buffer: len %d caps '%s'\n", GST_BUFFER_SIZE(buffer), gst_caps_to_string(gst_buffer_get_caps(buffer)) );
#endif
          ret = dfb_surface_lock_buffer( dst_data->surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
          if (ret)
               break;
#ifdef HAVE_GSTREAMER_1_0_API
          gst_buffer_extract( buffer, 0, lock.addr, gst_buffer_get_size(buffer) );
#else
          direct_memcpy( lock.addr, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer) );
#endif
          gst_buffer_unref( buffer );

          dfb_surface_unlock_buffer( dst_data->surface, &lock );

          if (data->callback)
               data->callback( data->ctx );

          update_status( data, data->appsink_video, true );
     }

     data->status = DVSTATE_FINISHED;

     D_DEBUG_AT( GST, "Video thread terminated.\n" );

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
     desc->pixelformat = DSPF_ARGB;

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

#ifdef HAVE_FUSIONSOUND
     if (data->parsed_audio && data->audio_playback) {
          data->audio_thread = direct_thread_create( DTT_DEFAULT, process_audio, (void*)data, "Gstreamer Audio" );
     } else {
          gst_bin_remove_many( GST_BIN(data->pipeline), data->queue_audio, data->appsink_audio, NULL );
          data->audio_thread = 0;
     }
#endif

     data->video_thread = direct_thread_create( DTT_DEFAULT, process_video, (void*)data, "Gstreamer Video" );

     gst_element_set_state( GST_ELEMENT(data->pipeline), GST_STATE_PLAYING );

     data->status  = DVSTATE_PLAY;
     data->speed   = 1.0f;
     data->playing = true;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_SetDestination( IDirectFBVideoProvider *thiz, IDirectFBSurface *dest, const DFBRectangle *dest_rect )
{

     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_SetDestination %d,%d - %d,%d\n", dest_rect->x, dest_rect->y, dest_rect->w, dest_rect->h );

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

     direct_mutex_lock( &data->video_lock );

     *seconds = data->secpos;

     direct_mutex_unlock( &data->video_lock );

     return DFB_OK;
}

static DFBResult
IDirectFBVideoProvider_GSTREAMER_GetLength( IDirectFBVideoProvider *thiz, double *seconds )
{    
     DIRECT_INTERFACE_GET_DATA( IDirectFBVideoProvider_GSTREAMER )

     D_DEBUG_AT( GST, "GSTREAMER_GetLength\n" );

     if (!seconds)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     *seconds = data->secdur;

     direct_mutex_unlock( &data->video_lock );

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

     D_DEBUG_AT( GST, "GSTREAMER_SetSpeed(%f)\n", multiplier );

     if (multiplier != 0.0 && multiplier != 1.0)
          return DFB_INVARG;

     direct_mutex_lock( &data->video_lock );

     if (!data->playing) {
          D_DEBUG_AT( GST, "GSTREAMER_SetSpeed(%f) no possible, playback not yet triggered\n", multiplier );
          direct_mutex_unlock( &data->video_lock );
          return DFB_INVARG;
     }

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

     direct_mutex_lock( &data->video_lock );

     *ret_level = data->volume;

     direct_mutex_unlock( &data->video_lock );

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

     data->ref     = 1;
     data->status  = DVSTATE_STOP;
     data->buffer  = buffer;
     data->speed   = 0.0;
     data->secpos  = 0.0;
     data->secdur  = 0.0;
     data->bytepos = 0;
     data->bytedur = 0;
     data->volume  = 0.0;

     data->events_mask = DVPET_NONE;

     if (!prepare( data, 0, NULL, ((IDirectFBDataBuffer_data*)buffer->priv)->filename )) {
          return DFB_UNSUPPORTED;
     }
#ifdef HAVE_FUSIONSOUND
     if (data->parsed_audio && idirectfb_singleton->GetInterface( idirectfb_singleton, "IFusionSound", 0, 0, (void **)&data->audio_interface ) == DFB_OK) {
          FSStreamDescription dsc;
          DFBResult           ret;

          if (data->audio_channels > FS_MAX_CHANNELS)
               data->audio_channels = FS_MAX_CHANNELS;

          dsc.flags        = FSSDF_BUFFERSIZE | FSSDF_CHANNELS | FSSDF_SAMPLEFORMAT | FSSDF_SAMPLERATE;
          dsc.channels     = data->audio_channels;
          dsc.samplerate   = data->audio_rate;
          dsc.buffersize   = dsc.samplerate;
          dsc.sampleformat = data->audio_format;

          D_DEBUG_AT( GST, "creating stream with %d channels at rate %d\n", data->audio_channels, data->audio_rate );

          ret = data->audio_interface->CreateStream( data->audio_interface, &dsc, &data->audio_stream );
          if (ret != DFB_OK) {
               D_INFO( "IDirectFBVideoProvider_Gstreamer: IFusionSound::CreateStream() failed! -> %s\n", DirectFBErrorString(ret) );
               data->audio_interface->Release( data->audio_interface );
               data->audio_interface = NULL;
          }
          else {
               data->audio_stream->GetPlayback( data->audio_stream, &data->audio_playback );
               data->audio_samplesize = 2 * dsc.channels;
          }
     }
#endif
     thiz->AddRef                = IDirectFBVideoProvider_GSTREAMER_AddRef;
     thiz->Release               = IDirectFBVideoProvider_GSTREAMER_Release;
     thiz->GetCapabilities       = IDirectFBVideoProvider_GSTREAMER_GetCapabilities;
     thiz->GetSurfaceDescription = IDirectFBVideoProvider_GSTREAMER_GetSurfaceDescription;
     thiz->GetStreamDescription  = IDirectFBVideoProvider_GSTREAMER_GetStreamDescription;
     thiz->PlayTo                = IDirectFBVideoProvider_GSTREAMER_PlayTo;
     thiz->SetDestination        = IDirectFBVideoProvider_GSTREAMER_SetDestination;
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

