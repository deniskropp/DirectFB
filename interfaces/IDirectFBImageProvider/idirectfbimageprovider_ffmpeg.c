/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 * (Implemented by Broadcom, based on code from Claudio).
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
#include <stdarg.h>
#include <string.h>

#include "directfb.h"

#include "display/idirectfbsurface.h"
#include "media/idirectfbimageprovider.h"
#include "core/surface.h"
#include "core/layers.h"
#include "misc/gfx_util.h"

#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"

static DFBResult Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider *thiz,
                            ... );

#include "direct/interface_implementation.h"

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, FFMPEG )

typedef struct {

     IDirectFBImageProvider_data base;

     int                    width;
     int                    height;

     u32                 peeked_bytes;
     u32                 *image;
     AVFormatContext     *av_fmt_ctx;
     AVIOContext         *avio_ctx;

} IDirectFBImageProvider_FFMPEG_data;

/*****************************************************************************/

static int
fetch_data ( void *opaque, uint8_t *buf, int buf_size)
{

     IDirectFBImageProvider_FFMPEG_data *data = opaque;
     DFBResult ret;
     int total = buf_size;

     if (data == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "fetch_data : data == null\n");
          return 0;
     }

     if (buf == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "fetch_data : buf == null\n");
          return 0;
     }

     while (buf_size > 0) {
          unsigned int read = 0;

          ret = data->base.buffer->WaitForData( data->base.buffer, buf_size );

          if (ret == DFB_OK)
               ret = data->base.buffer->PeekData( data->base.buffer, buf_size, data->peeked_bytes, buf, &read );

          if (ret)
               break;

          buf += read;
          data->peeked_bytes += read;
          buf_size -= read;
     }

     return total - buf_size;
}

static DFBResult
ffmpeg_parse( IDirectFBImageProvider_FFMPEG_data *data )
{
     u8        *buffer;
     u32       buffer_size;

     data->av_fmt_ctx = avformat_alloc_context();

     if (data->av_fmt_ctx == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "av_fmt_ctx == NULL!! \n");
          return DFB_FAILURE;
     }

     data->base.buffer->GetLength(data->base.buffer, &buffer_size);

     buffer = av_malloc(buffer_size);
     if (buffer == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "buffer == NULL!! \n");

          avformat_free_context(data->av_fmt_ctx);
          return DFB_FAILURE;
     }

     data->peeked_bytes = 0;

     data->avio_ctx = avio_alloc_context(buffer, buffer_size, 0,
                                         data,
                                         fetch_data,
                                         NULL,
                                         NULL );

     if (data->avio_ctx == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "avio_ctx == NULL!! \n");
          avformat_free_context(data->av_fmt_ctx);
          av_free(buffer);
          return DFB_FAILURE;
     }

     data->avio_ctx->seekable = 0;
     data->av_fmt_ctx->pb = data->avio_ctx;

     if ((avformat_open_input(&data->av_fmt_ctx, "", NULL, NULL)) != 0)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "Error from av_format_open_input \n");
          return DFB_FAILURE;
     }

     /*Turning off the parsers speeds things up, but produces errros.*/
    /*data->av_fmt_ctx->flags |= AVFMT_FLAG_NOFILLIN;
     data->av_fmt_ctx->flags |= AVFMT_FLAG_NOPARSE;*/

     /* Populate the format context with the stream information.*/
     avformat_find_stream_info(data->av_fmt_ctx, NULL);

     /*Dump the parsed information out to the console.*/
     /*av_dump_format(data->av_fmt_ctx, 0,"", 0);*/

     if (data->av_fmt_ctx->nb_streams == 1
          && data->av_fmt_ctx->streams[0]->codec->codec_type == AVMEDIA_TYPE_VIDEO)
     {
          AVCodec *codec;
          if (!(codec = avcodec_find_decoder(data->av_fmt_ctx->streams[0]->codec->codec_id)))
          {
               D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                        "No codec found for id: (%d)\n",
                        data->av_fmt_ctx->streams[0]->codec->codec_id );
               goto fail_av_format_close;
          }
          else if (avcodec_open2(data->av_fmt_ctx->streams[0]->codec, codec, NULL) < 0)
          {
               D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                        "Error opening codec\n");
               goto fail_av_format_close;
          }
               data->height = data->av_fmt_ctx->streams[0]->codec->height;
               data->width  = data->av_fmt_ctx->streams[0]->codec->width;
     }
      else
     {
          /*There was no vidio stream found, or too many, fail*/
          goto fail_av_format_close;
     }

     return DFB_OK;

fail_av_format_close:
          avformat_close_input(&data->av_fmt_ctx);
          return DFB_FAILURE;
}

/*****************************************************************************/

static void
IDirectFBImageProvider_FFMPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_FFMPEG_data *data = thiz->priv;

     if (data->av_fmt_ctx)
     {
          avformat_close_input(&data->av_fmt_ctx);
     }

     if (data->image)
          D_FREE( data->image );

}

static DFBResult
IDirectFBImageProvider_FFMPEG_RenderTo( IDirectFBImageProvider *thiz,
                                     IDirectFBSurface       *destination,
                                     const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data  *dst_data;
     CoreSurface            *dst_surface;
     DFBRectangle            rect;
     DFBRegion               clip;
     DFBResult               ret       = DFB_OK;
     AVFrame                 *av_picture;
     AVCodecContext          *av_codec_ctx;
     AVPacket                av_pkt;
     u32                     av_ret;
     s32                     av_got_picture;
     u8                      *comp_data_buffer;
     struct SwsContext       *sw_sca_ctx = NULL;
     s32                     pitch[4];
     u32                     buffer_size;

     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_FFMPEG )

     dst_data = (IDirectFBSurface_data*) destination->priv;
     if (!dst_data)
          return DFB_DEAD;

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          rect = *dest_rect;
          rect.w += dst_data->area.wanted.w;
          rect.h += dst_data->area.wanted.h;
     }
     else {
          rect = dst_data->area.wanted;
     }

     av_codec_ctx = data->av_fmt_ctx->streams[0]->codec;

     av_picture = avcodec_alloc_frame();

     if (av_picture == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "av_picture == NULL\n");
          return DFB_FAILURE;
     }

     /*No scaling just colourspace conversion*/
     sw_sca_ctx = sws_getCachedContext(sw_sca_ctx, av_codec_ctx->width, av_codec_ctx->height, av_codec_ctx->pix_fmt,
                                  av_codec_ctx->width, av_codec_ctx->height, PIX_FMT_BGRA,
                                  SWS_FAST_BILINEAR, NULL,
                                  NULL, NULL);
     if (sw_sca_ctx == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "sw_sca_ctx == NULL\n");
          ret = DFB_FAILURE;
          goto fail_swscale;
     }


     data->base.buffer->GetLength(data->base.buffer, &buffer_size);

     comp_data_buffer = malloc(buffer_size);
     if (comp_data_buffer == NULL)
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "comp_data_buffer == NULL\n");
          ret = DFB_FAILURE;
          goto fail_comp_data_buffer;
     }

     av_init_packet(&av_pkt);

     data->base.buffer->SeekTo( data->base.buffer, 0);

     av_pkt.data = comp_data_buffer;

     /*We don't pass complete frames*/
     av_codec_ctx->flags |= CODEC_FLAG_TRUNCATED;

     do
     {
          data->base.buffer->PeekData( data->base.buffer, buffer_size, 0, comp_data_buffer, (unsigned int *) &av_pkt.size);
          av_ret = avcodec_decode_video2(av_codec_ctx, av_picture, &av_got_picture, &av_pkt);
     }
     while (av_pkt.size && !av_got_picture);

     if (av_got_picture)
     {
          uint8_t * dst_stride[4] = {(uint8_t *) data->image ,0,0,0};
          pitch[0] = data->width * 4;

          sws_scale(sw_sca_ctx, (const uint8_t* const*) av_picture->data, av_picture->linesize, 0, av_codec_ctx->height, dst_stride, pitch);

          /* actual rendering */
          if (dfb_rectangle_region_intersects( &rect, &clip ))
          {
               CoreSurfaceBufferLock lock;

               ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_CPU, CSAF_WRITE, &lock );
               if (ret)
               {
                    goto fail_dfb;
               }

               dfb_scale_linear_32( data->image, data->width, data->height,
                                    lock.addr, lock.pitch, &rect, dst_surface, &clip );

               dfb_surface_unlock_buffer( dst_surface, &lock );
          }
     }
     else
     {
          D_ERROR( "IDirectFBImageProvider_FFMPEG: "
                   "Used up all the data, but couldn't decode a picture\n");
          ret = DFB_FAILURE;
     }

fail_dfb:
     free(comp_data_buffer);
fail_comp_data_buffer:
     sws_freeContext(sw_sca_ctx);
fail_swscale:
     av_free(av_picture);

     return ret;
}

static DFBResult
IDirectFBImageProvider_FFMPEG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_FFMPEG )

     data->base.render_callback         = callback;
     data->base.render_callback_context = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_FFMPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_FFMPEG )

     if (!desc)
          return DFB_INVARG;

     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = data->width;
     desc->height      = data->height;
     desc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_FFMPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_FFMPEG )

     if (!desc)
          return DFB_INVARG;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

/* exported symbols */

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     /*Start code of a frame*/
     if (( ctx->header[0] == 0x00 &&
          ctx->header[1] == 0x00 &&
          ctx->header[2] == 0x00 &&
          ctx->header[3] == 0x01 ) ||
        ( ctx->header[0] == 0x00 &&
          ctx->header[1] == 0x00 &&
          ctx->header[2] == 0x01 &&
          ctx->header[3] == 0xB3 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult ret = DFB_FAILURE;
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_FFMPEG )

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->base.ref    = 1;
     data->base.buffer = buffer;
     data->base.core   = core;

     buffer->AddRef( buffer );

     /*Could call specific codec, but we don't compile in many.*/
     av_register_all();

     av_log_set_level(AV_LOG_ERROR);

     ret = ffmpeg_parse( data );

     if (ret) {
          IDirectFBImageProvider_FFMPEG_Destruct( thiz );
          return ret;
     }

     /* Allocate image data. */
     data->image = D_MALLOC( data->width * data->height * 4 );

     data->base.Destruct         = IDirectFBImageProvider_FFMPEG_Destruct;
     thiz->RenderTo              = IDirectFBImageProvider_FFMPEG_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_FFMPEG_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_FFMPEG_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_FFMPEG_GetSurfaceDescription;

     return DFB_OK;
}
