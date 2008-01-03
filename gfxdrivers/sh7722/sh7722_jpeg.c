#ifdef SH7722_DEBUG_JPEG
#define DIRECT_ENABLE_DEBUG
#endif

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>

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
#include <media/idirectfbimageprovider.h>

#include "sh7722.h"

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

     int                  width;
     int                  height;
     bool                 mode420;

     CoreDFB             *core;

     IDirectFBDataBuffer *buffer;

     DIRenderCallback     render_callback;
     void                *render_callback_context;
} IDirectFBImageProvider_SH7722_JPEG_data;

/**********************************************************************************************************************/

static DFBResult
DecodeHW( IDirectFBImageProvider_SH7722_JPEG_data *data,
          CoreSurface                             *destination,
          const DFBRectangle                      *rect,
          const DFBRegion                         *clip )
{
     DFBResult              ret;
     CoreSurfaceBufferLock  lock;
     unsigned long          phys;
     unsigned int           len;
     int                    i;
     int                    cw, ch;
     bool                   reload = false;
     SH7722DriverData      *drv    = dfb_gfxcard_get_driver_data();
     SH7722DeviceData      *dev    = drv->dev;
     SH7722GfxSharedArea   *shared = drv->gfx_shared;
     SH7722JPEG             jpeg;
     u32                    vtrcr   = 0;
     u32                    vswpout = 0;

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( destination, CoreSurface );
     DFB_RECTANGLE_ASSERT( rect );
     DFB_REGION_ASSERT( clip );

     cw = clip->x2 - clip->x1 + 1;
     ch = clip->y2 - clip->y1 + 1;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p, %p [%dx%d %s] )\n", __FUNCTION__,
                 data, destination, destination->config.size.w, destination->config.size.h,
                 dfb_pixelformat_name(destination->config.format) );

     D_DEBUG_AT( SH7722_JPEG, "  -> %d,%d - %4dx%4d  [clip %d,%d - %4dx%4d]\n",
                 DFB_RECTANGLE_VALS( rect ), DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     /*
      * Kernel based state machine
      *
      * Execution enters the kernel and only returns to user space for
      *  - end of decoding
      *  - error in decoding
      *  - reload requested
      *
      * TODO
      * - finish clipping (maybe not all is possible without tricky code)
      * - modify state machine to be used by Construct(), GetSurfaceDescription() and RenderTo() to avoid redundancy
      * - check return code and length from GetData()
      */

     /* No cropping of top or left edge :( */
     if (clip->x1 > rect->x || clip->y1 > rect->y) {
          D_UNIMPLEMENTED();
          return DFB_UNIMPLEMENTED;
     }

     /* Init VEU transformation control (format conversion). */
     if (!data->mode420)
          vtrcr |= (1 << 14);

     switch (destination->config.format) {
          case DSPF_NV12:
               vswpout = 0x70;
               break;

          case DSPF_NV16:
               vswpout = 0x70;
               vtrcr  |= (1 << 22);
               break;

          case DSPF_RGB16:
               vswpout = 0x60;
               vtrcr  |= (6 << 16) | 2;
               break;

          case DSPF_RGB32:
               vswpout = 0x40;
               vtrcr  |= (19 << 16) | 2;
               break;

          case DSPF_RGB24:
               vswpout = 0x70;
               vtrcr  |= (21 << 16) | 2;
               break;

          default:
               D_BUG( "unexpected format %s", dfb_pixelformat_name(destination->config.format) );
               return DFB_BUG;
     }

     /* Lock destination surface. */
     ret = dfb_surface_lock_buffer( destination, CSBR_BACK, CSAF_GPU_WRITE, &lock );
     if (ret)
          return ret;

     /* Calculate destination base address. */
     phys = lock.phys + rect->x + rect->y * lock.pitch;

     D_DEBUG_AT( SH7722_JPEG, "  -> locking JPU...\n" );

     fusion_skirmish_prevail( &dev->jpeg_lock );

     D_DEBUG_AT( SH7722_JPEG, "  -> loading...\n" );

     /* Fill first reload buffers. */
     ret = data->buffer->GetData( data->buffer, SH7722GFX_JPEG_RELOAD_SIZE, (void*) drv->jpeg_virt, &len );
     if (ret) {
          D_DERROR( ret, "SH7722/JPEG: Could not fill first reload buffer!\n" );
          fusion_skirmish_dismiss( &dev->jpeg_lock );
          dfb_surface_unlock_buffer( destination, &lock );
          return DFB_IO;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> setting...\n" );

     /* Initialize JPEG state. */
     jpeg.state   = SH7722_JPEG_START;
     jpeg.flags   = 0;
     jpeg.buffers = 1;

     /* Enable reload if buffer was filled completely (coded data length >= one reload buffer). */
     if (len == SH7722GFX_JPEG_RELOAD_SIZE) {
          jpeg.flags |= SH7722_JPEG_FLAG_RELOAD;

          reload = true;
     }

     /* Program JPU from RESET. */
     SH7722_SETREG32( drv, JCCMD,    JCCMD_RESET );
     SH7722_SETREG32( drv, JCMOD,    JCMOD_INPUT_CTRL | JCMOD_DSP_DECODE );
     SH7722_SETREG32( drv, JIFCNT,   JIFCNT_VJSEL_JPU );
     SH7722_SETREG32( drv, JIFECNT,  JIFECNT_SWAP_4321 );
     SH7722_SETREG32( drv, JIFDSA1,  dev->jpeg_phys );
     SH7722_SETREG32( drv, JIFDSA2,  dev->jpeg_phys + SH7722GFX_JPEG_RELOAD_SIZE );
     SH7722_SETREG32( drv, JIFDDRSZ, len );

     if (data->width == cw && data->height == ch && rect->w == cw && rect->h == ch &&
         (( data->mode420 && destination->config.format == DSPF_NV12) ||
          (!data->mode420 && destination->config.format == DSPF_NV16)))
     {
          /* Setup JPU for decoding in frame mode (directly to surface). */
          SH7722_SETREG32( drv, JINTE,    JINTS_INS5_ERROR | JINTS_INS6_DONE |
                                          (reload ? JINTS_INS14_RELOAD : 0) );
          SH7722_SETREG32( drv, JIFDCNT,  JIFDCNT_SWAP_4321 | (reload ? JIFDCNT_RELOAD_ENABLE : 0) );

          SH7722_SETREG32( drv, JIFDDYA1, phys );
          SH7722_SETREG32( drv, JIFDDCA1, phys + lock.pitch * destination->config.size.h );
          SH7722_SETREG32( drv, JIFDDMW,  lock.pitch );
     }
     else {
          jpeg.flags |= SH7722_JPEG_FLAG_CONVERT;

          /* Setup JPU for decoding in line buffer mode. */
          SH7722_SETREG32( drv, JINTE,    JINTS_INS5_ERROR | JINTS_INS6_DONE |
                                          JINTS_INS11_LINEBUF0 | JINTS_INS12_LINEBUF1 |
                                          (reload ? JINTS_INS14_RELOAD : 0) );
          SH7722_SETREG32( drv, JIFDCNT,  JIFDCNT_LINEBUF_MODE | (SH7722GFX_JPEG_LINEBUFFER_HEIGHT << 16) |
                                          JIFDCNT_SWAP_4321 | (reload ? JIFDCNT_RELOAD_ENABLE : 0) );

          SH7722_SETREG32( drv, JIFDDYA1, dev->jpeg_lb1 );
          SH7722_SETREG32( drv, JIFDDCA1, dev->jpeg_lb1 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( drv, JIFDDYA2, dev->jpeg_lb2 );
          SH7722_SETREG32( drv, JIFDDCA2, dev->jpeg_lb2 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( drv, JIFDDMW,  SH7722GFX_JPEG_LINEBUFFER_PITCH );

          /* Setup VEU for conversion/scaling (from line buffer to surface). */
          SH7722_SETREG32( drv, VEU_VBSRR, 0x00000100 );
          SH7722_SETREG32( drv, VEU_VESTR, 0x00000000 );
          SH7722_SETREG32( drv, VEU_VESWR, SH7722GFX_JPEG_LINEBUFFER_PITCH );
          SH7722_SETREG32( drv, VEU_VESSR, (data->height << 16) | data->width );
          SH7722_SETREG32( drv, VEU_VBSSR, 16 );
          SH7722_SETREG32( drv, VEU_VEDWR, lock.pitch );
          SH7722_SETREG32( drv, VEU_VDAYR, phys );
          SH7722_SETREG32( drv, VEU_VDACR, phys + lock.pitch * destination->config.size.h );
          SH7722_SETREG32( drv, VEU_VTRCR, vtrcr );

          SH7722_SETREG32( drv, VEU_VRFCR, (((data->height << 12) / rect->h) << 16) |
                                            ((data->width  << 12) / rect->w) );
          SH7722_SETREG32( drv, VEU_VRFSR, (ch << 16) | cw );
          
          SH7722_SETREG32( drv, VEU_VENHR, 0x00000000 );
          SH7722_SETREG32( drv, VEU_VFMCR, 0x00000000 );
          SH7722_SETREG32( drv, VEU_VAPCR, 0x00000000 );
          SH7722_SETREG32( drv, VEU_VSWPR, 0x00000007 | vswpout );
          SH7722_SETREG32( drv, VEU_VEIER, 0x00000101 );
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> starting...\n" );

     /* Clear interrupts in shared flags. */
     shared->jpeg_ints = 0;

     /* State machine. */
     while (true) {
          /* Run the state machine. */
          if (ioctl( drv->gfx_fd, SH7722GFX_IOCTL_RUN_JPEG, &jpeg ) < 0) {
               ret = errno2result( errno );

               D_PERROR( "SH7722/JPEG: SH7722GFX_IOCTL_RUN_JPEG failed!\n" );
               break;
          }

          D_ASSERT( jpeg.state != SH7722_JPEG_START );

          /* Handle end (or error). */
          if (jpeg.state == SH7722_JPEG_END) {
               if (jpeg.error) {
                    D_ERROR( "SH7722/JPEG: ERROR 0x%x!\n", jpeg.error );
                    ret = DFB_IO;
               }

               break;
          }

          /* Check for reload requests. */
          for (i=1; i<=2; i++) {
               if (jpeg.buffers & i) {
                    if (jpeg.flags & SH7722_JPEG_FLAG_RELOAD) {
                         D_ASSERT( reload );

                         ret = data->buffer->GetData( data->buffer, SH7722GFX_JPEG_RELOAD_SIZE,
                                                      (void*) drv->jpeg_virt +
                                                      SH7722GFX_JPEG_RELOAD_SIZE * (i-1), &len );
                         if (ret) {
                              D_DERROR( ret, "SH7722/JPEG: Could not refill %s reload buffer!\n",
                                        i == 1 ? "first" : "second" );
                              jpeg.buffers &= ~i;
                              jpeg.flags   &= ~SH7722_JPEG_FLAG_RELOAD;
                         }
                         else if (len < SH7722GFX_JPEG_RELOAD_SIZE)
                              jpeg.flags &= ~SH7722_JPEG_FLAG_RELOAD;
                    }
                    else
                         jpeg.buffers &= ~i;
               }
          }
     }

     fusion_skirmish_dismiss( &dev->jpeg_lock );

     /* Unlock destination. */
     dfb_surface_unlock_buffer( destination, &lock );

     return ret;
}

static DFBResult
DecodeHeader( IDirectFBImageProvider_SH7722_JPEG_data *data )
{
     DFBResult              ret;
     unsigned int           len;
     SH7722DriverData      *drv    = dfb_gfxcard_get_driver_data();
     SH7722DeviceData      *dev    = drv->dev;
     SH7722GfxSharedArea   *shared = drv->gfx_shared;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p )\n", __FUNCTION__, data );

     D_ASSERT( data != NULL );

     /*
      * Do minimal stuff to decode the image header, serving as a good probe mechanism as well.
      */

     D_DEBUG_AT( SH7722_JPEG, "  -> locking JPU...\n" );

     fusion_skirmish_prevail( &dev->jpeg_lock );

     D_DEBUG_AT( SH7722_JPEG, "  -> loading 4k...\n" );

     /* Prefill reload buffer with 4k. */
     ret = data->buffer->PeekData( data->buffer, 4096, 0, (void*) drv->jpeg_virt, &len );
     if (ret) {
          fusion_skirmish_dismiss( &dev->jpeg_lock );
          D_DEBUG_AT( SH7722_JPEG, "  -> ERROR from PeekData(): %s\n", DirectResultString(ret) );
          return DFB_IO;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> %u bytes loaded, setting...\n", len );

     /* Program JPU from RESET. */
     SH7722_SETREG32( drv, JCCMD,    JCCMD_RESET );
     SH7722_SETREG32( drv, JCMOD,    JCMOD_INPUT_CTRL | JCMOD_DSP_DECODE );
     SH7722_SETREG32( drv, JINTE,    JINTS_INS3_HEADER | JINTS_INS5_ERROR );
     SH7722_SETREG32( drv, JIFCNT,   JIFCNT_VJSEL_JPU );
     SH7722_SETREG32( drv, JIFECNT,  JIFECNT_SWAP_4321 );
     SH7722_SETREG32( drv, JIFDCNT,  JIFDCNT_SWAP_4321 );
     SH7722_SETREG32( drv, JIFDSA1,  dev->jpeg_phys );
     SH7722_SETREG32( drv, JIFDDRSZ, len );

     D_DEBUG_AT( SH7722_JPEG, "  -> starting...\n" );

     /* Clear interrupts in shared flags. */
     shared->jpeg_ints = 0;

     /* Start decoder and begin reading from buffer. */
     SH7722_SETREG32( drv, JCCMD, JCCMD_START );

     /* Stall machine. */
     while (true) {
          /* Check for new interrupts in shared flags... */
          u32 ints = shared->jpeg_ints;
          if (ints) {
               /* ...and clear them (FIXME: race condition in case of multiple IRQs per command!). */
               shared->jpeg_ints &= ~ints;

               D_DEBUG_AT( SH7722_JPEG, "  -> JCSTS 0x%08x, JINTS 0x%08x\n", SH7722_GETREG32( drv, JCSTS ), ints );

               /* Check for errors! */
               if (ints & JINTS_INS5_ERROR) {
                    D_ERROR( "SH7722/JPEG: ERROR 0x%x!\n", SH7722_GETREG32( drv, JCDERR ) );
                    fusion_skirmish_dismiss( &dev->jpeg_lock );
                    return DFB_IO;
               }

               /* Check for header interception... */
               if (ints & JINTS_INS3_HEADER) {
                    /* ...remember image information... */
                    data->width   = SH7722_GETREG32( drv, JIFDDHSZ );
                    data->height  = SH7722_GETREG32( drv, JIFDDVSZ );
                    data->mode420 = (SH7722_GETREG32( drv, JCMOD ) & 2) ? true : false;

                    D_DEBUG_AT( SH7722_JPEG, "  -> %dx%d (4:2:%c)\n",
                                data->width, data->height, data->mode420 ? '0' : '2' );

                    break;
               }
          }
          else {
               D_DEBUG_AT( SH7722_JPEG, "  -> waiting...\n" );

               /* ...otherwise wait for the arrival of new interrupt(s). */
               if (ioctl( drv->gfx_fd, SH7722GFX_IOCTL_WAIT_JPEG ) < 0) {
                    D_PERROR( "SH7722/JPEG: Waiting for IRQ failed! (ints: 0x%x - JINTS 0x%x, JCSTS 0x%x)\n",
                              ints, SH7722_GETREG32( drv, JINTS ), SH7722_GETREG32( drv, JCSTS ) );
                    fusion_skirmish_dismiss( &dev->jpeg_lock );
                    return DFB_FAILURE;
               }
          }
     }

     fusion_skirmish_dismiss( &dev->jpeg_lock );

     if (data->width < 16 || data->width > 2560)
          return DFB_UNSUPPORTED;

     if (data->height < 16 || data->height > 1920)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

/**********************************************************************************************************************/

static void
IDirectFBImageProvider_SH7722_JPEG_Destruct( IDirectFBImageProvider *thiz )
{
     IDirectFBImageProvider_SH7722_JPEG_data *data = thiz->priv;

     data->buffer->Release( data->buffer );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_AddRef( IDirectFBImageProvider *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     data->ref++;

     return DFB_OK;
}

static DFBResult
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
     DFBSurfacePixelFormat  format;
     IDirectFBSurface_data *dst_data;
     CoreSurface           *dst_surface;

     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG);

     DIRECT_INTERFACE_GET_DATA_FROM(destination, dst_data, IDirectFBSurface);

     dst_surface = dst_data->surface;
     if (!dst_surface)
          return DFB_DESTROYED;

     ret = destination->GetPixelFormat( destination, &format );
     if (ret)
          return ret;

     switch (dst_surface->config.format) {
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

     return DecodeHW( data, dst_surface, &rect, &clip );
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
     
     desc->flags       = DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
     desc->height      = data->height;
     desc->width       = data->width;
     desc->pixelformat = data->mode420 ? DSPF_NV12 : DSPF_NV16;

     return DFB_OK;
}

static DFBResult
IDirectFBImageProvider_SH7722_JPEG_GetImageDescription( IDirectFBImageProvider *thiz,
                                                        DFBImageDescription    *desc )
{
     DIRECT_INTERFACE_GET_DATA(IDirectFBImageProvider_SH7722_JPEG)

     if (!desc)
          return DFB_INVARG;

     desc->caps = DICAPS_NONE;

     return DFB_OK;
}

/**********************************************************************************************************************/

static DFBResult
Probe( IDirectFBImageProvider_ProbeContext *ctx )
{
     if (ctx->header[0] == 0xff && ctx->header[1] == 0xd8)
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

     ret = buffer->AddRef( buffer );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return ret;
     }

     ret = DecodeHeader( data );
     if (ret) {
          buffer->Release( buffer );
          DIRECT_DEALLOCATE_INTERFACE(thiz);
          return ret;
     }

     thiz->AddRef                = IDirectFBImageProvider_SH7722_JPEG_AddRef;
     thiz->Release               = IDirectFBImageProvider_SH7722_JPEG_Release;
     thiz->RenderTo              = IDirectFBImageProvider_SH7722_JPEG_RenderTo;
     thiz->SetRenderCallback     = IDirectFBImageProvider_SH7722_JPEG_SetRenderCallback;
     thiz->GetImageDescription   = IDirectFBImageProvider_SH7722_JPEG_GetImageDescription;
     thiz->GetSurfaceDescription = IDirectFBImageProvider_SH7722_JPEG_GetSurfaceDescription;

     return DFB_OK;
}

