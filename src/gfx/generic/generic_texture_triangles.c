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

//#define DIRECT_ENABLE_DEBUG

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

D_DEBUG_DOMAIN( Genefx_TexTriangles, "Genefx/TexTriangles", "Genefx Texture Triangles" );

/**********************************************************************************************************************/
/**********************************************************************************************************************/

#define COORD_SHIFT 18

void
Genefx_TextureTriangleAffine( GenefxState        *gfxs,
                              GenefxVertexAffine *v0,
                              GenefxVertexAffine *v1,
                              GenefxVertexAffine *v2,
                              const DFBRegion    *clip )
{
     D_DEBUG_AT( Genefx_TexTriangles, "%s( state %p, v0 %p, v1 %p, v2 %p )\n", __func__, gfxs, v0, v1, v2 );

     if (v0->y == v1->y && v1->y == v2->y) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> all points on one horizontal line\n" );
          return;
     }

     D_DEBUG_AT( Genefx_TexTriangles, "  -> clip [%4d,%4d-%4d,%4d]\n", clip->x1, clip->y1, clip->x2, clip->y2 );

     GenefxVertexAffine *v_tmp;

     /*
      * Triangle Sorting (vertical)
      */
     if (v1->y < v0->y) {
          v_tmp = v0;
          v0 = v1;
          v1 = v_tmp;
     }
     if (v2->y < v0->y) {
          v_tmp = v2;
          v2 = v1;
          v1 = v0;
          v0 = v_tmp;
     }
     else if (v2->y < v1->y) {
          v_tmp = v1;
          v1 = v2;
          v2 = v_tmp;
     }

     /*
      * Vertical Pre-Clipping
      */
     int y_top    = v0->y;
     int y_bottom = v2->y - 1;

     if (y_top > clip->y2 || y_bottom < clip->y1) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> totally clipped (vertical)\n" );
          return;
     }

     /*
      * Horizontal Pre-Clipping
      */
     if (v0->x > clip->x2 && v1->x > clip->x2 && v2->x > clip->x2) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> totally clipped right\n" );
          return;
     }

     if (v0->x < clip->x1 && v1->x < clip->x1 && v2->x < clip->x1) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> totally clipped left\n" );
          return;
     }


     int y_update = -1;

     /*
      * Triangle Sorting (horizontal)
      */

     /* Flat top */
     if (v0->y == v1->y) {
          if (v0->x > v1->x) {
               v_tmp = v0;
               v0 = v1;
               v1 = v_tmp;
          }
     }
     /* Flat bottom */
     else if (v1->y == v2->y) {
          if (v1->x > v2->x) {
               v_tmp = v1;
               v1 = v2;
               v2 = v_tmp;
          }
     }
     /* Two parts */
     else
          y_update = v1->y;


     D_DEBUG_AT( Genefx_TexTriangles, "  -> [0] %4d,%4d\n", v0->x, v0->y );
     D_DEBUG_AT( Genefx_TexTriangles, "  -> [1] %4d,%4d\n", v1->x, v1->y );
     D_DEBUG_AT( Genefx_TexTriangles, "  -> [2] %4d,%4d\n", v2->x, v2->y );


     /*
      * Triangle Setup
      */

     int height      = v2->y - v0->y;
     int half_top    = v1->y - v0->y;
     int half_bottom = v2->y - v1->y;

     int xl, xr;
     int dxl, dxr;
     int dxl2, dxr2;

     int sl, sr;
     int tl, tr;
     int dsl, dsr;
     int dtl, dtr;
     int dsl2, dsr2;
     int dtl2, dtr2;

     /* Flat top */
     if (v0->y == v1->y) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> flat top\n" );

          if (v0->x == v1->x) {
               D_DEBUG_AT( Genefx_TexTriangles, "  -> top points equal\n" );
               return;
          }

          xl = v0->x << COORD_SHIFT;
          xr = v1->x << COORD_SHIFT;

          dxl = dxl2 = ((v2->x << COORD_SHIFT) - xl) / height;
          dxr = dxr2 = ((v2->x << COORD_SHIFT) - xr) / height;

          sl = v0->s;
          sr = v1->s;

          dsl = dsl2 = (v2->s - sl) / height;
          dsr = dsr2 = (v2->s - sr) / height;

          tl = v0->t;
          tr = v1->t;

          dtl = dtl2 = (v2->t - tl) / height;
          dtr = dtr2 = (v2->t - tr) / height;

          D_ASSERT( dxr < dxl );
     }
     /* Flat bottom */
     else if (v1->y == v2->y) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> flat bottom\n" );

          if (v1->x == v2->x) {
               D_DEBUG_AT( Genefx_TexTriangles, "  -> bottom points equal\n" );
               return;
          }

          xl = xr = v0->x << COORD_SHIFT;

          dxl = dxl2 = ((v1->x << COORD_SHIFT) - xl) / height;
          dxr = dxr2 = ((v2->x << COORD_SHIFT) - xr) / height;

          sl = sr = v0->s;

          dsl = dsl2 = (v1->s - sl) / height;
          dsr = dsr2 = (v2->s - sr) / height;

          tl = tr = v0->t;

          dtl = dtl2 = (v1->t - tl) / height;
          dtr = dtr2 = (v2->t - tr) / height;

          D_ASSERT( dxr > dxl );
     }
     /* Two parts */
     else {
          xl = xr = v0->x << COORD_SHIFT;
          sl = sr = v0->s;
          tl = tr = v0->t;

          int x_v1 = v0->x + (v2->x - v0->x) * (v1->y - v0->y) / height;

          /* Update left */
          if (x_v1 > v1->x) {
               D_DEBUG_AT( Genefx_TexTriangles, "  -> two parts, update left\n" );

               dxl = ((v1->x << COORD_SHIFT) - xl) / half_top;
               dxr = ((v2->x << COORD_SHIFT) - xr) / height;

               dxl2 = ((v2->x - v1->x) << COORD_SHIFT) / half_bottom;
               dxr2 = dxr;

               dsl = (v1->s - sl) / half_top;
               dsr = (v2->s - sr) / height;

               dsl2 = (v2->s - v1->s) / half_bottom;
               dsr2 = dsr;

               dtl = (v1->t - tl) / half_top;
               dtr = (v2->t - tr) / height;

               dtl2 = (v2->t - v1->t) / half_bottom;
               dtr2 = dtr;
          }
          /* Update right */
          else if (x_v1 < v1->x) {
               D_DEBUG_AT( Genefx_TexTriangles, "  -> two parts, update right\n" );

               dxl = ((v2->x << COORD_SHIFT) - xl) / height;
               dxr = ((v1->x << COORD_SHIFT) - xr) / half_top;

               dxl2 = dxl;
               dxr2 = ((v2->x - v1->x) << COORD_SHIFT) / half_bottom;

               dsl = (v2->s - sl) / height;
               dsr = (v1->s - sr) / half_top;

               dsl2 = dsl;
               dsr2 = (v2->s - v1->s) / half_bottom;

               dtl = (v2->t - tl) / height;
               dtr = (v1->t - tr) / half_top;

               dtl2 = dtl;
               dtr2 = (v2->t - v1->t) / half_bottom;
          }
          else {
               D_DEBUG_AT( Genefx_TexTriangles, "  -> all points on one line\n" );
               return;
          }

          D_ASSERT( dxr > dxl );
     }

     D_DEBUG_AT( Genefx_TexTriangles, "  -> dxl %d, dxr %d, dxl2 %d, dxr2 %d, height %d\n", dxl, dxr, dxl2, dxr2, height );


     /*
      * Vertical Clipping
      */
     if (y_top < clip->y1) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> clipped top from %d to %d\n", y_top, clip->y1 );

          y_top = clip->y1;
     }

     if (y_bottom > clip->y2) {
          D_DEBUG_AT( Genefx_TexTriangles, "  -> clipped bottom from %d to %d\n", y_bottom, clip->y2 );

          y_bottom = clip->y2;
     }

     if (y_top > v0->y) {
          /* Two parts */
          if (y_update != -1 && y_top > y_update) {
               xl += dxl * (y_update - v0->y) + dxl2 * (y_top - v1->y);
               xr += dxr * (y_update - v0->y) + dxr2 * (y_top - v1->y);

               sl += dsl * (y_update - v0->y) + dsl2 * (y_top - v1->y);
               sr += dsr * (y_update - v0->y) + dsr2 * (y_top - v1->y);

               tl += dtl * (y_update - v0->y) + dtl2 * (y_top - v1->y);
               tr += dtr * (y_update - v0->y) + dtr2 * (y_top - v1->y);

               dxl = dxl2;
               dxr = dxr2;

               dsl = dsl2;
               dsr = dsr2;

               dtl = dtl2;
               dtr = dtr2;

               D_DEBUG_AT( Genefx_TexTriangles, "  -> clipped two parts\n" );
          }
          /* One part or only top clipped */
          else {
               xl += dxl * (y_top - v0->y);
               xr += dxr * (y_top - v0->y);

               sl += dsl * (y_top - v0->y);
               sr += dsr * (y_top - v0->y);

               tl += dtl * (y_top - v0->y);
               tr += dtr * (y_top - v0->y);

               D_DEBUG_AT( Genefx_TexTriangles, "  -> clipped one part\n", y_top, clip->y1 );
          }
     }


     /*
      * Loop over clipped lines
      */
     for (int y=y_top; y<=y_bottom; y++) {
          /*
           * Slope update (for bottom half)
           */
          if (y == y_update) {
               /* Left */
               if (dxl != dxl2) {
                    dxl = dxl2;
                    dsl = dsl2;
                    dtl = dtl2;

                    D_DEBUG_AT( Genefx_TexTriangles, "  -> updating left, off by %d\n", xl - (v1->x << COORD_SHIFT) );

                    xl = v1->x << COORD_SHIFT;
                    sl = v1->s;
                    tl = v1->t;
               }
               /* Right */
               else if (dxr != dxr2) {
                    dxr = dxr2;
                    dsr = dsr2;
                    dtr = dtr2;

                    D_DEBUG_AT( Genefx_TexTriangles, "  -> updating right, off by %d\n", xr - (v1->x << COORD_SHIFT) );

                    xr = v1->x << COORD_SHIFT;
                    sr = v1->s;
                    tr = v1->t;
               }
          }


          D_ASSERT( xr - xl >= -100 );  // FIXME: try to get better precision

          if (xl > xr)
               xl = xr;

          /*
           * Scanline Setup
           */
          int len = ((xr - xl) >> COORD_SHIFT) + 1;
          int x1  = xl >> COORD_SHIFT;
          int x2  = x1 + len - 1;

          D_DEBUG_AT( Genefx_TexTriangles,
                      "  -> y %4d, xl %d (%d), xr %d (%d), len %d (%d), x1 %d, x2 %d\n",
                      y, xl, xl >> COORD_SHIFT, xr, xr >> COORD_SHIFT, len, x2 - x1, x1, x2 );


          if (x1 <= clip->x2 && x2 >= clip->x1) {
               int SperD = (sr - sl) / len;
               int TperD = (tr - tl) / len;

               /*
                * Horizontal Clipping
                */
               if (x1 < clip->x1) {
                    sl += SperD * (clip->x1 - x1);
                    tl += TperD * (clip->x1 - x1);

                    x1 = clip->x1;
               }

               if (x2 > clip->x2)
                    x2 = clip->x2;

               D_DEBUG_AT( Genefx_TexTriangles,
                           "  ->         xl %d (%d), xr %d (%d), len %d (%d), x1 %d, x2 %d, sl %d, sr %d, tl %d, tr %d, SperD %d, TperD %d\n",
                           xl, xl >> COORD_SHIFT, xr, xr >> COORD_SHIFT, len, x2 - x1, x1, x2, sl, sr, tl, tr, SperD, TperD );

               /*
                * Pipeline Setup
                */
               gfxs->Dlen   = x2 - x1 + 1;
               gfxs->length = gfxs->Dlen;

               gfxs->SperD  = SperD;
               gfxs->TperD  = TperD;
               gfxs->s      = sl;
               gfxs->t      = tl;

               Genefx_Aop_xy( gfxs, x1, y );

               /*
                * Run Pipeline
                */
               RUN_PIPELINE();
          }
          else
               D_DEBUG_AT( Genefx_TexTriangles, "  -> y %4d, totally clipped line\n", y );

          /*
           * Increments
           */
          xl += dxl;
          xr += dxr;

          sl += dsl;
          sr += dsr;

          tl += dtl;
          tr += dtr;
     }
}

/**********************************************************************************************************************/

void
Genefx_TextureTriangles( CardState            *state,
                         DFBVertex            *vertices,
                         int                   num,
                         DFBTriangleFormation  formation,
                         const DFBRegion      *clip )
{
     /*
      * Convert vertices
      */
     GenefxVertexAffine genefx_vertices[num];

     for (int i=0; i<num; i++) {
          genefx_vertices[i].x = vertices[i].x;
          genefx_vertices[i].y = vertices[i].y;
          genefx_vertices[i].s = vertices[i].s * state->source->config.size.w * 0x10000;
          genefx_vertices[i].t = vertices[i].t * state->source->config.size.h * 0x10000;
     }

     // FIXME: Implement perspective correct mapping
     Genefx_TextureTrianglesAffine( state, genefx_vertices, num, formation, clip );
}

/**********************************************************************************************************************/

void
Genefx_TextureTrianglesAffine( CardState            *state,
                               GenefxVertexAffine   *vertices,
                               int                   num,
                               DFBTriangleFormation  formation,
                               const DFBRegion      *clip )
{
     GenefxState *gfxs = state->gfxs;

     D_ASSERT( gfxs != NULL );

     CHECK_PIPELINE();


     if (!Genefx_ABacc_prepare( gfxs, state->destination->config.size.w ))
          return;

     /*
      * Reset Bop to 0,0 as texture lookup accesses the whole buffer arbitrarily
      */
     Genefx_Bop_xy( gfxs, 0, 0 );


     /*
      * Render triangles
      */
     int index = 0;

     for (index=0; index<num;) {
          GenefxVertexAffine *v[3];

          /*
           * Triangle Fetch
           */

          if (index == 0) {
               v[0] = &vertices[index+0];
               v[1] = &vertices[index+1];
               v[2] = &vertices[index+2];

               index += 3;
          }
          else {
               switch (formation) {
                    case DTTF_LIST:
                         v[0] = &vertices[index+0];
                         v[1] = &vertices[index+1];
                         v[2] = &vertices[index+2];

                         index += 3;
                         break;

                    case DTTF_STRIP:
                         v[0] = &vertices[index-2];
                         v[1] = &vertices[index-1];
                         v[2] = &vertices[index+0];

                         index += 1;
                         break;

                    case DTTF_FAN:
                         v[0] = &vertices[0];
                         v[1] = &vertices[index-1];
                         v[2] = &vertices[index+0];

                         index += 1;
                         break;

                    default:
                         D_BUG( "unknown formation %d", formation );
                         Genefx_ABacc_flush( gfxs );
                         return;
               }
          }

          if (dfb_config->software_warn) {
               D_WARN( "TextureTriangles   (%d,%d %d,%d %d,%d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x, source [%4d,%4d] %6s",
                       v[0]->x, v[0]->y, v[1]->x, v[1]->y, v[2]->x, v[2]->y,
                       dfb_pixelformat_name(gfxs->dst_format), state->blittingflags,
                       state->color.a, state->color.r, state->color.g, state->color.b,
                       state->source->config.size.w, state->source->config.size.h,
                       dfb_pixelformat_name(gfxs->src_format) );
          }

          Genefx_TextureTriangleAffine( gfxs, v[0], v[1], v[2], clip );
     }

     Genefx_ABacc_flush( gfxs );
}

