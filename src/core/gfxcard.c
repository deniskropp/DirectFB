/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/arena.h>
#include <fusion/property.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>
#include <core/gfxcard.h>
#include <core/fonts.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/system.h>

#include <gfx/generic/generic.h>
#include <gfx/clip.h>
#include <gfx/util.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>
#include <direct/tree.h>
#include <direct/utf8.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

DEFINE_MODULE_DIRECTORY( dfb_graphics_drivers, "gfxdrivers", DFB_GRAPHICS_DRIVER_ABI_VERSION );

/*
 * struct for graphics cards
 */
typedef struct {
     /* amount of usable video memory */
     unsigned int             videoram_length;

     char                    *module_name;

     GraphicsDriverInfo       driver_info;
     GraphicsDeviceInfo       device_info;
     void                    *device_data;

     FusionProperty           lock;
     GraphicsDeviceLockFlags  lock_flags;

     SurfaceManager          *surface_manager;

     /*
      * Points to the current state of the graphics card.
      */
     CardState               *state;
     int                      holder; /* Fusion ID of state owner. */
} GraphicsDeviceShared;

struct _GraphicsDevice {
     GraphicsDeviceShared      *shared;

     DirectModuleEntry         *module;
     const GraphicsDriverFuncs *driver_funcs;

     void                      *driver_data;
     void                      *device_data; /* copy of shared->device_data */

     CardCapabilities           caps;        /* local caps */

     GraphicsDeviceFuncs        funcs;
};


static GraphicsDevice *card = NULL;


static void dfb_gfxcard_find_driver();
static void dfb_gfxcard_load_driver();

DFB_CORE_PART( gfxcard, sizeof(GraphicsDevice), sizeof(GraphicsDeviceShared) )


/** public **/

static DFBResult
dfb_gfxcard_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult             ret;
     int                   videoram_length;
     GraphicsDeviceShared *shared;

     D_ASSERT( card == NULL );

     card         = data_local;
     card->shared = data_shared;

     shared = card->shared;

     /* fill generic driver info */
     gGetDriverInfo( &shared->driver_info );

     /* fill generic device info */
     gGetDeviceInfo( &shared->device_info );

     /* Limit video ram length */
     videoram_length = dfb_system_videoram_length();
     if (videoram_length) {
          if (dfb_config->videoram_limit > 0 &&
              dfb_config->videoram_limit < videoram_length)
               shared->videoram_length = dfb_config->videoram_limit;
          else
               shared->videoram_length = videoram_length;
     }

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver */
     dfb_gfxcard_find_driver();
     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;

          card->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          card->device_data   =
          shared->device_data = SHCALLOC( 1, shared->driver_info.device_data_size );

          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data );
          if (ret) {
               SHFREE( shared->device_data );
               SHFREE( shared->module_name );
               D_FREE( card->driver_data );
               card = NULL;
               return ret;
          }

          ret = funcs->InitDevice( card, &shared->device_info,
                                   card->driver_data, card->device_data );
          if (ret) {
               funcs->CloseDriver( card, card->driver_data );
               SHFREE( shared->device_data );
               SHFREE( shared->module_name );
               D_FREE( card->driver_data );
               card = NULL;
               return ret;
          }

          if (card->funcs.EngineReset)
               card->funcs.EngineReset( card->driver_data, card->device_data );
     }

     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n",
             shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major,
             shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          memset( &shared->device_info.caps, 0, sizeof(CardCapabilities) );

          if (card->funcs.CheckState) {
               card->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else
          card->caps = shared->device_info.caps;

     shared->surface_manager = dfb_surfacemanager_create( shared->videoram_length,
                                                          &shared->device_info.limits );

     fusion_property_init( &shared->lock );

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_join( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult             ret;
     GraphicsDeviceShared *shared;

     D_ASSERT( card == NULL );

     card         = data_local;
     card->shared = data_shared;

     shared = card->shared;

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     dfb_gfxcard_load_driver();
     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;

          card->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          card->device_data = shared->device_data;

          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data );
          if (ret) {
               D_FREE( card->driver_data );
               card = NULL;
               return ret;
          }
     }
     else if (shared->module_name) {
          D_ERROR( "DirectFB/Graphics: Could not load driver used by the running session!\n" );
          card = NULL;
          return DFB_UNSUPPORTED;
     }

     if (dfb_config->software_only && card->funcs.CheckState) {
          card->funcs.CheckState = NULL;

          D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
     }
     else
          card->caps = shared->device_info.caps;

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_shutdown( CoreDFB *core, bool emergency )
{
     GraphicsDeviceShared *shared;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;

     D_ASSERT( shared->surface_manager != NULL );

     dfb_gfxcard_lock( GDLF_SYNC );

     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;

          funcs->CloseDevice( card, card->driver_data, card->device_data );
          funcs->CloseDriver( card, card->driver_data );

          direct_module_unref( card->module );

          SHFREE( card->device_data );
          D_FREE( card->driver_data );
     }

     dfb_surfacemanager_destroy( shared->surface_manager );

     fusion_property_destroy( &shared->lock );

     if (shared->module_name)
          SHFREE( shared->module_name );

     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_leave( CoreDFB *core, bool emergency )
{
     D_ASSERT( card != NULL );

     dfb_gfxcard_sync();

     if (card->driver_funcs) {
          card->driver_funcs->CloseDriver( card, card->driver_data );

          direct_module_unref( card->module );

          D_FREE( card->driver_data );
     }

     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_suspend( CoreDFB *core )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     return dfb_surfacemanager_suspend( card->shared->surface_manager );
}

static DFBResult
dfb_gfxcard_resume( CoreDFB *core )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     dfb_gfxcard_unlock();

     return dfb_surfacemanager_resume( card->shared->surface_manager );
}

DFBResult
dfb_gfxcard_lock( GraphicsDeviceLockFlags flags )
{
     GraphicsDeviceShared *shared;
     GraphicsDeviceFuncs  *funcs;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     shared = card->shared;
     funcs  = &card->funcs;

     if ( ((flags & GDLF_WAIT) ?
           fusion_property_purchase( &shared->lock ) :
           fusion_property_lease( &shared->lock )) )
          return DFB_FAILURE;

     if ((flags & GDLF_SYNC) && funcs->EngineSync)
          funcs->EngineSync( card->driver_data, card->device_data );

     if (shared->lock_flags & GDLF_INVALIDATE)
          shared->state = NULL;

     if ((shared->lock_flags & GDLF_RESET) && funcs->EngineReset)
          funcs->EngineReset( card->driver_data, card->device_data );

     shared->lock_flags = flags;

     return DFB_OK;
}

void
dfb_gfxcard_unlock()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     fusion_property_cede( &card->shared->lock );
}

void
dfb_gfxcard_holdup()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     fusion_property_holdup( &card->shared->lock );
}

/*
 * This function returns non zero if acceleration is available
 * for the specific function using the given state.
 */
bool
dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     D_ASSERT( card != NULL );
     D_ASSERT( state != NULL );

     /*
      * If there's no CheckState function there's no acceleration at all.
      */
     if (!card->funcs.CheckState)
          return false;

     /* Destination may have been destroyed. */
     if (!state->destination) {
          D_BUG( "no destination" );
          return false;
     }

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !state->source) {
          D_BUG( "no source" );
          return false;
     }

     /*
      * If back_buffer policy is 'system only' there's no acceleration
      * available.
      */
     if (state->destination->back_buffer->policy == CSP_SYSTEMONLY) {
          /* Clear 'accelerated functions'. */
          state->accel   = 0;
          state->checked = DFXL_ALL;

          /* Return immediately. */
          return false;
     }

     /*
      * If the front buffer policy of the source is 'system only'
      * no accelerated blitting is available.
      */
     if (state->source &&
         state->source->front_buffer->policy == CSP_SYSTEMONLY)
     {
          /* Clear 'accelerated blitting functions'. */
          state->accel   &= 0x0000FFFF;
          state->checked |= 0xFFFF0000;

          /* Return if a blitting function was requested. */
          if (DFB_BLITTING_FUNCTION( accel ))
               return false;
     }

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND)) {
          /* ...force rechecking for all functions. */
          state->checked = 0;
     }
     else {
          /* If source or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS)) {
               /* ...force rechecking for all blitting functions. */
               state->checked &= 0x0000FFFF;
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->checked &= 0xFFFF0000;
          }
     }

     /* If the function needs to be checked... */
     if (!(state->checked & accel)) {
          /* Unset function bit. */
          state->accel &= ~accel;

          /* Call driver to (re)set the bit if the function is supported. */
          card->funcs.CheckState( card->driver_data, card->device_data, state, accel );

          /* Add the function to 'checked functions'. */
          state->checked |= accel;

          /* Add additional functions the driver might have checked, too. */
          state->checked |= state->accel;
     }

     /* Return whether the function bit is set. */
     return (state->accel & accel);
}

/*
 * This function returns non zero after successful locking the surface(s)
 * for access by hardware. Propagate state changes to driver.
 */
static bool
dfb_gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel )
{
     GraphicsDeviceShared *shared;
     DFBSurfaceLockFlags   lock_flags;
     int                   fid = fusion_id();

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );

     /* Destination may have been destroyed. */
     if (!state->destination)
          return false;

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !state->source)
          return false;

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel ))
          lock_flags = (state->blittingflags & ( DSBLIT_BLEND_ALPHACHANNEL |
                                                 DSBLIT_BLEND_COLORALPHA   |
                                                 DSBLIT_DST_COLORKEY ) ?
                        DSLF_READ | DSLF_WRITE : DSLF_WRITE) | CSLF_FORCE;
     else
          lock_flags = (state->drawingflags & ( DSDRAW_BLEND |
                                               DSDRAW_DST_COLORKEY ) ?
                        DSLF_READ | DSLF_WRITE : DSLF_WRITE) | CSLF_FORCE;

     shared = card->shared;

     /* lock surface manager */
     dfb_surfacemanager_lock( shared->surface_manager );

     /* if blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading */
          if (dfb_surface_hardware_lock( state->source, DSLF_READ, 1 )) {
               dfb_surfacemanager_unlock( shared->surface_manager );
               return false;
          }

          state->source_locked = 1;
     }
     else
          state->source_locked = 0;

     /* lock destination */
     if (dfb_surface_hardware_lock( state->destination, lock_flags, 0 )) {
          if (state->source_locked)
               dfb_surface_unlock( state->source, 1 );

          dfb_surfacemanager_unlock( shared->surface_manager );
          return false;
     }

     /* unlock surface manager */
     dfb_surfacemanager_unlock( shared->surface_manager );

     /*
      * Make sure that state setting with subsequent command execution
      * isn't done by two processes simultaneously.
      *
      * This will timeout if the hardware is locked by another party with
      * the first argument being true (e.g. DRI).
      */
     if (dfb_gfxcard_lock( GDLF_NONE )) {
          dfb_surface_unlock( state->destination, false );

          if (state->source_locked)
               dfb_surface_unlock( state->source, true );

          return false;
     }

     /* if we are switching to another state... */
     if (state != shared->state || fid != shared->holder) {
          /* ...set all modification bits and clear 'set functions' */
          state->modified |= SMF_ALL;
          state->set       = 0;

          shared->state  = state;
          shared->holder = fid;
     }

     /*
      * If function hasn't been set or state is modified
      * call the driver function to propagate the state changes.
      */
     if (state->modified || !(state->set & accel))
          card->funcs.SetState( card->driver_data, card->device_data,
                                &card->funcs, state, accel );

     return true;
}

/*
 * Unlock destination and possibly the source.
 */
static void
dfb_gfxcard_state_release( CardState *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );

     /* start command processing if not already running */
     if (card->funcs.EmitCommands)
          card->funcs.EmitCommands( card->driver_data, card->device_data );

     /* destination always gets locked during acquisition */
     dfb_surface_unlock( state->destination, false );

     /* if source got locked this value is true */
     if (state->source_locked)
          dfb_surface_unlock( state->source, true );

     /* allow others to use the hardware */
     dfb_gfxcard_unlock();
}

/** DRAWING FUNCTIONS **/

void dfb_gfxcard_fillrectangle( DFBRectangle *rect, CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( rect != NULL );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Check for acceleration and setup execution. */
     if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
     {
          /*
           * Either hardware has clipping support or the software clipping
           * routine returned that there's something to do.
           */
          if ((card->caps.flags & CCF_CLIPPING) ||
              dfb_clip_rectangle( &state->clip, rect ))
          {
               /*
                * Now everything is prepared for execution of the
                * FillRectangle driver function.
                */
               hw = card->funcs.FillRectangle( card->driver_data,
                                               card->device_data, rect );
          }

          /* Release after state acquisition. */
          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          /*
           * Otherwise use the software clipping routine and execute the
           * software fallback if the rectangle isn't completely clipped.
           */
          if (dfb_clip_rectangle( &state->clip, rect ) &&
              gAcquire( state, DFXL_FILLRECTANGLE ))
          {
               gFillRectangle( state, rect );
               gRelease( state );
          }
     }

     /* Unlock after execution. */
     dfb_state_unlock( state );
}

void dfb_gfxcard_drawrectangle( DFBRectangle *rect, CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( rect != NULL );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_DRAWRECTANGLE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_DRAWRECTANGLE ))
     {
          if (card->caps.flags & CCF_CLIPPING  ||
              dfb_clip_rectangle( &state->clip, rect ))
          {
               /* FIXME: correct clipping like below */
               hw = card->funcs.DrawRectangle( card->driver_data,
                                               card->device_data, rect );
          }

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          DFBEdgeFlags edges = dfb_clip_edges (&state->clip, rect);

          if (edges) {
               if (gAcquire( state, DFXL_DRAWLINE )) {
                    DFBRegion line;

                    if (edges & DFEF_LEFT) {
                         line.x1 = line.x2 = rect->x;
                         line.y1 = rect->y + (edges & DFEF_TOP ? 1 : 0);
                         line.y2 = rect->y + rect->h - 1;
                         gDrawLine( state, &line );
                    }
                    if (edges & DFEF_TOP) {
                         line.x1 = rect->x;
                         line.x2 = rect->x + rect->w - (edges & DFEF_RIGHT ? 2 : 1);
                         line.y1 = line.y2 = rect->y;
                         gDrawLine( state, &line );
                    }
                    if (edges & DFEF_RIGHT) {
                         line.x1 = line.x2 = rect->x + rect->w - 1;
                         line.y1 = rect->y;
                         line.y2 = rect->y + rect->h - (edges & DFEF_BOTTOM ? 2 : 1);
                         gDrawLine( state, &line );
                    }
                    if (edges & DFEF_BOTTOM) {
                         line.x1 = rect->x + (edges & DFEF_LEFT ? 1 : 0);
                         line.x2 = rect->x + rect->w - 1;
                         line.y1 = line.y2 = rect->y + rect->h - 1;
                         gDrawLine( state, &line );
                    }

                    gRelease (state);
               }
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state )
{
     int i;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( lines != NULL );
     D_ASSERT( num_lines > 0 );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_DRAWLINE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_DRAWLINE ))
     {
          if (card->caps.flags & CCF_CLIPPING)
               for (i=0; i<num_lines; i++)
                    card->funcs.DrawLine( card->driver_data,
                                          card->device_data, &lines[i] );
          else
               for (i=0; i<num_lines; i++) {
                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         card->funcs.DrawLine( card->driver_data,
                                               card->device_data, &lines[i] );
               }

          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAcquire( state, DFXL_DRAWLINE )) {
               for (i=0; i<num_lines; i++) {
                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         gDrawLine( state, &lines[i] );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}


typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)         \
     do {                                  \
          int dx = xe - xs;                \
          int dy = ye - ys;                \
          dda.xi = xs;                     \
          if (dy != 0) {                   \
               dda.mi = dx / dy;           \
               dda.mf = 2*(dx % dy);       \
               dda.xf = -dy;               \
               dda._2dy = 2 * dy;          \
               if (dda.mf < 0) {           \
                    dda.mf += 2 * ABS(dy); \
                    dda.mi--;              \
               }                           \
          }                                \
     } while (0)


#define INC_DDA(dda)                       \
     do {                                  \
          dda.xi += dda.mi;                \
          dda.xf += dda.mf;                \
          if (dda.xf > 0) {                \
               dda.xi++;                   \
               dda.xf -= dda._2dy;         \
          }                                \
     } while (0)


/**
 *  render a triangle using two parallel DDA's
 */
static void
fill_tri( DFBTriangle *tri, CardState *state, bool accelerated )
{
     int y, yend;
     DDA dda1, dda2;
     int clip_x1 = state->clip.x1;
     int clip_x2 = state->clip.x2;

     y = tri->y1;
     yend = tri->y3;

     if (y < state->clip.y1)
          y = state->clip.y1;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
     SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);

     while (y < yend) {
          DFBRectangle rect;

          if (y == tri->y2) {
               if (tri->y2 == tri->y3)
                    return;
               SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
          }

          rect.w = ABS(dda1.xi - dda2.xi);
          rect.x = MIN(dda1.xi, dda2.xi);

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x;

          if (rect.w > 0) {
               if (clip_x1 > rect.x)
                    rect.x = clip_x1;
               rect.y = y;
               rect.h = 1;

               if (accelerated)
                    card->funcs.FillRectangle( card->driver_data,
                                               card->device_data, &rect );
               else
                    gFillRectangle( state, &rect );
          }

          INC_DDA(dda1);
          INC_DDA(dda2);

          y++;
     }
}


void dfb_gfxcard_filltriangle( DFBTriangle *tri, CardState *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( tri != NULL );

     dfb_state_lock( state );

     /* if hardware has clipping try directly accelerated triangle filling */
     if ((card->caps.flags & CCF_CLIPPING) &&
          dfb_gfxcard_state_check( state, DFXL_FILLTRIANGLE ) &&
          dfb_gfxcard_state_acquire( state, DFXL_FILLTRIANGLE ))
     {
          card->funcs.FillTriangle( card->driver_data,
                                    card->device_data, tri );
          dfb_gfxcard_state_release( state );
     }
     else {
          /* otherwise use the spanline rasterizer (fill_tri)
             and fill the triangle using a rectangle for each spanline */

          dfb_sort_triangle( tri );

          if (tri->y3 - tri->y1 > 0) {
               /* try hardware accelerated rectangle filling */
               if (! (card->caps.flags & CCF_NOTRIEMU) &&
                   dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
                   dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
               {
                    fill_tri( tri, state, true );

                    dfb_gfxcard_state_release( state );
               }
               else if (gAcquire( state, DFXL_FILLTRIANGLE )) {
                    fill_tri( tri, state, false );

                    gRelease( state );
               }
          }
     }

     dfb_state_unlock( state );
}


void dfb_gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( rect != NULL );

     dfb_state_lock( state );

     if (!dfb_clip_blit_precheck( &state->clip, rect->w, rect->h, dx, dy )) {
          /* no work at all */
          dfb_state_unlock( state );
          return;
     }

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT ))
     {
          if (!(card->caps.flags & CCF_CLIPPING))
               dfb_clip_blit( &state->clip, rect, &dx, &dy );

          hw = card->funcs.Blit( card->driver_data, card->device_data,
                                 rect, dx, dy );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_BLIT )) {
               dfb_clip_blit( &state->clip, rect, &dx, &dy );
               gBlit( state, rect, dx, dy );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_batchblit( DFBRectangle *rects, DFBPoint *points,
                            int num, CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( num > 0 );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT ))
     {
          int i;

          for (i=0; i<num; i++) {
               if (dfb_clip_blit_precheck( &state->clip,
                                           rects[i].w, rects[i].h,
                                           points[i].x, points[i].y ))
               {
                    if (!(card->caps.flags & CCF_CLIPPING))
                         dfb_clip_blit( &state->clip, &rects[i],
                                        &points[i].x, &points[i].y );

                    hw = card->funcs.Blit( card->driver_data, card->device_data,
                                           &rects[i], points[i].x, points[i].y );
               }
          }

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_BLIT )) {
               int i;

               for (i=0; i<num; i++) {
                    if (dfb_clip_blit_precheck( &state->clip,
                                                rects[i].w, rects[i].h,
                                                points[i].x, points[i].y ))
                    {
                         dfb_clip_blit( &state->clip, &rects[i],
                                        &points[i].x, &points[i].y );

                         gBlit( state, &rects[i], points[i].x, points[i].y );
                    }
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_tileblit( DFBRectangle *rect, int dx, int dy, int w, int h,
                           CardState *state )
{
     int          x, y;
     int          odx;
     DFBRectangle srect;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( rect != NULL );

     /* If called with an invalid rectangle, the algorithm goes into an
        infinite loop. This should never happen but it's safer to check. */
     D_ASSERT( rect->w >= 1 );
     D_ASSERT( rect->h >= 1 );

     odx = dx;

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT )) {

          for (; dy < h; dy += rect->h) {
               for (dx = odx; dx < w; dx += rect->w) {

                    if (!dfb_clip_blit_precheck( &state->clip,
                                                 rect->w, rect->h, dx, dy ))
                         continue;

                    x = dx;
                    y = dy;
                    srect = *rect;

                    if (!(card->caps.flags & CCF_CLIPPING))
                         dfb_clip_blit( &state->clip, &srect, &x, &y );

                    card->funcs.Blit( card->driver_data, card->device_data,
                                      &srect, x, y );
               }
          }
          dfb_gfxcard_state_release( state );
     }
     else {
          if (gAcquire( state, DFXL_BLIT )) {

               for (; dy < h; dy += rect->h) {
                    for (dx = odx; dx < w; dx += rect->w) {

                         if (!dfb_clip_blit_precheck( &state->clip,
                                                      rect->w, rect->h,
                                                      dx, dy ))
                              continue;

                         x = dx;
                         y = dy;
                         srect = *rect;

                         dfb_clip_blit( &state->clip, &srect, &x, &y );

                         gBlit( state, &srect, x, y );
                    }
               }
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                              CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( srect != NULL );
     D_ASSERT( drect != NULL );

     dfb_state_lock( state );

     if (!dfb_clip_blit_precheck( &state->clip, drect->w, drect->h,
                                  drect->x, drect->y ))
     {
          dfb_state_unlock( state );
          return;
     }

     if (dfb_gfxcard_state_check( state, DFXL_STRETCHBLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_STRETCHBLIT ))
     {
          if (!(card->caps.flags & CCF_CLIPPING))
               dfb_clip_stretchblit( &state->clip, srect, drect );

          hw = card->funcs.StretchBlit( card->driver_data,
                                        card->device_data, srect, drect );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_STRETCHBLIT )) {
               dfb_clip_stretchblit( &state->clip, srect, drect );
               gStretchBlit( state, srect, drect );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_texture_triangles( DFBVertex *vertices, int num,
                                    DFBTriangleFormation formation,
                                    CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( vertices != NULL );
     D_ASSERT( num >= 3 );
     D_ASSERT( state != NULL );

     dfb_state_lock( state );

     if ((card->caps.flags & CCF_CLIPPING) &&
         dfb_gfxcard_state_check( state, DFXL_TEXTRIANGLES ) &&
         dfb_gfxcard_state_acquire( state, DFXL_TEXTRIANGLES ))
     {
          hw = card->funcs.TextureTriangles( card->driver_data,
                                             card->device_data,
                                             vertices, num, formation );

          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
          if (gAcquire( state, DFXL_TEXTRIANGLES )) {
               //dfb_clip_stretchblit( &state->clip, srect, drect );
               //gStretchBlit( state, srect, drect );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawstring( const __u8 *text, int bytes,
                             int x, int y,
                             CoreFont *font, CardState *state )
{
     int                      steps[bytes];
     unichar                  chars[bytes];
     CoreGlyphData           *glyphs[bytes];
     DFBSurfaceBlittingFlags  blittingflags;

     unichar prev = 0;

     int hw_clipping = (card->caps.flags & CCF_CLIPPING);
     int kern_x;
     int kern_y;
     int offset;
     int blit = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( text != NULL );
     D_ASSERT( bytes > 0 );
     D_ASSERT( font != NULL );

     dfb_state_lock( state );
     dfb_font_lock( font );

     /* Prepare glyphs data. */
     for (offset = 0; offset < bytes; offset += steps[offset]) {
          unsigned int c = text[offset];

          if (c < 128) {
               steps[offset] = 1;
               chars[offset] = c;
          }
          else {
               steps[offset] = DIRECT_UTF8_SKIP(c);
               chars[offset] = DIRECT_UTF8_GET_CHAR( &text[offset] );
          }

          if (c >= 32 && c < 128)
               glyphs[offset] = font->glyph_infos->fast_keys[c-32];
          else
               glyphs[offset] = direct_tree_lookup( font->glyph_infos, (void *)chars[offset] );

          if (!glyphs[offset]) {
               if (dfb_font_get_glyph_data (font, chars[offset], &glyphs[offset]) != DFB_OK)
                    glyphs[offset] = NULL;
          }
     }

     /* simple prechecks */
     if (x > state->clip.x2 || y > state->clip.y2 ||
         y + font->ascender - font->descender <= state->clip.y1) {
          dfb_font_unlock( font );
          dfb_state_unlock( state );
          return;
     }

     /* set destination */
     font->state.destination  = state->destination;
     font->state.modified    |= SMF_DESTINATION;

     /* set clip */
     if (!DFB_REGION_EQUAL(font->state.clip, state->clip)) {
          font->state.clip      = state->clip;
          font->state.modified |= SMF_CLIP;
     }

     /* set color */
     if (!DFB_COLOR_EQUAL(font->state.color, state->color) ||
         font->state.color_index != state->color_index)
     {
          font->state.color        = state->color;
          font->state.color_index  = state->color_index;
          font->state.modified    |= SMF_COLOR;
     }

     /* set blitting flags */
     if (state->drawingflags & DSDRAW_BLEND)
          blittingflags = font->state.blittingflags | DSBLIT_BLEND_COLORALPHA;
     else
          blittingflags = font->state.blittingflags & ~DSBLIT_BLEND_COLORALPHA;

     if (font->state.blittingflags != blittingflags) {
          font->state.blittingflags  = blittingflags;
          font->state.modified      |= SMF_BLITTING_FLAGS;
     }

     /* blit glyphs */
     for (offset = 0; offset < bytes; offset += steps[offset]) {

          unichar current = chars[offset];

          if (glyphs[offset]) {
               CoreGlyphData *data = glyphs[offset];

               if (prev && font->GetKerning &&
                   (* font->GetKerning) (font,
                                         prev, current,
                                         &kern_x, &kern_y) == DFB_OK) {
                    x += kern_x;
                    y += kern_y;
               }

               if (data->width) {
                    int xx = x + data->left;
                    int yy = y + data->top;
                    DFBRectangle rect = { data->start, 0,
                                          data->width, data->height };

                    if (font->state.source != data->surface || !blit) {
                         switch (blit) {
                              case 1:
                                   dfb_gfxcard_state_release( &font->state );
                                   break;
                              case 2:
                                   gRelease( &font->state );
                                   break;
                              default:
                                   break;
                         }
                         dfb_state_set_source( &font->state, data->surface );

                         if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ) &&
                             dfb_gfxcard_state_acquire( &font->state, DFXL_BLIT ))
                              blit = 1;
                         else if (gAcquire( &font->state, DFXL_BLIT ))
                              blit = 2;
                         else
                              blit = 0;
                    }

                    if (dfb_clip_blit_precheck( &font->state.clip,
                                                rect.w, rect.h, xx, yy )) {
                         switch (blit) {
                              case 1:
                                   if (!hw_clipping)
                                        dfb_clip_blit( &font->state.clip,
                                                       &rect, &xx, &yy );
                                   card->funcs.Blit( card->driver_data,
                                                     card->device_data,
                                                     &rect, xx, yy );
                                   break;
                              case 2:
                                   dfb_clip_blit( &font->state.clip,
                                                  &rect, &xx, &yy );
                                   gBlit( &font->state, &rect, xx, yy );
                                   break;
                              default:
                                   break;
                         }
                    }
               }
               x += data->advance;
               prev = current;
          }
     }

     switch (blit) {
          case 1:
               dfb_gfxcard_state_release( &font->state );
               break;
          case 2:
               gRelease( &font->state );
               break;
          default:
               break;
     }

     font->state.destination = NULL;

     dfb_font_unlock( font );
     dfb_state_unlock( state );
}

void dfb_gfxcard_drawglyph( unichar index, int x, int y,
                            CoreFont *font, CardState *state )
{
     CoreGlyphData *data;
     DFBRectangle   rect;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( font != NULL );

     dfb_state_lock( state );
     dfb_font_lock( font );

     if (dfb_font_get_glyph_data (font, index, &data) != DFB_OK || !data->width) {
          dfb_font_unlock( font );
          dfb_state_unlock( state );
          return;
     }

     x += data->left;
     y += data->top;

     if (! dfb_clip_blit_precheck( &state->clip,
                                   data->width, data->height, x, y )) {
          dfb_font_unlock( font );
          dfb_state_unlock( state );
          return;
     }

     /* set destination */
     font->state.destination  = state->destination;
     font->state.modified    |= SMF_DESTINATION;

     /* set clip and color */
     font->state.clip        = state->clip;
     font->state.color       = state->color;
     font->state.color_index = state->color_index;

     if (state->drawingflags & DSDRAW_BLEND)
          font->state.blittingflags |= DSBLIT_BLEND_COLORALPHA;
     else
          font->state.blittingflags &= ~DSBLIT_BLEND_COLORALPHA;

     font->state.modified |= SMF_CLIP | SMF_COLOR | SMF_BLITTING_FLAGS;

     dfb_state_set_source( &font->state, data->surface );

     rect.x = data->start;
     rect.y = 0;
     rect.w = data->width;
     rect.h = data->height;

     if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( &font->state, DFXL_BLIT )) {

          if (!(card->caps.flags & CCF_CLIPPING))
               dfb_clip_blit( &font->state.clip, &rect, &x, &y );

          card->funcs.Blit( card->driver_data, card->device_data, &rect, x, y);
          dfb_gfxcard_state_release( &font->state );
     }
     else if (gAcquire( &font->state, DFXL_BLIT )) {

          dfb_clip_blit( &font->state.clip, &rect, &x, &y );
          gBlit( &font->state, &rect, x, y );
          gRelease( &font->state );
     }

     font->state.destination = NULL;

     dfb_font_unlock( font );
     dfb_state_unlock( state );
}

void dfb_gfxcard_drawstring_check_state( CoreFont *font, CardState *state )
{
     CoreGlyphData *data;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( state != NULL );
     D_ASSERT( font != NULL );

     dfb_state_lock( state );
     dfb_font_lock( font );

     if (dfb_font_get_glyph_data (font, 'a', &data)) {
          dfb_font_unlock( font );
          dfb_state_unlock( state );
          return;
     }

     /* set destination */
     font->state.destination  = state->destination;
     font->state.modified    |= SMF_DESTINATION;

     /* set blitting flags */
     if (state->drawingflags & DSDRAW_BLEND)
          dfb_state_set_blitting_flags( &font->state,
                                        font->state.blittingflags | DSBLIT_BLEND_COLORALPHA );
     else
          dfb_state_set_blitting_flags( &font->state,
                                        font->state.blittingflags & ~DSBLIT_BLEND_COLORALPHA );

     /* set the source */
     dfb_state_set_source( &font->state, data->surface );

     /* check for blitting and report */
     if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ))
          state->accel |= DFXL_DRAWSTRING;
     else
          state->accel &= ~DFXL_DRAWSTRING;

     font->state.destination = NULL;

     dfb_font_unlock( font );
     dfb_state_unlock( state );
}

void dfb_gfxcard_sync()
{
     D_ASSERT( card != NULL );

     if (card && card->funcs.EngineSync)
          card->funcs.EngineSync( card->driver_data, card->device_data );
}

void dfb_gfxcard_flush_texture_cache()
{
     D_ASSERT( card != NULL );

     if (card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void dfb_gfxcard_after_set_var()
{
     D_ASSERT( card != NULL );

     if (card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

DFBResult
dfb_gfxcard_adjust_heap_offset( int offset )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return dfb_surfacemanager_adjust_heap_offset( card->shared->surface_manager, offset );
}

SurfaceManager *
dfb_gfxcard_surface_manager()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return card->shared->surface_manager;
}

void
dfb_gfxcard_get_capabilities( CardCapabilities *caps )
{
     D_ASSERT( card != NULL );

     *caps = card->caps;
}

int
dfb_gfxcard_reserve_memory( GraphicsDevice *device, unsigned int size )
{
     GraphicsDeviceShared *shared;

     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     shared = device->shared;

     if (shared->surface_manager)
          return -1;

     if (shared->videoram_length < size)
          return -1;

     shared->videoram_length -= size;

     return shared->videoram_length;
}

unsigned int
dfb_gfxcard_memory_length()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return card->shared->videoram_length;
}

volatile void *
dfb_gfxcard_map_mmio( GraphicsDevice *device,
                      unsigned int    offset,
                      int             length )
{
     return dfb_system_map_mmio( offset, length );
}

void
dfb_gfxcard_unmap_mmio( GraphicsDevice *device,
                        volatile void  *addr,
                        int             length )
{
     dfb_system_unmap_mmio( addr, length );
}

int
dfb_gfxcard_get_accelerator( GraphicsDevice *device )
{
     return dfb_system_get_accelerator();
}

unsigned long
dfb_gfxcard_memory_physical( GraphicsDevice *device,
                             unsigned int    offset )
{
     return dfb_system_video_memory_physical( offset );
}

void *
dfb_gfxcard_memory_virtual( GraphicsDevice *device,
                            unsigned int    offset )
{
     return dfb_system_video_memory_virtual( offset );
}

/** internal **/

/*
 * loads/probes/unloads one driver module after another until a suitable
 * driver is found and returns its symlinked functions
 */
static void dfb_gfxcard_find_driver()
{
     DirectLink *link;

     if (dfb_system_type() != CORE_FBDEV)
          return;

     direct_list_foreach (link, dfb_graphics_drivers.entries) {
          DirectModuleEntry *module = (DirectModuleEntry*) link;

          const GraphicsDriverFuncs *funcs = direct_module_ref( module );

          if (!funcs)
               continue;

          if (!card->module && funcs->Probe( card )) {
               funcs->GetDriverInfo( card, &card->shared->driver_info );

               card->module       = module;
               card->driver_funcs = funcs;

               card->shared->module_name = SHSTRDUP( module->name );
          }
          else
               direct_module_unref( module );
     }
}

/*
 * loads the driver module used by the session
 */
static void dfb_gfxcard_load_driver()
{
     DirectLink *link;

     if (dfb_system_type() != CORE_FBDEV)
          return;

     if (!card->shared->module_name)
          return;

     direct_list_foreach (link, dfb_graphics_drivers.entries) {
          DirectModuleEntry *module = (DirectModuleEntry*) link;

          const GraphicsDriverFuncs *funcs = direct_module_ref( module );

          if (!funcs)
               continue;

          if (!card->module &&
              !strcmp( module->name, card->shared->module_name ))
          {
               card->module       = module;
               card->driver_funcs = funcs;
          }
          else
               direct_module_unref( module );
     }
}

