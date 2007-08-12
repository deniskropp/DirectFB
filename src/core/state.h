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

#ifndef __CORE__STATE_H__
#define __CORE__STATE_H__

#include <pthread.h>

#include <directfb.h>

#include <direct/serial.h>

#include <fusion/reactor.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/gfxcard.h>
#include <core/surface_buffer.h>

#include <gfx/generic/generic.h>

#include <misc/conf.h>
#include <misc/util.h>


typedef enum {
     SMF_DRAWING_FLAGS     = 0x00000001,
     SMF_BLITTING_FLAGS    = 0x00000002,
     SMF_CLIP              = 0x00000004,
     SMF_COLOR             = 0x00000008,
     SMF_SRC_BLEND         = 0x00000010,
     SMF_DST_BLEND         = 0x00000020,
     SMF_SRC_COLORKEY      = 0x00000040,
     SMF_DST_COLORKEY      = 0x00000080,

     SMF_DESTINATION       = 0x00000100,
     SMF_SOURCE            = 0x00000200,

     SMF_INDEX_TRANSLATION = 0x00001000,

     SMF_RENDER_OPTIONS    = 0x00010000,

     SMF_ALL               = 0x000113FF
} StateModificationFlags;

typedef enum {
     CSF_NONE            = 0x00000000,

     CSF_DESTINATION     = 0x00000001,  /* destination is set using dfb_state_set_destination() */
     CSF_SOURCE          = 0x00000002,  /* source is set using dfb_state_set_source() */

     CSF_SOURCE_LOCKED   = 0x00000010,  /* source surface is locked */

     CSF_DRAWING         = 0x00010000,  /* something has been rendered with this state,
                                           this is cleared by flushing the state, e.g. upon flip */

     CSF_ALL             = 0x00010013
} CardStateFlags;

struct _CardState {
     int                      magic;

     CoreDFB                 *core;
     CoreGraphicsDevice      *device;
     FusionID                 fusion_id;

     pthread_mutex_t          lock;          /* lock for state handling */

     CardStateFlags           flags;

     StateModificationFlags   modified;      /* indicate which fields have been
                                                modified, these flags will be
                                                cleared by the gfx drivers */
     StateModificationFlags   mod_hw;

     /* values forming the state for graphics operations */

     DFBSurfaceDrawingFlags   drawingflags;  /* drawing flags */
     DFBSurfaceBlittingFlags  blittingflags; /* blitting flags */

     DFBRegion                clip;          /* clipping rectangle */
     DFBColor                 color;         /* color for drawing or modulation */
     unsigned int             color_index;   /* index to color in palette */
     DFBSurfaceBlendFunction  src_blend;     /* blend function for source */
     DFBSurfaceBlendFunction  dst_blend;     /* blend function for destination */
     u32                      src_colorkey;  /* colorkey for source */
     u32                      dst_colorkey;  /* colorkey for destination */

     CoreSurface             *destination;   /* destination surface */
     CoreSurface             *source;        /* source surface */

     DirectSerial             dst_serial;    /* last destination surface serial */
     DirectSerial             src_serial;    /* last source surface serial */

     int                     *index_translation;
     int                      num_translation;


     /* hardware abstraction and state handling helpers */

     DFBAccelerationMask      accel;         /* remember checked commands if they are accelerated */
     DFBAccelerationMask      checked;       /* commands for which a state has been checked */
     DFBAccelerationMask      set;           /* commands for which a state is valid */
     DFBAccelerationMask      disabled;      /* commands which are disabled temporarily */

     CoreGraphicsSerial       serial;        /* hardware serial of the last operation */

     /* from/to buffers */

     CoreSurfaceBufferRole    from;          /* usually CSBR_FRONT */
     CoreSurfaceBufferRole    to;            /* usually CSBR_BACK */

     /* read/write locks during operation */
     
     CoreSurfaceBufferLock    dst;
     CoreSurfaceBufferLock    src;

     /* software driver */

     GenefxState             *gfxs;

     DFBSurfaceRenderOptions  render_options;
};

int  dfb_state_init( CardState *state, CoreDFB *core );
void dfb_state_destroy( CardState *state );

void dfb_state_set_destination( CardState *state, CoreSurface *destination );
void dfb_state_set_source( CardState *state, CoreSurface *source );

void dfb_state_update( CardState *state, bool update_source );

DFBResult dfb_state_set_index_translation( CardState *state,
                                           const int *indices,
                                           int        num_indices );

static inline void
dfb_state_get_serial( const CardState *state, CoreGraphicsSerial *ret_serial )
{
     D_ASSERT( state != NULL );
     D_ASSERT( ret_serial != NULL );

     *ret_serial = state->serial;
}

static inline void
dfb_state_lock( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     pthread_mutex_lock( &state->lock );
}

static inline void
dfb_state_start_drawing( CardState *state, CoreGraphicsDevice *device )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( device != NULL );
     D_ASSERT( state->destination != NULL );

     if (dfb_config->startstop) {
          if (state->flags & CSF_DRAWING)
               D_ASSERT( state->device == device );
          else {
               dfb_gfxcard_start_drawing( device, state );

               state->flags |= CSF_DRAWING;
               state->device = device;
          }
     }
}

static inline void
dfb_state_stop_drawing( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination != NULL );

     if (dfb_config->startstop) {
          if (state->flags & CSF_DRAWING) {
               D_ASSERT( state->device != NULL );
     
               dfb_gfxcard_stop_drawing( state->device, state );
     
               state->flags &= ~CSF_DRAWING;
               state->device = NULL;
          }
          else
               D_ASSERT( state->device == NULL );
     }
}

static inline void
dfb_state_unlock( CardState *state )
{
     D_MAGIC_ASSERT( state, CardState );

     DFB_REGION_ASSERT( &state->clip );

     pthread_mutex_unlock( &state->lock );
}


#define _dfb_state_set_checked(member,flag,state,value)     \
do {                                                        \
     D_MAGIC_ASSERT( state, CardState );                    \
                                                            \
     if ((value) != (state)->member) {                      \
          (state)->member    = (value);                     \
          (state)->modified |= SMF_##flag;                  \
     }                                                      \
} while (0)


#define dfb_state_set_blitting_flags(state,flags) _dfb_state_set_checked( blittingflags,  \
                                                                          BLITTING_FLAGS, \
                                                                          state, flags )

#define dfb_state_set_drawing_flags(state,flags)  _dfb_state_set_checked( drawingflags,   \
                                                                          DRAWING_FLAGS,  \
                                                                          state, flags )

#define dfb_state_set_color_index(state,index)    _dfb_state_set_checked( color_index,    \
                                                                          COLOR,          \
                                                                          state, index )

#define dfb_state_set_src_blend(state,blend)      _dfb_state_set_checked( src_blend,      \
                                                                          SRC_BLEND,      \
                                                                          state, blend )

#define dfb_state_set_dst_blend(state,blend)      _dfb_state_set_checked( dst_blend,      \
                                                                          DST_BLEND,      \
                                                                          state, blend )

#define dfb_state_set_src_colorkey(state,key)     _dfb_state_set_checked( src_colorkey,   \
                                                                          SRC_COLORKEY,   \
                                                                          state, key )

#define dfb_state_set_dst_colorkey(state,key)     _dfb_state_set_checked( dst_colorkey,   \
                                                                          DST_COLORKEY,   \
                                                                          state, key )


#define dfb_state_set_render_options(state,opts)  _dfb_state_set_checked( render_options, \
                                                                          RENDER_OPTIONS, \
                                                                          state, opts )

static inline void dfb_state_set_clip( CardState *state, const DFBRegion *clip )
{
     D_MAGIC_ASSERT( state, CardState );
     DFB_REGION_ASSERT( clip );

     if (! DFB_REGION_EQUAL( state->clip, *clip )) {
          state->clip     = *clip;
          state->modified = (StateModificationFlags)( state->modified | SMF_CLIP );
     }
}

static inline void dfb_state_set_color( CardState *state, const DFBColor *color )
{
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( color != NULL );

     if (! DFB_COLOR_EQUAL( state->color, *color )) {
          state->color    = *color;
          state->modified = (StateModificationFlags)( state->modified | SMF_COLOR );
     }
}

/*
 * Multifunctional color configuration function.
 *
 * Always tries to set both color and index.
 *
 * If color index is -1, color is used and searched in palette of destination surface if present.
 * If color index is valid the color is looked up in palette if present.
 */
void dfb_state_set_color_or_index( CardState      *state,
                                   const DFBColor *color,
                                   int             index );

#endif

