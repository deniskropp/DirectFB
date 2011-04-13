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

void gDrawLine( CardState *state, DFBRegion *line )
{
     GenefxState *gfxs = state->gfxs;

     int i,dx,dy,sdy,dxabs,dyabs,x,y,px,py;

     D_ASSERT( gfxs != NULL );

     CHECK_PIPELINE();

     /* the horizontal distance of the line */
     dx = line->x2 - line->x1;
     dxabs = abs(dx);

     if (!Genefx_ABacc_prepare( gfxs, dxabs ))
          return;

     /* the vertical distance of the line */
     dy = line->y2 - line->y1;
     dyabs = abs(dy);

     if (!dx || !dy) {              /* draw horizontal/vertical line */
          DFBRectangle rect = {
               MIN (line->x1, line->x2),
               MIN (line->y1, line->y2),
               dxabs + 1, dyabs + 1};

          gFillRectangle( state, &rect );
          return;
     }

     if (dfb_config->software_warn) {
          D_WARN( "DrawLine      (%4d,%4d-%4d,%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x",
                  DFB_RECTANGLE_VALS_FROM_REGION(line), dfb_pixelformat_name(gfxs->dst_format), state->drawingflags,
                  state->color.a, state->color.r, state->color.g, state->color.b );
     }

     sdy = SIGN(dy) * SIGN(dx);
     x = dyabs >> 1;
     y = dxabs >> 1;

     if (dx > 0) {
          px  = line->x1;
          py  = line->y1;
     }
     else {
          px  = line->x2;
          py  = line->y2;
     }

     if (dxabs >= dyabs) { /* the line is more horizontal than vertical */

          for (i=0, gfxs->length=1; i<dxabs; i++, gfxs->length++) {
               y += dyabs;
               if (y >= dxabs) {
                    Genefx_Aop_xy( gfxs, px, py );
                    RUN_PIPELINE();
                    px += gfxs->length;
                    gfxs->length = 0;
                    y -= dxabs;
                    py += sdy;
               }
          }
          Genefx_Aop_xy( gfxs, px, py );
          RUN_PIPELINE();
     }
     else { /* the line is more vertical than horizontal */

          gfxs->length = 1;
          Genefx_Aop_xy( gfxs, px, py );
          RUN_PIPELINE();

          for (i=0; i<dyabs; i++) {
               x += dxabs;
               if (x >= dyabs) {
                    x -= dyabs;
                    px++;
               }
               py += sdy;

               Genefx_Aop_xy( gfxs, px, py );
               RUN_PIPELINE();
          }
     }

     Genefx_ABacc_flush( gfxs );
}

