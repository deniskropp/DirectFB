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

#include <config.h>

#include <string.h>

#include <core/fusion/shmalloc.h>
#include <core/fusion/arena.h>
#include <core/fusion/property.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>
#include <core/modules.h>
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

#include <misc/utf8.h>
#include <misc/mem.h>
#include <misc/util.h>

DEFINE_MODULE_DIRECTORY( dfb_graphics_drivers, "gfxdrivers",
                         DFB_GRAPHICS_DRIVER_ABI_VERSION );

typedef enum {
   GLF_INVALIDATE_STATE = 0x00000001,
   GLF_ENGINE_RESET     = 0x00000002
} GraphicsLockFlags;

/*
 * struct for graphics cards
 */
typedef struct {
     /* amount of usable video memory */
     unsigned int          videoram_length;

     char                 *module_name;
     
     GraphicsDriverInfo    driver_info;
     GraphicsDeviceInfo    device_info;
     void                 *device_data;

     FusionProperty        lock;
     GraphicsLockFlags     lock_flags;

     SurfaceManager       *surface_manager;

     FusionObjectPool     *surface_pool;
     FusionObjectPool     *palette_pool;

     /*
      * Points to the current state of the graphics card.
      */
     CardState            *state;
     int                   holder; /* Fusion ID of state owner. */
} GraphicsDeviceShared;

struct _GraphicsDevice {
     GraphicsDeviceShared      *shared;

     ModuleEntry               *module;
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
dfb_gfxcard_initialize( void *data_local, void *data_shared )
{
     DFBResult ret;
     int       videoram_length;

     DFB_ASSERT( card == NULL );

     card         = data_local;
     card->shared = data_shared;

     /* fill generic driver info */
     gGetDriverInfo( &card->shared->driver_info );

     /* fill generic device info */
     gGetDeviceInfo( &card->shared->device_info );

     /* Limit video ram length */
     videoram_length = dfb_system_videoram_length();
     if (videoram_length) {
          if (dfb_config->videoram_limit > 0 &&
              dfb_config->videoram_limit < videoram_length)
               card->shared->videoram_length = dfb_config->videoram_limit;
          else
               card->shared->videoram_length = videoram_length;
     }

     /* Build a list of available drivers. */
     dfb_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver */
     dfb_gfxcard_find_driver();
     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;
          
          card->driver_data = DFBCALLOC( 1,
                                         card->shared->driver_info.driver_data_size );

          card->device_data = card->shared->device_data =
               SHCALLOC( 1, card->shared->driver_info.device_data_size );
          
          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data );
          if (ret) {
               SHFREE( card->shared->device_data );
               SHFREE( card->shared->module_name );
               DFBFREE( card->driver_data );
               card = NULL;
               return ret;
          }

          ret = funcs->InitDevice( card, &card->shared->device_info,
                                   card->driver_data, card->device_data );
          if (ret) {
               funcs->CloseDriver( card, card->driver_data );
               SHFREE( card->shared->device_data );
               SHFREE( card->shared->module_name );
               DFBFREE( card->driver_data );
               card = NULL;
               return ret;
          }

          if (card->funcs.EngineReset)
               card->funcs.EngineReset( card->driver_data, card->device_data );
     }

     INITMSG( "DirectFB/GraphicsDevice: %s %s %d.%d (%s)\n",
              card->shared->device_info.vendor, card->shared->device_info.name,
              card->shared->driver_info.version.major,
              card->shared->driver_info.version.minor, card->shared->driver_info.vendor );

     if (dfb_config->software_only) {
          memset( &card->shared->device_info.caps, 0, sizeof(CardCapabilities) );

          if (card->funcs.CheckState) {
               card->funcs.CheckState = NULL;
               
               INITMSG( "DirectFB/GraphicsDevice: "
                        "acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else
          card->caps = card->shared->device_info.caps;
     
     card->shared->surface_manager = dfb_surfacemanager_create( card->shared->videoram_length,
                card->shared->device_info.limits.surface_byteoffset_alignment,
                card->shared->device_info.limits.surface_pixelpitch_alignment );

     card->shared->palette_pool = dfb_palette_pool_create();
     card->shared->surface_pool = dfb_surface_pool_create();

     fusion_property_init( &card->shared->lock );

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_join( void *data_local, void *data_shared )
{
     DFBResult ret;

     DFB_ASSERT( card == NULL );
     
     card         = data_local;
     card->shared = data_shared;
     
     /* Build a list of available drivers. */
     dfb_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     dfb_gfxcard_load_driver();
     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;
          
          card->driver_data = DFBCALLOC( 1,
                                         card->shared->driver_info.driver_data_size );

          card->device_data = card->shared->device_data;
          
          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data );
          if (ret) {
               DFBFREE( card->driver_data );
               card = NULL;
               return ret;
          }
     }
     else if (card->shared->module_name) {
          ERRORMSG( "DirectFB/core/gfxcard: "
                    "Could not load driver used by the running session!\n" );
          card = NULL;
          return DFB_UNSUPPORTED;
     }

     if (dfb_config->software_only && card->funcs.CheckState) {
          card->funcs.CheckState = NULL;
          
          INITMSG( "DirectFB/GraphicsDevice: "
                   "acceleration disabled (by 'no-hardware')\n" );
     }
     else
          card->caps = card->shared->device_info.caps;
     
     return DFB_OK;
}

static DFBResult
dfb_gfxcard_shutdown( bool emergency )
{
     GraphicsDeviceShared *shared;

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );

     shared = card->shared;

     DFB_ASSERT( shared->surface_pool != NULL );
     DFB_ASSERT( shared->palette_pool != NULL );
     DFB_ASSERT( shared->surface_manager != NULL );

     dfb_gfxcard_lock( true, true, false, false );

     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;
          
          funcs->CloseDevice( card, card->driver_data, card->device_data );
          funcs->CloseDriver( card, card->driver_data );

          dfb_module_unref( card->module );

          SHFREE( card->device_data );
          DFBFREE( card->driver_data );
     }

     fusion_object_pool_destroy( shared->surface_pool );
     fusion_object_pool_destroy( shared->palette_pool );

     dfb_surfacemanager_destroy( shared->surface_manager );

     fusion_property_destroy( &shared->lock );

     if (shared->module_name)
          SHFREE( shared->module_name );
     
     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_leave( bool emergency )
{
     DFB_ASSERT( card != NULL );

     dfb_gfxcard_sync();

     if (card->driver_funcs) {
          card->driver_funcs->CloseDriver( card, card->driver_data );

          dfb_module_unref( card->module );

          DFBFREE( card->driver_data );
     }

     card = NULL;

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_suspend()
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );

     dfb_gfxcard_lock( true, true, true, true );

     return dfb_surfacemanager_suspend( card->shared->surface_manager );
}

static DFBResult
dfb_gfxcard_resume()
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );

     dfb_gfxcard_unlock();

     return dfb_surfacemanager_resume( card->shared->surface_manager );
}

DFBResult
dfb_gfxcard_lock( bool wait, bool sync,
                  bool invalidate_state, bool engine_reset )
{
/*     DEBUGMSG("DirectFB/core/gfxcard: %s (%d, %d, %d, %d)\n",
              __FUNCTION__, wait, sync, invalidate_state, engine_reset);*/

     if (card && card->shared) {
          GraphicsDeviceShared *shared = card->shared;

          if (wait) {
               if (fusion_property_purchase( &shared->lock )) {
                    /*DEBUGMSG("DirectFB/core/gfxcard: %s FAILED.\n", __FUNCTION__);*/
                    return DFB_FAILURE;
               }
          }
          else {
               if (fusion_property_lease( &shared->lock )) {
                    /*DEBUGMSG("DirectFB/core/gfxcard: %s FAILED.\n", __FUNCTION__);*/
                    return DFB_FAILURE;
               }
          }

          /*DEBUGMSG("DirectFB/core/gfxcard: %s got lock...\n", __FUNCTION__);*/

          if (sync)
               dfb_gfxcard_sync();
          
          if (shared->lock_flags & GLF_INVALIDATE_STATE)
               shared->state = NULL;
          
          if ((shared->lock_flags & GLF_ENGINE_RESET) && card->funcs.EngineReset)
               card->funcs.EngineReset( card->driver_data, card->device_data );
          
          shared->lock_flags = 0;
          
          if (invalidate_state)
               shared->lock_flags |= GLF_INVALIDATE_STATE;
          
          if (engine_reset)
               shared->lock_flags |= GLF_ENGINE_RESET;
     }

     /*DEBUGMSG("DirectFB/core/gfxcard: %s OK.\n", __FUNCTION__);*/

     return DFB_OK;
}

void
dfb_gfxcard_unlock()
{
     if (card && card->shared) {
          fusion_property_cede( &card->shared->lock );
     }
}

void
dfb_gfxcard_holdup()
{
     if (card && card->shared) {
          fusion_property_holdup( &card->shared->lock );
     }
}

/*
 * This function returns non zero if acceleration is available
 * for the specific function using the given state.
 */
bool
dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( state != NULL );

     /*
      * If there's no CheckState function there's no acceleration at all.
      */
     if (!card->funcs.CheckState)
          return false;

     /* Destination may have been destroyed. */
     if (!state->destination)
          return false;
     
     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !state->source)
          return false;

     /*
      * If back_buffer policy is 'system only' there's no acceleration
      * available.
      */
     if (state->destination->back_buffer->policy == CSP_SYSTEMONLY) {
          /* Clear 'accelerated functions'. */
          state->accel = 0;

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
          state->accel &= 0x0000FFFF;

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
          card->funcs.CheckState( card->driver_data,
                                  card->device_data, state, accel );

          /* Add the function to 'checked functions'. */
          state->checked |= accel;
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     
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
     if (dfb_gfxcard_lock( false, false, false, false )) {
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
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( rect != NULL );
     
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
              gAquire( state, DFXL_FILLRECTANGLE ))
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( rect != NULL );
     
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
               if (gAquire( state, DFXL_DRAWLINE )) {
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( lines != NULL );
     DFB_ASSERT( num_lines > 0 );
     
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
          if (gAquire( state, DFXL_DRAWLINE )) {
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
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( tri != NULL );
     
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
               else if (gAquire( state, DFXL_FILLTRIANGLE )) {
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( rect != NULL );
     
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
          if (gAquire( state, DFXL_BLIT )) {
               dfb_clip_blit( &state->clip, rect, &dx, &dy );
               gBlit( state, rect, dx, dy );
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( rect != NULL );

     /* If called with an invalid rectangle, the algorithm goes into an
        infinite loop. This should never happen but it's safer to check. */
     DFB_ASSERT( rect->w >= 1 );
     DFB_ASSERT( rect->h >= 1 );

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
          if (gAquire( state, DFXL_BLIT )) {

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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( srect != NULL );
     DFB_ASSERT( drect != NULL );
     
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
          if (gAquire( state, DFXL_STRETCHBLIT )) {
               dfb_clip_stretchblit( &state->clip, srect, drect );
               gStretchBlit( state, srect, drect );
               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawstring( const unsigned char *text, int bytes,
                             int x, int y,
                             CoreFont *font, CardState *state )
{
     int            steps[bytes];
     unichar        chars[bytes];
     CoreGlyphData *glyphs[bytes];


     unichar prev = 0;

     int hw_clipping = (card->caps.flags & CCF_CLIPPING);
     int kern_x;
     int kern_y;
     int offset;
     int blit = 0;

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( text != NULL );
     DFB_ASSERT( bytes > 0 );
     DFB_ASSERT( font != NULL );
     
     dfb_state_lock( state );
     dfb_font_lock( font );

     /* preload glyphs to avoid deadlock */
     for (offset = 0; offset < bytes; offset += steps[offset]) {
          steps[offset] = dfb_utf8_skip[text[offset]];
          chars[offset] = dfb_utf8_get_char (&text[offset]);

          if (dfb_font_get_glyph_data (font, chars[offset],
                                       &glyphs[offset]) != DFB_OK)
               glyphs[offset] = NULL;
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

     /* set clip and color */
     font->state.clip         = state->clip;
     font->state.color        = state->color;
     font->state.color_index  = state->color_index;
     if (state->drawingflags & DSDRAW_BLEND)
          font->state.blittingflags |= DSBLIT_BLEND_COLORALPHA;
     else
          font->state.blittingflags &= ~DSBLIT_BLEND_COLORALPHA;
     font->state.modified |= SMF_CLIP | SMF_COLOR | SMF_BLITTING_FLAGS;

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
                         else if (gAquire( &font->state, DFXL_BLIT ))
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

     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     DFB_ASSERT( state != NULL );
     DFB_ASSERT( font != NULL );
     
     dfb_state_lock( state );
     dfb_font_lock( font );

     if (dfb_font_get_glyph_data (font, index, &data) != DFB_OK ||
         !data->width) {

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
     else if (gAquire( &font->state, DFXL_BLIT )) {

          dfb_clip_blit( &font->state.clip, &rect, &x, &y );
          gBlit( &font->state, &rect, x, y );
          gRelease( &font->state );
     }

     font->state.destination = NULL;
     
     dfb_font_unlock( font );
     dfb_state_unlock( state );
}

void dfb_gfxcard_sync()
{
     if (card && card->funcs.EngineSync)
          card->funcs.EngineSync( card->driver_data, card->device_data );
}

void dfb_gfxcard_flush_texture_cache()
{
     DFB_ASSERT( card != NULL );
     
     if (card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void dfb_gfxcard_after_set_var()
{
     DFB_ASSERT( card != NULL );
     
     if (card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

DFBResult
dfb_gfxcard_adjust_heap_offset( int offset )
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     
     return dfb_surfacemanager_adjust_heap_offset( card->shared->surface_manager, offset );
}

SurfaceManager *
dfb_gfxcard_surface_manager()
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     
     return card->shared->surface_manager;
}

FusionObjectPool *
dfb_gfxcard_surface_pool()
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     
     return card->shared->surface_pool;
}

FusionObjectPool *
dfb_gfxcard_palette_pool()
{
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     
     return card->shared->palette_pool;
}

void
dfb_gfxcard_get_capabilities( CardCapabilities *caps )
{
     DFB_ASSERT( card != NULL );
     
     *caps = card->caps;
}

int
dfb_gfxcard_reserve_memory( GraphicsDevice *device, unsigned int size )
{
     GraphicsDeviceShared *shared;

     DFB_ASSERT( device != NULL );
     DFB_ASSERT( device->shared != NULL );

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
     DFB_ASSERT( card != NULL );
     DFB_ASSERT( card->shared != NULL );
     
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
     FusionLink *link;

     if (dfb_system_type() != CORE_FBDEV)
          return;

     fusion_list_foreach (link, dfb_graphics_drivers.entries) {
          ModuleEntry *module = (ModuleEntry*) link;

          const GraphicsDriverFuncs *funcs = dfb_module_ref( module );

          if (!funcs)
               continue;

          if (!card->module && funcs->Probe( card )) {
               funcs->GetDriverInfo( card, &card->shared->driver_info );

               card->module       = module;
               card->driver_funcs = funcs;

               card->shared->module_name = SHSTRDUP( module->name );
          }
          else
               dfb_module_unref( module );
     }
}

/*
 * loads the driver module used by the session
 */
static void dfb_gfxcard_load_driver()
{
     FusionLink *link;

     if (dfb_system_type() != CORE_FBDEV)
          return;

     if (!card->shared->module_name)
          return;

     fusion_list_foreach (link, dfb_graphics_drivers.entries) {
          ModuleEntry *module = (ModuleEntry*) link;

          const GraphicsDriverFuncs *funcs = dfb_module_ref( module );

          if (!funcs)
               continue;

          if (!card->module &&
              !strcmp( module->name, card->shared->module_name ))
          {
               card->module       = module;
               card->driver_funcs = funcs;
          }
          else
               dfb_module_unref( module );
     }
}

