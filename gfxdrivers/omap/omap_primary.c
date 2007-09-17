/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <string.h>
#include <sys/ioctl.h>

#include <dfb_types.h>

#include <fbdev/fb.h>
#include "omapfb.h"

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/layers.h>

#include <fbdev/fbdev.h>

#include "omap.h"

/* */

static DFBResult
omapUpdateRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreSurface           *surface,
                  const DFBRegion       *update,
                  CoreSurfaceBufferLock *lock )
{
     FBDev *dfb_fbdev = dfb_system_data();
     struct omapfb_update_window window;
     DFBRectangle rect;

     dfb_rectangle_from_region( &rect, update );

     D_DEBUG_AT( omap, "Update rectangle %d %d %dx%d\n",
                 rect.x, rect.y, rect.w, rect.h );

     if (rect.x & 1)
          rect.w++;
     if (rect.y & 1)
          rect.h++;

     window.x = rect.x & ~1;
     window.y = rect.y & ~1;

     window.width  = (rect.w + 1) & ~1;
     window.height = (rect.h + 1) & ~1;

     window.out_x = window.x;
     window.out_y = window.y;

     window.out_width  = window.width;
     window.out_height = window.height;

     window.format = 0;

     D_DEBUG_AT( omap, "Update window %d %d %dx%d\n",
                 window.x, window.y, window.width, window.height );

     if (ioctl( dfb_fbdev->fd, OMAPFB_UPDATE_WINDOW, &window ))
          D_DEBUG_AT( omap, "Can't update window -> %s\n", strerror( errno ) );

     return DFB_OK;
}

DisplayLayerFuncs omapPrimaryLayerFuncs = {
     UpdateRegion:       omapUpdateRegion,
};
