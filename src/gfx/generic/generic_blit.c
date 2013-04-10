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

void gBlit( CardState *state, DFBRectangle *rect, int dx, int dy )
{
     GenefxState    *gfxs = state->gfxs;
     int             h;
     XopAdvanceFunc  Aop_advance;
     XopAdvanceFunc  Bop_advance;
     XopAdvanceFunc  Mop_advance;
     int             Aop_X;
     int             Aop_Y;
     int             Bop_X;
     int             Bop_Y;
     int             Mop_X;
     int             Mop_Y;

     DFBSurfaceBlittingFlags rotflip_blittingflags = state->blittingflags;

     dfb_simplify_blittingflags( &rotflip_blittingflags );
     rotflip_blittingflags &= (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL | DSBLIT_ROTATE90 );

     D_ASSERT( gfxs != NULL );

     if (dfb_config->software_warn) {
          D_WARN( "Blit          (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, funcs %d/%d, color 0x%02x%02x%02x%02x, source (%4d,%4d) %6s",
                  dx, dy, rect->w, rect->h, dfb_pixelformat_name(gfxs->dst_format), state->blittingflags,
                  state->src_blend, state->dst_blend,
                  state->color.a, state->color.r, state->color.g, state->color.b, rect->x, rect->y,
                  dfb_pixelformat_name(gfxs->src_format) );
     }

     D_ASSERT( state->clip.x1 <= dx );
     D_ASSERT( state->clip.y1 <= dy );
     D_ASSERT( (rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.x2 >= (dx + rect->w - 1) );
     D_ASSERT( (rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.y2 >= (dy + rect->h - 1) );
     D_ASSERT( !(rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.x2 >= (dx + rect->h - 1) );
     D_ASSERT( !(rotflip_blittingflags & DSBLIT_ROTATE90) || state->clip.y2 >= (dy + rect->w - 1) );

     CHECK_PIPELINE();

     if (!Genefx_ABacc_prepare( gfxs, rect->w ))
          return;


     switch (gfxs->src_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               rect->x &= ~1;
               break;
          default:
               break;
     }

     switch (gfxs->dst_format) {
          case DSPF_A4:
          case DSPF_YUY2:
          case DSPF_UYVY:
               dx &= ~1;
               break;
          default:
               break;
     }

     gfxs->length = rect->w;


     if (gfxs->src_org[0] == gfxs->dst_org[0] && dy == rect->y && dx > rect->x)
          /* we must blit from right to left */
          gfxs->Astep = gfxs->Bstep = -1;
     else
          /* we must blit from left to right*/
          gfxs->Astep = gfxs->Bstep = 1;


     int mask_x = 0;
     int mask_y = 0;
     int mask_h = gfxs->mask_height;

     if ((state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) && (state->src_mask_flags & DSMF_STENCIL)) {
          mask_x = state->src_mask_offset.x;
          mask_y = state->src_mask_offset.y;
     }

     if (rotflip_blittingflags == (DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL)) { // 180 deg
          gfxs->Astep *= -1;

          Aop_X = dx + rect->w - 1;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_FLIP_HORIZONTAL) {
          gfxs->Astep *= -1;

          Aop_X = dx + rect->w - 1;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_FLIP_VERTICAL) {
          Aop_X = dx;
          Aop_Y = dy + rect->h - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_prev;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL | DSBLIT_FLIP_VERTICAL)) { // 270 deg ccw
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else if (rotflip_blittingflags == DSBLIT_ROTATE90) { // 90 deg ccw
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy + rect->w - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_VERTICAL)) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= -gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx + rect->h - 1;
          Aop_Y = dy + rect->w - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_prev_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (rotflip_blittingflags == (DSBLIT_ROTATE90 | DSBLIT_FLIP_HORIZONTAL)) {
          if (gfxs->dst_bpp == 0) {
               D_UNIMPLEMENTED();
               return;
          }

          gfxs->Astep *= gfxs->dst_pitch / gfxs->dst_bpp;

          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_crab;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }
     else if (gfxs->src_org[0] == gfxs->dst_org[0] && dy > rect->y && !(state->blittingflags & DSBLIT_DEINTERLACE)) {
          /* we must blit from bottom to top */
          Aop_X = dx;
          Aop_Y = dy + rect->h - 1;

          Bop_X = rect->x;
          Bop_Y = rect->y + rect->h - 1;

          Aop_advance = Genefx_Aop_prev;
          Bop_advance = Genefx_Bop_prev;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y + mask_h - 1;

               Mop_advance = Genefx_Mop_prev;
          }
     }
     else {
          /* we must blit from top to bottom */
          Aop_X = dx;
          Aop_Y = dy;

          Bop_X = rect->x;
          Bop_Y = rect->y;

          Aop_advance = Genefx_Aop_next;
          Bop_advance = Genefx_Bop_next;

          if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
               Mop_X = mask_x;
               Mop_Y = mask_y;

               Mop_advance = Genefx_Mop_next;
          }
     }



     Genefx_Aop_xy( gfxs, Aop_X, Aop_Y );
     Genefx_Bop_xy( gfxs, Bop_X, Bop_Y );
     if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
          Genefx_Mop_xy( gfxs, Mop_X, Mop_Y );

     if (state->blittingflags & DSBLIT_DEINTERLACE) {
          if (state->source->field) {
               Aop_advance( gfxs );
               Bop_advance( gfxs );
               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
                    Mop_advance( gfxs );
               rect->h--;
          }

          for (h = rect->h/2; h; h--) {
               RUN_PIPELINE();

               Aop_advance( gfxs );

               RUN_PIPELINE();

               Aop_advance( gfxs );

               Bop_advance( gfxs );
               Bop_advance( gfxs );

               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR)) {
                    Mop_advance( gfxs );
                    Mop_advance( gfxs );
               }
          }
     } /* ! DSBLIT_DEINTERLACE */
     else {
          for (h = rect->h; h; h--) {
               RUN_PIPELINE();

               Aop_advance( gfxs );
               Bop_advance( gfxs );
               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
                    Mop_advance( gfxs );
          }
     }

     Genefx_ABacc_flush( gfxs );
}

