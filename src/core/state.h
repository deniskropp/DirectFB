/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __STATE_H__
#define __STATE_H__

#include <pthread.h>

#include <directfb.h>
#include <core/coretypes.h>

typedef enum {
     SMF_DRAWING_FLAGS   = 0x00000001,
     SMF_BLITTING_FLAGS  = 0x00000002,
     SMF_CLIP            = 0x00000004,
     SMF_COLOR           = 0x00000008,
     SMF_SRC_BLEND       = 0x00000010,
     SMF_DST_BLEND       = 0x00000020,
     SMF_SRC_COLORKEY    = 0x00000040,
     SMF_DST_COLORKEY    = 0x00000080,

     SMF_DESTINATION     = 0x00000100,
     SMF_SOURCE          = 0x00000200,

     SMF_ALL             = 0x000003FF
} StateModificationFlags;

struct _CardState {
     StateModificationFlags  modified;     /* indicate which fields have been
                                              modified, these flags will be
                                              cleared by the gfx drivers */

     /* values forming the state for graphics operations */

     DFBSurfaceDrawingFlags  drawingflags; /* drawing flags */
     DFBSurfaceBlittingFlags blittingflags;/* blitting flags */

     DFBRegion               clip;         /* clipping rectangle */
     DFBColor                color;        /* color for drawing or modulation */
     unsigned int            color_index;  /* index to color in palette */
     DFBSurfaceBlendFunction src_blend;    /* blend function for source */
     DFBSurfaceBlendFunction dst_blend;    /* blend function for destination */
     __u32                   src_colorkey; /* colorkey for source */
     __u32                   dst_colorkey; /* colorkey for destination */

     CoreSurface             *destination; /* destination surface */
     CoreSurface             *source;      /* source surface */


     /* hardware abstraction and state handling helpers */

     DFBAccelerationMask     accel;        /* cache for checked commands
                                              if they are accelerated */
     DFBAccelerationMask     checked;      /* commands for which a state has
                                              already checked */
     DFBAccelerationMask     set;          /* commands for which a state has
                                              been set */

     int                     source_locked;/* when state is acquired for a blit
                                              mark that the source needs to be
                                              unlocked when state is released */

     pthread_mutex_t         lock;         /* lock for state handling */
};

int dfb_state_init( CardState *state );
void dfb_state_destroy( CardState *state );

void dfb_state_set_destination( CardState *state, CoreSurface *destination );
void dfb_state_set_source( CardState *state, CoreSurface *source );

static inline int dfb_state_lock( CardState *state )
{
     return pthread_mutex_lock( &state->lock );
}

static inline int dfb_state_unlock( CardState *state )
{
     return pthread_mutex_unlock( &state->lock );
}

#endif

