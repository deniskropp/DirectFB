/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

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

#include <limits.h>
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

#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>
#include <direct/utf8.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

DEFINE_MODULE_DIRECTORY( dfb_graphics_drivers, "gfxdrivers", DFB_GRAPHICS_DRIVER_ABI_VERSION );


D_DEBUG_DOMAIN( Core_Graphics, "Core/Graphics", "DirectFB Graphics Core" );

/*
 * struct for graphics cards
 */
typedef struct {
     /* amount of usable memory */
     unsigned int             videoram_length;
     unsigned int             auxram_length;
     unsigned int             auxram_offset;

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
     FusionID                 holder; /* Fusion ID of state owner. */
} GraphicsDeviceShared;

struct _GraphicsDevice {
     GraphicsDeviceShared      *shared;

     DirectModuleEntry         *module;
     const GraphicsDriverFuncs *driver_funcs;

     void                      *driver_data;
     void                      *device_data; /* copy of shared->device_data */

     CardCapabilities           caps;        /* local caps */
     CardLimitations            limits;      /* local limits */

     GraphicsDeviceFuncs        funcs;
};


static GraphicsDevice *card = NULL;

/* Hook for registering additional screen(s) and layer(s) in app or lib initializing DirectFB. */
void (*__DFB_CoreRegisterHook)( CoreDFB *core, CoreGraphicsDevice *device, void *ctx ) = NULL;
void  *__DFB_CoreRegisterHookCtx = NULL;


static void dfb_gfxcard_find_driver( CoreDFB *core );
static void dfb_gfxcard_load_driver();

DFB_CORE_PART( gfxcard, sizeof(GraphicsDevice), sizeof(GraphicsDeviceShared) )


/** public **/

static DFBResult
dfb_gfxcard_initialize( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult             ret;
     int                   videoram_length;
     int                   auxram_length;
     GraphicsDeviceShared *shared;
     FusionSHMPoolShared  *pool = dfb_core_shmpool( core );

     D_ASSERT( card == NULL );

     card         = data_local;
     card->shared = data_shared;

     shared = card->shared;

     /* fill generic driver info */
     gGetDriverInfo( &shared->driver_info );

     /* fill generic device info */
     gGetDeviceInfo( &shared->device_info );

     if (!shared->device_info.limits.dst_max.w)
          shared->device_info.limits.dst_max.w = INT_MAX;

     if (!shared->device_info.limits.dst_max.h)
          shared->device_info.limits.dst_max.h = INT_MAX;

     /* Limit video ram length */
     videoram_length = dfb_system_videoram_length();
     if (videoram_length) {
          if (dfb_config->videoram_limit > 0 &&
              dfb_config->videoram_limit < videoram_length)
               shared->videoram_length = dfb_config->videoram_limit;
          else
               shared->videoram_length = videoram_length;
     }

     /* Limit auxiliary memory length (currently only AGP) */
     auxram_length = dfb_system_auxram_length();
     if (auxram_length) {
          if (dfb_config->agpmem_limit > 0 &&
              dfb_config->agpmem_limit < auxram_length)
               shared->auxram_length = dfb_config->agpmem_limit;
          else
               shared->auxram_length = auxram_length;
     }

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver */
     if (dfb_system_caps() & CSCAPS_ACCELERATION)
          dfb_gfxcard_find_driver( core );

     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;

          card->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          card->device_data   =
          shared->device_data = SHCALLOC( pool, 1, shared->driver_info.device_data_size );

          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data, core );
          if (ret) {
               SHFREE( pool, shared->device_data );
               SHFREE( pool, shared->module_name );
               D_FREE( card->driver_data );
               card = NULL;
               return ret;
          }

          ret = funcs->InitDevice( card, &shared->device_info,
                                   card->driver_data, card->device_data );
          if (ret) {
               funcs->CloseDriver( card, card->driver_data );
               SHFREE( pool, shared->device_data );
               SHFREE( pool, shared->module_name );
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
          if (card->funcs.CheckState) {
               card->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          card->caps   = shared->device_info.caps;
          card->limits = shared->device_info.limits;
     }

     shared->surface_manager = dfb_surfacemanager_create( core,
                                                          &shared->device_info.limits );

     ret = dfb_surfacemanager_add_heap( shared->surface_manager,
                                        CSS_VIDEO, 0, shared->videoram_length );
     /* FIXME: what to do in case of failure? */

     if (shared->auxram_length && card->caps.flags & CCF_AUXMEMORY) {
          ret = dfb_surfacemanager_add_heap( shared->surface_manager, CSS_AUXILIARY,
                                             shared->auxram_offset, shared->auxram_length );
          /* FIXME: what to do in case of failure? */
     }

     fusion_property_init( &shared->lock, dfb_core_world(core) );

     if (__DFB_CoreRegisterHook)
         __DFB_CoreRegisterHook( core, card, __DFB_CoreRegisterHookCtx );

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_join( CoreDFB *core, void *data_local, void *data_shared )
{
     DFBResult             ret;
     GraphicsDeviceShared *shared;
     GraphicsDriverInfo    driver_info;

     D_ASSERT( card == NULL );

     card         = data_local;
     card->shared = data_shared;

     shared = card->shared;

     /* Initialize software rasterizer. */
     gGetDriverInfo( &driver_info );

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     if (dfb_system_caps() & CSCAPS_ACCELERATION)
          dfb_gfxcard_load_driver();

     if (card->driver_funcs) {
          const GraphicsDriverFuncs *funcs = card->driver_funcs;

          card->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          card->device_data = shared->device_data;

          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data, core );
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


     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n",
             shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major,
             shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          if (card->funcs.CheckState) {
               card->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          card->caps   = shared->device_info.caps;
          card->limits = shared->device_info.limits;
     }

     return DFB_OK;
}

static DFBResult
dfb_gfxcard_shutdown( CoreDFB *core, bool emergency )
{
     GraphicsDeviceShared *shared;
     FusionSHMPoolShared  *pool = dfb_core_shmpool( core );

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

          SHFREE( pool, card->device_data );
          D_FREE( card->driver_data );
     }

     dfb_surfacemanager_destroy( shared->surface_manager );

     fusion_property_destroy( &shared->lock );

     if (shared->module_name)
          SHFREE( pool, shared->module_name );

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
     DFBResult             ret;
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

     if ((flags & GDLF_SYNC) && funcs->EngineSync) {
          ret = funcs->EngineSync( card->driver_data, card->device_data );
          if (ret) {
               if (funcs->EngineReset)
                    funcs->EngineReset( card->driver_data, card->device_data );

               shared->state = NULL;

               fusion_property_cede( &shared->lock );

               return ret;
          }
     }

     if ((shared->lock_flags & GDLF_RESET) && funcs->EngineReset)
          funcs->EngineReset( card->driver_data, card->device_data );

     if (shared->lock_flags & GDLF_INVALIDATE)
          shared->state = NULL;

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
     int cx2, cy2;

     D_ASSERT( card != NULL );
     D_MAGIC_ASSERT( state, CardState );

     D_ASSERT( state->clip.x2 >= state->clip.x1 );
     D_ASSERT( state->clip.y2 >= state->clip.y1 );
     D_ASSERT( state->clip.x1 >= 0 );
     D_ASSERT( state->clip.y1 >= 0 );

     if (state->clip.x1 < 0) {
          state->clip.x1   = 0;
          state->modified |= SMF_CLIP;
     }

     if (state->clip.y1 < 0) {
          state->clip.y1   = 0;
          state->modified |= SMF_CLIP;
     }

     /* Destination may have been destroyed. */
     if (!state->destination) {
          D_BUG( "no destination" );
          return false;
     }

     D_ASSUME( state->clip.x2 < state->destination->width );
     D_ASSUME( state->clip.y2 < state->destination->height );

     cx2 = state->destination->width  - 1;
     cy2 = state->destination->height - 1;

     if (state->clip.x2 > cx2) {
          state->clip.x2 = cx2;

          if (state->clip.x1 > cx2)
               state->clip.x1 = cx2;

          state->modified |= SMF_CLIP;
     }

     if (state->clip.y2 > cy2) {
          state->clip.y2 = cy2;

          if (state->clip.y1 > cy2)
               state->clip.y1 = cy2;

          state->modified |= SMF_CLIP;
     }

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !state->source) {
          D_BUG( "no source" );
          return false;
     }

     /*
      * If there's no CheckState function there's no acceleration at all.
      */
     if (!card->funcs.CheckState)
          return false;

     /*
      * Check if this function has been disabled temporarily.
      */
     if (state->disabled & accel)
          return false;

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
     if (!(card->caps.flags & CCF_READSYSMEM) && state->source &&
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
     return !!(state->accel & accel);
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

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );

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
          if (dfb_surface_hardware_lock( state->source, DSLF_READ, true )) {
               dfb_surfacemanager_unlock( shared->surface_manager );
               return false;
          }

          state->flags |= CSF_SOURCE_LOCKED;
     }

     /* lock destination */
     if (dfb_surface_hardware_lock( state->destination, lock_flags, false )) {
          if (state->flags & CSF_SOURCE_LOCKED) {
               dfb_surface_unlock( state->source, true );
               state->flags &= ~CSF_SOURCE_LOCKED;
          }

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

          if (state->flags & CSF_SOURCE_LOCKED) {
               dfb_surface_unlock( state->source, true );
               state->flags &= ~CSF_SOURCE_LOCKED;
          }

          return false;
     }

     /* if we are switching to another state... */
     if (state != shared->state || state->fusion_id != shared->holder) {
          /* ...set all modification bits and clear 'set functions' */
          state->modified |= SMF_ALL;
          state->set       = 0;

          shared->state  = state;
          shared->holder = state->fusion_id;
     }

     dfb_state_update( state, state->flags & CSF_SOURCE_LOCKED );

     /*
      * If function hasn't been set or state is modified,
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
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->destination != NULL );
     D_ASSERT( state->destination->back_buffer != NULL );

     /* start command processing if not already running */
     if (card->funcs.EmitCommands)
          card->funcs.EmitCommands( card->driver_data, card->device_data );

     /* Store the serial of the operation. */
     if (card->funcs.GetSerial) {
          card->funcs.GetSerial( card->driver_data, card->device_data, &state->serial );

          state->destination->back_buffer->video.serial = state->serial;
     }

     /* allow others to use the hardware */
     dfb_gfxcard_unlock();

     /* destination always gets locked during acquisition */
     dfb_surface_unlock( state->destination, false );

     /* if source got locked this value is true */
     if (state->flags & CSF_SOURCE_LOCKED) {
          dfb_surface_unlock( state->source, true );

          state->flags &= ~CSF_SOURCE_LOCKED;
     }
}

/** DRAWING FUNCTIONS **/

void
dfb_gfxcard_fillrectangles( const DFBRectangle *rects, int num, CardState *state )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( num > 0 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     while (num > 0) {
          if (dfb_rectangle_region_intersects( rects, &state->clip ))
               break;

          rects++;
          num--;
     }

     if (num > 0) {
          int          i = 0;
          DFBRectangle rect;

          /* Check for acceleration and setup execution. */
          if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
              dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
          {
               /*
                * Now everything is prepared for execution of the
                * FillRectangle driver function.
                */
               for (; i<num; i++) {
                    if (!dfb_rectangle_region_intersects( &rects[i], &state->clip ))
                         continue;

                    rect = rects[i];

                    if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h) {
                         dfb_clip_rectangle( &state->clip, &rect );

                         if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h)
                              break;
                    }
                    else if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ))
                         dfb_clip_rectangle( &state->clip, &rect );

                    if (!card->funcs.FillRectangle( card->driver_data,
                                                    card->device_data, &rect ))
                         break;
               }

               /* Release after state acquisition. */
               dfb_gfxcard_state_release( state );
          }

          if (i < num) {
               /* Use software fallback. */
               if (gAcquire( state, DFXL_FILLRECTANGLE )) {
                    for (; i<num; i++) {
                         rect = rects[i];

                         if (dfb_clip_rectangle( &state->clip, &rect ))
                              gFillRectangle( state, &rect );
                    }

                    gRelease( state );
               }
          }
     }

     /* Unlock after execution. */
     dfb_state_unlock( state );
}

static void
build_clipped_rectangle_outlines( DFBRectangle    *rect,
                                  const DFBRegion *clip,
                                  DFBRectangle    *ret_outlines,
                                  int             *ret_num )
{
     DFBEdgeFlags edges = dfb_clip_edges( clip, rect );
     int          t     = (edges & DFEF_TOP ? 1 : 0);
     int          tb    = t + (edges & DFEF_BOTTOM ? 1 : 0);
     int          num   = 0;

     DFB_RECTANGLE_ASSERT( rect );

     D_ASSERT( ret_outlines != NULL );
     D_ASSERT( ret_num != NULL );

     if (edges & DFEF_TOP) {
          DFBRectangle *out = &ret_outlines[num++];

          out->x = rect->x;
          out->y = rect->y;
          out->w = rect->w;
          out->h = 1;
     }

     if (rect->h > t) {
          if (edges & DFEF_BOTTOM) {
               DFBRectangle *out = &ret_outlines[num++];

               out->x = rect->x;
               out->y = rect->y + rect->h - 1;
               out->w = rect->w;
               out->h = 1;
          }

          if (rect->h > tb) {
               if (edges & DFEF_LEFT) {
                    DFBRectangle *out = &ret_outlines[num++];

                    out->x = rect->x;
                    out->y = rect->y + t;
                    out->w = 1;
                    out->h = rect->h - tb;
               }

               if (rect->w > 1 || !(edges & DFEF_LEFT)) {
                    if (edges & DFEF_RIGHT) {
                         DFBRectangle *out = &ret_outlines[num++];

                         out->x = rect->x + rect->w - 1;
                         out->y = rect->y + t;
                         out->w = 1;
                         out->h = rect->h - tb;
                    }
               }
          }
     }

     *ret_num = num;
}

void dfb_gfxcard_drawrectangle( DFBRectangle *rect, CardState *state )
{
     DFBRectangle rects[4];
     bool         hw = false;
     int          i = 0, num = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rect != NULL );

     dfb_state_lock( state );

     if (!dfb_rectangle_region_intersects( rect, &state->clip )) {
          dfb_state_unlock( state );
          return;
     }

     if ((card->caps.flags & CCF_CLIPPING) || !dfb_clip_needed( &state->clip, rect )) {
          if (rect->w <= card->limits.dst_max.w && rect->h <= card->limits.dst_max.h &&
              dfb_gfxcard_state_check( state, DFXL_DRAWRECTANGLE ) &&
              dfb_gfxcard_state_acquire( state, DFXL_DRAWRECTANGLE ))
          {
               hw = card->funcs.DrawRectangle( card->driver_data,
                                               card->device_data, rect );

               dfb_gfxcard_state_release( state );
          }
     }

     if (!hw) {
          build_clipped_rectangle_outlines( rect, &state->clip, rects, &num );

          if (!num) {
               dfb_state_unlock( state );
               return;
          }

          if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
              dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
          {
               for (; i<num; i++) {
                    hw = rects[i].w <= card->limits.dst_max.w && rects[i].h <= card->limits.dst_max.h
                         && card->funcs.FillRectangle( card->driver_data,
                                                       card->device_data, &rects[i] );
                    if (!hw)
                         break;
               }

               dfb_gfxcard_state_release( state );
          }
     }

     if (!hw && gAcquire( state, DFXL_FILLRECTANGLE )) {
          for (; i<num; i++)
               gFillRectangle( state, &rects[i] );

          gRelease (state);
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state )
{
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( lines != NULL );
     D_ASSERT( num_lines > 0 );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_DRAWLINE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_DRAWLINE ))
     {
          for (; i<num_lines; i++) {
               if (!(card->caps.flags & CCF_CLIPPING)) {
                    if (!dfb_clip_line( &state->clip, &lines[i] ))
                         continue;
               }

               if (!card->funcs.DrawLine( card->driver_data,
                                          card->device_data, &lines[i] ))
                    break;
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num_lines) {
          if (gAcquire( state, DFXL_DRAWLINE )) {
               for (; i<num_lines; i++) {
                    if (dfb_clip_line( &state->clip, &lines[i] ))
                         gDrawLine( state, &lines[i] );
               }

               gRelease( state );
          }
     }

     dfb_state_unlock( state );
}

void dfb_gfxcard_fillspans( int y, DFBSpan *spans, int num_spans, CardState *state )
{
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( spans != NULL );
     D_ASSERT( num_spans > 0 );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_FILLRECTANGLE ) &&
         dfb_gfxcard_state_acquire( state, DFXL_FILLRECTANGLE ))
     {
          for (; i<num_spans; i++) {
               DFBRectangle rect = { spans[i].x, y + i, spans[i].w, 1 };

               if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h) {
                    if (!dfb_clip_rectangle( &state->clip, &rect ))
                         continue;

                    if (rect.w > card->limits.dst_max.w || rect.h > card->limits.dst_max.h)
                         break;
               }
               else if (!D_FLAGS_IS_SET( card->caps.flags, CCF_CLIPPING ))
                    if (!dfb_clip_rectangle( &state->clip, &rect ))
                         continue;

               if (!card->funcs.FillRectangle( card->driver_data,
                                               card->device_data, &rect ))
                    break;
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num_spans) {
          if (gAcquire( state, DFXL_FILLRECTANGLE )) {
               for (; i<num_spans; i++) {
                    DFBRectangle rect = { spans[i].x, y + i, spans[i].w, 1 };

                    if (dfb_clip_rectangle( &state->clip, &rect ))
                         gFillRectangle( state, &rect );
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
          else {                           \
               dda.mi = 0;                 \
               dda.mf = 0;                 \
               dda.xf = 0;                 \
               dda._2dy = 0;               \
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

     D_MAGIC_ASSERT( state, CardState );

     y = tri->y1;
     yend = tri->y3;

     if (yend > state->clip.y2)
          yend = state->clip.y2;

     SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
     SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);

     while (y <= yend) {
          DFBRectangle rect;

          if (y == tri->y2) {
               if (tri->y2 == tri->y3)
                    return;
               SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
          }

          rect.w = ABS(dda1.xi - dda2.xi);
          rect.x = MIN(dda1.xi, dda2.xi);

          if (clip_x2 < rect.x + rect.w)
               rect.w = clip_x2 - rect.x + 1;

          if (rect.w > 0) {
               if (clip_x1 > rect.x) {
                    rect.w -= (clip_x1 - rect.x);
                    rect.x = clip_x1;
               }
               rect.y = y;
               rect.h = 1;

               if (rect.w > 0 && rect.y >= state->clip.y1) {
                    if (accelerated)
                         card->funcs.FillRectangle( card->driver_data,
                                                    card->device_data, &rect );
                    else
                         gFillRectangle( state, &rect );
               }
          }

          INC_DDA(dda1);
          INC_DDA(dda2);

          y++;
     }
}


void dfb_gfxcard_filltriangle( DFBTriangle *tri, CardState *state )
{
     bool hw = false;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( tri != NULL );

     dfb_state_lock( state );

     /* if hardware has clipping try directly accelerated triangle filling */
     if ((card->caps.flags & CCF_CLIPPING) &&
          dfb_gfxcard_state_check( state, DFXL_FILLTRIANGLE ) &&
          dfb_gfxcard_state_acquire( state, DFXL_FILLTRIANGLE ))
     {
          hw = card->funcs.FillTriangle( card->driver_data,
                                         card->device_data, tri );
          dfb_gfxcard_state_release( state );
     }

     if (!hw) {
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
               else if (gAcquire( state, DFXL_FILLRECTANGLE )) {
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
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( state->source != NULL );
     D_ASSERT( rect != NULL );
     D_ASSERT( rect->x >= 0 );
     D_ASSERT( rect->y >= 0 );
     D_ASSERT( rect->x < state->source->width );
     D_ASSERT( rect->y < state->source->height );
     D_ASSERT( rect->x + rect->w - 1 < state->source->width );
     D_ASSERT( rect->y + rect->h - 1 < state->source->height );

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
     int i = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( num > 0 );

     dfb_state_lock( state );

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT ))
     {
          for (; i<num; i++) {
               if (dfb_clip_blit_precheck( &state->clip,
                                           rects[i].w, rects[i].h,
                                           points[i].x, points[i].y ))
               {
                    if (!(card->caps.flags & CCF_CLIPPING))
                         dfb_clip_blit( &state->clip, &rects[i],
                                        &points[i].x, &points[i].y );

                    if (!card->funcs.Blit( card->driver_data, card->device_data,
                                           &rects[i], points[i].x, points[i].y ))
                         break;
               }
          }

          dfb_gfxcard_state_release( state );
     }

     if (i < num) {
          if (gAcquire( state, DFXL_BLIT )) {
               for (; i<num; i++) {
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

void dfb_gfxcard_tileblit( DFBRectangle *rect, int dx1, int dy1, int dx2, int dy2,
                           CardState *state )
{
     int           x, y;
     int           odx;
     DFBRectangle  srect;
     DFBRegion    *clip;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rect != NULL );

     /* If called with an invalid rectangle, the algorithm goes into an
        infinite loop. This should never happen but it's safer to check. */
     D_ASSERT( rect->w >= 1 );
     D_ASSERT( rect->h >= 1 );


     odx = dx1;

     dfb_state_lock( state );

     clip = &state->clip;

     /* Check if anything is drawn at all. */
     if (!dfb_clip_blit_precheck( clip, dx2-dx1+1, dy2-dy1+1, dx1, dy1 )) {
          dfb_state_unlock( state );
          return;
     }

     /* Remove clipped tiles. */
     if (dx1 < clip->x1) {
          int outer = clip->x1 - dx1;

          dx1 += outer - (outer % rect->w);
     }

     if (dy1 < clip->y1) {
          int outer = clip->y1 - dy1;

          dy1 += outer - (outer % rect->h);
     }

     if (dx2 > clip->x2) {
          int outer = clip->x2 - dx2;

          dx2 -= outer - (outer % rect->w);
     }

     if (dy2 > clip->y2) {
          int outer = clip->y2 - dy2;

          dy2 -= outer - (outer % rect->h);
     }

     dx1 = odx;

     if (dfb_gfxcard_state_check( state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( state, DFXL_BLIT )) {
          bool hw = true;

          for (; dy1 < dy2; dy1 += rect->h) {
               for (; dx1 < dx2; dx1 += rect->w) {

                    if (!dfb_clip_blit_precheck( clip, rect->w, rect->h, dx1, dy1 ))
                         continue;

                    x = dx1;
                    y = dy1;
                    srect = *rect;

                    if (!(card->caps.flags & CCF_CLIPPING))
                         dfb_clip_blit( clip, &srect, &x, &y );

                    hw = card->funcs.Blit( card->driver_data,
                                           card->device_data, &srect, x, y );
                    if (!hw)
                         break;
               }
               if (!hw)
                    break;
               dx1 = odx;
          }
          dfb_gfxcard_state_release( state );
     }

     if (dy1 < dy2) {
          if (gAcquire( state, DFXL_BLIT )) {
               for (; dy1 < dy2; dy1 += rect->h) {
                    for (; dx1 < dx2; dx1 += rect->w) {

                         if (!dfb_clip_blit_precheck( clip, rect->w, rect->h, dx1, dy1 ))
                              continue;

                         x = dx1;
                         y = dy1;
                         srect = *rect;

                         dfb_clip_blit( clip, &srect, &x, &y );

                         gBlit( state, &srect, x, y );
                    }
                    dx1 = odx;
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
     D_MAGIC_ASSERT( state, CardState );
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
               /* Clipping is performed in the following function. */
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
     D_MAGIC_ASSERT( state, CardState );

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

static void
setup_font_state( CoreFont *font, CardState *state )
{
     DFBSurfaceBlittingFlags flags = font->state.blittingflags;

     D_MAGIC_ASSERT( state, CardState );

     /* set destination */
     dfb_state_set_destination( &font->state, state->destination );

     /* set clip */
     dfb_state_set_clip( &font->state, &state->clip );

     /* set color */
     if (DFB_PIXELFORMAT_IS_INDEXED( state->destination->format ))
          dfb_state_set_color_index( &font->state, state->color_index );
     else
          dfb_state_set_color( &font->state, &state->color );

     /* additional blending? */
     if ((state->drawingflags & DSDRAW_BLEND) && (state->color.a != 0xff))
          flags |= DSBLIT_BLEND_COLORALPHA;

     if (state->drawingflags & DSDRAW_DST_COLORKEY) {
          flags |= DSBLIT_DST_COLORKEY;
          dfb_state_set_dst_colorkey( &font->state, state->dst_colorkey );
     }

     if (state->drawingflags & DSDRAW_XOR)
          flags |= DSBLIT_XOR;

     /* Porter/Duff SRC_OVER composition */
     if ((DFB_PIXELFORMAT_HAS_ALPHA( state->destination->format )
          && (state->destination->caps & DSCAPS_PREMULTIPLIED))
         ||
         (font->surface_caps & DSCAPS_PREMULTIPLIED))
     {
          if (font->surface_caps & DSCAPS_PREMULTIPLIED) {
               if (flags & DSBLIT_BLEND_COLORALPHA)
                    flags |= DSBLIT_SRC_PREMULTCOLOR;
          }
          else
               flags |= DSBLIT_SRC_PREMULTIPLY;

          dfb_state_set_src_blend( &font->state, DSBF_ONE );
     }
     else
          dfb_state_set_src_blend( &font->state, DSBF_SRCALPHA );


     /* set blitting flags */
     dfb_state_set_blitting_flags( &font->state, flags );


     /* set disabled functions */
     font->state.disabled = state->disabled;
}

void
dfb_gfxcard_drawstring( const u8 *text, int bytes,
                        DFBTextEncodingID encoding, int x, int y,
                        CoreFont *font, CardState *state )
{
     unsigned int prev = 0;
     unsigned int indices[bytes];
     int          i, num;

     int hw_clipping = (card->caps.flags & CCF_CLIPPING);
     int kern_x;
     int kern_y;
     int blit = 0;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( text != NULL );
     D_ASSERT( bytes > 0 );
     D_ASSERT( font != NULL );

     /* simple prechecks */
     if (x > state->clip.x2 || y > state->clip.y2 ||
         y + font->ascender - font->descender <= state->clip.y1) {
          return;
     }

     dfb_font_lock( font );

     /* Decode string to character indices. */
     dfb_font_decode_text( font, encoding, text, bytes, indices, &num );

     setup_font_state( font, state );

     /* blit glyphs */
     for (i=0; i<num; i++) {
          CoreGlyphData *glyph;
          unsigned int   current = indices[i];

          if (current < 128)
               glyph = font->glyph_data[current];
          else
               glyph = direct_hash_lookup( font->glyph_hash, current );

          if (!glyph) {
               switch (blit) {
                    case 1:
                         dfb_gfxcard_state_release( &font->state );
                         break;
                    case 2:
                         gRelease( &font->state );
                         break;
               }
               blit = 0;

               if (dfb_font_get_glyph_data( font, current, &glyph )) {
                    prev = current;
                    continue;
               }
          }

          if (prev && font->GetKerning &&
              font->GetKerning( font, prev, current, &kern_x, &kern_y) == DFB_OK)
          {
               x += kern_x;
               y += kern_y;
          }

          if (glyph->width) {
               int xx = x + glyph->left;
               int yy = y + glyph->top;
               DFBRectangle rect = { glyph->start, 0,
                                     glyph->width, glyph->height };

               if (font->state.source != glyph->surface || !blit) {
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
                    dfb_state_set_source( &font->state, glyph->surface );

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

          x   += glyph->advance;
          prev = current;
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

     dfb_font_unlock( font );
}

void dfb_gfxcard_drawglyph( unsigned int index, int x, int y,
                            CoreFont *font, CardState *state )
{
     DFBResult      ret;
     CoreGlyphData *data;
     DFBRectangle   rect;
     bool           hw = false;

     D_DEBUG_AT( Core_Graphics, "%s( %d, %d,%d, %p, %p )\n",
                 __FUNCTION__, index, x, y, font, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( font != NULL );

     dfb_font_lock( font );

     ret = dfb_font_get_glyph_data (font, index, &data);
     if (ret)
          D_DEBUG_AT( Core_Graphics, "  -> dfb_font_get_glyph_data() failed! [%s]\n",
                      DirectFBErrorString( ret ) );

     if (ret || !data->width) {
          dfb_font_unlock( font );
          return;
     }

     x += data->left;
     y += data->top;

     if (! dfb_clip_blit_precheck( &state->clip,
                                   data->width, data->height, x, y )) {
          dfb_font_unlock( font );
          return;
     }

     setup_font_state( font, state );


     /* set blitting source */
     dfb_state_set_source( &font->state, data->surface );

     rect.x = data->start;
     rect.y = 0;
     rect.w = data->width;
     rect.h = data->height;

     if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ) &&
         dfb_gfxcard_state_acquire( &font->state, DFXL_BLIT )) {

          if (!(card->caps.flags & CCF_CLIPPING))
               dfb_clip_blit( &font->state.clip, &rect, &x, &y );

          hw = card->funcs.Blit( card->driver_data, card->device_data, &rect, x, y);
          dfb_gfxcard_state_release( &font->state );
     }

     if (!hw) {
          if (gAcquire( &font->state, DFXL_BLIT )) {
               dfb_clip_blit( &font->state.clip, &rect, &x, &y );
               gBlit( &font->state, &rect, x, y );
               gRelease( &font->state );
          }
     }

     dfb_font_unlock( font );
}

void dfb_gfxcard_drawstring_check_state( CoreFont *font, CardState *state )
{
     int            i;
     CoreGlyphData *data = NULL;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( font != NULL );

     dfb_font_lock( font );

     for (i=0; i<128; i++) {
          if (dfb_font_get_glyph_data (font, i, &data) == DFB_OK)
               break;
     }

     if (!data) {
          dfb_font_unlock( font );
          return;
     }

     setup_font_state( font, state );

     /* set the source */
     dfb_state_set_source( &font->state, data->surface );

     /* check for blitting and report */
     if (dfb_gfxcard_state_check( &font->state, DFXL_BLIT ))
          state->accel |= DFXL_DRAWSTRING;
     else
          state->accel &= ~DFXL_DRAWSTRING;

     dfb_font_unlock( font );
}

DFBResult dfb_gfxcard_sync()
{
     DFBResult ret;

     D_ASSUME( card != NULL );

     if (!card)
          return DFB_OK;

     ret = dfb_gfxcard_lock( GDLF_SYNC );
     if (ret)
          return ret;

     dfb_gfxcard_unlock();

     return DFB_OK;
}

void dfb_gfxcard_invalidate_state()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     card->shared->state = NULL;
}

DFBResult dfb_gfxcard_wait_serial( const CoreGraphicsSerial *serial )
{
     DFBResult ret;

     D_ASSERT( serial != NULL );
     D_ASSUME( card != NULL );

     if (!card)
          return DFB_OK;

     D_ASSERT( card->shared != NULL );

     ret = dfb_gfxcard_lock( GDLF_NONE );
     if (ret)
          return ret;

     if (card->funcs.WaitSerial)
          ret = card->funcs.WaitSerial( card->driver_data, card->device_data, serial );
     else if (card->funcs.EngineSync)
          ret = card->funcs.EngineSync( card->driver_data, card->device_data );

     if (ret) {
          if (card->funcs.EngineReset)
               card->funcs.EngineReset( card->driver_data, card->device_data );

          card->shared->state = NULL;
     }

     dfb_gfxcard_unlock();

     return ret;
}

void dfb_gfxcard_flush_texture_cache()
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.FlushTextureCache)
          card->funcs.FlushTextureCache( card->driver_data, card->device_data );
}

void dfb_gfxcard_flush_read_cache()
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.FlushReadCache)
          card->funcs.FlushReadCache( card->driver_data, card->device_data );
}

void dfb_gfxcard_after_set_var()
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.AfterSetVar)
          card->funcs.AfterSetVar( card->driver_data, card->device_data );
}

void dfb_gfxcard_surface_enter( SurfaceBuffer *buffer, DFBSurfaceLockFlags flags )
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.SurfaceEnter)
          card->funcs.SurfaceEnter( card->driver_data, card->device_data, buffer, flags );
}

void dfb_gfxcard_surface_leave( SurfaceBuffer *buffer )
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.SurfaceLeave)
          card->funcs.SurfaceLeave( card->driver_data, card->device_data, buffer );
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
dfb_gfxcard_get_capabilities( CardCapabilities *ret_caps )
{
     D_ASSERT( card != NULL );

     D_ASSERT( ret_caps != NULL );

     *ret_caps = card->caps;
}

void
dfb_gfxcard_get_device_info( GraphicsDeviceInfo *ret_info )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_ASSERT( ret_info != NULL );

     *ret_info = card->shared->device_info;
}

void
dfb_gfxcard_get_driver_info( GraphicsDriverInfo *ret_info )
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_ASSERT( ret_info != NULL );

     *ret_info = card->shared->driver_info;
}


int
dfb_gfxcard_reserve_memory( GraphicsDevice *device, unsigned int size )
{
     GraphicsDeviceShared *shared;

     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     shared = device->shared;

     if (shared->surface_manager) {
          D_WARN( "too late" );
          return -1;
     }

     if (shared->videoram_length < size) {
          D_WARN( "not enough video memory" );
          return -1;
     }

     shared->videoram_length -= size;

     return shared->videoram_length;
}

int
dfb_gfxcard_reserve_auxmemory( GraphicsDevice *device, unsigned int size )
{
     GraphicsDeviceShared *shared;
     int                   offset;

     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     shared = device->shared;

     if (shared->surface_manager)
          return -1;

     /* Reserve memory at the beginning of the aperture
      * to prevent overflows on DMA buffers. */

     offset = shared->auxram_offset;

     if (shared->auxram_length < (offset + size))
          return -1;

     shared->auxram_offset += size;

     return offset;
}

unsigned int
dfb_gfxcard_memory_length()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return card->shared->videoram_length;
}

unsigned int
dfb_gfxcard_auxmemory_length()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return card->shared->auxram_length;
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

unsigned long
dfb_gfxcard_auxmemory_physical( GraphicsDevice *device,
                                unsigned int    offset )
{
     return dfb_system_aux_memory_physical( offset );
}

void *
dfb_gfxcard_auxmemory_virtual( GraphicsDevice *device,
                               unsigned int    offset )
{
     return dfb_system_aux_memory_virtual( offset );
}

/** internal **/

/*
 * loads/probes/unloads one driver module after another until a suitable
 * driver is found and returns its symlinked functions
 */
static void dfb_gfxcard_find_driver( CoreDFB *core )
{
     DirectLink          *link;
     FusionSHMPoolShared *pool = dfb_core_shmpool( core );

     direct_list_foreach (link, dfb_graphics_drivers.entries) {
          DirectModuleEntry *module = (DirectModuleEntry*) link;

          const GraphicsDriverFuncs *funcs = direct_module_ref( module );

          if (!funcs)
               continue;

          if (!card->module && funcs->Probe( card )) {
               funcs->GetDriverInfo( card, &card->shared->driver_info );

               card->module       = module;
               card->driver_funcs = funcs;

               card->shared->module_name = SHSTRDUP( pool, module->name );
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

