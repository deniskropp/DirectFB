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
#include <pthread.h>

#include <directfb.h>
#include <directfb_version.h>
#include <directfb_strings.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <display/idirectfbsurface.h>

#include <misc/util.h>

#define LOG_MODULE "video_out_dfb"

#include <xine.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>

#include "video_out_dfb.h"


/* TODO: add more matrices */
static const DVProcMatrix proc_matrices[] = {
     { /* ITU Rec. BT.601 (default) */
          .name    = "ITURBT601",
          .y       = {
               .off = +16,
               .min = +16,
               .max = +235
          },
          .uv      = {
               .off = +128,
               .min = +16,
               .max = +240
          },
          .v_for_r = 11485, // 1.40200 * 2^13
          .v_for_g = 5850,  // 0.71414 * 2^13
          .u_for_g = 2819,  // 0.34414 * 2^13
          .u_for_b = 14516  // 1.77200 * 2^13
     },
     { /* ITU Rec. BT.709 */
          .name    = "ITURBT709",
          .y       = {
               .off = +16,
               .min = +16,
               .max = +235
          },
          .uv      = {
               .off = +128,
               .min = +16,
               .max = +240
          },
          .v_for_r = 12862, // 1.57010 * 2^13
          .v_for_g = 3820,  // 0.46640 * 2^13
          .u_for_g = 1531,  // 0.18700 * 2^13
          .u_for_b = 15201  // 1.85560 * 2^13
     },
     { /* European Broadcasting Union */
          .name    = "EBU",
          .y       = {
               .off = +16,
               .min = +16,
               .max = +235
          },
          .uv      = {
               .off = +128,
               .min = +16,
               .max = +240
          },
          .v_for_r = 9338,  // 1.14000 * 2^13
          .v_for_g = 4759,  // 0.58100 * 2^13
          .u_for_g = 3244,  // 0.39600 * 2^13
          .u_for_b = 16621  // 2.02900 * 2^13
     },
     { /* JFIF (this must be the last since it is not a standard for video coding) */
          .name    = "JFIF",
          .y       = {
               .off =  0,
               .min =  0,
               .max = +255,
          },
          .uv      = {
               .off = +128,
               .min =  0,
               .max = +255,
          },
          .v_for_r = 11485, // 1.40200 * 2^13
          .v_for_g = 5850,  // 0.71414 * 2^13
          .u_for_g = 2819,  // 0.34414 * 2^13
          .u_for_b = 14516  // 1.77200 * 2^13
     }
};          

#define N_MATRICES  (sizeof(proc_matrices)/sizeof(DVProcMatrix))


static struct {
#ifdef USE_MMX
     struct {
          int16_t  lm_sub[4]; /* luma           (offset  0) */
          int16_t  lm_add[4]; /* luma           (offset  8) */
          int16_t  uv_sub[4]; /* chroma         (offset 16) */
          int16_t  lm_cfc[4]; /* y coeff.       (offset 24) */
          int16_t  uv_cfc[4]; /* u/v coeff.     (offset 32) */
          int16_t  vr_cfc[4]; /* v red coeff.   (offset 40) */
          int16_t  vg_cfc[4]; /* v green coeff. (offset 48) */
          int16_t  ug_cfc[4]; /* u green coeff. (offset 56) */
          int16_t  ub_cfc[4]; /* u blue coeff.  (offset 64) */
     } mmx;
#endif
     int16_t       lm[256];   /* luma    */
     int16_t       uv[256];   /* chroma  */
     int16_t       vr[256];   /* v red   */
     int16_t       vg[256];   /* v green */
     int16_t       ug[256];   /* u green */
     int16_t       ub[256];   /* u blue  */
     struct {
          void    *buf;
          void    *r;
          void    *g;
          void    *b;
     } clamp;
} __aligned( 8 ) proc_table;



/************************* Begin Conversion methods **************************/

#include "video_out_dfb_proc.h"

#ifdef USE_MMX
# include "video_out_dfb_mmxproc.h"
#endif


static DVProcFunc ProcFuncs[2][DFB_NUM_PIXELFORMATS] = {
     { /* YUY2 */
          DFB_PFUNCTION_NAME( generic, yuy2, rgb16 ),  // ARGB1555
          DFB_PFUNCTION_NAME( generic, yuy2, rgb16 ),  // RGB16
          DFB_PFUNCTION_NAME( generic, yuy2, rgb24 ),  // RGB24
          DFB_PFUNCTION_NAME( generic, yuy2, rgb32 ),  // RGB32
          DFB_PFUNCTION_NAME( generic, yuy2, rgb32 ),  // ARGB
          NULL,                                        // A8
          DFB_PFUNCTION_NAME( generic, yuy2, yuy2 ),   // YUY2
          DFB_PFUNCTION_NAME( generic, yuy2, rgb8 ),   // RGB332
          DFB_PFUNCTION_NAME( generic, yuy2, uyvy ),   // UYVY
          DFB_PFUNCTION_NAME( generic, yuy2, yv12 ),   // I420
          DFB_PFUNCTION_NAME( generic, yuy2, yv12 ),   // YV12
          NULL,                                        // LUT8
          NULL,                                        // ALUT44
          DFB_PFUNCTION_NAME( generic, yuy2, rgb32 ),  // AiRGB
          NULL,                                        // A1
          DFB_PFUNCTION_NAME( generic, yuy2, nv12 ),   // NV12
          DFB_PFUNCTION_NAME( generic, yuy2, nv16 ),   // NV16
          DFB_PFUNCTION_NAME( generic, yuy2, rgb16 ),  // ARGB2554
          DFB_PFUNCTION_NAME( generic, yuy2, rgb16 ),  // ARGB4444
          DFB_PFUNCTION_NAME( generic, yuy2, nv21 )    // NV21
     },
     { /* YV12 */
          DFB_PFUNCTION_NAME( generic, yv12, rgb16 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb16 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb24 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb32 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb32 ),
          NULL,
          DFB_PFUNCTION_NAME( generic, yv12, yuy2 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb8 ),
          DFB_PFUNCTION_NAME( generic, yv12, uyvy ),
          DFB_PFUNCTION_NAME( generic, yv12, yv12 ),
          DFB_PFUNCTION_NAME( generic, yv12, yv12 ),
          NULL,
          NULL,
          DFB_PFUNCTION_NAME( generic, yv12, rgb32 ),
          NULL,
          DFB_PFUNCTION_NAME( generic, yv12, nv12 ),
          DFB_PFUNCTION_NAME( generic, yv12, nv16 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb16 ),
          DFB_PFUNCTION_NAME( generic, yv12, rgb16 ),
          DFB_PFUNCTION_NAME( generic, yv12, nv21 )
     }
};

/************************* End of Conversion methods *************************/


/*************************** Begin Blend methods *****************************/

#include "video_out_dfb_blend.h"


static DVBlendFunc BlendFuncs[DFB_NUM_PIXELFORMATS] =
{
     DFB_BFUNCTION_NAME( generic, argb1555 ),
     DFB_BFUNCTION_NAME( generic, rgb16 ),
     DFB_BFUNCTION_NAME( generic, rgb24 ),
     DFB_BFUNCTION_NAME( generic, rgb32 ),
     DFB_BFUNCTION_NAME( generic, argb ),
     NULL,
     DFB_BFUNCTION_NAME( generic, yuy2 ),
     DFB_BFUNCTION_NAME( generic, rgb332 ),
     DFB_BFUNCTION_NAME( generic, uyvy ),
     DFB_BFUNCTION_NAME( generic, yv12 ),
     DFB_BFUNCTION_NAME( generic, yv12 ),
     NULL,
     NULL,
     DFB_BFUNCTION_NAME( generic, rgb32 ),
     NULL,
     DFB_BFUNCTION_NAME( generic, nv12 ),
     DFB_BFUNCTION_NAME( generic, nv12 ),
     DFB_BFUNCTION_NAME( generic, argb2554 ),
     DFB_BFUNCTION_NAME( generic, argb4444 ),
     DFB_BFUNCTION_NAME( generic, nv21 )
};

/*************************** End of Blend methods ****************************/



static void
vo_dfb_tables_regen( dfb_driver_t          *this,
                     MixerFlags             flags,
                     DFBSurfacePixelFormat  format )
{
     const DVProcMatrix *m      = &proc_matrices[this->config.proc_matrix];
     int                 lm_cfc;
     int                 vr_cfc;
     int                 vg_cfc;
     int                 ug_cfc;
     int                 ub_cfc;
     int                 b      = this->mixer.b;
     int                 c      = this->mixer.c;
     int                 s      = this->mixer.s;
     bool                clamp  = false;
     int                 i;
     
     lprintf( "regenerating tables\n" );
     
     if (flags & MF_B) {
          if (this->mixer.b == 0)
               this->mixer.set &= ~MF_B;
          else
               this->mixer.set |=  MF_B;
     }

     if (flags & MF_C) {
          if (this->mixer.c == 128)
               this->mixer.set &= ~MF_C;
          else
               this->mixer.set |=  MF_C;
     }

     if (flags & MF_S) {
          if (this->mixer.s == 128)
               this->mixer.set &= ~MF_S;
          else
               this->mixer.set |=  MF_S;
     }

     if (format != this->dest_format)
          flags = MF_ALL;

     /* generate coefficients */
     lm_cfc = + 0x00002000 * 255 / (m->y.max  - m->y.min );
     vr_cfc = + m->v_for_r * 255 / (m->uv.max - m->uv.min);
     vg_cfc = - m->v_for_g * 255 / (m->uv.max - m->uv.min);
     ug_cfc = - m->u_for_g * 255 / (m->uv.max - m->uv.min);
     ub_cfc = + m->u_for_b * 255 / (m->uv.max - m->uv.min);

     switch (format) {
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_YV12:
          case DSPF_I420:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
               for (i = 0; i < 256; i++) {
                    int lm, uv;
                    
                    if (flags & (MF_B | MF_C)) {
                         lm = CLAMP( i, m->y.min, m->y.max );
                         lm = (((lm - m->y.off + b) * c) >> 7) + m->y.off;
                         proc_table.lm[i] = CLAMP( lm, 0, 0xff );
                    }
                    if (flags & MF_S) {
                         uv = CLAMP( i, m->uv.min, m->uv.max );
                         uv = (((uv - m->uv.off)    * s) >> 7) + m->uv.off;
                         proc_table.uv[i] = CLAMP( uv, 0, 0xff );
                    }
               }
               break;

          default:
               for (i = 0; i < 256; i++) {
                    int lm, uv, vr, vg, ug, ub;
               
                    if (flags & (MF_B | MF_C)) {
                         lm = CLAMP( i, m->y.min, m->y.max );
                         lm = ((lm - m->y.off + b) * lm_cfc * c) >> 20;
                         proc_table.lm[i] = CLAMP( lm, 0, 0xff );
                    }
                    if (flags & MF_S) {
                         uv = CLAMP( i, m->uv.min, m->uv.max );
                         vr = ((uv - m->uv.off)    * vr_cfc * s) >> 20;
                         vg = ((uv - m->uv.off)    * vg_cfc * s) >> 20;
                         ug = ((uv - m->uv.off)    * ug_cfc * s) >> 20;
                         ub = ((uv - m->uv.off)    * ub_cfc * s) >> 20;
                         proc_table.vr[i] = vr;
                         proc_table.vg[i] = vg;
                         proc_table.ug[i] = ug;
                         proc_table.ub[i] = ub;
                    }
               }
               clamp = (this->dest_format != format);
               break;
     }

#ifdef USE_MMX
     if (this->accel == MM_MMX) {
          for (i = 0; i < 4; i++) { 
               proc_table.mmx.lm_sub[i] = m->y.off - b;
               proc_table.mmx.lm_add[i] = m->y.off;
               proc_table.mmx.uv_sub[i] = m->uv.off;
          }
          
          switch (format) {
               case DSPF_UYVY:
               case DSPF_YUY2:
               case DSPF_YV12:
               case DSPF_I420:
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_NV16:
                    for (i = 0; i < 4; i++) {
                         proc_table.mmx.lm_cfc[i] = (0x2000 * c) >> 7;
                         proc_table.mmx.uv_cfc[i] = (0x2000 * s) >> 7;
                    }
                    break;
               case DSPF_ARGB2554:
               case DSPF_ARGB4444:
                    /* unsupported */
                    break;
               default:
                    for (i = 0; i < 4; i++) {
                         proc_table.mmx.lm_cfc[i] = (lm_cfc * c) >> 7;
                         proc_table.mmx.vr_cfc[i] = (vr_cfc * s) >> 7;
                         proc_table.mmx.vg_cfc[i] = (vg_cfc * s) >> 7;
                         proc_table.mmx.ug_cfc[i] = (ug_cfc * s) >> 7;
                         proc_table.mmx.ub_cfc[i] = (ub_cfc * s) >> 7;
                    }
                    clamp = false;
                    break;
          }
     }
#endif

     if (clamp) {
          int       uv_min, uv_max;
          int       r_min,  r_max;
          int       g_min,  g_max;
          int       b_min,  b_max;
          void     *clamp_r;
          void     *clamp_g;
          void     *clamp_b;
          uint32_t  alpha;
          int       cpp;
         
          lprintf( "regenerating clamp tables\n" );
          
          uv_min = (m->uv.min - m->uv.off) * 255;
          uv_max = (m->uv.max - m->uv.off) * 255;
          
          r_min = 0x00 + ((uv_min * vr_cfc) >> 20);
          r_max = 0xff + ((uv_max * vr_cfc) >> 20);
          g_min = 0x00 + ((uv_max * vg_cfc) >> 20) + ((uv_max * ug_cfc) >> 20);
          g_max = 0xff + ((uv_min * vg_cfc) >> 20) + ((uv_min * ug_cfc) >> 20);
          b_min = 0x00 +                             ((uv_min * ub_cfc) >> 20);
          b_max = 0xff +                             ((uv_max * ub_cfc) >> 20);
          
          lprintf( "colors range after conversion:\n"
                   "\tRed   [min:%4i max:%4i]\n"
                   "\tGreen [min:%4i max:%4i]\n"
                   "\tBlue  [min:%4i max:%4i]\n",
                   r_min, r_max, g_min, g_max, b_min, b_max );
          
          cpp = DFB_BYTES_PER_PIXEL(format);
          
          if (DFB_BYTES_PER_PIXEL(this->dest_format) != cpp) {
               void *clamp_buf;
               int   r_size;
               int   g_size;
               int   b_size;
               int   size;
              
               cpp = (cpp == 3) ? 1 : cpp; 
               
               r_size = abs(r_min) + r_max + 1;
               g_size = abs(g_min) + g_max + 1;
               b_size = abs(b_min) + b_max + 1;
               size   = (r_size + g_size + b_size) * cpp;
               
               if (proc_table.clamp.buf) {
                    free( proc_table.clamp.buf );
                    proc_table.clamp.buf = NULL;
               }

               clamp_buf = xine_xmalloc_aligned( 4, size, &proc_table.clamp.buf );
               if (!clamp_buf) {
                    lprintf( "memory allocation failed!!!\n" );
                    _x_abort();
               }

               proc_table.clamp.r = clamp_buf +                    abs(r_min)  * cpp;
               proc_table.clamp.g = clamp_buf + (         r_size + abs(g_min)) * cpp;
               proc_table.clamp.b = clamp_buf + (g_size + r_size + abs(b_min)) * cpp;
          }
                         
          clamp_r = proc_table.clamp.r;
          clamp_g = proc_table.clamp.g;
          clamp_b = proc_table.clamp.b;

          switch (format) {
               case DSPF_ARGB2554:
                    alpha = 0x0000c000;
                    break;
               case DSPF_ARGB4444:
                    alpha = 0x0000f000;
                    break;
               case DSPF_ARGB1555:
                    alpha = 0x00008000;
                    break;
               case DSPF_ARGB:
                    alpha = 0xff000000;
                    break;
               default:
                    alpha = 0;
                    break;
          }
          
          switch (format) {
               case DSPF_RGB332:
                    for (i = 1; i < 256; i++) {
                         ((uint8_t *)clamp_r)[i] = ((i & 0xe0));
                         ((uint8_t *)clamp_g)[i] = ((i & 0xe0) >> 3);
                         ((uint8_t *)clamp_b)[i] = ((i & 0xc0) >> 6);
                    }
                    break;
               case DSPF_ARGB2554:
                    for (i = 1; i < 256; i++) {
                         ((uint16_t*)clamp_r)[i] = ((i & 0xf8) << 6) | 0xc000;
                         ((uint16_t*)clamp_g)[i] = ((i & 0xf8) << 1);
                         ((uint16_t*)clamp_b)[i] = ((i & 0xf0) >> 4);
                    }
                    break;
               case DSPF_ARGB4444:
                    for (i = 1; i < 256; i++) {
                         ((uint16_t*)clamp_r)[i] = ((i & 0xf0) << 4) | 0xf000;
                         ((uint16_t*)clamp_g)[i] = ((i & 0xf0));
                         ((uint16_t*)clamp_b)[i] = ((i & 0xf0) >> 4);
                    }
                    break;
               case DSPF_ARGB1555:
                    for (i = 1; i < 256; i++) {
                         ((uint16_t*)clamp_r)[i] = ((i & 0xf8) << 7) | 0x8000;
                         ((uint16_t*)clamp_g)[i] = ((i & 0xf8) << 2);
                         ((uint16_t*)clamp_b)[i] = ((i & 0xf8) >> 3);
                    }
                    break;
               case DSPF_RGB16:
                    for (i = 1; i < 256; i++) {
                         ((uint16_t*)clamp_r)[i] = ((i & 0xf8) << 8);
                         ((uint16_t*)clamp_g)[i] = ((i & 0xfc) << 3);
                         ((uint16_t*)clamp_b)[i] = ((i & 0xf8) >> 3);
                    }
                    break;
               case DSPF_RGB24:
                    for (i = 1; i < 256; i++) {
                         ((uint8_t *)clamp_r)[i] = i;
                         ((uint8_t *)clamp_g)[i] = i;
                         ((uint8_t *)clamp_b)[i] = i;
                    }
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    for (i = 1; i < 256; i++) {
                         ((uint32_t*)clamp_r)[i] = (i << 16) | alpha;
                         ((uint32_t*)clamp_g)[i] = (i <<  8);
                         ((uint32_t*)clamp_b)[i] =  i;
                    }
                    break;
               default:
                    break;
          }

          switch (cpp) {
               case 1:
               case 3:
                    memset( clamp_r + r_min, alpha, abs(r_min)+1 );
                    memset( clamp_r + 256, *((uint8_t*)clamp_r+255), r_max-255 );
                    memset( clamp_g + 256, *((uint8_t*)clamp_g+255), g_max-255 );
                    memset( clamp_b + 256, *((uint8_t*)clamp_b+255), b_max-255 );
                    break;
               case 2:
                    for (i = r_min; i <= 0; i++)
                         ((uint16_t*)clamp_r)[i] = alpha;
                    for (i = 256; i <= r_max; i++)
                         ((uint16_t*)clamp_r)[i] = *((uint16_t*)clamp_r+255);
                    for (i = 256; i <= g_max; i++)
                         ((uint16_t*)clamp_g)[i] = *((uint16_t*)clamp_g+255);
                    for (i = 256; i <= b_max; i++)
                         ((uint16_t*)clamp_b)[i] = *((uint16_t*)clamp_b+255);
                    break;
               case 4:
                    for (i = r_min; i <= 0; i++)
                         ((uint32_t*)clamp_r)[i] = alpha;
                    for (i = 256; i <= r_max; i++)
                         ((uint32_t*)clamp_r)[i] = *((uint32_t*)clamp_r+255);
                    for (i = 256; i <= g_max; i++)
                         ((uint32_t*)clamp_g)[i] = *((uint32_t*)clamp_g+255);
                    for (i = 256; i <= b_max; i++)
                         ((uint32_t*)clamp_b)[i] = *((uint32_t*)clamp_b+255);
                    break;
               default:
                    break;
          }
     }
}

static uint32_t
vo_dfb_get_capabilities( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;
     uint32_t      caps = VO_CAP_YV12 | VO_CAP_YUY2;

     if (this->ovl)
          caps |= VO_CAP_UNSCALED_OVERLAY;
    
     lprintf( "capabilities = 0x%08x\n", caps );     
     return caps;
}

static void
vo_dfb_proc_frame( vo_frame_t *vo_frame )
{
     dfb_driver_t *this  = (dfb_driver_t*) vo_frame->driver;
     dfb_frame_t  *frame = (dfb_frame_t*)  vo_frame;

     _x_assert( frame->surface != NULL );
     
     if (!frame->surface)
          return;

     vo_frame->proc_called = 1;

     if (frame->proc_needed) {
          DVProcFunc procf = ProcFuncs[frame->in_format][frame->out_format];
          
          BENCH_BEGIN( frame->area );
          
          procf( frame );
          
          BENCH_END();
     }
}

static void
vo_dfb_frame_field( vo_frame_t *vo_frame,
                    int         which_field )
{
     /* not needed */
}

static void
vo_dfb_frame_dispose( vo_frame_t *vo_frame )
{
     dfb_frame_t *frame = (dfb_frame_t*) vo_frame;

     if (frame) {
          if (frame->surface)
               dfb_surface_unref( frame->surface );
          if (frame->chunk[0])
               free( frame->chunk[0] );
          if (frame->chunk[1])
               free( frame->chunk[1] );
          if (frame->chunk[2])
               free( frame->chunk[2] );
          free( frame );
     }
}

static vo_frame_t*
vo_dfb_alloc_frame( vo_driver_t *vo_driver )
{
     dfb_frame_t *frame;

     frame = (dfb_frame_t*) xine_xmalloc( sizeof(dfb_frame_t) );
     if (!frame) {
          lprintf( "frame allocation failed!!!\n" );
          return NULL;
     }

     pthread_mutex_init( &frame->vo_frame.mutex, NULL );

     frame->vo_frame.proc_slice = NULL;
     frame->vo_frame.proc_frame = vo_dfb_proc_frame;
     frame->vo_frame.field      = vo_dfb_frame_field;
     frame->vo_frame.dispose    = vo_dfb_frame_dispose;
     frame->vo_frame.driver     = vo_driver;

     return (vo_frame_t*) frame;
}

static inline void
vo_dfb_allocate_yuy2( dfb_frame_t           *frame,
                      DFBSurfacePixelFormat  dst_format )
{
     vo_frame_t *vo_frame = &frame->vo_frame;
     
     switch (dst_format) {
          case DSPF_YUY2:
               if (frame->mixer_set == MF_NONE)
                    frame->proc_needed = false;
               /* fall through */
          case DSPF_UYVY:
          case DSPF_ARGB2554:
          case DSPF_ARGB4444:
          case DSPF_ARGB1555:
          case DSPF_RGB16:
               vo_frame->pitches[0] = frame->out_pitch[0];
               vo_frame->base[0]    = frame->out_plane[0];
               break;
          default:
               vo_frame->pitches[0] = frame->width * 2;
               vo_frame->base[0]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[0]*frame->height,
                                             &frame->chunk[0] );
               break;
     }
}

static inline void
vo_dfb_allocate_yv12( dfb_frame_t           *frame,
                      DFBSurfacePixelFormat  dst_format )
{
     vo_frame_t *vo_frame = &frame->vo_frame;
     
     switch (dst_format) {
          case DSPF_YV12:
          case DSPF_I420:
               if (frame->mixer_set == MF_NONE)
                    frame->proc_needed = false;
               vo_frame->pitches[0] = frame->out_pitch[0];
               vo_frame->pitches[1] = frame->out_pitch[1];
               vo_frame->pitches[2] = frame->out_pitch[2];
               vo_frame->base[0]    = frame->out_plane[0];
               vo_frame->base[1]    = frame->out_plane[1];
               vo_frame->base[2]    = frame->out_plane[2];
               break;
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
               vo_frame->pitches[0] = frame->out_pitch[0];
               vo_frame->pitches[1] = frame->width / 2;
               vo_frame->pitches[2] = frame->width / 2;
               vo_frame->base[0]    = frame->out_plane[0];
               vo_frame->base[1]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[1]*(frame->height/2)+2,
                                             &frame->chunk[1] );
               vo_frame->base[2]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[2]*(frame->height/2)+2,
                                             &frame->chunk[2] );
               break;
          default:
               vo_frame->pitches[0] = frame->width;
               vo_frame->pitches[1] = frame->width / 2;
               vo_frame->pitches[2] = frame->width / 2;
               vo_frame->base[0]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[0]*frame->height,
                                             &frame->chunk[0] );
               vo_frame->base[1]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[1]*(frame->height/2)+2,
                                             &frame->chunk[1] );
               vo_frame->base[2]    = xine_xmalloc_aligned( 16,
                                             vo_frame->pitches[2]*(frame->height/2)+2,
                                             &frame->chunk[2] );
               break;
     }
}

static inline void
vo_dfb_notify_reformat( dfb_driver_t          *this,
                        int                    width,
                        int                    height,
                        double                 ratio,
                        DFBSurfacePixelFormat  format )
{
     DFBRectangle           rect;
     int                    src_width   = max( width,  2 );
     int                    src_height  = max( height, 2 );
     double                 src_ratio   = this->output_ratio ? : ratio;
     DFBSurfacePixelFormat  dest_format = DSPF_UNKNOWN;

     if (this->aspect_ratio == XINE_VO_ASPECT_SQUARE) {
          /* update aspect ratio */
          src_ratio = (double) (width  ? : 1) /
                      (double) (height ? : 1);
          this->output_ratio = src_ratio;
     }
     
     this->output_cb( this->output_cdata, src_width, src_height,
                      src_ratio ? : 1, format, &rect );

     /* check if destination format changed */
     this->dest->GetPixelFormat( this->dest, &dest_format );
     vo_dfb_tables_regen( this, MF_NONE, dest_format );
     this->dest_format = dest_format;
}     

static void
vo_dfb_update_frame_format( vo_driver_t *vo_driver,
                            vo_frame_t  *vo_frame,
                            uint32_t     width,
                            uint32_t     height,
                            double       ratio,
                            int          format,
                            int          flags )
{
     dfb_driver_t  *this       = (dfb_driver_t*) vo_driver;
     dfb_frame_t   *frame      = (dfb_frame_t*) vo_frame;
     int            in_format;
     int            out_format;

     _x_assert( this->dest != NULL );
     
     if (!this->dest)
          goto failure;

     width      = (width > 8) ? ((width+3) & ~3) : 8;
     height     = (height > 2) ? ((height+1) & ~1) : 2;
     in_format  = (format == XINE_IMGFMT_YUY2) ? 0 : 1;
     out_format = DFB_PIXELFORMAT_INDEX( this->dest_format );
     
     if (frame->width      != width          ||
         frame->height     != height         ||
         frame->in_format  != in_format      ||
         frame->out_format != out_format     ||
         frame->mixer_set  != this->mixer.set)
     {
          SurfaceBuffer         *buffer;
          DFBSurfacePixelFormat  frame_format;
          DFBResult              err;

          lprintf( "reformatting frame %p\n", frame );
          
          frame_format = (in_format) ? DSPF_YV12 : DSPF_YUY2;
          if (this->frame_format != frame_format    ||
              this->frame_width  != vo_frame->width ||
              this->frame_height != vo_frame->height)
          {
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: frame format changed to %dx%d %s\n",
                        vo_frame->width, vo_frame->height,
                        in_format ? "YV12" : "YUY2" );
               
               vo_dfb_notify_reformat( this, vo_frame->width, vo_frame->height,
                                       vo_frame->ratio, frame_format );
               out_format = DFB_PIXELFORMAT_INDEX( this->dest_format );
               
               this->frame_format = frame_format;
               this->frame_width  = vo_frame->width;
               this->frame_height = vo_frame->height;
          }
          
          if (!frame->surface)
               err = dfb_surface_create( NULL, width, height, this->dest_format,
                                        CSP_SYSTEMONLY, DSCAPS_SYSTEMONLY,
                                        NULL, &frame->surface );
          else
               err = dfb_surface_reformat( NULL, frame->surface,
                                           width, height, this->dest_format );

          if (err != DFB_OK) {
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: "
                        "error creating/reformatting frame surface (%s)\n",
                        DirectFBErrorString( err ) );
               goto failure;
          }
          
          if (frame->chunk[0])
               free( frame->chunk[0] );
          if (frame->chunk[1])
               free( frame->chunk[1] );
          if (frame->chunk[2])
               free( frame->chunk[2] );

          frame->width       = width;
          frame->height      = height;
          frame->area        = width * height;
          frame->in_format   = in_format;
          frame->out_format  = out_format;
          frame->chunk[0]    = NULL;
          frame->chunk[1]    = NULL;
          frame->chunk[2]    = NULL;
          frame->mixer_set   = this->mixer.set;
          frame->proc_needed = true;

          buffer = frame->surface->back_buffer;
          
          frame->out_pitch[0] = buffer->system.pitch;
          frame->out_plane[0] = buffer->system.addr;

          switch (buffer->format) {
               case DSPF_YV12: 
                    frame->out_pitch[1] = frame->out_pitch[0] / 2;
                    frame->out_pitch[2] = frame->out_pitch[0] / 2; 
                    frame->out_plane[2] = frame->out_plane[0] + 
                                          frame->out_pitch[0]*height;
                    frame->out_plane[1] = frame->out_plane[2] + 
                                          frame->out_pitch[2]*height/2;
                    break;
               case DSPF_I420:
                    frame->out_pitch[1] = frame->out_pitch[0] / 2;
                    frame->out_pitch[2] = frame->out_pitch[0] / 2; 
                    frame->out_plane[1] = frame->out_plane[0] +
                                          frame->out_pitch[0]*height;
                    frame->out_plane[2] = frame->out_plane[1] +
                                          frame->out_pitch[1]*height/2;
                    break;
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_NV16:
                    frame->out_pitch[1] = frame->out_pitch[0] & ~1; 
                    frame->out_plane[1] = frame->out_plane[0] +
                                          frame->out_pitch[0]*height;
                    break;
               default:
                    break;
          }
          
          switch (format) {
               case XINE_IMGFMT_YUY2:            
                    vo_dfb_allocate_yuy2( frame, buffer->format );
                    break;
               case XINE_IMGFMT_YV12:
               default:
                    vo_dfb_allocate_yv12( frame, buffer->format );
                    break;
          }
     }
     
     return;

failure:
     if (frame->surface) {
          dfb_surface_unref( frame->surface );
          frame->surface = NULL;
     }
}

#if VIDEO_OUT_DRIVER_IFACE_VERSION >= 21
# define HILI( x ) hili_##x
#else
# define HILI( x ) clip_##x
#endif

static void
vo_dfb_set_palette( dfb_driver_t          *this,
                    vo_overlay_t          *overlay,
                    DFBSurfacePixelFormat  format )
{
     DVColor *palette;
     uint8_t *clut;
     uint8_t *trans;
     int      i;
     
     if (!overlay->rgb_clut) {
          palette = (DVColor*) &overlay->color[0];
          clut    = (uint8_t*) &overlay->color[0];
          trans   = &overlay->trans[0];
                 
          switch (format) { 
               case DSPF_UYVY:
               case DSPF_YUY2:
               case DSPF_YV12:
               case DSPF_I420:
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_NV16:           
               for (i = 0; i < OVL_PALETTE_SIZE; i++, clut += 4) {
                    int y, u, v;

                    y = proc_table.lm[*(clut+2)];
                    u = proc_table.uv[*(clut+0)];
                    v = proc_table.uv[*(clut+1)];
                    
                    palette[i].yuv.a = trans[i] * 255 / 15;
                    palette[i].yuv.y = y;
                    palette[i].yuv.u = u;
                    palette[i].yuv.v = v;
               }
               break;

          default:
               for (i = 0; i < OVL_PALETTE_SIZE; i++, clut += 4) {
                    int y, u, v, r, g, b;

                    y = proc_table.lm[*(clut+2)];
                    u = *(clut+0);
                    v = *(clut+1);
                    r = y + proc_table.vr[v];
                    g = y + proc_table.vg[v] + proc_table.ug[u];
                    b = y                    + proc_table.ub[u];
                    
                    palette[i].rgb.a = trans[i] * 255 / 15;
                    palette[i].rgb.r = CLAMP( r, 0, 0xff );
                    palette[i].rgb.g = CLAMP( g, 0, 0xff );
                    palette[i].rgb.b = CLAMP( b, 0, 0xff );
               }
               break;
          }

          overlay->rgb_clut++;
     }

     if (!overlay->HILI(rgb_clut)) { 
          palette = (DVColor*) &overlay->HILI(color[0]);
          clut    = (uint8_t*) &overlay->HILI(color[0]);
          trans   = &overlay->HILI(trans[0]);
     
          switch (format) { 
               case DSPF_UYVY:
               case DSPF_YUY2:
               case DSPF_YV12:
               case DSPF_I420:
               case DSPF_NV12:
               case DSPF_NV21:
               case DSPF_NV16:           
               for (i = 0; i < OVL_PALETTE_SIZE; i++, clut += 4) {
                    int y, u, v;

                    y = proc_table.lm[*(clut+2)];
                    u = proc_table.uv[*(clut+0)];
                    v = proc_table.uv[*(clut+1)];
                    
                    palette[i].yuv.a = trans[i] * 255 / 15;
                    palette[i].yuv.y = y;
                    palette[i].yuv.u = u;
                    palette[i].yuv.v = v;
               }
               break;

          default:
               for (i = 0; i < OVL_PALETTE_SIZE; i++, clut += 4) {
                    int y, u, v, r, g, b;

                    y = proc_table.lm[*(clut+2)];
                    u = *(clut+0);
                    v = *(clut+1);
                    r = y + proc_table.vr[v];
                    g = y + proc_table.vg[v] + proc_table.ug[u];
                    b = y                    + proc_table.ub[u];
                    
                    palette[i].rgb.a = trans[i] * 255 / 15;
                    palette[i].rgb.r = CLAMP( r, 0, 0xff );
                    palette[i].rgb.g = CLAMP( g, 0, 0xff );
                    palette[i].rgb.b = CLAMP( b, 0, 0xff );
               }
               break;
          }

          overlay->HILI(rgb_clut)++;
     }
}

static void
vo_dfb_overlay_begin( vo_driver_t *vo_driver,
                      vo_frame_t  *vo_frame,
                      int          changed )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     this->ovl_changed += changed;

     if (this->ovl) {
          IDirectFBSurface_data *ovl_data   = this->ovl_data;
          int                    ovl_width  = ovl_data->area.current.w;
          int                    ovl_height = ovl_data->area.current.h;

          if (this->ovl_changed              ||
              this->ovl_width  != ovl_width  ||
              this->ovl_height != ovl_height)
          {
               DFBColor     color = { 0, 0, 0, 0 }; 
               DFBRectangle rect  = ovl_data->area.current;
               DFBRegion    clip;

               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: "
                        "overlay changed, clearing subpicture surface\n" );
          
               dfb_region_from_rectangle( &clip, &rect );
          
               dfb_state_set_destination( &this->state, ovl_data->surface );
               dfb_state_set_color( &this->state, &color );         
               dfb_state_set_clip( &this->state, &clip );
          
               dfb_gfxcard_fillrectangles( &rect, 1, &this->state );

               this->ovl_width  = ovl_width;
               this->ovl_height = ovl_height;
               if (!this->ovl_changed)
                    this->ovl_changed = 1;
          }
     }
}          
 
static void
vo_dfb_overlay_blend( vo_driver_t  *vo_driver,
                      vo_frame_t   *vo_frame,
                      vo_overlay_t *overlay )
{
     dfb_driver_t *this    = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame   = (dfb_frame_t*) vo_frame;
     bool          use_ovl = false;
     int           x       = overlay->x;
     int           y       = overlay->y;
     DFBRegion     clip;
     DFBRegion     subclip;
     int           i;

     _x_assert( frame->surface != NULL );
     _x_assert( overlay->rle != NULL );
     
     if (!frame->surface)
          return;

     if (!overlay->rle)
          return;

     if (this->ovl) {
          use_ovl = (overlay->unscaled || 
                     (vo_frame->width  == this->ovl_data->area.wanted.w &&
                      vo_frame->height == this->ovl_data->area.wanted.h));
          //lprintf( "%s using hardware osd\n", use_ovl ? "" : "not" );
     }

     if (!overlay->rgb_clut || !overlay->HILI(rgb_clut)) {
          lprintf( "regenerating palette for overlay %p\n", overlay );
          vo_dfb_set_palette( this, overlay,
                              (!use_ovl) ? frame->surface->format 
                                         : this->ovl_data->surface->format );
     }

     /* FIXME: apparently bottom-right borders of the overlay get cut away */
     
     BENCH_BEGIN( overlay->width*overlay->height );
     
     subclip.x1 = x + overlay->HILI(left); 
     subclip.y1 = y + overlay->HILI(top);
     subclip.x2 = x + overlay->HILI(right);
     subclip.y2 = y + overlay->HILI(bottom);
     
     if (use_ovl) {
          IDirectFBSurface_data *ovl_data  = this->ovl_data;
          int                    r         = 0;
          DFBRectangle           rects[98];
          
          if (!this->ovl_changed)
               return;

          clip.x1 = ovl_data->area.wanted.x + max( x, 0 );
          clip.y1 = ovl_data->area.wanted.y + max( y, 0 );
          clip.x2 = clip.x1 + overlay->width  - 1;
          clip.y2 = clip.y1 + overlay->height - 1;

          if (!dfb_region_rectangle_intersect( &clip, 
                                               &ovl_data->area.current ))
               return;
          
          dfb_state_set_destination( &this->state, ovl_data->surface );
          dfb_state_set_clip( &this->state, &clip );
          
          for (i = 0; i < overlay->num_rle; i++) {
               int index = overlay->rle[i].color;
               int len   = overlay->rle[i].len;

               while (len > 0) {
                    DVColor *palette = (DVColor*) &overlay->color[0];
                    int      width;

                    if (len > overlay->width) {
                         width  = overlay->width;
                         len   -= overlay->width;
                    } else {
                         width  = len;
                         len    = 0;
                    }
               
                    if (y >= subclip.y1 && y <= subclip.y2 && x <= subclip.x2) {
                         if (x < subclip.x1 && (x + width - 1) >= subclip.x1) {
                              width -= subclip.x1 - x;
                              len   += subclip.x1 - x;
                         } else
                         if (x > subclip.x1) {
                              palette = (DVColor*) &overlay->HILI(color[0]);

                              if ((x + width - 1) > subclip.x2) {
                                   width -= subclip.x2 - x;
                                   len   += subclip.x2 - x;
                              }
                         }
                    }
              
                    if (palette[index].rgb.a) {
                         rects[r].x = x;
                         rects[r].y = y;
                         rects[r].w = width;
                         rects[r].h = 1;
                         r++;

                         dfb_state_set_color( &this->state,
                                              (DFBColor*) &palette[index] );

                         if (this->state.modified & SMF_COLOR   ||
                             r == (sizeof(rects)/sizeof(rects[0])))
                         {
                              dfb_gfxcard_fillrectangles( &rects[0], r,
                                                          &this->state );
                              r = 0;
                         }
                    }
            
                    x += width;
                    if (x >= (overlay->x + overlay->width)) {
                         x = overlay->x;
                         y++;
                    }
               }
          }

          if (r)
               dfb_gfxcard_fillrectangles( &rects[0], r, &this->state );
     }
     else {
          DVBlendFunc blendf  = BlendFuncs[frame->out_format];
          DVBlender   blender;

          if (x >= frame->width || y >= frame->height)
               return;

          clip.x2  = clip.x1 = max( x, 0 );
          clip.y2  = clip.y1 = max( y, 0 );
          clip.x2 += CLAMP( overlay->width, 2, frame->width - clip.x1 ) - 1;
          clip.y2 += CLAMP( overlay->height, 2, frame->height - clip.y1 ) - 1;

          blender.plane[0]  = frame->out_plane[0];
          blender.plane[1]  = frame->out_plane[1];
          blender.plane[2]  = frame->out_plane[2];
          blender.pitch[0]  = frame->out_pitch[0];
          blender.pitch[1]  = frame->out_pitch[1];
          blender.pitch[2]  = frame->out_pitch[2];
          blender.period[0] = 0;
          blender.period[1] = 0;

          blender.plane[0] += clip.y1 * blender.pitch[0];

          switch (frame->surface->format) {
               case DSPF_NV16:
                    blender.plane[1]  += clip.y1  * blender.pitch[1];
                    blender.period[0]  = 0xffffffff;
                    break;
               case DSPF_YV12:
               case DSPF_I420:
                    blender.plane[2]  += clip.y1/2 * blender.pitch[2];
                    blender.period[1]  = 0x00000001;
                    /* fall through */
               case DSPF_NV12:
               case DSPF_NV21:
                    blender.plane[1]  += clip.y1/2 * blender.pitch[1];
                    blender.period[0]  = 0x00000001;
                    break;
               default:
                    break;
          }

          for (i = 0; i < overlay->num_rle; i++) {
               int index = overlay->rle[i].color;
               int len   = overlay->rle[i].len;

               while (len > 0) {
                    DVColor *palette = (DVColor*) &overlay->color[0];
                    int      width;

                    if (len > overlay->width) {
                         width  = overlay->width;
                         len   -= overlay->width;
                    } else {
                         width  = len;
                         len    = 0;
                    }
               
                    if (y >= subclip.y1 && y <= subclip.y2 && x <= subclip.x2) {
                         if (x < subclip.x1 && (x + width - 1) >= subclip.x1) {
                              width -= subclip.x1 - x;
                              len   += subclip.x1 - x;
                         } else
                         if (x > subclip.x1) {
                              palette = (DVColor*) &overlay->HILI(color[0]);

                              if ((x + width - 1) > subclip.x2) {
                                   width -= subclip.x2 - x;
                                   len   += subclip.x2 - x;
                              }
                         }
                    }
              
                    if (palette[index].rgb.a) {
                         if (y >= clip.y1 && x >= clip.x1 && x <= clip.x2) {
                              blender.x   = x;
                              blender.y   = y;
                              blender.len = min( width, clip.x2 - x + 1 );
                              if (blender.len > 0)
                                   blendf( &blender, &palette[index] );
                         }
                    }
            
                    x += width;
                    if (x >= (overlay->x + overlay->width)) {
                         x = overlay->x;
                         if (++y > clip.y2)
                              goto end;
                         
                         if (y > clip.y1) {
                              blender.plane[0] += blender.pitch[0];
                              if (y & blender.period[0])
                                   blender.plane[1] += blender.pitch[1];
                              if (y & blender.period[1])
                                   blender.plane[2] += blender.pitch[2];
                         }
                    }
               }
          }
     }

end:
     BENCH_END();
}

static void
vo_dfb_overlay_end( vo_driver_t *vo_driver,
                    vo_frame_t  *vo_frame )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     if (this->ovl && this->ovl_changed) {
          if (this->ovl_data->caps & DSCAPS_FLIPPING)
               this->ovl->Flip( this->ovl, NULL, DSFLIP_ONSYNC );
          dfb_state_set_destination( &this->state, this->dest_data->surface );
     }

     this->ovl_changed = 0;
}          

static int
vo_dfb_redraw_needed( vo_driver_t *vo_driver )
{
     return 0;
}

static void
vo_dfb_display_frame( vo_driver_t *vo_driver,
                      vo_frame_t  *vo_frame )
{
     dfb_driver_t *this      = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame     = (dfb_frame_t*) vo_frame;
     DFBRectangle  dst_rect  = { 0, 0, 0, 0 };
     DFBRectangle  src_rect  = { 0, 0, };
     double        src_ratio;

     _x_assert( frame->surface != NULL );
     
     if (!frame->surface)
          goto failure;

     src_rect.w = max( vo_frame->width,  2 );
     src_rect.h = max( vo_frame->height, 2 );
     src_ratio  = this->output_ratio ? : vo_frame->ratio;
     
     this->output_cb( this->output_cdata, src_rect.w, src_rect.h,
                      src_ratio ? : 1.0, this->frame_format, &dst_rect );
     
     dst_rect.x += this->dest_data->area.wanted.x;
     dst_rect.y += this->dest_data->area.wanted.y;

     if (dst_rect.w < 1 || dst_rect.h < 1)
          dst_rect = this->dest_data->area.wanted;

     if (!dfb_rectangle_intersect( &dst_rect,
                                   &this->dest_data->area.current ))
          goto failure;
   
     this->state.clip.x1   = dst_rect.x;
     this->state.clip.x2   = dst_rect.x + dst_rect.w - 1;
     this->state.clip.y1   = dst_rect.y;
     this->state.clip.y2   = dst_rect.y + dst_rect.h - 1;
     this->state.source    = frame->surface;
     this->state.modified |= (SMF_CLIP | SMF_SOURCE);

     if (this->deinterlace) {
          frame->surface->field = this->deinterlace - 1;
          dfb_state_set_blitting_flags( &this->state, DSBLIT_DEINTERLACE );
     } else
          dfb_state_set_blitting_flags( &this->state, DSBLIT_NOFX );
     
     if (dst_rect.w == src_rect.w && dst_rect.h == src_rect.h)
          dfb_gfxcard_blit( &src_rect, dst_rect.x, dst_rect.y, &this->state );
     else
          dfb_gfxcard_stretchblit( &src_rect, &dst_rect, &this->state );

     if (this->frame_cb)
          this->frame_cb( this->frame_cdata );

failure:
     vo_frame->free( vo_frame );
}

static int
vo_dfb_get_property( vo_driver_t *vo_driver,
                    int          property )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property) {
          case VO_PROP_INTERLACED:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: interlaced is %i\n",
                        this->deinterlace );
               return this->deinterlace;

          case VO_PROP_ASPECT_RATIO:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: aspect ratio is %i (%.4f)\n",
                        this->aspect_ratio, this->output_ratio );
               return this->aspect_ratio;
                   
          case VO_PROP_BRIGHTNESS:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: brightness is %i\n",
                        this->mixer.b );
               return this->mixer.b;
         
          case VO_PROP_CONTRAST:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: contrast is %i\n",
                        this->mixer.c );
               return this->mixer.c;
         
          case VO_PROP_SATURATION:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: saturation is %i\n", this->mixer.s );
               return this->mixer.s;
         
          case VO_PROP_MAX_NUM_FRAMES:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: maximum number of frames is %i\n",
                        this->config.max_num_frames );
               return this->config.max_num_frames;
         
          case VO_PROP_WINDOW_WIDTH:
               if (this->ovl || this->dest) {
                    IDirectFBSurface_data *data = this->ovl_data ? : this->dest_data;
                    xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                             "video_out_dfb: window width is %i\n",
                             data->area.wanted.w );
                    return data->area.wanted.w;
               }
               break;

          case VO_PROP_WINDOW_HEIGHT:
               if (this->ovl || this->dest) {
                    IDirectFBSurface_data *data = this->ovl_data ? : this->dest_data;
                    xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                             "video_out_dfb: window height is %i\n",
                             data->area.wanted.h );
                    return data->area.wanted.h;
               }
               break;
               
          default:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: tried to get unsupported property %i\n",
                        property );
               break;
     }
     
     return 0;
}

static inline void
vo_dfb_set_output_ratio( dfb_driver_t *this,
                         int           ratio )
{
     double frame_width  = (this->frame_width ? : 1);
     double frame_height = (this->frame_height ? : 1);

     switch (ratio) {
          case XINE_VO_ASPECT_AUTO:
               this->output_ratio = 0.0;
               break;
          case XINE_VO_ASPECT_SQUARE:
               this->output_ratio = frame_width/frame_height;
               break;
          case XINE_VO_ASPECT_4_3:
               this->output_ratio = 4.0/3.0;
               break;
          case XINE_VO_ASPECT_ANAMORPHIC:
               this->output_ratio = 16.0/9.0;
               break;
          case XINE_VO_ASPECT_DVB:
               this->output_ratio = 2.0;
               break;
          default:
               break;
     }
}

static int
vo_dfb_set_property( vo_driver_t *vo_driver,
                     int          property,
                     int          value )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property) {
          case VO_PROP_INTERLACED:
               if (value >= 0 && value <= 2) {
                    xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                             "video_out_dfb: setting deinterlacing to %i\n",
                             value );
                    this->deinterlace = value;
               }
               break;

          case VO_PROP_ASPECT_RATIO:
               if (value >= 0 && value <= 4) {
                    xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                             "video_out_dfb: setting aspect ratio to %i\n",
                             value );
                    vo_dfb_set_output_ratio( this, value );
                    this->aspect_ratio = value;
               }
               break;
          
          case VO_PROP_BRIGHTNESS:
               if (value >= -128 && value <= 127) {
                    if (this->mixer.b != value) {
                         xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                                  "video_out_dfb: setting brightness to %i\n",
                                  value );
                         this->mixer.b = value;
                         vo_dfb_tables_regen( this, MF_B, this->dest_format );
                    }
               }
               break;

          case VO_PROP_CONTRAST:
               if (value >= 0 && value <= 255) {
                    if (this->mixer.c != value ) {
                         xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                                  "video_out_dfb: setting contrast to %i\n",
                                  value );
                         this->mixer.c = value;
                         vo_dfb_tables_regen( this, MF_C, this->dest_format );
                    }
               }
               break;

          case VO_PROP_SATURATION:
               if (value >= 0 && value <= 255) {
                    if (this->mixer.s != value) {
                         xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                                  "video_out_dfb: setting saturation to %i\n",
                                  value );
                         this->mixer.s = value;
                         vo_dfb_tables_regen( this, MF_S, this->dest_format );
                    }
               }
               break;
          
          default:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: tried to set unsupported property %i\n",
                        property );
               break;
     }

     return value;
}

static void
vo_dfb_get_property_min_max( vo_driver_t *vo_driver,
                             int          property,
                             int         *min,
                             int         *max )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property) {
          case VO_PROP_INTERLACED:
               *min =  0;
               *max = +2;
               break;

          case VO_PROP_ASPECT_RATIO:
               *min =  0;
               *max = +4;
               break;
               
          case VO_PROP_BRIGHTNESS:
               *min = -128;
               *max = +127;
               break;

          case VO_PROP_CONTRAST:
               *min = 0;
               *max = 255;
               break;

          case VO_PROP_SATURATION:
               *min = 0;
               *max = 255;
               break;

          default:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: requested min/max for unsupported property %i\n",
                        property );
               *min = 0;
               *max = 0;
               break;
     }
}

static bool
vo_dfb_set_destination( dfb_driver_t     *this,
                        IDirectFBSurface *destination )
{
     DirectFBPixelFormatNames(format_names);
     DFBSurfacePixelFormat format = DSPF_UNKNOWN;
     DFBResult             err;

     if (this->dest) {
          this->dest->Release( this->dest );
          this->dest      = NULL;
          this->dest_data = NULL;
     }
     
     if (!destination->priv) {
          xprintf( this->xine, XINE_VERBOSITY_LOG,
                   "video_out_dfb: got a dead destination surface!!!\n" );
          return false;
     }

     err = destination->GetPixelFormat( destination, &format );
     if (err != DFB_OK) {
          xprintf( this->xine, XINE_VERBOSITY_LOG,
                   "video_out_dfb: "
                   "IDirectFBSurface::GetPixelFormat() returned \"%s\"!!\n",
                   DirectFBErrorString( err ));
          return false;
     }

     if (!ProcFuncs[0][DFB_PIXELFORMAT_INDEX(format)] ||
         !ProcFuncs[1][DFB_PIXELFORMAT_INDEX(format)])
     {
          xprintf( this->xine, XINE_VERBOSITY_LOG,
                   "video_out_dfb: "
                   "unsupported destination surface format (%s)\n",
                    format_names[DFB_PIXELFORMAT_INDEX(format)].name );
          return false;
     }

     xprintf( this->xine, XINE_VERBOSITY_LOG,
              "video_out_dfb: destination surface is %s\n",
              format_names[DFB_PIXELFORMAT_INDEX(format)].name );

     destination->AddRef( destination );
     this->dest      = destination;
     this->dest_data = (IDirectFBSurface_data*) destination->priv;
     dfb_state_set_destination( &this->state, this->dest_data->surface );

     vo_dfb_tables_regen( this, MF_NONE, format );
     this->dest_format = format;

     return true;
}

static bool
vo_dfb_set_subpicture( dfb_driver_t     *this,
                       IDirectFBSurface *subpicture )
{
     DirectFBPixelFormatNames(format_names);   
     DFBSurfacePixelFormat format = DSPF_UNKNOWN;
     DFBResult             err;
     
     if (this->ovl) {
          this->ovl->Release( this->ovl );
          this->ovl      = NULL;
          this->ovl_data = NULL;
     }
     
     if (!subpicture->priv) {
          xprintf( this->xine, XINE_VERBOSITY_LOG,
                   "video_out_dfb: got a dead subpicture surface!!!\n" );
          return false;
     }
               
     err = subpicture->GetPixelFormat( subpicture, &format );
     if (err != DFB_OK) {
          xprintf( this->xine, XINE_VERBOSITY_LOG,
                   "video_out_dfb: "
                   "IDirectFBSurface::GetPixelFormat() returned \"%s\"!!\n",
                   DirectFBErrorString( err ));
          return false;
     }
     
     if (DFB_PIXELFORMAT_IS_INDEXED(format)) {
          xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                   "video_out_dfb: "
                   "indexed formats not yet supported for subpicture!!\n" );
          return false;
     }
     
     xprintf( this->xine, XINE_VERBOSITY_LOG,
              "video_out_dfb: subpicture surface is %s\n",
               format_names[DFB_PIXELFORMAT_INDEX(format)].name );

     subpicture->AddRef( subpicture );
     this->ovl        = subpicture;
     this->ovl_data   = (IDirectFBSurface_data*) subpicture->priv;
     this->ovl_width  = -1;
     this->ovl_height = -1;

     return true;
}

static int
vo_dfb_gui_data_exchange( vo_driver_t *vo_driver,
                          int          data_type,
                          void        *data )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (data_type) {
          /* update destination surface (deprecated) */
          case XINE_GUI_SEND_DRAWABLE_CHANGED:
               if (data) {
                    IDirectFBSurface *surface = (IDirectFBSurface*) data;
                    
                    if (surface != this->dest)
                         return vo_dfb_set_destination( this, surface );
                    return 1;
               }
               break;

          /* update visual */
          case XINE_GUI_SEND_SELECT_VISUAL:
               if (data) {
                    dfb_visual_t *visual = (dfb_visual_t*) data;

                    if (!visual->output_cb || !visual->destination)
                         return 0;

                    this->output_cb    = visual->output_cb;
                    this->output_cdata = visual->output_cdata;
                    this->frame_cb     = visual->frame_cb;
                    this->frame_cdata  = visual->frame_cdata;
                    
                    if (visual->destination != this->dest) {
                         if (!vo_dfb_set_destination( this, visual->destination ))
                              return 0;
                    }
                    
                    if (!visual->subpicture) {
                         if (this->ovl)
                              this->ovl->Release( this->ovl );
                         this->ovl      = NULL;
                         this->ovl_data = NULL;
                    } else
                    if (visual->subpicture != this->ovl) {
                         if (!vo_dfb_set_subpicture( this, visual->subpicture ))
                              return 0;
                    }
                    
                    return 1;
               }
               break;

          /* register/unregister DVFrameCallback (deprecated) */
          case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
               if (data) {
                    this->frame_cb    = ((dfb_framecallback_t*)data)->frame_cb;
                    this->frame_cdata = ((dfb_framecallback_t*)data)->cdata;
               } else {
                    this->frame_cb    = NULL;
                    this->frame_cdata = NULL;
               }
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: %s DVFrameCallback\n",
                        (this->frame_cb) ? "registered new" : "unregistered" );
               return 1;
          
          default:
               xprintf( this->xine, XINE_VERBOSITY_DEBUG,
                        "video_out_dfb: unknown data type %i", data_type );
               break;
     }
     
     return 0;
}

static void
vo_dfb_dispose( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;
     
     if (this) {
          if (proc_table.clamp.buf)
               free( proc_table.clamp.buf );
          if (this->dest)
               this->dest->Release( this->dest ); 
          if (this->ovl)
               this->ovl->Release( this->ovl );
          dfb_state_set_destination( &this->state, NULL );
          this->state.source = NULL; // never set by dfb_state_set_source()
          dfb_state_destroy( &this->state );
          free( this );
     }
}

static void
vo_dfb_init_config( dfb_driver_t *this )
{
     config_values_t   *config = this->xine->config;
     static const char *matrices_enum[N_MATRICES];
     int                i;

     for (i = 0; i < N_MATRICES; i++)
          matrices_enum[i] = proc_matrices[i].name;

     this->config.max_num_frames = config->register_num( config,
                                        "video.dfb.max_num_frames", 15,
                                        "Maximum number of allocated frames (at least 5)",
                                        NULL, 10, NULL, NULL );
     
     this->config.proc_matrix    = config->register_enum( config,
                                        "video.dfb.conversion_matrix", 0,
                                        (char**) &matrices_enum[0],
                                        "Select the conversion matrix",
                                        NULL, 10, NULL, NULL );

#ifdef USE_MMX
     this->config.enable_mmx     = config->register_bool( config, 
                                        "video.dfb.enable_mmx", 1,
                                        "Enable MMX when available",
                                        NULL, 10, NULL,NULL );
#endif
}

static vo_driver_t*
open_plugin( video_driver_class_t *vo_class,
             const void           *vo_visual )
{
     dfb_driver_class_t *class  = (dfb_driver_class_t*) vo_class;
     dfb_visual_t       *visual = (dfb_visual_t*) vo_visual;
     dfb_driver_t       *this   = NULL;
     
     _x_assert( visual != NULL );
     
     if (!visual->output_cb)
          return NULL;

     this = (dfb_driver_t*) xine_xmalloc( sizeof(dfb_driver_t) );
     if (!this) {
          lprintf( "memory allocation failed!!!\n" );
          return NULL;
     }
 
     this->vo_driver.get_capabilities     = vo_dfb_get_capabilities;
     this->vo_driver.alloc_frame          = vo_dfb_alloc_frame;
     this->vo_driver.update_frame_format  = vo_dfb_update_frame_format;
     this->vo_driver.overlay_begin        = vo_dfb_overlay_begin;
     this->vo_driver.overlay_blend        = vo_dfb_overlay_blend;
     this->vo_driver.overlay_end          = vo_dfb_overlay_end;
     this->vo_driver.display_frame        = vo_dfb_display_frame;
     this->vo_driver.get_property         = vo_dfb_get_property;
     this->vo_driver.set_property         = vo_dfb_set_property;
     this->vo_driver.get_property_min_max = vo_dfb_get_property_min_max;
     this->vo_driver.gui_data_exchange    = vo_dfb_gui_data_exchange;
     this->vo_driver.redraw_needed        = vo_dfb_redraw_needed;
     this->vo_driver.dispose              = vo_dfb_dispose; 
     this->xine                           = class->xine;

     vo_dfb_init_config( this );

     xprintf( this->xine, XINE_VERBOSITY_LOG,
              "video_out_dfb: using %s conversion matrix\n",
              proc_matrices[this->config.proc_matrix].name );
     
#ifdef USE_MMX
     if ((xine_mm_accel() & MM_MMX) == MM_MMX) {
          if (this->config.enable_mmx) {
               xprintf( this->xine, XINE_VERBOSITY_LOG,
                        "video_out_dfb: MMX detected and enabled\n" );
               this->accel = MM_MMX;
               /* YUY2 */
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, yuy2, DSPF_YUY2 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, uyvy, DSPF_UYVY );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, yv12, DSPF_YV12 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, yv12, DSPF_I420 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, nv12, DSPF_NV12 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, nv21, DSPF_NV21 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, nv16, DSPF_NV16 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, rgb332, DSPF_RGB332 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, argb1555, DSPF_ARGB1555 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, rgb16, DSPF_RGB16 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, rgb24, DSPF_RGB24 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, rgb32, DSPF_RGB32 );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, argb,  DSPF_ARGB );
               DFB_PFUNCTION_ASSIGN( mmx, yuy2, rgb32, DSPF_AiRGB );
               /* YV12 */
               DFB_PFUNCTION_ASSIGN( mmx, yv12, yuy2, DSPF_YUY2 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, uyvy, DSPF_UYVY );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, yv12, DSPF_YV12 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, yv12, DSPF_I420 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, nv12, DSPF_NV12 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, nv21, DSPF_NV21 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, nv16, DSPF_NV16 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, rgb332, DSPF_RGB332 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, argb1555, DSPF_ARGB1555 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, rgb16, DSPF_RGB16 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, rgb24, DSPF_RGB24 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, rgb32, DSPF_RGB32 );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, argb, DSPF_ARGB );
               DFB_PFUNCTION_ASSIGN( mmx, yv12, rgb32, DSPF_AiRGB );
          } else
               xprintf( this->xine, XINE_VERBOSITY_LOG,
                        "video_out_dfb: MMX detected but disabled\n" );
     }
#endif
     
     dfb_state_init( &this->state );

     this->mixer.b   =  0;
     this->mixer.c   = +128;
     this->mixer.s   = +128;
     this->mixer.set =  MF_NONE;

     memset( &proc_table, 0, sizeof(proc_table) );

     if (visual->destination) {
          if (!vo_dfb_set_destination( this, visual->destination )) {
               free( this );
               return NULL;
          }
     }
     
     if (visual->subpicture)
          vo_dfb_set_subpicture( this, visual->subpicture );
     
     this->output_cb    = visual->output_cb;
     this->output_cdata = visual->output_cdata;
     this->frame_cb     = visual->frame_cb;
     this->frame_cdata  = visual->frame_cdata;
     
     return (vo_driver_t*) this;
}

static char*
get_identifier( video_driver_class_t *vo_class )
{
     return "DFB";
}

static char*
get_description( video_driver_class_t *vo_class)
{
     return "generic DirectFB video output driver";
}

static void
dispose_class( video_driver_class_t *vo_class )
{
     free( vo_class );
}

static void*
init_class( xine_t *xine,
            void   *vo_visual )
{
     dfb_driver_class_t *class;
     const char         *error;

     if (!vo_visual) {
          xprintf( xine, XINE_VERBOSITY_DEBUG,
                   "video_out_dfb: got a null visual!\n" );
          return NULL;
     }

     error = DirectFBCheckVersion( DIRECTFB_MAJOR_VERSION,
                                   DIRECTFB_MINOR_VERSION,
                                   DIRECTFB_MICRO_VERSION );
     if (error) {
          fprintf( stderr, "video_out_dfb: %s !!!\n", error );
          return NULL;
     }                   

     class = (dfb_driver_class_t*) xine_xmalloc( sizeof(dfb_driver_class_t) );
     if (!class) {
          lprintf( "memory allocation failed!!!\n" );
          return NULL;
     }

     class->vo_class.open_plugin     = open_plugin;
     class->vo_class.get_identifier  = get_identifier;
     class->vo_class.get_description = get_description;
     class->vo_class.dispose         = dispose_class;
     class->xine                     = xine;

     return class;
}

static vo_info_t vo_info_dfb = {
     8,
     XINE_VISUAL_TYPE_DFB
};


plugin_info_t xine_plugin_info[] = {
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, VIDEO_OUT_DRIVER_IFACE_VERSION, "DFB",
       XINE_VERSION_CODE, &vo_info_dfb, init_class },
  { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

