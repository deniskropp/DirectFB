/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dfb_types.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/gfxcard.h>
#include <core/state.h>
#include <core/palette.h>

#include <misc/gfx_util.h>
#include <misc/util.h>
#include <misc/conf.h>

#include <direct/clock.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <gfx/convert.h>
#include <gfx/util.h>

#include "generic.h"

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/
/*********           **************************************************************************************************/
/*** Smooth scaling routines ******************************************************************************************/
/*********           **************************************************************************************************/
/**********************************************************************************************************************/

#if DFB_SMOOTH_SCALING

typedef struct {
     DFBRegion   clip;
     const void *colors;
     ulong       protect;
     ulong       key;
} StretchCtx;

typedef void (*StretchHVx)( void             *dst,
                            int               dpitch,
                            const void       *src,
                            int               spitch,
                            int               width,
                            int               height,
                            int               dst_width,
                            int               dst_height,
                            const StretchCtx *ctx );

#define STRETCH_NONE           0
#define STRETCH_SRCKEY         1
#define STRETCH_PROTECT        2
#define STRETCH_SRCKEY_PROTECT 3
#define STRETCH_NUM            4

typedef struct {
     struct {
          StretchHVx     up[STRETCH_NUM];
          StretchHVx     down[STRETCH_NUM];
     } f[DFB_NUM_PIXELFORMATS];
} StretchFunctionTable;

/**********************************************************************************************************************/
/*** 16 bit RGB 565 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGB16
#define TABLE_NAME              stretch_hvx_RGB16
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGB16_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5                5
#define SHIFT_R6                6
#define X_F81F                  0xf81f
#define X_07E0                  0x07e0
#define MASK_RGB                0xffff

#define FORMAT_RGB16
#include "stretch_up_down_16.h"
#undef FORMAT_RGB16

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB

/**********************************************************************************************************************/
/*** 16 bit ARGB 4444 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_ARGB4444
#define TABLE_NAME              stretch_hvx_ARGB4444
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_ARGB4444_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5          4
#define SHIFT_R6          4
#define X_F81F            0x0f0f
#define X_07E0            0xf0f0
#define MASK_RGB          0x0fff
#define HAS_ALPHA

#define FORMAT_ARGB4444
#include "stretch_up_down_16.h"
#undef FORMAT_ARGB4444

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB
#undef HAS_ALPHA

/**********************************************************************************************************************/
/*** 16 bit RGBA 4444 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGBA4444
#define TABLE_NAME              stretch_hvx_RGBA4444
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGBA4444_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R5          4
#define SHIFT_R6          4
#define X_F81F            0x0f0f
#define X_07E0            0xf0f0
#define MASK_RGB          0xfff0
#define HAS_ALPHA

#define FORMAT_RGBA4444
#include "stretch_up_down_16.h"
#undef FORMAT_RGBA4444

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R5
#undef SHIFT_R6
#undef X_F81F
#undef X_07E0
#undef MASK_RGB
#undef HAS_ALPHA

/**********************************************************************************************************************/
/*** 32 bit ARGB 8888 scalers *****************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_ARGB
#define TABLE_NAME              stretch_hvx_ARGB
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_ARGB_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R8                8
#define SHIFT_L8                8
#define X_00FF00FF              0x00ff00ff
#define X_FF00FF00              0xff00ff00
#define MASK_RGB                0x00ffffff
#define HAS_ALPHA

#include "stretch_up_down_32.h"

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R8
#undef SHIFT_L8
#undef X_00FF00FF
#undef X_FF00FF00
#undef MASK_RGB
#undef HAS_ALPHA

/**********************************************************************************************************************/
/*** 32 bit RGB 888 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define DST_FORMAT              DSPF_RGB32
#define TABLE_NAME              stretch_hvx_RGB32
#define FUNC_NAME(UPDOWN,K,P,F) stretch_hvx_RGB32_ ## UPDOWN ## _ ## K ## P ## _ ## F
#define SHIFT_R8                8
#define SHIFT_L8                8
#define X_00FF00FF              0x00ff00ff
#define X_FF00FF00              0x0000ff00
#define MASK_RGB                0x00ffffff

#include "stretch_up_down_32.h"

#undef DST_FORMAT
#undef TABLE_NAME
#undef FUNC_NAME
#undef SHIFT_R8
#undef SHIFT_L8
#undef X_00FF00FF
#undef X_FF00FF00
#undef MASK_RGB

#if 0
/**********************************************************************************************************************/
/*** 16 bit YUV 422 scalers *******************************************************************************************/
/**********************************************************************************************************************/

#define FUNC_NAME(UPDOWN) stretch_hvx_nv16_ ## UPDOWN

#include "stretch_up_down_8.h"

#undef FUNC_NAME

/**********************************************************************************************************************/

#define FUNC_NAME(UPDOWN) stretch_hvx_nv16_uv_ ## UPDOWN

#include "stretch_up_down_88.h"

#undef FUNC_NAME

#endif
/**********************************************************************************************************************/
/**********************************************************************************************************************/

static const StretchFunctionTable *stretch_tables[DFB_NUM_PIXELFORMATS] = {
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1555)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB16)]    = &stretch_hvx_RGB16,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB24)]    = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB32)]    = &stretch_hvx_RGB32,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB)]     = &stretch_hvx_ARGB,
     [DFB_PIXELFORMAT_INDEX(DSPF_ABGR)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A8)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUY2)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB332)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_UYVY)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_I420)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV12)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT8)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ALUT44)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AiRGB)]    = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV12)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV16)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB2554)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB4444)] = &stretch_hvx_ARGB4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA4444)] = &stretch_hvx_RGBA4444,
     [DFB_PIXELFORMAT_INDEX(DSPF_NV21)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AYUV)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A4)]       = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB1666)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB6666)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB18)]    = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_LUT2)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB444)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGB555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_BGR555)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_RGBA5551)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YUV444P)]  = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_ARGB8565)] = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_AVYU)]     = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_VYU)]      = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_A1_LSB)]   = NULL,
     [DFB_PIXELFORMAT_INDEX(DSPF_YV16)]     = NULL,
};

/**********************************************************************************************************************/

__attribute__((noinline))
static bool
stretch_hvx( CardState *state, DFBRectangle *srect, DFBRectangle *drect )
{
     GenefxState                *gfxs;
     const StretchFunctionTable *table;
     StretchHVx                  stretch;
     bool                        down = false;
     void                       *dst;
     void                       *src;
     StretchCtx                  ctx;
     int                         idx = STRETCH_NONE;
     u32                         colors[256];

     D_ASSERT( state != NULL );
     DFB_RECTANGLE_ASSERT( srect );
     DFB_RECTANGLE_ASSERT( drect );

     gfxs = state->gfxs;

     if (state->blittingflags & ~(DSBLIT_COLORKEY_PROTECT | DSBLIT_SRC_COLORKEY | DSBLIT_SRC_PREMULTIPLY))
          return false;

     if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY && !DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format ))
          return false;

     if (DFB_PIXELFORMAT_INDEX(gfxs->dst_format) >= D_ARRAY_SIZE(stretch_tables))
          return false;

     if (DFB_PIXELFORMAT_INDEX(gfxs->src_format) >= D_ARRAY_SIZE((stretch_tables[0])->f))
          return false;

     table = stretch_tables[DFB_PIXELFORMAT_INDEX(gfxs->dst_format)];
     if (!table)
          return false;

     if (srect->w > drect->w && srect->h > drect->h)
          down = true;

     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          idx |= STRETCH_SRCKEY;

     if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
          idx |= STRETCH_PROTECT;

     if (down) {
          if (!(state->render_options & DSRO_SMOOTH_DOWNSCALE))
               return false;

          stretch = table->f[DFB_PIXELFORMAT_INDEX(gfxs->src_format)].down[idx];
     }
     else {
          if (!(state->render_options & DSRO_SMOOTH_UPSCALE))
               return false;

          stretch = table->f[DFB_PIXELFORMAT_INDEX(gfxs->src_format)].up[idx];
     }

     if (!stretch)
          return false;

     ctx.clip = state->clip;

     if (!dfb_region_rectangle_intersect( &ctx.clip, drect ))
          return false;

     dfb_region_translate( &ctx.clip, - drect->x, - drect->y );

     if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->src_format )) {
          int             i;
          const DFBColor *entries;
          u16            *colors16 = (void*) colors;

          D_ASSERT( gfxs->Blut != NULL );

          entries = gfxs->Blut->entries;

          switch (gfxs->dst_format) {
               case DSPF_ARGB:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i=0; i<gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors[i] = 0;
                                        break;

                                   case 255:
                                        colors[i] = PIXEL_ARGB( entries[i].a,
                                                                entries[i].r,
                                                                entries[i].g,
                                                                entries[i].b );
                                        break;

                                   default:
                                        colors[i] = PIXEL_ARGB( entries[i].a,
                                                                (alpha * entries[i].r) >> 8,
                                                                (alpha * entries[i].g) >> 8,
                                                                (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i=0; i<gfxs->Blut->num_entries; i++)
                              colors[i] = PIXEL_ARGB( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_ABGR:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i=0; i<gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors[i] = 0;
                                        break;

                                   case 255:
                                        colors[i] = PIXEL_ABGR( entries[i].a,
                                                                entries[i].r,
                                                                entries[i].g,
                                                                entries[i].b );
                                        break;

                                   default:
                                        colors[i] = PIXEL_ABGR( entries[i].a,
                                                                (alpha * entries[i].r) >> 8,
                                                                (alpha * entries[i].g) >> 8,
                                                                (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i=0; i<gfxs->Blut->num_entries; i++)
                              colors[i] = PIXEL_ABGR( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGB32:
                    for (i=0; i<gfxs->Blut->num_entries; i++)
                         colors[i] = PIXEL_RGB32( entries[i].r, entries[i].g, entries[i].b );
                    break;

               case DSPF_RGB16:
                    for (i=0; i<gfxs->Blut->num_entries; i++)
                         colors16[i] = PIXEL_RGB16( entries[i].r, entries[i].g, entries[i].b );
                    break;

               case DSPF_ARGB4444:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i=0; i<gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors16[i] = 0;
                                        break;

                                   case 255:
                                        colors16[i] = PIXEL_ARGB4444( entries[i].a,
                                                                      entries[i].r,
                                                                      entries[i].g,
                                                                      entries[i].b );
                                        break;

                                   default:
                                        colors16[i] = PIXEL_ARGB4444( entries[i].a,
                                                                      (alpha * entries[i].r) >> 8,
                                                                      (alpha * entries[i].g) >> 8,
                                                                      (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i=0; i<gfxs->Blut->num_entries; i++)
                              colors16[i] = PIXEL_ARGB4444( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGBA4444:
                    if (state->blittingflags & DSBLIT_SRC_PREMULTIPLY) {
                         for (i=0; i<gfxs->Blut->num_entries; i++) {
                              int alpha = entries[i].a + 1;

                              switch (alpha) {
                                   case 0:
                                        colors16[i] = 0;
                                        break;

                                   case 255:
                                        colors16[i] = PIXEL_RGBA4444( entries[i].a,
                                                                      entries[i].r,
                                                                      entries[i].g,
                                                                      entries[i].b );
                                        break;

                                   default:
                                        colors16[i] = PIXEL_RGBA4444( entries[i].a,
                                                                      (alpha * entries[i].r) >> 8,
                                                                      (alpha * entries[i].g) >> 8,
                                                                      (alpha * entries[i].b) >> 8 );
                              }
                         }
                    }
                    else {
                         for (i=0; i<gfxs->Blut->num_entries; i++)
                              colors16[i] = PIXEL_RGBA4444( entries[i].a, entries[i].r, entries[i].g, entries[i].b );
                    }
                    break;

               case DSPF_RGB444:
                    for (i=0; i<gfxs->Blut->num_entries; i++)
                         colors16[i] = PIXEL_RGB444( entries[i].r, entries[i].g, entries[i].b );
                    break;

               default:
                    D_UNIMPLEMENTED();
          }

          ctx.colors = colors;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
                    ctx.key = state->src_colorkey;
               else {
                    const DFBColor *color = &entries[state->src_colorkey % gfxs->Blut->num_entries];

                    ctx.key = dfb_color_to_pixel( gfxs->dst_format, color->r, color->g, color->b );
               }
          }
     }
     else {
          ctx.colors = NULL;

          if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
               DFBColor color;

               dfb_pixel_to_color( gfxs->src_format, state->src_colorkey, &color );

               ctx.key = dfb_color_to_pixel( gfxs->dst_format, color.r, color.g, color.b );
          }
     }

     if (state->blittingflags & DSBLIT_COLORKEY_PROTECT) {
          if (DFB_PIXELFORMAT_IS_INDEXED( gfxs->dst_format ))
               ctx.protect = state->colorkey.index;
          else
               ctx.protect = dfb_color_to_pixel( gfxs->dst_format,
                                                 state->colorkey.r,
                                                 state->colorkey.g,
                                                 state->colorkey.b );
     }

     dst = gfxs->dst_org[0] + drect->y * gfxs->dst_pitch + DFB_BYTES_PER_LINE( gfxs->dst_format, drect->x );
     src = gfxs->src_org[0] + srect->y * gfxs->src_pitch + DFB_BYTES_PER_LINE( gfxs->src_format, srect->x );

     stretch( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
              srect->w, srect->h, drect->w, drect->h, &ctx );

#if 0     /* FIXME: repair */
     switch (gfxs->dst_format) {
          case DSPF_NV16:
               ctx.clip.x1 /= 2;
               ctx.clip.x2 /= 2;
               if (srect->w < drect->w || srect->h < drect->h) {
                    stretch_hvx_nv16_uv_up( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                            srect->w/2, srect->h, drect->w/2, drect->h, &ctx );
               }
               else {
                    stretch_hvx_nv16_uv_down( dst, gfxs->dst_pitch, src, gfxs->src_pitch,
                                              srect->w/2, srect->h, drect->w/2, drect->h, &ctx );
               }
               break;

          default:
               break;
     }
#endif

     return true;
}
#endif /* DFB_SMOOTH_SCALING */

void gStretchBlit( CardState *state, DFBRectangle *srect, DFBRectangle *drect )
{
     GenefxState    *gfxs  = state->gfxs;
     DFBRectangle    orect = *drect;
     XopAdvanceFunc  Aop_advance;
     XopAdvanceFunc  Bop_advance;
     int             Aop_X;
     int             Aop_Y;
     int             Bop_X;
     int             Bop_Y;

     int fx, fy;
     int ix, iy;
     int h;

     D_ASSERT( gfxs != NULL );

     if (dfb_config->software_warn) {
          D_WARN( "StretchBlit   (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x, source (%4d,%4d-%4dx%4d) %6s",
                  drect->x, drect->y, drect->w, drect->h, dfb_pixelformat_name(gfxs->dst_format), state->blittingflags,
                  state->color.a, state->color.r, state->color.g, state->color.b, srect->x, srect->y, srect->w, srect->h,
                  dfb_pixelformat_name(gfxs->src_format) );
     }

     CHECK_PIPELINE();

#if DFB_SMOOTH_SCALING
     if (state->render_options & (DSRO_SMOOTH_UPSCALE | DSRO_SMOOTH_DOWNSCALE) &&
         stretch_hvx( state, srect, drect ))
          return;
#endif

     /* Clip destination rectangle. */
     if (!dfb_rectangle_intersect_by_region( drect, &state->clip ))
          return;

     /* Calculate fractions. */
     fx = (srect->w << 16) / orect.w;
     fy = (srect->h << 16) / orect.h;

     /* Calculate horizontal phase and offset. */
     ix = fx * (drect->x - orect.x);
     srect->x += ix >> 16;
     ix &= 0xFFFF;

     /* Calculate vertical phase and offset. */
     iy = fy * (drect->y - orect.y);
     srect->y += iy >> 16;
     iy &= 0xFFFF;

     /* Adjust source size. */
     srect->w = ((drect->w * fx + ix) + 0xFFFF) >> 16;
     srect->h = ((drect->h * fy + iy) + 0xFFFF) >> 16;

     D_ASSERT( srect->x + srect->w <= state->source->config.size.w );
     D_ASSERT( srect->y + srect->h <= state->source->config.size.h );
     D_ASSERT( drect->x + drect->w <= state->clip.x2 + 1 );
     D_ASSERT( drect->y + drect->h <= state->clip.y2 + 1 );


     if (!Genefx_ABacc_prepare( gfxs, MAX( srect->w, drect->w ) ))
          return;


     switch (gfxs->src_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               srect->x &= ~1;
               break;
          default:
               break;
     }

     switch (gfxs->dst_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               drect->x &= ~1;
               break;
          default:
               break;
     }

     gfxs->Slen   = srect->w;
     gfxs->Dlen   = drect->w;
     gfxs->length = gfxs->Dlen;
     gfxs->SperD  = fx;
     gfxs->Xphase = ix;

     h = drect->h;

     if (state->blittingflags & DSBLIT_FLIP_HORIZONTAL) {
          gfxs->Astep *= -1;

          Aop_X = drect->x + drect->w - 1;
          Aop_Y = drect->y;

          Bop_X = srect->x;
          Bop_Y = srect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;
     }
     else if (state->blittingflags & DSBLIT_FLIP_VERTICAL) {
          Aop_X = drect->x;
          Aop_Y = drect->y + drect->h - 1;

          Bop_X = srect->x;
          Bop_Y = srect->y;

          Aop_advance = Genefx_Aop_prev;
          Bop_advance = Genefx_Bop_next;
     }
     else {
          Aop_X = drect->x;
          Aop_Y = drect->y;

          Bop_X = srect->x;
          Bop_Y = srect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;
     }

     Genefx_Aop_xy( gfxs, Aop_X, Aop_Y );
     Genefx_Bop_xy( gfxs, Bop_X, Bop_Y );

     while (h--) {
          RUN_PIPELINE();

          Aop_advance( gfxs );

          iy += fy;

          while (iy > 0xFFFF) {
               iy -= 0x10000;
               Bop_advance( gfxs );
          }
     }

     Genefx_ABacc_flush( gfxs );
}

