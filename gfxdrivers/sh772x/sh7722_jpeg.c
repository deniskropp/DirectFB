#ifdef SH7722_DEBUG_JPEG
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>
#include <jpeglib.h>

#undef HAVE_STDLIB_H

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>

#include <asm/types.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <directfb.h>

#include <core/layers.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/system.h>

#include <display/idirectfbsurface.h>
#include <media/idirectfbdatabuffer.h>
#include <media/idirectfbimageprovider.h>

#include "sh7722.h"
#include "sh7722_jpeglib.h"

D_DEBUG_DOMAIN( SH7722_JPEG, "SH7722/JPEG", "SH7722 JPEG Processing Unit" );

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx );

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBImageProvider, SH7722_JPEG )

/*
 * private data struct of IDirectFBImageProvider_SH7722_JPEG
 */
typedef struct {
     int                  ref;      /* reference counter */

     SH7722_JPEG_context  info;

     CoreDFB             *core;

     IDirectFBDataBuffer *buffer;
     DirectStream        *stream;

     DIRenderCallback     render_callback;
     void                *render_callback_context;
} IDirectFBImageProvider_SH7722_JPEG_data;

/**********************************************************************************************************************/

static void
IDirectFBImageProvider_SH7722_JPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_SH7722_JPEG_data *data = thiz->priv;

     data->buffer->Release( data->buffer );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBImageProvider_SH7722_JPEG_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBImageProvider_SH7722_JPEG_Release( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     if (--data->ref == 0)
          IDirectFBImageProvider_SH7722_JPEG_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_RenderTo( IDirectFBImageProvider *thiz,
                                             IDirectFBSurface       *destination,
                                             const DFBRectangle     *dest_rect )
{
     DFBResult              ret;
     DFBRegion              clip;
     DFBRectangle           rect; 
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;
     CoreSurfaceBufferLock  lock;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG);

     if (!data->buffer)
          return DFB_BUFFEREMPTY;

     DIRECT_INTERFACE_GET_DATA_FROM(destination, dst_data, IDirectFBSurface);

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     dfb_region_from_rectangle( &clip, &dst_data->area.current );

     if (dest_rect) {
          if (dest_rect->w < 1 || dest_rect->h < 1)
               return DFB_INVARG;
          
          rect.x = dest_rect->x + dst_data->area.wanted.x;
          rect.y = dest_rect->y + dst_data->area.wanted.y;
          rect.w = dest_rect->w;
          rect.h = dest_rect->h;
     }
     else
          rect = dst_data->area.wanted;

     if (!dfb_rectangle_region_intersects( &rect, &clip ))
          return DFB_OK;

     ret = dfb_surface_lock_buffer( dst_surface, CSBR_BACK, CSAID_GPU, CSAF_WRITE, &lock );
     if (ret)
          return ret;

     ret = SH7722_JPEG_Decode( &data->info, &rect, &clip, dst_surface->config.format,
                               lock.phys, lock.addr, lock.pitch, dst_surface->config.size.w, dst_surface->config.size.h );

     dfb_surface_unlock_buffer( dst_surface, &lock );

     return ret;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_SetRenderCallback( IDirectFBImageProvider *thiz,
                                                      DIRenderCallback        callback,
                                                      void                   *context )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBImageProvider_SH7722_JPEG)

     data->render_callback         = callback;
     data->render_callback_context = context;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_GetSurfaceDescription( IDirectFBImageProvider *thiz,
                                                          DFBSurfaceDescription  *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     if (!data->buffer)
          return DFB_BUFFEREMPTY;
     
     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->height      = data->info.height;
     desc->width       = data->info.width;
     desc->pixelformat = data->info.mode420 ? DSPF_NV12 : DSPF_NV16;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                        DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     if (!desc)
          return DFB_INVARG;

     if (!data->buffer)
          return DFB_BUFFEREMPTY;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_WriteBack( IDirectFBImageProvider *thiz,
                                              IDirectFBSurface       *surface,
                                              const DFBRectangle     *src_rect,
                                              const char             *filename )
{
     DFBResult              ret;
     DFBRegion              clip;
     DFBRectangle           rect; 
     IDirectFBSurface_data *src_data;
     CoreSurface           *src_surface;
     CoreSurfaceBufferLock  lock;
     DFBDimension           jpeg_size;

     CoreSurface           *tmp_surface;
     CoreSurfaceBufferLock  tmp_lock;
     int                    tmp_pitch;
     unsigned int           tmp_phys;
     
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     if (!surface || !filename)
          return DFB_INVARG;

     DIRECT_INTERFACE_GET_DATA_FROM(surface, src_data, IDirectFBSurface);

     D_DEBUG_AT( SH7722_JPEG, "%s - surface %p, rect %p to file %s\n",
               __FUNCTION__, surface, src_rect, filename );

     src_surface = src_data->surface;
     if (!src_surface)
          return DFB_DESTROYED;

     switch (src_surface->config.format) {
          case DSPF_NV12:
          case DSPF_NV16:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_RGB24:
               break;

          default:
               /* FIXME: implement fallback */
               D_UNIMPLEMENTED();
               return DFB_UNIMPLEMENTED;
     }

     dfb_region_from_rectangle( &clip, &src_data->area.current );

     if (src_rect) {
          if (src_rect->w < 1 || src_rect->h < 1)
               return DFB_INVARG;

          rect.x = src_rect->x + src_data->area.wanted.x;
          rect.y = src_rect->y + src_data->area.wanted.y;
          rect.w = src_rect->w;
          rect.h = src_rect->h;
     }
     else
          rect = src_data->area.wanted;

     if (!dfb_rectangle_region_intersects( &rect, &clip ))
          return DFB_INVAREA;

     jpeg_size.w = src_surface->config.size.w;
     jpeg_size.h = src_surface->config.size.h;
     
     /* it would be great if we had intermediate storage, since 
      * this prevents handling the encoding in 16-line chunks,
      * causing scaling artefacts at the border of these chunks */
     
     tmp_pitch = (jpeg_size.w + 3) & ~3;
     ret = dfb_surface_create_simple( data->core, tmp_pitch, jpeg_size.h,
                                      DSPF_NV16, DSCAPS_VIDEOONLY,
                                      CSTF_NONE, 0, 0, &tmp_surface );
     if( ret ) {
          /* too bad, we proceed without */
          D_DEBUG_AT( SH7722_JPEG, "%s - failed to create intermediate storage: %d\n",
               __FUNCTION__, ret );
          tmp_surface = 0;
          tmp_phys    = 0;
     }
     else {
          /* lock it to get the address */
          ret = dfb_surface_lock_buffer( tmp_surface, CSBR_FRONT, CSAID_GPU, CSAF_READ | CSAF_WRITE, &tmp_lock );
          if (ret) {
               dfb_surface_unref( tmp_surface );
               return ret;
          }
          tmp_phys = tmp_lock.phys;
          D_DEBUG_AT( SH7722_JPEG, "%s - surface locked at %x\n", __FUNCTION__, tmp_phys );
     }

     ret = dfb_surface_lock_buffer( src_surface, CSBR_FRONT, CSAID_GPU, CSAF_READ, &lock );
     if ( ret == DFB_OK ) {
          ret = SH7722_JPEG_Encode( filename, &rect, src_surface->config.format, lock.phys, lock.pitch,
                                    jpeg_size.w, jpeg_size.h, tmp_phys );
                                    
          dfb_surface_unlock_buffer( src_surface, &lock );
     }
     
     if( tmp_surface ) {
          /* unlock and release the created surface */
          dfb_surface_unlock_buffer( tmp_surface, &tmp_lock );
          dfb_surface_unref( tmp_surface );
     }

     return ret;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     SH7722DeviceData *sdev = dfb_gfxcard_get_device_data();

     if (sdev->sh772x != 7722)
          return DFB_UNSUPPORTED;

     /* Called with NULL when used for encoding. */
     if (!ctx)
          return DFB_OK;
          
     if (ctx->header[0] == 0xff && ctx->header[1] == 0xd8 && ctx->filename)
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
Construct( IDirectFBImageProvider *thiz,
           ... )
{
     DFBResult            ret;
     IDirectFBDataBuffer *buffer;
     CoreDFB             *core;
     va_list              tag;

     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDirectFBImageProvider_SH7722_JPEG);

     va_start( tag, thiz );
     buffer = va_arg( tag, IDirectFBDataBuffer * );
     core = va_arg( tag, CoreDFB * );
     va_end( tag );

     data->ref    = 1;
     data->buffer = buffer;
     data->core   = core;

     if (buffer) {
          IDirectFBDataBuffer_File_data *file_data;

          ret = buffer->AddRef( buffer );
          if (ret) {
               DIRECT_DEALLOCATE_INTERFACE(thiz);
               return ret;
          }

          DIRECT_INTERFACE_GET_DATA_FROM( buffer, file_data, IDirectFBDataBuffer_File );
               
          data->stream = file_data->stream;

          ret = SH7722_JPEG_Open( file_data->stream, &data->info );
          if (ret) {
               buffer->Release( buffer );
               DIRECT_DEALLOCATE_INTERFACE(thiz);
               return ret;
          }
     }

     thiz->AddRef                = IDirectFBImageProvider_SH7722_JPEG_AddRef;
     thiz->Release               = IDirectFBImageProvider_SH7722_JPEG_Release;
     thiz->RenderTo              = IDirectFBImageProvider_SH7722_JPEG_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_SH7722_JPEG_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_SH7722_JPEG_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_SH7722_JPEG_GetSurfaceDescription;
     thiz->WriteBack             = IDirectFBImageProvider_SH7722_JPEG_WriteBack;

     return DFB_OK;
}

