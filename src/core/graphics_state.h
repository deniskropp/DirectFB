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

#ifndef __GRAPHICS_STATE_H__
#define __GRAPHICS_STATE_H__

#include <directfb.h>

#include <fusion/object.h>

#include <core/state.h>


struct __DFB_CoreGraphicsState {
     FusionObject             object;
     int                      magic;

     FusionCall               call;

     CardState                state;

     /*
      * New state logic
      */
     StateModificationFlags   modified;

     bool                     hw;
     DFBAccelerationMask      sw;

     StateModificationFlags   mod_sw;
};

typedef enum {
     CGSNF_NONE     = 0x00000000
} CoreGraphicsStateNotificationFlags;

typedef struct {
     CoreGraphicsStateNotificationFlags  flags;
} CoreGraphicsStateNotification;


DFBResult dfb_graphics_state_create( CoreDFB            *core,
                                     CoreGraphicsState **ret_state );


/*
 * Creates a pool of graphics state objects.
 */
FusionObjectPool *dfb_graphics_state_pool_create( const FusionWorld *world );

/*
 * Generates dfb_graphics_state_ref(), dfb_graphics_state_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreGraphicsState, dfb_graphics_state )


#endif

