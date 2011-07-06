/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__WINDOWSTACK_H__
#define __CORE__WINDOWSTACK_H__

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <fusion/lock.h>

/*
 * allocates a WindowStack, initializes it, registers it for input events
 */
CoreWindowStack *dfb_windowstack_create ( CoreLayerContext *context );

void             dfb_windowstack_detach_devices( CoreWindowStack  *stack );

void             dfb_windowstack_destroy( CoreWindowStack  *stack );

void             dfb_windowstack_resize ( CoreWindowStack  *stack,
                                          int               width,
                                          int               height,
                                          int               rotation );

DirectResult     dfb_windowstack_lock   ( CoreWindowStack  *stack );

DirectResult     dfb_windowstack_unlock ( CoreWindowStack  *stack );





/*
 * repaints all window on a window stack
 */
DFBResult dfb_windowstack_repaint_all( CoreWindowStack *stack );

/*
 * background handling
 */
DFBResult dfb_windowstack_set_background_mode ( CoreWindowStack               *stack,
                                                DFBDisplayLayerBackgroundMode  mode );

DFBResult dfb_windowstack_set_background_image( CoreWindowStack               *stack,
                                                CoreSurface                   *image );

DFBResult dfb_windowstack_set_background_color( CoreWindowStack               *stack,
                                                const DFBColor                *color );

DFBResult dfb_windowstack_set_background_color_index( CoreWindowStack         *stack,
                                                      int                      index );


/*
 * cursor control
 */
DFBResult dfb_windowstack_cursor_enable( CoreDFB         *core,
                                         CoreWindowStack *stack,
                                         bool             enable );

DFBResult dfb_windowstack_cursor_set_shape( CoreWindowStack *stack,
                                            CoreSurface     *shape,
                                            int              hot_x,
                                            int              hot_y );

DFBResult dfb_windowstack_cursor_set_opacity( CoreWindowStack *stack,
                                              u8               opacity );

DFBResult dfb_windowstack_cursor_set_acceleration( CoreWindowStack *stack,
                                                   int              numerator,
                                                   int              denominator,
                                                   int              threshold );

DFBResult dfb_windowstack_cursor_warp( CoreWindowStack *stack,
                                       int              x,
                                       int              y );


DFBResult dfb_windowstack_get_cursor_position (CoreWindowStack *stack,
                                               int             *x,
                                               int             *y);


#endif
