/*
 * Copyright (C) 2004 Claudio "KLaN" Ciccani <klan82@cheapnet.it>
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <media/idirectfbvideoprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layers.h>
#include <core/surfaces.h>
#include <display/idirectfbsurface.h>

#include <xine.h>
#include <xine/xineutils.h>
#include <xine/configfile.h>
#include <xine/xine_internal.h>
#include <xine/video_out.h>

#include "video_out_dfb.h"


static DFBResult Probe(IDirectFBVideoProvider_ProbeContext *ctx);

static DFBResult Construct(IDirectFBVideoProvider *thiz, const char* filename);


#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION(IDirectFBVideoProvider, Xine)


typedef struct
{
	int                 ref;
	char*               mrl;
	char*               cfg;

	xine_t*             xine;
	xine_video_port_t*  vo;
	xine_audio_port_t*  ao;
	xine_post_t*        post;
	xine_stream_t*      stream;
	xine_event_queue_t* queue;

	int                 lenght;
	
	char                is_playing;
	char                is_paused;

	IDirectFBSurface*   dest;
	DFBRectangle        dest_rect;

} IDirectFBVideoProvider_Xine_data;




static void DFBxine_frame_output(void* cdata, int width, int height,
					DFBRectangle* dest_rect);

static void DFBxine_event_listner(void* cdata, const xine_event_t* event);






static void
IDirectFBVideoProvider_Xine_Destruct(IDirectFBVideoProvider* thiz)
{
	IDirectFBVideoProvider_Xine_data* data;

	data = (IDirectFBVideoProvider_Xine_data*) thiz->priv;

	if(data->xine)
	{
		if(data->stream)
		{
			if(data->is_playing)
				xine_stop(data->stream);

			xine_close(data->stream);

			if(data->queue)
				xine_event_dispose_queue(data->queue);
			
			xine_dispose(data->stream);
		}

		if(data->post)
			xine_post_dispose(data->xine, data->post);

		if(data->vo)
			xine_close_video_driver(data->xine, data->vo);

		if(data->ao)
			xine_close_audio_driver(data->xine, data->ao);

		if(data->cfg)
		{
			xine_config_save(data->xine, data->cfg);
			free(data->cfg);
		}

		xine_exit(data->xine);
	}

	DFBFREE(data->mrl);
	DFB_DEALLOCATE_INTERFACE(thiz);
}


static DFBResult
IDirectFBVideoProvider_Xine_AddRef(IDirectFBVideoProvider* thiz)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	data->ref++;

	return(DFB_OK);
}


static DFBResult
IDirectFBVideoProvider_Xine_Release(IDirectFBVideoProvider *thiz)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(--(data->ref) == 0)
		IDirectFBVideoProvider_Xine_Destruct(thiz);

	return(DFB_OK);
}


static DFBResult
IDirectFBVideoProvider_Xine_GetCapabilities(IDirectFBVideoProvider* thiz,
					DFBVideoProviderCapabilities* caps)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)
	
	if(!caps)
		return(DFB_INVARG);

	*caps = (DVCAPS_BASIC | DVCAPS_SCALE | 
		 DVCAPS_BRIGHTNESS | DVCAPS_CONTRAST);
	
	if(xine_get_stream_info(data->stream,
				XINE_STREAM_INFO_SEEKABLE))
		*caps |= DVCAPS_SEEK;

	return(DFB_OK);
}


static DFBResult
IDirectFBVideoProvider_Xine_GetSurfaceDescription(IDirectFBVideoProvider* thiz,
					DFBSurfaceDescription* desc)
{
	
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(!desc)
		return(DFB_INVARG);
	
	if(data->stream)
	{
		int width  = xine_get_stream_info(data->stream,
					XINE_STREAM_INFO_VIDEO_WIDTH);
		int height = xine_get_stream_info(data->stream,
					XINE_STREAM_INFO_VIDEO_HEIGHT);
		
		desc->flags  = (DSDESC_WIDTH | DSDESC_HEIGHT
					| DSDESC_PIXELFORMAT);

		if(xine_get_stream_info(data->stream, XINE_STREAM_INFO_HAS_VIDEO))
		{
			/* width must be a multiple of 4 */
			desc->width  = width + ((width & 3) ? (4 - (width & 3)) : 0);
			/* height must be a multiple of 2 */
			desc->height = height + (height & 1);
		} else
		{
			/* we are usign a post plugin */
			desc->width  = 320;
			desc->height = 240;
		}
		
		desc->pixelformat = dfb_primary_layer_pixelformat();
		
		return(DFB_OK);
	}

	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_PlayTo(IDirectFBVideoProvider* thiz, 
				   IDirectFBSurface* dest, 
				   const DFBRectangle* dest_rect,
				   DVFrameCallback callback, void* ctx)
{
	IDirectFBSurface_data* dest_data;

	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(!dest)
		return(DFB_INVARG);

	if(!data->stream)
		return(DFB_FAILURE);

	dest_data = (IDirectFBSurface_data*) dest->priv;
	if(!dest_data)
		return(DFB_DEAD);

	if(data->is_playing && !data->is_paused)
		return(DFB_UNSUPPORTED);

	data->dest = dest;

	memset(&(data->dest_rect), 0, sizeof(DFBRectangle));

	if(dest_rect)
	{
		if(dest_rect->w < 1 || dest_rect->h < 1)
			return(DFB_INVARG);

		data->dest_rect = *dest_rect;
	}

	if(!xine_port_send_gui_data(data->vo,
			XINE_GUI_SEND_DRAWABLE_CHANGED,
			(void*) data->dest))
		return(DFB_FAILURE);

	if(callback)
	{
		dfb_frame_callback_t  frame_callback;

		frame_callback.frame_cb = callback;
		frame_callback.cdata    = ctx;

		xine_port_send_gui_data(data->vo,
				XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
				(void*) &frame_callback);
	}		

	if(data->is_paused)
	{
		xine_set_param(data->stream, XINE_PARAM_SPEED,
					XINE_SPEED_NORMAL);
		data->is_paused = 0;
	} else
	{
		if(data->post)
		{
			xine_post_out_t* audio_source;

			audio_source = xine_get_audio_source(data->stream);
			xine_post_wire_audio_port(audio_source,
					data->post->audio_input[0]);
		}
		
		xine_play(data->stream, 0, 0);
		data->is_playing = 1;
	}
	
	return(DFB_OK);
}


static DFBResult
IDirectFBVideoProvider_Xine_Stop(IDirectFBVideoProvider* thiz)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(data->is_playing && !data->is_paused)
	{
		xine_set_param(data->stream, XINE_PARAM_SPEED,
					XINE_SPEED_PAUSE);
		data->is_paused = 1;		
		return(DFB_OK);
	}

	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_SeekTo(IDirectFBVideoProvider* thiz,
					double seconds)
{	
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)
		
	if(data->stream && data->is_playing)
	{		
		int offset;

		if(!xine_get_stream_info(data->stream,
					XINE_STREAM_INFO_SEEKABLE))
			return(DFB_UNSUPPORTED);

		offset = (int) seconds * 1000;

		if(offset < 0)
			offset = 0;
		if(data->lenght && offset > data->lenght)
			offset = data->lenght - 1;

		xine_play(data->stream, 0, offset);
		data->is_paused = 0;
		
		return(DFB_OK);	
	}

	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_GetPos(IDirectFBVideoProvider* thiz,
					double* seconds)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)
		
	if(!seconds)
		return(DFB_INVARG);

	if(data->stream && data->is_playing)
	{
		int pos = 0;
		int try = 5;

		while(try--)
		{
			if(xine_get_pos_length(data->stream, NULL, &pos, NULL))
				break;

			xine_usec_sleep(100000);				
		}

		*seconds = (double) pos / 1000.0;
		return(DFB_OK);
	}

	*seconds = 0.0;
		
	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_GetLength(IDirectFBVideoProvider* thiz,
					double* seconds)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)
		
	if(!seconds)
		return(DFB_INVARG);

	if(data->stream)
	{
		if(xine_get_pos_length(data->stream, NULL,
					NULL, &(data->lenght)))
		{
			*seconds = (double) data->lenght / 1000.0;
			return(DFB_OK);
		}
	}

	*seconds = 0.0;
		
	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_GetColorAdjustment(IDirectFBVideoProvider* thiz,
						DFBColorAdjustment* adj)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(!adj)
		return(DFB_INVARG);

	if(data->stream)
	{
		adj->flags      = (DCAF_BRIGHTNESS | DCAF_CONTRAST);
		adj->brightness = xine_get_param(data->stream,
						XINE_PARAM_VO_BRIGHTNESS);
		adj->contrast   = xine_get_param(data->stream,
						XINE_PARAM_VO_CONTRAST);
		return(DFB_OK);
	}
	
	return(DFB_UNSUPPORTED);
}


static DFBResult
IDirectFBVideoProvider_Xine_SetColorAdjustment(IDirectFBVideoProvider* thiz,
						DFBColorAdjustment* adj)
{
	INTERFACE_GET_DATA(IDirectFBVideoProvider_Xine)

	if(!adj)
		return(DFB_INVARG);

	if(data->stream)
	{
		if(adj->flags & DCAF_BRIGHTNESS)
		{
			xine_set_param(data->stream, XINE_PARAM_VO_BRIGHTNESS,
							adj->brightness);
		}

		if(adj->flags & DCAF_CONTRAST)
		{
			xine_set_param(data->stream, XINE_PARAM_VO_CONTRAST,
							adj->contrast);
		}

		return(DFB_OK);
	}
		
	return(DFB_UNSUPPORTED);
}


static DFBResult
Probe(IDirectFBVideoProvider_ProbeContext* ctx)
{
	xine_t*            xine;
	xine_video_port_t* vo;
	xine_audio_port_t* ao;
	xine_stream_t*     stream;
	dfb_visual_t       visual;
	const char*        home;
	char*              cfg;
	DFBResult          result;
	
	
	if(!(xine = xine_new()))
		return(DFB_FAILURE);

	if((home = xine_get_homedir()))
	{
		asprintf(&cfg, "%s/.xine/config", home);
		xine_config_load(xine, cfg);
		free(cfg);
	}

	xine_init(xine);

	visual.surface   = NULL;
	visual.output_cb = DFBxine_frame_output;

	vo = xine_open_video_driver(xine, "DFB",
				XINE_VISUAL_TYPE_DFB, (void*) &visual);

	ao = xine_open_audio_driver(xine, "oss", NULL);
	
	stream = xine_stream_new(xine, ao, vo);

	result = (xine_open(stream, ctx->filename))
			? DFB_OK : DFB_UNSUPPORTED;

	if(stream)
	{
		xine_close(stream);
		xine_dispose(stream);
	}
	
	if(vo) xine_close_video_driver(xine, vo);
	
	if(ao) xine_close_audio_driver(xine, ao);
	
	xine_exit(xine);

	return(result);	
}


static DFBResult
Construct(IDirectFBVideoProvider* thiz, const char *filename)
{
	dfb_visual_t visual;
	const char*  home;
	
	DFB_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBVideoProvider_Xine)

	data->ref = 1;
	data->mrl = DFBSTRDUP(filename);

	if(!(data->xine = xine_new()))
		return(DFB_FAILURE);

	if(getenv("XINERC"))
	{
		data->cfg = DFBSTRDUP(getenv("XINERC"));
	} else
	if((home = xine_get_homedir()))
	{
		char* xine_dir;
		
		asprintf(&(xine_dir), "%s/.xine", home);
		mkdir(xine_dir, 755);
		asprintf(&(data->cfg), "%s/config", xine_dir);
		free(xine_dir);		
	}

	xine_config_load(data->xine, data->cfg);

	xine_init(data->xine);
	xine_engine_set_param(data->xine, XINE_ENGINE_PARAM_VERBOSITY,
						XINE_VERBOSITY_LOG);

	visual.surface   = NULL;
	visual.output_cb = DFBxine_frame_output;
	visual.cdata     = (void*) data;
	
	data->vo = xine_open_video_driver(data->xine, "DFB",
				XINE_VISUAL_TYPE_DFB, (void*) &visual);
	if(!data->vo)
	{
		xine_exit(data->xine);
		return(DFB_FAILURE);
	}

	/* try alsa first */
	data->ao = xine_open_audio_driver(data->xine, "alsa", NULL);
	if(!data->ao)
	{
		data->ao = xine_open_audio_driver(data->xine, "oss", NULL);

		if(!data->ao)
		{
			xine_close_video_driver(data->xine, data->vo);
			xine_exit(data->xine);
			return(DFB_FAILURE);
		}
	}

	data->stream = xine_stream_new(data->xine, data->ao, data->vo);
	if(!data->stream)
	{
		xine_close_video_driver(data->xine, data->vo);
		xine_close_audio_driver(data->xine, data->ao);
		xine_exit(data->xine);
		return(DFB_FAILURE);
	}

	xine_set_param(data->stream, XINE_PARAM_VERBOSITY,
					XINE_VERBOSITY_LOG);

	data->queue = xine_event_new_queue(data->stream);
	if(data->queue)
	{
		xine_event_create_listener_thread(data->queue,
				DFBxine_event_listner, (void*) data);
	}

	if(!xine_open(data->stream, data->mrl))
	{
		if(data->queue)
			xine_event_dispose_queue(data->queue);		
		xine_dispose(data->stream);
		xine_close_video_driver(data->xine, data->vo);
		xine_close_audio_driver(data->xine, data->ao);
		xine_exit(data->xine);
		return(DFB_FAILURE);
	}

	/* init a post plugin if no video */
	if(!xine_get_stream_info(data->stream, XINE_STREAM_INFO_HAS_VIDEO))
	{
		config_values_t* config = data->xine->config;
		cfg_entry_t* entry      = NULL;
		const char* post_plugin = NULL;

		entry = config->lookup_entry(config, "gui.post_audio_plugin");

		if(entry)
		{
			post_plugin = (entry->type) ? entry->str_value
						    : entry->unknown_value;
		}

		if(!post_plugin)
		{
			const char* const* post_list;

			post_list = xine_list_post_plugins_typed(data->xine,
					XINE_POST_TYPE_AUDIO_VISUALIZATION);

			if(post_list)
			{
				post_plugin = config->register_string(config,
						"gui.post_audio_plugin", post_list[0],
						"Audio visualization plugin",
						NULL, 0, NULL, NULL);
			}
		}

		if(post_plugin)
		{
			xine_audio_port_t* aos[2] = {data->ao, NULL};
			xine_video_port_t* vos[2] = {data->vo, NULL};
			
			data->post = xine_post_init(data->xine,
						post_plugin, 0, aos, vos);
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
			

	return(DFB_OK);
}


static void 
DFBxine_frame_output(void* cdata, int width, int height,
			DFBRectangle* dest_rect)
{
	IDirectFBVideoProvider_Xine_data* data;

	data = (IDirectFBVideoProvider_Xine_data*) cdata;

	if(!data) return;

	*dest_rect = data->dest_rect;
}


static void
DFBxine_event_listner(void* cdata, const xine_event_t* event)
{
	IDirectFBVideoProvider_Xine_data* data;

	data = (IDirectFBVideoProvider_Xine_data*) cdata;

	if(!data) return;

	switch(event->type)
	{
		case XINE_EVENT_UI_PLAYBACK_FINISHED:
		{
			xine_stop(data->stream);
			data->is_playing = 0;
			data->is_paused  = 0;
		}
		break;

		default:
		break;
	}
}

