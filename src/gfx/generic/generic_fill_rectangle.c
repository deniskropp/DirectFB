/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

void gFillRectangle( CardState *state, DFBRectangle *rect )
{
     int          h;
     GenefxState *gfxs = state->gfxs;

     D_ASSERT( gfxs != NULL );

     if (dfb_config->software_warn) {
          D_WARN( "FillRectangle (%4d,%4d-%4dx%4d) %6s, flags 0x%08x, color 0x%02x%02x%02x%02x",
                  DFB_RECTANGLE_VALS(rect), dfb_pixelformat_name(gfxs->dst_format), state->drawingflags,
                  state->color.a, state->color.r, state->color.g, state->color.b );
     }

     D_ASSERT( state->clip.x1 <= rect->x );
     D_ASSERT( state->clip.y1 <= rect->y );
     D_ASSERT( state->clip.x2 >= (rect->x + rect->w - 1) );
     D_ASSERT( state->clip.y2 >= (rect->y + rect->h - 1) );

     CHECK_PIPELINE();

     if (!Genefx_ABacc_prepare( gfxs, rect->w ))
          return;

     gfxs->length = rect->w;

     Genefx_Aop_xy( gfxs, rect->x, rect->y );

     h = rect->h;
     while (h--) {
          RUN_PIPELINE();

          Genefx_Aop_next( gfxs );
     }

     Genefx_ABacc_flush( gfxs );
}

