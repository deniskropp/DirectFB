/*
 * Copyright (C) 2007 Claudio Ciccani <klan@users.sf.net>
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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <jasper/jasper.h>

#include <directfb.h>

#include <display/idirectfbsurface.h>

#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>
#include <core/surface.h>

#include <misc/gfx_util.h>
#include <misc/util.h>

#include <direct/types.h>
#include <direct/messages.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/interface.h>


static DFBResult Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult Construct( IDirectFBImageProvider *thiz,
                            IDirectFBDataBuffer    *buffer );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, JPEG2000 )


typedef struct {
     int                    ref;
     
     jas_image_t           *image;
     
     u32                   *buf;
     
     DIRenderCallback       render_callback;
     void                  *render_callback_ctx;
} IDirectFBImageProvider_JPEG2000_data;


/*****************************************************************************/

static int             jasper_refs = 0;
static pthread_mutex_t jasper_lock = PTHREAD_MUTEX_INITIALIZER;

static void
init_jasper( void )
{
     pthread_mutex_lock( &jasper_lock );
     
     if (++jasper_refs == 1) {
          jas_image_fmtops_t fmtops;
          int                fmtid = 0;
	
          fmtops.decode = jp2_decode;
          fmtops.encode = jp2_encode;
          fmtops.validate = jp2_validate;
          jas_image_addfmt(fmtid, "jp2", "jp2",
               "JPEG-2000 JP2 File Format Syntax (ISO/IEC 15444-1)", &fmtops);
          ++fmtid;
	
          fmtops.decode = jpc_decode;
          fmtops.encode = jpc_encode;
          fmtops.validate = jpc_validate;
          jas_image_addfmt(fmtid, "jpc", "jpc",
               "JPEG-2000 Code Stream Syntax (ISO/IEC 15444-1)", &fmtops);
          ++fmtid;
     }
     
     pthread_mutex_unlock( &jasper_lock );
}

static void
release_jasper( void )
{
     pthread_mutex_lock( &jasper_lock );
     
     if (--jasper_refs == 0)
          jas_cleanup();
     
     pthread_mutex_unlock( &jasper_lock );
}     

/*****************************************************************************/

static void
IDirectFBImageProvider_JPEG2000_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_JPEG2000_data *data = thiz->priv;

     if (data->buf)
          D_FREE( data->buf );
    
     if (data->image)
          jas_image_destroy( data->image );
          
     release_jasper();

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_JPEG2000_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )

     data->ref++;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG2000_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )

     if (--data->ref == 0)
           IDirectFBImageProvider_JPEG2000_Destruct( thiz );

     return DFB_OK;
}  

static DFBResult
IDirectFBImageProvider_JPEG2000_RenderTo( IDirectFBImageProvider *thiz,
                                          IDirectFBSurface       *destination,
                                          const DFBRectangle     *dest_rect )
{
     IDirectFBSurface_data  *dst_data;
     CoreSurface            *dst_surface;
     CoreSurfaceBufferLock   lock;
     DFBRectangle            rect;
     DFBRegion               clip;
     DIRenderCallbackResult  cb_result = DIRCR_OK;
     DFBResult               ret       = DFB_OK;
     
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )
     
     if (!destination)
          return DFB_INVARG;
          
     dst_data = destination->priv;
     if (!dst_data || !dst_data->surface)
          return DFB_DESTROYED;
          
     dst_surface = dst_data->surface;
     
     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          rect = *dest_rect;
          rect.x += dst_data->area.wanted.x;
          rect.y += dst_data->area.wanted.y;
     }
     else {
          rect = dst_data->area.wanted;
     }
     
     dfb_region_from_rectangle( &clip, &dst_data->area.current );
     if (!dfb_rectangle_region_intersects( &rect, &clip ))
          return DFB_OK;

     ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAF_CPU_WRITE, &lock );
     if (ret)
          return ret;
     
     if (!data->buf) {
          int  cmptlut[3];
          int  width, height;
          int  cw, ch;
          int  tlx, tly;
          int  hs, vs;
          int  i, j;
          bool direct, mono;
     
          if (jas_image_numcmpts(data->image) > 1) {
               cmptlut[0] = jas_image_getcmptbytype(data->image,
                              JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_R));
               cmptlut[1] = jas_image_getcmptbytype(data->image,
                              JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_G));
               cmptlut[2] = jas_image_getcmptbytype(data->image,
                              JAS_IMAGE_CT_COLOR(JAS_CLRSPC_CHANIND_RGB_B));
               if (cmptlut[0] < 0 || cmptlut[1] < 0 || cmptlut[2] < 0) {
                    dfb_surface_unlock_buffer( dst_surface, &lock );
                    return DFB_UNSUPPORTED;
               }
               mono = false;
          }
          else {
               cmptlut[0] = cmptlut[1] = cmptlut[2] = 0;
               mono = true;
          }
     
          width = jas_image_width(data->image);
          height = jas_image_height(data->image);
          cw = jas_image_cmptwidth(data->image, 0);
          ch = jas_image_cmptheight(data->image, 0);
          tlx = jas_image_cmpttlx(data->image, 0);
          tly = jas_image_cmpttly(data->image, 0);
          hs = jas_image_cmpthstep(data->image, 0);
          vs = jas_image_cmptvstep(data->image, 0);
          
          data->buf = D_MALLOC( width*height*4 );
          if (!data->buf) {
               dfb_surface_unlock_buffer( dst_surface, &lock );
               return D_OOM();
          }
          
          direct = (rect.w == width && rect.h == height && data->render_callback);
          
#define GET_SAMPLE( n, x, y ) ({ \
     int _s; \
     _s = jas_image_readcmptsample(data->image, cmptlut[n], x, y); \
     _s >>= jas_image_cmptprec(data->image, cmptlut[n]) - 8; \
     if (_s > 255) \
          _s = 255; \
     else if (_s < 0) \
          _s = 0; \
     _s; \
})
            
          for (i = 0; i < height; i++) {
               u32 *dst = data->buf + i * width;
               int  x, y;
               
               y = (i - tly) / vs;
               if (y >= 0 && y < height) {     
                    for (j = 0; j < width; j++) {
                         x = (j - tlx) / hs;
                         if (x >= 0 && x < width) {
                              unsigned int r, g, b;
                              if (mono) {
                                   r = g = b = GET_SAMPLE(0, x, y);
                              }
                              else {
                                   r = GET_SAMPLE(0, x, y);
                                   g = GET_SAMPLE(1, x, y);
                                   b = GET_SAMPLE(2, x, y);
                              }
                              *dst++ = 0xff000000 | (r << 16) | (g << 8) | b;
                         }
                         else {
                              *dst++ = 0;
                         }
                    }
               }
               else {
                    memset( dst, 0, width*4 );
               }
               
               if (direct) {
                    DFBRectangle r = { rect.x, rect.y+i, width, 1 };
                    
                    dfb_copy_buffer_32( data->buf + i*width,
                                        lock.addr, lock.pitch, &r, dst_surface, &clip );
                    
                    if (data->render_callback) {                    
                         r = (DFBRectangle) { 0, i, width, 1 };
                         cb_result = data->render_callback( &r, data->render_callback_ctx );
                         if (cb_result != DIRCR_OK)
                              break;
                    }
               }
          }
          
          if (!direct) {
               dfb_scale_linear_32( data->buf, width, height,
                                    lock.addr, lock.pitch, &rect, dst_surface, &clip );

               if (data->render_callback) {
                    DFBRectangle r = { 0, 0, width, height };
                    data->render_callback( &r, data->render_callback_ctx );
               }
          }
          
          if (cb_result != DIRCR_OK) {
               D_FREE( data->buf );
               data->buf = NULL;
               ret = DFB_INTERRUPTED;
          }
     }
     else {
          int width  = jas_image_width(data->image);
          int height = jas_image_height(data->image);
          
          dfb_scale_linear_32( data->buf, width, height,
                               lock.addr, lock.pitch, &rect, dst_surface, &clip );
          
          if (data->render_callback) {
               DFBRectangle r = {0, 0, width, height};
               data->render_callback( &r, data->render_callback_ctx );
          }
     }
     
     dfb_surface_unlock_buffer( dst_surface, &lock );
     
     return ret;
}

static DFBResult
IDirectFBImageProvider_JPEG2000_SetRenderCallback( IDirectFBImageProvider *thiz,
                                              DIRenderCallback        callback,
                                              void                   *ctx )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )

     data->render_callback     = callback;
     data->render_callback_ctx = ctx;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG2000_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                  DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )

     if (!desc)
          return DFB_INVARG;

     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->width       = jas_image_width(data->image);
     desc->height      = jas_image_height(data->image);
     desc->pixelformat = dfb_primary_layer_pixelformat();

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_JPEG2000_GetImageDescription( IDirectFBImageProvider *thiz,
                                                DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA( IDirectFBImageProvider_JPEG2000 )

     if (!desc)
          return DFB_INVARG;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

/* exported symbols */

#define JP2_SIGNATURE "\x00\x00\x00\x0C\x6A\x50\x20\x20\x0D\x0A\x87\x0A"
#define JPC_SIGNATURE "\xFF\x4F"

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (!memcmp( ctx->header, JP2_SIGNATURE, sizeof(JP2_SIGNATURE)-1 ) ||
         !memcmp( ctx->header, JPC_SIGNATURE, sizeof(JPC_SIGNATURE)-1 ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           IDirectFBDataBuffer    *buffer )
{
     IDirectFBDataBuffer_data *buffer_data = buffer->priv;
     jas_stream_t             *stream      = NULL;
     char                     *chunk       = NULL;

     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBImageProvider_JPEG2000 )
     
     data->ref = 1;
     
     init_jasper();
     
     if (buffer_data->is_memory) {
          IDirectFBDataBuffer_Memory_data *memory_data = buffer->priv;
          stream = jas_stream_memopen( (void*)memory_data->buffer, memory_data->length );
     }
     else if (buffer_data->filename && access( buffer_data->filename, F_OK ) == 0) {
          stream = jas_stream_fopen( buffer_data->filename, "rb" );
     }
     else { /* download */
          unsigned int size = 0;
          
          while (1) {
               unsigned int len = 0;
               
               chunk = D_REALLOC( chunk, size+4096 );
               if (!chunk) {
                    IDirectFBImageProvider_JPEG2000_Destruct( thiz );
                    return D_OOM();
               }
               
               buffer->WaitForData( buffer, 4096 );
               if (buffer->GetData( buffer, 4096, chunk+size, &len ))
                    break;
               size += len;
          }

          if (!size) {
               D_FREE( chunk );
               IDirectFBImageProvider_JPEG2000_Destruct( thiz );
               return DFB_IO;
          }

          stream = jas_stream_memopen( chunk, size );
     }    
     
     if (!stream) {
          if (chunk)
               D_FREE( chunk );
          IDirectFBImageProvider_JPEG2000_Destruct( thiz );
          return DFB_UNSUPPORTED;
     }
          
     data->image = jas_image_decode( stream, -1, 0 );

     jas_stream_close( stream );
     if (chunk)
          D_FREE( chunk );
     
     if (!data->image) {
          IDirectFBImageProvider_JPEG2000_Destruct( thiz );
          return DFB_FAILURE;
     }
     
     switch (jas_image_numcmpts(data->image)) {
          case 1:
          case 3:
               break;
          default:
               IDirectFBImageProvider_JPEG2000_Destruct( thiz );
               return DFB_UNSUPPORTED;
     }

     thiz->AddRef                = IDirectFBImageProvider_JPEG2000_AddRef;
     thiz->Release               = IDirectFBImageProvider_JPEG2000_Release;
     thiz->RenderTo              = IDirectFBImageProvider_JPEG2000_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_JPEG2000_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_JPEG2000_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_JPEG2000_GetSurfaceDescription;

     return DFB_OK;
}
