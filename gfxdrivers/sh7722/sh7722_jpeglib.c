#ifdef SH7722_DEBUG_JPEG
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>
#include <jpeglib.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include <asm/types.h>

#ifdef STANDALONE
#include "sh7722_jpeglib_standalone.h"
#else
#undef HAVE_STDLIB_H
#include <config.h>

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/stream.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include <directfb.h>
#include <directfb_util.h>
#endif

#include <jpeglib.h>
#include <setjmp.h>

#include <sh772x_gfx.h>

#include "sh7722_jpeglib.h"
#include "sh7722_regs.h"


D_DEBUG_DOMAIN( SH7722_JPEG, "SH7722/JPEG", "SH7722 JPEG Processing Unit" );

/**********************************************************************************************************************/

/*
 * private data struct of SH7722_JPEG
 */
typedef struct {
     int                            ref_count;

     int                            gfx_fd;
     SH772xGfxSharedArea           *gfx_shared;

     unsigned long                  jpeg_phys;
     unsigned long                  jpeg_lb1;
     unsigned long                  jpeg_lb2;

     volatile void                 *jpeg_virt;

     unsigned long                  mmio_phys;
     volatile void                 *mmio_base;
} SH7722_JPEG_data;

/**********************************************************************************************************************/

#if 1
static inline u32
SH7722_GETREG32( SH7722_JPEG_data *data,
                 u32               address )
{
     SH772xRegister reg = { address, 0 };

     if (ioctl( data->gfx_fd, SH772xGFX_IOCTL_GETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_GETREG32( 0x%08x )\n", reg.address );

     return reg.value;
}

static inline void
SH7722_SETREG32( SH7722_JPEG_data *data,
                 u32               address,
                 u32               value )
{
     SH772xRegister reg = { address, value };

     if (ioctl( data->gfx_fd, SH772xGFX_IOCTL_SETREG32, &reg ) < 0)
          D_PERROR( "SH772xGFX_IOCTL_SETREG32( 0x%08x, 0x%08x )\n", reg.address, reg.value );
}
#else
static inline u32
SH7722_GETREG32( SH7722_JPEG_data *data,
                 u32               address )
{
     D_ASSERT( address >= data->mmio_phys );
     D_ASSERT( address < (data->mmio_phys + data->mmio_length) );

     return *(volatile u32*)(data->mmio_base + (address - data->mmio_phys));
}

static inline void
SH7722_SETREG32( SH7722_JPEG_data *data,
                 u32               address,
                 u32               value )
{
     D_ASSERT( address >= data->mmio_phys );
     D_ASSERT( address < (data->mmio_phys + data->mmio_length) );

     *(volatile u32*)(data->mmio_base + (address - data->mmio_phys)) = value;
}
#endif

static inline int
coded_data_amount( SH7722_JPEG_data *data )
{
     return (SH7722_GETREG32(data, JCDTCU) << 16) | (SH7722_GETREG32(data, JCDTCM) << 8) | SH7722_GETREG32(data, JCDTCD);
}

/**********************************************************************************************************************/

static DirectResult
DecodeHW( SH7722_JPEG_data      *data,
          SH7722_JPEG_context   *info,
          const DFBRectangle    *rect,
          const DFBRegion       *clip,
          DFBSurfacePixelFormat  format,
          unsigned long          phys,
          int                    pitch,
          unsigned int           width,
          unsigned int           height )
{
     DirectResult           ret;
     unsigned int           len;
     int                    i;
     int                    cw, ch;
     bool                   reload = false;
     SH772xGfxSharedArea   *shared = data->gfx_shared;
     SH7722JPEG             jpeg;
     u32                    vtrcr   = 0;
     u32                    vswpout = 0;
     DirectStream          *stream  = info->stream;

     D_ASSERT( data != NULL );
     DFB_RECTANGLE_ASSERT( rect );
     DFB_REGION_ASSERT( clip );

     cw = clip->x2 - clip->x1 + 1;
     ch = clip->y2 - clip->y1 + 1;

     if (cw < 1 || ch < 1)
          return DR_INVAREA;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p, 0x%08lx|%d [%dx%d] %s )\n", __FUNCTION__,
                 data, phys, pitch, info->width, info->height,
                 dfb_pixelformat_name(format) );

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
          return DR_UNIMPLEMENTED;
     }

     /* Init VEU transformation control (format conversion). */
     if (!info->mode420)
          vtrcr |= (1 << 14);

     switch (format) {
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
               D_BUG( "unexpected format %s", dfb_pixelformat_name(format) );
               return DR_BUG;
     }

     /* Calculate destination base address. */
     phys += DFB_BYTES_PER_LINE(format, rect->x) + rect->y * pitch;
     jpeg.phys = phys;

     D_DEBUG_AT( SH7722_JPEG, "  -> locking JPU...\n" );

     if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_LOCK_JPEG )) {
          ret = errno2result( errno );
          D_PERROR( "SH7722/JPEG: Could not lock JPEG engine!\n" );
          return ret;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> loading...\n" );

     /* Fill first reload buffer. */
     ret = direct_stream_read( stream, SH7722GFX_JPEG_RELOAD_SIZE, (void*) data->jpeg_virt, &len );
     if (ret) {
          ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );
          D_DERROR( ret, "SH7722/JPEG: Could not fill first reload buffer!\n" );
          return DR_IO;
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
     SH7722_SETREG32( data, JCCMD,    JCCMD_RESET );
     SH7722_SETREG32( data, JCMOD,    JCMOD_INPUT_CTRL | JCMOD_DSP_DECODE );
     SH7722_SETREG32( data, JIFCNT,   JIFCNT_VJSEL_JPU );
     SH7722_SETREG32( data, JIFECNT,  JIFECNT_SWAP_4321 );
     SH7722_SETREG32( data, JIFDSA1,  data->jpeg_phys );
     SH7722_SETREG32( data, JIFDSA2,  data->jpeg_phys + SH7722GFX_JPEG_RELOAD_SIZE );
     SH7722_SETREG32( data, JIFDDRSZ, len & 0x00FFFF00 );

     if (info->width == cw && info->height == ch && rect->w == cw && rect->h == ch &&
         (( info->mode420 && format == DSPF_NV12) ||
          (!info->mode420 && format == DSPF_NV16)))
     {
          /* Setup JPU for decoding in frame mode (directly to surface). */
          SH7722_SETREG32( data, JINTE,    JINTS_INS5_ERROR | JINTS_INS6_DONE |
                                          (reload ? JINTS_INS14_RELOAD : 0) );
          SH7722_SETREG32( data, JIFDCNT,  JIFDCNT_SWAP_4321 | (reload ? JIFDCNT_RELOAD_ENABLE : 0) );

          SH7722_SETREG32( data, JIFDDYA1, phys );
          SH7722_SETREG32( data, JIFDDCA1, phys + pitch * height );
          SH7722_SETREG32( data, JIFDDMW,  pitch );
     }
     else {
          jpeg.flags |= SH7722_JPEG_FLAG_CONVERT;

          /* Setup JPU for decoding in line buffer mode. */
          SH7722_SETREG32( data, JINTE,    JINTS_INS5_ERROR | JINTS_INS6_DONE |
                                          JINTS_INS11_LINEBUF0 | JINTS_INS12_LINEBUF1 |
                                          (reload ? JINTS_INS14_RELOAD : 0) );
          SH7722_SETREG32( data, JIFDCNT,  JIFDCNT_LINEBUF_MODE | (SH7722GFX_JPEG_LINEBUFFER_HEIGHT << 16) |
                                          JIFDCNT_SWAP_4321 | (reload ? JIFDCNT_RELOAD_ENABLE : 0) );

          SH7722_SETREG32( data, JIFDDYA1, data->jpeg_lb1 );
          SH7722_SETREG32( data, JIFDDCA1, data->jpeg_lb1 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( data, JIFDDYA2, data->jpeg_lb2 );
          SH7722_SETREG32( data, JIFDDCA2, data->jpeg_lb2 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( data, JIFDDMW,  SH7722GFX_JPEG_LINEBUFFER_PITCH );

          /* Setup VEU for conversion/scaling (from line buffer to surface). */
          SH7722_SETREG32( data, VEU_VBSRR, 0x00000100 );
          SH7722_SETREG32( data, VEU_VESTR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VESWR, SH7722GFX_JPEG_LINEBUFFER_PITCH );
          SH7722_SETREG32( data, VEU_VESSR, (info->height << 16) | info->width );
          SH7722_SETREG32( data, VEU_VBSSR, 16 );
          SH7722_SETREG32( data, VEU_VEDWR, pitch );
          SH7722_SETREG32( data, VEU_VDAYR, phys );
          SH7722_SETREG32( data, VEU_VDACR, phys + pitch * height );
          SH7722_SETREG32( data, VEU_VTRCR, vtrcr );

          SH7722_SETREG32( data, VEU_VRFCR, (((info->height << 12) / rect->h) << 16) |
                                            ((info->width  << 12) / rect->w) );
          SH7722_SETREG32( data, VEU_VRFSR, (ch << 16) | cw );
          
          SH7722_SETREG32( data, VEU_VENHR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VFMCR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VAPCR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VSWPR, 0x00000007 | vswpout );
          SH7722_SETREG32( data, VEU_VEIER, 0x00000101 );
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> starting...\n" );

     /* Clear interrupts in shared flags. */
     shared->jpeg_ints = 0;

     /* State machine. */
     while (true) {
          /* Run the state machine. */
          if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_RUN_JPEG, &jpeg ) < 0) {
               ret = errno2result( errno );

               D_PERROR( "SH7722/JPEG: SH7722GFX_IOCTL_RUN_JPEG failed!\n" );
               break;
          }

          D_ASSERT( jpeg.state != SH7722_JPEG_START );

          /* Handle end (or error). */
          if (jpeg.state == SH7722_JPEG_END) {
               if (jpeg.error) {
                    D_ERROR( "SH7722/JPEG: ERROR 0x%x!\n", jpeg.error );
                    ret = DR_IO;
               }

               break;
          }

          /* Check for reload requests. */
          for (i=1; i<=2; i++) {
               if (jpeg.buffers & i) {
                    if (jpeg.flags & SH7722_JPEG_FLAG_RELOAD) {
                         D_ASSERT( reload );

                         ret = direct_stream_read( stream, SH7722GFX_JPEG_RELOAD_SIZE,
                                                   (void*) data->jpeg_virt +
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

     ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );

     return ret;
}

static int calculate_scaling( int input, int output )
{
     int frac = 0;
     int mant = 0;
     
     if( input == output ) { /* no scaling, done */
          return 0;
     }

     mant = input / output;     
     frac = ((input * 4096 / output) & ~7) - mant * 4096;
     
     if( input < output ) { /* upscaling */
          if( input*8 < output ) /* out-of-range */
               return -1;

          while( output > 1 + (int)((input-1)*4096/frac) ) {
               frac -= 8;
          }
     }
     else { /* downscaling */
          int a,size,pmant;
          
          if( output*16 < input ) /* out-of-range */
               return -1;

          while(1) {
               pmant = "1122333344444444"[mant] - '0';
               a     = mant * 4096 + frac;
               size  = (2*(input-1)*pmant)/(2*pmant);
               size  = (((size-1) * 4096 * pmant) + a) / a;
               
               if( output <= size )
                    break;
                    
               if( frac )
                    frac -= 8;
               else {
                    mant--;
                    frac = 0xff8;
               }
          }
     }
     
     return (mant << 12) + frac;
}

static DirectResult
EncodeHW( SH7722_JPEG_data      *data,
          const char            *filename,
          const DFBRectangle    *rect,
          DFBSurfacePixelFormat  format,
          unsigned long          phys,
          int                    pitch,
          unsigned int           width,
          unsigned int           height )
{
     DirectResult           ret;
     int                    i, fd;
     int                    written = 0;
     SH772xGfxSharedArea   *shared  = data->gfx_shared;
     u32                    vtrcr   = 0;
     u32                    vswpin  = 0;
     bool                   mode420 = false;
     SH7722JPEG             jpeg;
         
     int                    horizontalscaling = 0;
     int                    verticalscaling   = 0;

     D_ASSERT( data != NULL );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( SH7722_JPEG, "%s( %p, 0x%08lx|%d [%dx%d] %s )\n", __FUNCTION__,
                 data, phys, pitch, width, height,
                 dfb_pixelformat_name(format) );

     D_DEBUG_AT( SH7722_JPEG, "  -> %d,%d - %4dx%4d\n",
                 DFB_RECTANGLE_VALS( rect ) );

     /* JPU input is 16x16 to 2560x1920 */
     if (width < 16 || width > 2560 || height < 16 || height > 1920)
          return DR_INVAREA;

     if (rect->w < 1 || rect->h < 1)
          return DR_INVAREA;

     horizontalscaling = calculate_scaling( rect->w, width  );
     verticalscaling   = calculate_scaling( rect->h * SH7722GFX_JPEG_LINEBUFFER_HEIGHT / height, SH7722GFX_JPEG_LINEBUFFER_HEIGHT );

     /* scaling out-of-range? */
     if( horizontalscaling == -1 || verticalscaling == -1 )
          return DR_INVAREA;

     /*
      * Kernel based state machine
      *
      * Execution enters the kernel and only returns to user space for
      *  - end of encoding
      *  - error in encoding
      *  - buffer loaded
      *
      * TODO
      * - finish clipping (maybe not all is possible without tricky code)
      */

     /* Init VEU transformation control (format conversion). */
     if (format == DSPF_NV12)
          mode420 = true;
     else
          vtrcr |= (1 << 22);

     switch (format) {
          case DSPF_NV12:
               vswpin = 0x07;
               break;

          case DSPF_NV16:
               vswpin  = 0x07;
               vtrcr  |= (1 << 14);
               break;

          case DSPF_RGB16:
               vswpin  = 0x06;
               vtrcr  |= (3 << 8) | 3;
               break;

          case DSPF_RGB32:
               vswpin  = 0x04;
               vtrcr  |= (0 << 8) | 3;
               break;

          case DSPF_RGB24:
               vswpin  = 0x07;
               vtrcr  |= (2 << 8) | 3;
               break;

          default:
               D_BUG( "unexpected format %s", dfb_pixelformat_name(format) );
               return DR_BUG;
     }

     /* Calculate source base address. */
     phys += DFB_BYTES_PER_LINE(format, rect->x) + rect->y * pitch;
     jpeg.phys = phys;

     D_DEBUG_AT( SH7722_JPEG, "  -> locking JPU...\n" );

     if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_LOCK_JPEG )) {
          ret = errno2result( errno );
          D_PERROR( "SH7722/JPEG: Could not lock JPEG engine!\n" );
          return ret;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> opening '%s' for writing...\n", filename );

     fd = open( filename, O_WRONLY | O_CREAT | O_TRUNC, 0644 );
     if (fd < 0) {
          ret = errno2result( errno );
          ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );
          D_PERROR( "SH7722/JPEG: Failed to open '%s' for writing!\n", filename );
          return ret;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> setting...\n" );

     /* Initialize JPEG state. */
     jpeg.state   = SH7722_JPEG_START;
     jpeg.flags   = SH7722_JPEG_FLAG_ENCODE;
     jpeg.buffers = 3;

     /* Always enable reload mode. */
     jpeg.flags |= SH7722_JPEG_FLAG_RELOAD;

     /* Program JPU from RESET. */
     SH7722_SETREG32( data, JCCMD, JCCMD_RESET );
     SH7722_SETREG32( data, JCMOD, JCMOD_INPUT_CTRL | JCMOD_DSP_ENCODE | (mode420 ? 2 : 1) );

     SH7722_SETREG32( data, JCQTN,    0x14 );
     SH7722_SETREG32( data, JCHTN,    0x3C );
     SH7722_SETREG32( data, JCDRIU,   0x02 );
     SH7722_SETREG32( data, JCDRID,   0x00 );
     SH7722_SETREG32( data, JCHSZU,   width >> 8 );
     SH7722_SETREG32( data, JCHSZD,   width & 0xff );
     SH7722_SETREG32( data, JCVSZU,   height >> 8 );
     SH7722_SETREG32( data, JCVSZD,   height & 0xff );
     SH7722_SETREG32( data, JIFCNT,   JIFCNT_VJSEL_JPU );
     SH7722_SETREG32( data, JIFDCNT,  JIFDCNT_SWAP_4321 );
     SH7722_SETREG32( data, JIFEDA1,  data->jpeg_phys );
     SH7722_SETREG32( data, JIFEDA2,  data->jpeg_phys + SH7722GFX_JPEG_RELOAD_SIZE );
     SH7722_SETREG32( data, JIFEDRSZ, SH7722GFX_JPEG_RELOAD_SIZE );
     SH7722_SETREG32( data, JIFESHSZ, width );
     SH7722_SETREG32( data, JIFESVSZ, height );

     if (width == rect->w && height == rect->h && (format == DSPF_NV12 || format == DSPF_NV16))
     {
          /* no scaling, and supported format - so no VEU needed */
          /* Setup JPU for encoding in frame mode (directly from surface). */
          SH7722_SETREG32( data, JINTE,    JINTS_INS10_XFER_DONE | JINTS_INS13_LOADED );
          SH7722_SETREG32( data, JIFECNT,  JIFECNT_SWAP_4321 | JIFECNT_RELOAD_ENABLE | (mode420 ? 1 : 0) );

          SH7722_SETREG32( data, JIFESYA1, phys );
          SH7722_SETREG32( data, JIFESCA1, phys + pitch * height );
          SH7722_SETREG32( data, JIFESMW,  pitch );
     }
     else {
          /* Setup JPU for encoding in line buffer mode. */
          
          jpeg.flags       |= SH7722_JPEG_FLAG_CONVERT;
          jpeg.height       = height;
          jpeg.inputheight  = rect->h;

          SH7722_SETREG32( data, JINTE,    JINTS_INS11_LINEBUF0 | JINTS_INS12_LINEBUF1 |
                                           JINTS_INS10_XFER_DONE | JINTS_INS13_LOADED );
          SH7722_SETREG32( data, JIFECNT,  JIFECNT_LINEBUF_MODE | (SH7722GFX_JPEG_LINEBUFFER_HEIGHT << 16) |
                                           JIFECNT_SWAP_4321 | JIFECNT_RELOAD_ENABLE | (mode420 ? 1 : 0) );

          SH7722_SETREG32( data, JIFESYA1, data->jpeg_lb1 );
          SH7722_SETREG32( data, JIFESCA1, data->jpeg_lb1 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( data, JIFESYA2, data->jpeg_lb2 );
          SH7722_SETREG32( data, JIFESCA2, data->jpeg_lb2 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( data, JIFESMW,  SH7722GFX_JPEG_LINEBUFFER_PITCH );

          /* FIXME: Setup VEU for conversion/scaling (from surface to line buffer). */
          /* we will not use the VEU in burst mode since we cannot program the 
           * destination addresses intermediately. */
          SH7722_SETREG32( data, VEU_VBSRR, 0x00000100 );
          SH7722_SETREG32( data, VEU_VESTR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VESWR, pitch );
          //SH7722_SETREG32( data, VEU_VESSR, (224 << 16) | cw );
          SH7722_SETREG32( data, VEU_VESSR, (rect->h * SH7722GFX_JPEG_LINEBUFFER_HEIGHT / height)<<16 | rect->w );
          //SH7722_SETREG32( data, VEU_VBSSR, 16 );
           
          SH7722_SETREG32( data, VEU_VEDWR, SH7722GFX_JPEG_LINEBUFFER_PITCH );
          SH7722_SETREG32( data, VEU_VDAYR, data->jpeg_lb1 );
          SH7722_SETREG32( data, VEU_VDACR, data->jpeg_lb1 + SH7722GFX_JPEG_LINEBUFFER_SIZE_Y );
          SH7722_SETREG32( data, VEU_VSAYR, phys );
          SH7722_SETREG32( data, VEU_VSACR, phys + pitch * height );
          SH7722_SETREG32( data, VEU_VTRCR, vtrcr );
          SH7722_SETREG32( data, VEU_VRFCR, (verticalscaling << 16) | horizontalscaling );
          /* SH7722GFX_JPEG_LINEBUFFER_HEIGHT should be 16 for this to work */
          SH7722_SETREG32( data, VEU_VRFSR, (SH7722GFX_JPEG_LINEBUFFER_HEIGHT << 16) | width );
          SH7722_SETREG32( data, VEU_VENHR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VFMCR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VAPCR, 0x00000000 );
          SH7722_SETREG32( data, VEU_VSWPR, 0x00000070 | vswpin );
          SH7722_SETREG32( data, VEU_VEIER, 0x00000101 );
     }

     /* Init quantization tables. */
     SH7722_SETREG32( data, JCQTBL0( 0), 0x100B0B0E );
     SH7722_SETREG32( data, JCQTBL0( 1), 0x0C0A100E );
     SH7722_SETREG32( data, JCQTBL0( 2), 0x0D0E1211 );
     SH7722_SETREG32( data, JCQTBL0( 3), 0x10131828 );
     SH7722_SETREG32( data, JCQTBL0( 4), 0x1A181616 );
     SH7722_SETREG32( data, JCQTBL0( 5), 0x18312325 );
     SH7722_SETREG32( data, JCQTBL0( 6), 0x1D283A33 );
     SH7722_SETREG32( data, JCQTBL0( 7), 0x3D3C3933 );
     SH7722_SETREG32( data, JCQTBL0( 8), 0x38374048 );
     SH7722_SETREG32( data, JCQTBL0( 9), 0x5C4E4044 );
     SH7722_SETREG32( data, JCQTBL0(10), 0x57453738 );
     SH7722_SETREG32( data, JCQTBL0(11), 0x506D5157 );
     SH7722_SETREG32( data, JCQTBL0(12), 0x5F626768 );
     SH7722_SETREG32( data, JCQTBL0(13), 0x673E4D71 );
     SH7722_SETREG32( data, JCQTBL0(14), 0x79706478 );
     SH7722_SETREG32( data, JCQTBL0(15), 0x5C656763 );

     SH7722_SETREG32( data, JCQTBL1( 0), 0x11121218 );
     SH7722_SETREG32( data, JCQTBL1( 1), 0x15182F1A );
     SH7722_SETREG32( data, JCQTBL1( 2), 0x1A2F6342 );
     SH7722_SETREG32( data, JCQTBL1( 3), 0x38426363 );
     SH7722_SETREG32( data, JCQTBL1( 4), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1( 5), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1( 6), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1( 7), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1( 8), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1( 9), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(10), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(11), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(12), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(13), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(14), 0x63636363 );
     SH7722_SETREG32( data, JCQTBL1(15), 0x63636363 );

     /* Init huffman tables. */
     SH7722_SETREG32( data, JCHTBD0(0), 0x00010501 );
     SH7722_SETREG32( data, JCHTBD0(1), 0x01010101 );
     SH7722_SETREG32( data, JCHTBD0(2), 0x01000000 );
     SH7722_SETREG32( data, JCHTBD0(3), 0x00000000 );
     SH7722_SETREG32( data, JCHTBD0(4), 0x00010203 );
     SH7722_SETREG32( data, JCHTBD0(5), 0x04050607 );
     SH7722_SETREG32( data, JCHTBD0(6), 0x08090A0B );

     SH7722_SETREG32( data, JCHTBD1(0), 0x00030101 );
     SH7722_SETREG32( data, JCHTBD1(1), 0x01010101 );
     SH7722_SETREG32( data, JCHTBD1(2), 0x01010100 );
     SH7722_SETREG32( data, JCHTBD1(3), 0x00000000 );
     SH7722_SETREG32( data, JCHTBD1(4), 0x00010203 );
     SH7722_SETREG32( data, JCHTBD1(5), 0x04050607 );
     SH7722_SETREG32( data, JCHTBD1(6), 0x08090A0B );

     SH7722_SETREG32( data, JCHTBA0( 0), 0x00020103 );
     SH7722_SETREG32( data, JCHTBA0( 1), 0x03020403 );
     SH7722_SETREG32( data, JCHTBA0( 2), 0x05050404 );
     SH7722_SETREG32( data, JCHTBA0( 3), 0x0000017D );
     SH7722_SETREG32( data, JCHTBA0( 4), 0x01020300 );
     SH7722_SETREG32( data, JCHTBA0( 5), 0x04110512 );
     SH7722_SETREG32( data, JCHTBA0( 6), 0x21314106 );
     SH7722_SETREG32( data, JCHTBA0( 7), 0x13516107 );
     SH7722_SETREG32( data, JCHTBA0( 8), 0x22711432 );
     SH7722_SETREG32( data, JCHTBA0( 9), 0x8191A108 );
     SH7722_SETREG32( data, JCHTBA0(10), 0x2342B1C1 );
     SH7722_SETREG32( data, JCHTBA0(11), 0x1552D1F0 );
     SH7722_SETREG32( data, JCHTBA0(12), 0x24336272 );
     SH7722_SETREG32( data, JCHTBA0(13), 0x82090A16 );
     SH7722_SETREG32( data, JCHTBA0(14), 0x1718191A );
     SH7722_SETREG32( data, JCHTBA0(15), 0x25262728 );
     SH7722_SETREG32( data, JCHTBA0(16), 0x292A3435 );
     SH7722_SETREG32( data, JCHTBA0(17), 0x36373839 );
     SH7722_SETREG32( data, JCHTBA0(18), 0x3A434445 );
     SH7722_SETREG32( data, JCHTBA0(19), 0x46474849 );
     SH7722_SETREG32( data, JCHTBA0(20), 0x4A535455 );
     SH7722_SETREG32( data, JCHTBA0(21), 0x56575859 );
     SH7722_SETREG32( data, JCHTBA0(22), 0x5A636465 );
     SH7722_SETREG32( data, JCHTBA0(23), 0x66676869 );
     SH7722_SETREG32( data, JCHTBA0(24), 0x6A737475 );
     SH7722_SETREG32( data, JCHTBA0(25), 0x76777879 );
     SH7722_SETREG32( data, JCHTBA0(26), 0x7A838485 );
     SH7722_SETREG32( data, JCHTBA0(27), 0x86878889 );
     SH7722_SETREG32( data, JCHTBA0(28), 0x8A929394 );
     SH7722_SETREG32( data, JCHTBA0(29), 0x95969798 );
     SH7722_SETREG32( data, JCHTBA0(30), 0x999AA2A3 );
     SH7722_SETREG32( data, JCHTBA0(31), 0xA4A5A6A7 );
     SH7722_SETREG32( data, JCHTBA0(32), 0xA8A9AAB2 );
     SH7722_SETREG32( data, JCHTBA0(33), 0xB3B4B5B6 );
     SH7722_SETREG32( data, JCHTBA0(34), 0xB7B8B9BA );
     SH7722_SETREG32( data, JCHTBA0(35), 0xC2C3C4C5 );
     SH7722_SETREG32( data, JCHTBA0(36), 0xC6C7C8C9 );
     SH7722_SETREG32( data, JCHTBA0(37), 0xCAD2D3D4 );
     SH7722_SETREG32( data, JCHTBA0(38), 0xD5D6D7D8 );
     SH7722_SETREG32( data, JCHTBA0(39), 0xD9DAE1E2 );
     SH7722_SETREG32( data, JCHTBA0(40), 0xE3E4E5E6 );
     SH7722_SETREG32( data, JCHTBA0(41), 0xE7E8E9EA );
     SH7722_SETREG32( data, JCHTBA0(42), 0xF1F2F3F4 );
     SH7722_SETREG32( data, JCHTBA0(43), 0xF5F6F7F8 );
     SH7722_SETREG32( data, JCHTBA0(44), 0xF9FA0000 );

     SH7722_SETREG32( data, JCHTBA1( 0), 0x00020102 );
     SH7722_SETREG32( data, JCHTBA1( 1), 0x04040304 );
     SH7722_SETREG32( data, JCHTBA1( 2), 0x07050404 );
     SH7722_SETREG32( data, JCHTBA1( 3), 0x00010277 );
     SH7722_SETREG32( data, JCHTBA1( 4), 0x00010203 );
     SH7722_SETREG32( data, JCHTBA1( 5), 0x11040521 );
     SH7722_SETREG32( data, JCHTBA1( 6), 0x31061241 );
     SH7722_SETREG32( data, JCHTBA1( 7), 0x51076171 );
     SH7722_SETREG32( data, JCHTBA1( 8), 0x13223281 );
     SH7722_SETREG32( data, JCHTBA1( 9), 0x08144291 );
     SH7722_SETREG32( data, JCHTBA1(10), 0xA1B1C109 );
     SH7722_SETREG32( data, JCHTBA1(11), 0x233352F0 );
     SH7722_SETREG32( data, JCHTBA1(12), 0x156272D1 );
     SH7722_SETREG32( data, JCHTBA1(13), 0x0A162434 );
     SH7722_SETREG32( data, JCHTBA1(14), 0xE125F117 );
     SH7722_SETREG32( data, JCHTBA1(15), 0x18191A26 );
     SH7722_SETREG32( data, JCHTBA1(16), 0x2728292A );
     SH7722_SETREG32( data, JCHTBA1(17), 0x35363738 );
     SH7722_SETREG32( data, JCHTBA1(18), 0x393A4344 );
     SH7722_SETREG32( data, JCHTBA1(19), 0x45464748 );
     SH7722_SETREG32( data, JCHTBA1(20), 0x494A5354 );
     SH7722_SETREG32( data, JCHTBA1(21), 0x55565758 );
     SH7722_SETREG32( data, JCHTBA1(22), 0x595A6364 );
     SH7722_SETREG32( data, JCHTBA1(23), 0x65666768 );
     SH7722_SETREG32( data, JCHTBA1(24), 0x696A7374 );
     SH7722_SETREG32( data, JCHTBA1(25), 0x75767778 );
     SH7722_SETREG32( data, JCHTBA1(26), 0x797A8283 );
     SH7722_SETREG32( data, JCHTBA1(27), 0x84858687 );
     SH7722_SETREG32( data, JCHTBA1(28), 0x88898A92 );
     SH7722_SETREG32( data, JCHTBA1(29), 0x93949596 );
     SH7722_SETREG32( data, JCHTBA1(30), 0x9798999A );
     SH7722_SETREG32( data, JCHTBA1(31), 0xA2A3A4A5 );
     SH7722_SETREG32( data, JCHTBA1(32), 0xA6A7A8A9 );
     SH7722_SETREG32( data, JCHTBA1(33), 0xAAB2B3B4 );
     SH7722_SETREG32( data, JCHTBA1(34), 0xB5B6B7B8 );
     SH7722_SETREG32( data, JCHTBA1(35), 0xB9BAC2C3 );
     SH7722_SETREG32( data, JCHTBA1(36), 0xC4C5C6C7 );
     SH7722_SETREG32( data, JCHTBA1(37), 0xC8C9CAD2 );
     SH7722_SETREG32( data, JCHTBA1(38), 0xD3D4D5D6 );
     SH7722_SETREG32( data, JCHTBA1(39), 0xD7D8D9DA );
     SH7722_SETREG32( data, JCHTBA1(40), 0xE2E3E4E5 );
     SH7722_SETREG32( data, JCHTBA1(41), 0xE6E7E8E9 );
     SH7722_SETREG32( data, JCHTBA1(42), 0xEAF2F3F4 );
     SH7722_SETREG32( data, JCHTBA1(43), 0xF5F6F7F8 );
     SH7722_SETREG32( data, JCHTBA1(44), 0xF9FA0000 );

     /* Clear interrupts in shared flags. */
     shared->jpeg_ints = 0;

     D_DEBUG_AT( SH7722_JPEG, "  -> starting...\n" );

     /* State machine. */
     while (true) {
          /* Run the state machine. */
          if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_RUN_JPEG, &jpeg ) < 0) {
               ret = errno2result( errno );

               D_PERROR( "SH7722/JPEG: SH7722GFX_IOCTL_RUN_JPEG failed!\n" );
               break;
          }

          D_ASSERT( jpeg.state != SH7722_JPEG_START );

          /* Check for loaded buffers. */
          for (i=1; i<=2; i++) {
               if (jpeg.buffers & i) {
                    int amount = coded_data_amount( data ) - written;

                    if (amount > SH7722GFX_JPEG_RELOAD_SIZE)
                         amount = SH7722GFX_JPEG_RELOAD_SIZE;

                    D_INFO( "SH7722/JPEG: Coded data amount: + %5d (buffer %d)\n", amount, i );

                    written += write( fd, (void*) data->jpeg_virt + SH7722GFX_JPEG_RELOAD_SIZE * (i-1), amount );
               }
          }

          /* Handle end (or error). */
          if (jpeg.state == SH7722_JPEG_END) {
               if (jpeg.error) {
                    D_ERROR( "SH7722/JPEG: ERROR 0x%x!\n", jpeg.error );
                    ret = DR_IO;
               }

               break;
          }
     }

     D_INFO( "SH7722/JPEG: Coded data amount: = %5d (written: %d, buffers: %d)\n",
             coded_data_amount( data ), written, jpeg.buffers );

     ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );

     close( fd );

     return DR_OK;
}

#if 0
static DirectResult
DecodeHeader( SH7722_JPEG_data    *data,
              DirectStream        *stream,
              SH7722_JPEG_context *info )
{
     DirectResult         ret;
     unsigned int         len;
     SH772xGfxSharedArea *shared;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p )\n", __FUNCTION__, data );

     D_ASSERT( data != NULL );

     shared = data->gfx_shared;

     /*
      * Do minimal stuff to decode the image header, serving as a good probe mechanism as well.
      */

     D_DEBUG_AT( SH7722_JPEG, "  -> locking JPU...\n" );

     if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_LOCK_JPEG )) {
          ret = errno2result( errno );
          D_PERROR( "SH7722/JPEG: Could not lock JPEG engine!\n" );
          return ret;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> loading 32k...\n" );

     /* Prefill reload buffer with 32k. */
     ret = direct_stream_peek( stream, 32*1024, 0, (void*) data->jpeg_virt, &len );
     if (ret) {
          ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );
          D_DEBUG_AT( SH7722_JPEG, "  -> ERROR from PeekData(): %s\n", DirectResultString(ret) );
          return DR_IO;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> %u bytes loaded, setting...\n", len );

     /* Program JPU from RESET. */
     SH7722_SETREG32( data, JCCMD,    JCCMD_RESET );
     SH7722_SETREG32( data, JCMOD,    JCMOD_INPUT_CTRL | JCMOD_DSP_DECODE );
     SH7722_SETREG32( data, JINTE,    JINTS_INS3_HEADER | JINTS_INS5_ERROR );
     SH7722_SETREG32( data, JIFCNT,   JIFCNT_VJSEL_JPU );
     SH7722_SETREG32( data, JIFECNT,  JIFECNT_SWAP_4321 );
     SH7722_SETREG32( data, JIFDCNT,  JIFDCNT_SWAP_4321 );
     SH7722_SETREG32( data, JIFDSA1,  data->jpeg_phys );
     SH7722_SETREG32( data, JIFDDRSZ, len );

     D_DEBUG_AT( SH7722_JPEG, "  -> starting...\n" );

     /* Clear interrupts in shared flags. */
     shared->jpeg_ints = 0;

     /* Start decoder and begin reading from buffer. */
     SH7722_SETREG32( data, JCCMD, JCCMD_START );

     /* Stall machine. */
     while (true) {
          /* Check for new interrupts in shared flags... */
          u32 ints = shared->jpeg_ints;
          if (ints) {
               /* ...and clear them (FIXME: race condition in case of multiple IRQs per command!). */
               shared->jpeg_ints &= ~ints;

               D_DEBUG_AT( SH7722_JPEG, "  -> JCSTS 0x%08x, JINTS 0x%08x\n", SH7722_GETREG32( data, JCSTS ), ints );

               /* Check for errors! */
               if (ints & JINTS_INS5_ERROR) {
                    D_ERROR( "SH7722/JPEG: ERROR 0x%x!\n", SH7722_GETREG32( data, JCDERR ) );
                    ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );
                    return DR_IO;
               }

               /* Check for header interception... */
               if (ints & JINTS_INS3_HEADER) {
                    /* ...remember image information... */
                    info->width   = SH7722_GETREG32( data, JIFDDHSZ );
                    info->height  = SH7722_GETREG32( data, JIFDDVSZ );
                    info->mode420 = (SH7722_GETREG32( data, JCMOD ) & 2) ? true : false;

                    D_DEBUG_AT( SH7722_JPEG, "  -> %dx%d (4:2:%c)\n",
                                info->width, info->height, info->mode420 ? '0' : '2' );

                    break;
               }
          }
          else {
               D_DEBUG_AT( SH7722_JPEG, "  -> waiting...\n" );

               /* ...otherwise wait for the arrival of new interrupt(s). */
               if (ioctl( data->gfx_fd, SH7722GFX_IOCTL_WAIT_JPEG ) < 0) {
                    D_PERROR( "SH7722/JPEG: Waiting for IRQ failed! (ints: 0x%x - JINTS 0x%x, JCSTS 0x%x)\n",
                              ints, SH7722_GETREG32( data, JINTS ), SH7722_GETREG32( data, JCSTS ) );
                    ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );
                    return DR_FAILURE;
               }
          }
     }

     ioctl( data->gfx_fd, SH7722GFX_IOCTL_UNLOCK_JPEG );

     if (info->width < 16 || info->width > 2560)
          return DR_UNSUPPORTED;

     if (info->height < 16 || info->height > 1920)
          return DR_UNSUPPORTED;

     return DR_OK;
}
#endif

/**********************************************************************************************************************/

static void write_rgb_span( u8 *src, void *dst, int len, DFBSurfacePixelFormat format )
{
     int i;

     switch (format) {
          case DSPF_RGB332:
               for (i = 0; i < len; i++)
                    ((u8*)dst)[i] = PIXEL_RGB332( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_ARGB1555:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_ARGB1555( 0xff, src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_ARGB2554:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_ARGB2554( 0xff, src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_ARGB4444:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_ARGB4444( 0xff, src[i*3+0], src[i*3+1], src[i*3+2] );
               break;
               
          case DSPF_RGB16:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_RGB16( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_RGB24:
               direct_memcpy( dst, src, len*3 );
               break;

          case DSPF_RGB32:
               for (i = 0; i < len; i++)
                    ((u32*)dst)[i] = PIXEL_RGB32( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_ARGB:
               for (i = 0; i < len; i++)
                    ((u32*)dst)[i] = PIXEL_ARGB( 0xff, src[i*3+0], src[i*3+1], src[i*3+2] );
               break;
               
          case DSPF_AiRGB:
               for (i = 0; i < len; i++)
                    ((u32*)dst)[i] = PIXEL_AiRGB( 0xff, src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_RGB555:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_RGB555( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_BGR555:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_BGR555( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          case DSPF_RGB444:
               for (i = 0; i < len; i++)
                    ((u16*)dst)[i] = PIXEL_RGB444( src[i*3+0], src[i*3+1], src[i*3+2] );
               break;

          default:
               D_ONCE( "unimplemented destination format (0x%08x)", format );
               break;
     }
}

static inline void
copy_line_nv16( u16 *yy, u16 *cbcr, const u8 *src_ycbcr, int width )
{
     int x;

     D_ASSUME( !(width & 1) );

     for (x=0; x<width/2; x++) {
#ifdef WORDS_BIGENDIAN
          yy[x] = (src_ycbcr[0] << 8) | src_ycbcr[3];
#else
          yy[x] = (src_ycbcr[3] << 8) | src_ycbcr[0];
#endif

          cbcr[x] = (((src_ycbcr[2] + src_ycbcr[5]) << 7) & 0xff00) |
                     ((src_ycbcr[1] + src_ycbcr[4]) >> 1);

          src_ycbcr += 6;
     }
}

static inline void
copy_line_y( u16 *yy, const u8 *src_ycbcr, int width )
{
     int x;

     D_ASSUME( !(width & 1) );

     for (x=0; x<width/2; x++) {
#ifdef WORDS_BIGENDIAN
          yy[x] = (src_ycbcr[0] << 8) | src_ycbcr[3];
#else
          yy[x] = (src_ycbcr[3] << 8) | src_ycbcr[0];
#endif

          src_ycbcr += 6;
     }
}

static DirectResult
DecodeSW( SH7722_JPEG_context   *info,
          const DFBRectangle    *rect,
          const DFBRegion       *clip,
          DFBSurfacePixelFormat  format,
          void                  *addr,
          int                    pitch,
          unsigned int           width,
          unsigned int           height )
{
     int cw, ch;
     JSAMPARRAY buffer;      /* Output row buffer */
     int row_stride;         /* physical row width in output buffer */
     void *addr_uv = addr + height * pitch;

     D_ASSERT( info != NULL );
     DFB_RECTANGLE_ASSERT( rect );
     DFB_REGION_ASSERT( clip );

     cw = clip->x2 - clip->x1 + 1;
     ch = clip->y2 - clip->y1 + 1;

     if (cw < 1 || ch < 1)
          return DR_INVAREA;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p, %p|%d [%dx%d] %s )\n", __FUNCTION__,
                 info, addr, pitch, info->width, info->height,
                 dfb_pixelformat_name(format) );

     D_DEBUG_AT( SH7722_JPEG, "  -> %d,%d - %4dx%4d  [clip %d,%d - %4dx%4d]\n",
                 DFB_RECTANGLE_VALS( rect ), DFB_RECTANGLE_VALS_FROM_REGION( clip ) );

     /* No cropping or clipping yet :( */
     if (clip->x1 != 0 || clip->y1 != 0 ||
         clip->x2 != rect->w - 1 || clip->y2 != rect->h - 1 || rect->w != width || rect->h != height)
     {
          D_UNIMPLEMENTED();
          return DR_UNIMPLEMENTED;
     }

     info->cinfo.output_components = 3;

     /* Calculate destination base address. */
     addr += DFB_BYTES_PER_LINE( format, rect->x ) + rect->y * pitch;

     /* Not all formats yet :( */
     switch (format) {
          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_RGB16:
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGB444:
               info->cinfo.out_color_space = JCS_RGB;
               break;

          case DSPF_NV12:
               if (rect->x & 1)
                    return DFB_INVARG;

               if (rect->y & 1)
                    return DFB_INVARG;

               addr_uv += rect->x + rect->y / 2 * pitch;

               info->cinfo.out_color_space = JCS_YCbCr;
               break;
                    
          case DSPF_NV16:
               if (rect->x & 1)
                    return DFB_INVARG;

               addr_uv += rect->x + rect->y * pitch;

               info->cinfo.out_color_space = JCS_YCbCr;
               break;

          default:
               D_UNIMPLEMENTED();
               return DR_UNIMPLEMENTED;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> decoding...\n" );

     jpeg_start_decompress( &info->cinfo );

     row_stride = ((info->cinfo.output_width + 1) & ~1) * 3;

     buffer = (*info->cinfo.mem->alloc_sarray)((j_common_ptr) &info->cinfo, JPOOL_IMAGE, row_stride, 1);

     while (info->cinfo.output_scanline < info->cinfo.output_height) {
          jpeg_read_scanlines( &info->cinfo, buffer, 1 );

          switch (format) {
               case DSPF_NV12:
                    if (info->cinfo.output_scanline & 1) {
                         copy_line_nv16( addr, addr_uv, *buffer, (rect->w + 1) & ~1 );
                         addr_uv += pitch;
                    }
                    else
                         copy_line_y( addr, *buffer, (rect->w + 1) & ~1 );
                    break;

               case DSPF_NV16:
                    copy_line_nv16( addr, addr_uv, *buffer, (rect->w + 1) & ~1 );
                    addr_uv += pitch;
                    break;

               default:
                    write_rgb_span( *buffer, addr, rect->w, format );
                    break;
          }

          addr += pitch;
     }

     jpeg_finish_decompress( &info->cinfo );

     return DFB_OK;
}

/**********************************************************************************************************************/

static DirectResult
Initialize_GFX( SH7722_JPEG_data *data )
{
     D_DEBUG_AT( SH7722_JPEG, "%s( %p )\n", __FUNCTION__, data );

     /* Open the drawing engine device. */
     data->gfx_fd = direct_try_open( "/dev/sh772x_gfx", "/dev/misc/sh772x_gfx", O_RDWR, true );
     if (data->gfx_fd < 0)
          return DR_INIT;

     /* Map its shared data. */
     data->gfx_shared = mmap( NULL, direct_page_align( sizeof(SH772xGfxSharedArea) ),
                              PROT_READ | PROT_WRITE,
                              MAP_SHARED, data->gfx_fd, 0 );
     if (data->gfx_shared == MAP_FAILED) {
          D_PERROR( "SH7722/GFX: Could not map shared area!\n" );
          close( data->gfx_fd );
          return DR_INIT;
     }

     D_DEBUG_AT( SH7722_JPEG, "  -> magic   0x%08x\n",  data->gfx_shared->magic );
     D_DEBUG_AT( SH7722_JPEG, "  -> buffer  0x%08lx\n", data->gfx_shared->buffer_phys );
     D_DEBUG_AT( SH7722_JPEG, "  -> jpeg    0x%08lx\n", data->gfx_shared->jpeg_phys );

     /* Check the magic value. */
     if (data->gfx_shared->magic != SH7722GFX_SHARED_MAGIC) {
          D_ERROR( "SH7722/GFX: Magic value 0x%08x doesn't match 0x%08x!\n",
                   data->gfx_shared->magic, SH7722GFX_SHARED_MAGIC );
          munmap( (void*) data->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );
          close( data->gfx_fd );
          return DR_INIT;
     }

     return DR_OK;
}

static DirectResult
Shutdown_GFX( SH7722_JPEG_data *data )
{
     munmap( (void*) data->gfx_shared, direct_page_align( sizeof(SH772xGfxSharedArea) ) );

     close( data->gfx_fd );

     return DR_OK;
}

/**********************************************************************************************************************/

static DirectResult
Initialize_Mem( SH7722_JPEG_data *data,
                unsigned long     phys )
{
     int fd;

     D_DEBUG_AT( SH7722_JPEG, "%s( %p, 0x%08lx )\n", __FUNCTION__, data, phys );

     fd = open( "/dev/mem", O_RDWR | O_SYNC );
     if (fd < 0) {
          D_PERROR( "SH7722/JPEG: Could not open /dev/mem!\n" );
          return DR_INIT;
     }

     data->jpeg_virt = mmap( NULL, direct_page_align( SH7722GFX_JPEG_SIZE ),
                             PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys );
     if (data->jpeg_virt == MAP_FAILED) {
          D_PERROR( "SH7722/JPEG: Could not map /dev/mem at 0x%08lx (length %lu)!\n",
                    phys, direct_page_align( SH7722GFX_JPEG_SIZE ) );
          close( fd );
          return DR_INIT;
     }

     data->jpeg_phys = phys;
     data->jpeg_lb1  = data->jpeg_phys + SH7722GFX_JPEG_RELOAD_SIZE * 2;
     data->jpeg_lb2  = data->jpeg_lb1  + SH7722GFX_JPEG_LINEBUFFER_SIZE;

     close( fd );

     return DR_OK;
}

static DirectResult
Shutdown_Mem( SH7722_JPEG_data *data )
{
     munmap( (void*) data->jpeg_virt, direct_page_align( SH7722GFX_JPEG_SIZE ) );

     return DR_OK;
}

/**********************************************************************************************************************/

#define JPEG_PROG_BUF_SIZE    0x10000

typedef struct {
     struct jpeg_source_mgr  pub; /* public fields */

     JOCTET                 *data;       /* start of buffer */

     DirectStream           *stream;

     int                     peekonly;
     int                     peekoffset;
} stream_source_mgr;

typedef stream_source_mgr * stream_src_ptr;

static void
stream_init_source (j_decompress_ptr cinfo)
{
     stream_src_ptr src = (stream_src_ptr) cinfo->src;

     direct_stream_seek( src->stream, 0 ); /* ignore return value */
}

static boolean
stream_fill_input_buffer (j_decompress_ptr cinfo)
{
     DFBResult      ret;
     unsigned int   nbytes = 0;
     stream_src_ptr src    = (stream_src_ptr) cinfo->src;

     struct timeval tv;

     tv.tv_sec  = 0;
     tv.tv_usec = 50000;

     direct_stream_wait( src->stream, JPEG_PROG_BUF_SIZE, &tv );

     if (src->peekonly) {
          ret = direct_stream_peek( src->stream, JPEG_PROG_BUF_SIZE, src->peekoffset, src->data, &nbytes );
          if (ret && ret != DFB_EOF)
               D_DERROR( ret, "SH7722/JPEG: direct_stream_peek() failed!\n" );

          src->peekoffset += MAX( nbytes, 0 );
     }
     else {
          ret = direct_stream_read( src->stream, JPEG_PROG_BUF_SIZE, src->data, &nbytes );
          if (ret && ret != DFB_EOF)
               D_DERROR( ret, "SH7722/JPEG: direct_stream_read() failed!\n" );
     }
     
     if (ret || nbytes <= 0) {
          /* Insert a fake EOI marker */
          src->data[0] = (JOCTET) 0xFF;
          src->data[1] = (JOCTET) JPEG_EOI;
          nbytes = 2;
     }

     src->pub.next_input_byte = src->data;
     src->pub.bytes_in_buffer = nbytes;

     return TRUE;
}

static void
stream_skip_input_data (j_decompress_ptr cinfo, long num_bytes)
{
     stream_src_ptr src = (stream_src_ptr) cinfo->src;

     if (num_bytes > 0) {
          while (num_bytes > (long) src->pub.bytes_in_buffer) {
               num_bytes -= (long) src->pub.bytes_in_buffer;
               (void)stream_fill_input_buffer(cinfo);
          }
          src->pub.next_input_byte += (size_t) num_bytes;
          src->pub.bytes_in_buffer -= (size_t) num_bytes;
     }
}

static void
stream_term_source (j_decompress_ptr cinfo)
{
}

static void
jpeg_stream_src (j_decompress_ptr cinfo, DirectStream *stream, int peekonly)
{
     stream_src_ptr src;

     cinfo->src = (struct jpeg_source_mgr *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           sizeof (stream_source_mgr));

     src = (stream_src_ptr) cinfo->src;

     src->data = (JOCTET *)
                  cinfo->mem->alloc_small ((j_common_ptr) cinfo, JPOOL_PERMANENT,
                                           JPEG_PROG_BUF_SIZE * sizeof (JOCTET));

     src->stream = stream;
     src->peekonly = peekonly;
     src->peekoffset = 0;

     src->pub.init_source       = stream_init_source;
     src->pub.fill_input_buffer = stream_fill_input_buffer;
     src->pub.skip_input_data   = stream_skip_input_data;
     src->pub.resync_to_restart = jpeg_resync_to_restart; /* use default method */
     src->pub.term_source       = stream_term_source;
     src->pub.bytes_in_buffer   = 0; /* forces fill_input_buffer on first read */
     src->pub.next_input_byte   = NULL; /* until buffer loaded */
}

struct my_error_mgr {
     struct jpeg_error_mgr pub;     /* "public" fields */
     jmp_buf  setjmp_buffer;          /* for return to caller */
};

static void
jpeglib_panic(j_common_ptr cinfo)
{
     struct my_error_mgr *myerr = (struct my_error_mgr*) cinfo->err;
     longjmp(myerr->setjmp_buffer, 1);
}

/**********************************************************************************************************************/

static SH7722_JPEG_data data;

DirectResult
SH7722_JPEG_Initialize()
{
     DirectResult ret;

     if (data.ref_count) {
          data.ref_count++;
          return DR_OK;
     }

     ret = Initialize_GFX( &data );
     if (ret)
          return ret;

     ret = Initialize_Mem( &data, data.gfx_shared->jpeg_phys );
     if (ret) {
          Shutdown_GFX( &data );
          return ret;
     }

     data.ref_count = 1;

     return DR_OK;
}

DirectResult
SH7722_JPEG_Shutdown()
{
     if (!data.ref_count)
          return DR_DEAD;
          
     if (--data.ref_count)
          return DR_OK;

     Shutdown_Mem( &data );

     Shutdown_GFX( &data );

     return DR_OK;
}

DirectResult
SH7722_JPEG_Open( DirectStream        *stream,
                  SH7722_JPEG_context *context )
{
     struct my_error_mgr jerr;

     if (!data.ref_count)
          return DR_DEAD;

     context->cinfo.err  = jpeg_std_error( &jerr.pub );
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp( jerr.setjmp_buffer )) {
          D_ERROR( "SH7722/JPEG: Error while reading headers!\n" );

          jpeg_destroy_decompress( &context->cinfo );
          return DFB_FAILURE;
     }

     jpeg_create_decompress( &context->cinfo );
     jpeg_stream_src( &context->cinfo, stream, 1 );
     jpeg_read_header( &context->cinfo, TRUE );
     jpeg_calc_output_dimensions( &context->cinfo );

     context->stream = stream;
     context->width  = context->cinfo.output_width;
     context->height = context->cinfo.output_height;

     context->mode420 = context->cinfo.comp_info[1].h_samp_factor == context->cinfo.comp_info[0].h_samp_factor / 2 &&
                        context->cinfo.comp_info[1].v_samp_factor == context->cinfo.comp_info[0].v_samp_factor / 2 &&
                        context->cinfo.comp_info[2].h_samp_factor == context->cinfo.comp_info[0].h_samp_factor / 2 &&
                        context->cinfo.comp_info[2].v_samp_factor == context->cinfo.comp_info[0].v_samp_factor / 2;

     context->mode444 = context->cinfo.comp_info[1].h_samp_factor == context->cinfo.comp_info[0].h_samp_factor &&
                        context->cinfo.comp_info[1].v_samp_factor == context->cinfo.comp_info[0].v_samp_factor &&
                        context->cinfo.comp_info[2].h_samp_factor == context->cinfo.comp_info[0].h_samp_factor &&
                        context->cinfo.comp_info[2].v_samp_factor == context->cinfo.comp_info[0].v_samp_factor;

     return DFB_OK;
}

DirectResult
SH7722_JPEG_Decode( SH7722_JPEG_context   *context,
                    const DFBRectangle    *rect,
                    const DFBRegion       *clip,
                    DFBSurfacePixelFormat  format,
                    unsigned long          phys,
                    void                  *addr,
                    int                    pitch,
                    unsigned int           width,
                    unsigned int           height )
{
     DFBResult           ret = DFB_UNSUPPORTED;
     DFBRectangle        _rect;
     DFBRegion           _clip;
     struct my_error_mgr jerr;
     bool                sw_only = false;

     if (!data.ref_count)
          return DR_DEAD;

     context->cinfo.err  = jpeg_std_error( &jerr.pub );
     jerr.pub.error_exit = jpeglib_panic;

     if (setjmp( jerr.setjmp_buffer )) {
          D_ERROR( "SH7722/JPEG: Error while decoding image!\n" );
          return DFB_FAILURE;
     }

     switch (format) {
          case DSPF_NV12:
          case DSPF_NV16:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_RGB24:
               break;

          case DSPF_RGB332:
          case DSPF_ARGB1555:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_ARGB:
          case DSPF_AiRGB:
          case DSPF_RGB555:
          case DSPF_BGR555:
          case DSPF_RGB444:
               sw_only = true;
               break;

          default:
               return DR_UNSUPPORTED;
     }

     if (!rect) {
          _rect.x = 0;
          _rect.y = 0;
          _rect.w = width;
          _rect.h = height;

          rect = &_rect;
     }

     if (!clip) {
          _clip.x1 = _rect.x;
          _clip.y1 = _rect.y;
          _clip.x2 = _rect.x + _rect.w - 1;
          _clip.y2 = _rect.y + _rect.h - 1;

          clip = &_clip;
     }

     if (!context->mode444 && !sw_only)
          ret = DecodeHW( &data, context, rect, clip, format, phys, pitch, width, height );

     if (ret) {
          if (addr) {
               ret = DecodeSW( context, rect, clip, format, addr, pitch, width, height );
          }
          else {
               int fd, len = direct_page_align( DFB_PLANE_MULTIPLY( format, height ) * pitch );

               fd = open( "/dev/mem", O_RDWR | O_SYNC );
               if (fd < 0) {
                    D_PERROR( "SH7722/JPEG: Could not open /dev/mem!\n" );
                    return DR_INIT;
               }

               addr = mmap( NULL, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, phys );
               if (addr == MAP_FAILED) {
                    D_PERROR( "SH7722/JPEG: Could not map /dev/mem at 0x%08lx (length %d)!\n", phys, len );
                    close( fd );
                    return DR_INIT;
               }

               ret = DecodeSW( context, rect, clip, format, addr, pitch, width, height );

               munmap( addr, len );
          }
     }

     return ret;
}


DirectResult
SH7722_JPEG_Close( SH7722_JPEG_context *context )
{
     jpeg_destroy_decompress( &context->cinfo );

     return DFB_OK;
}

DirectResult
SH7722_JPEG_Encode( const char            *filename,
                    const DFBRectangle    *rect,
                    DFBSurfacePixelFormat  format,
                    unsigned long          phys,
                    int                    pitch,
                    unsigned int           width,
                    unsigned int           height )
{
     DFBRectangle _rect;

     if (!data.ref_count)
          return DR_DEAD;

     switch (format) {
          case DSPF_NV12:
          case DSPF_NV16:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_RGB24:
               break;

          default:
               return DR_UNSUPPORTED;
     }

     if (!rect) {
          _rect.x = 0;
          _rect.y = 0;
          _rect.w = width;
          _rect.h = height;

          rect = &_rect;
     }

     return EncodeHW( &data, filename, rect, format, phys, pitch, width, height );
}

