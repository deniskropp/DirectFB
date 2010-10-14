#ifdef SH7722_DEBUG_JPEG
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>

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
#include <shjpeg/shjpeg.h>

D_DEBUG_DOMAIN( SH7722_JPEG, "SH7722/JPEG", "SH7722 JPEG Processing Unit" );


/* callbacks for libshjpeg */
int sops_init_databuffer( void *private )
{
     IDirectFBDataBuffer *buffer = (IDirectFBDataBuffer*)private;
     buffer->SeekTo(buffer, 0);

     return 0;
}

int sops_read_databuffer( void *private, size_t *nbytes, void *dataptr )
{
     IDirectFBDataBuffer *buffer = (IDirectFBDataBuffer*)private;

     int n;
 
     buffer->GetData(buffer, *nbytes, dataptr, &n);
     if (n < 0) {
          *nbytes = 0;
          return -1;
     }

     *nbytes = n;
     return 0;
}

int sops_write_databuffer( void *private, size_t *nbytes, void *dataptr )
{
     IDirectFBDataBuffer *buffer = (IDirectFBDataBuffer*)private;

     int n;

     if (buffer->PutData(buffer, dataptr, *nbytes) != DFB_OK);
          return -1;

     return 0;
}

void sops_finalize_databuffer( void *private )
{
     IDirectFBDataBuffer *buffer = (IDirectFBDataBuffer*)private;
     buffer->Finish(buffer);
}

int sops_init_file(void *private)
{
     int fd = *(int*)private;

     lseek(fd, 0L, SEEK_SET);

     return 0;
}

int sops_read_file(void *private, size_t *nbytes, void *dataptr)
{
     int fd = *(int*)private;
     int n;

     n = read(fd, dataptr, *nbytes);
     if (n < 0) {
         *nbytes = 0;
	     return -1;
     }

     *nbytes = n;
     return 0;
}

int sops_write_file(void *private, size_t *nbytes, void *dataptr)
{
     int fd = *(int*)private;
     int n;

     n = write(fd, dataptr, *nbytes);
     if (n < 0) {
         *nbytes = 0;
	     return -1;
     }

     *nbytes = n;
     return 0;
}

void sops_finalize_file(void *private)
{
     int fd = *(int*)private;
     close(fd);
}


shjpeg_sops sops_databuffer = {
    .init     = sops_init_databuffer,
    .read     = sops_read_databuffer,
    .write    = sops_write_databuffer,
    .finalize = sops_finalize_databuffer,
};

shjpeg_sops sops_file = {
    .init     = sops_init_file,
    .read     = sops_read_file,
    .write    = sops_write_file,
    .finalize = sops_finalize_file,
};

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

     shjpeg_context_t    *info;

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

     shjpeg_decode_shutdown( data->info );

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


     /* calculate physical address according to destination rect */
     unsigned long phys = lock.phys + DFB_BYTES_PER_LINE(dst_surface->config.format, rect.x) + rect.y * lock.pitch;

     /* physical address of the c plane */
     unsigned long cphys = 0;

     shjpeg_pixelformat pixelfmt;

     switch (dst_surface->config.format) {
          case DSPF_RGB16:
               pixelfmt = SHJPEG_PF_RGB16;
               break;
          case DSPF_RGB24:
               pixelfmt = SHJPEG_PF_RGB24;
               break;
          case DSPF_RGB32:
               pixelfmt = SHJPEG_PF_RGB32;
               break;
          case DSPF_NV12:
               pixelfmt = SHJPEG_PF_NV12;
               cphys = lock.phys + lock.pitch * dst_surface->config.size.h 
                         + DFB_BYTES_PER_LINE(dst_surface->config.format, rect.x) + (rect.y/2) * lock.pitch;
               break;
          case DSPF_NV16:
               pixelfmt = SHJPEG_PF_NV16;
               cphys = phys + lock.pitch * dst_surface->config.size.h;
               break;
          case DSPF_A8:
               pixelfmt = SHJPEG_PF_GRAYSCALE;
               break;
          default:
               ret = DFB_UNSUPPORTED;
     
     }


     if (shjpeg_decode_run( data->info, pixelfmt, phys, cphys, rect.w, rect.h, lock.pitch) < 0)
          ret = DFB_FAILURE;

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
     desc->height      = data->info->height;
     desc->width       = data->info->width;
     desc->pixelformat = data->info->mode420 ? DSPF_NV12 : DSPF_NV16;

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

     shjpeg_pixelformat pixelfmt;

     switch (src_surface->config.format) {
          case DSPF_NV12:
               pixelfmt = SHJPEG_PF_NV12;
               break;
          case DSPF_NV16:
               pixelfmt = SHJPEG_PF_NV16;
               break;

          default:
               /* FIXME: implement fallback */
               D_UNIMPLEMENTED();
               return DFB_UNIMPLEMENTED;
     }

    /* open file */
    int fd;
    if ((fd = open(filename, O_RDWR | O_CREAT, 0644)) < 0)
         return DFB_IO;

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
               D_DEBUG_AT( SH7722_JPEG, "%s - failed to lock intermediate storage: %d\n",
                    __FUNCTION__, ret );
               dfb_surface_unref( tmp_surface );
               tmp_surface = 0;
               tmp_phys    = 0;
          }
          else {
               tmp_phys = tmp_lock.phys;
               D_DEBUG_AT( SH7722_JPEG, "%s - surface locked at %x\n", __FUNCTION__, tmp_phys );
          }
     }

     ret = dfb_surface_lock_buffer( src_surface, CSBR_FRONT, CSAID_GPU, CSAF_READ, &lock );
     if ( ret == DFB_OK ) {

          /* backup callbacks and private data and setup for file io */
          shjpeg_sops *sops_tmp       = data->info->sops;
          void        *private_tmp = data->info->private;

          data->info->sops = &sops_file;
          data->info->private = (void*) &fd;

          if (shjpeg_encode(data->info, pixelfmt, lock.phys, jpeg_size.w, jpeg_size.h, lock.pitch) < 0)
               ret = DFB_FAILURE;

          /* restore callbacks and private data */
          data->info->sops = sops_tmp;
          data->info->private = private_tmp;
                                              
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

          if ((data->info = shjpeg_init( 1 )) == NULL) {
               buffer->Release( buffer );
               DIRECT_DEALLOCATE_INTERFACE(thiz);

               return DFB_FAILURE;
          }

          /* set callbacks to context */
          data->info->sops = &sops_databuffer;
          data->info->private = (void*) buffer;

          if (shjpeg_decode_init( data->info ) < 0) {
               buffer->Release( buffer );
               DIRECT_DEALLOCATE_INTERFACE(thiz);

               return DFB_FAILURE;
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

