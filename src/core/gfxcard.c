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

#include <limits.h>
#include <string.h>

#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>
#include <fusion/arena.h>
#include <fusion/property.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/core_parts.h>
#include <core/gfxcard.h>
#include <core/fonts.h>
#include <core/state.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
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


D_DEBUG_DOMAIN( Core_Graphics, "Core/Graphics", "DirectFB Graphics Core" );
D_DEBUG_DOMAIN( Core_GraphicsOps, "Core/GraphicsOps", "DirectFB Graphics Core Operations" );


DEFINE_MODULE_DIRECTORY( dfb_graphics_drivers, "gfxdrivers", DFB_GRAPHICS_DRIVER_ABI_VERSION );

/**********************************************************************************************************************/

static void dfb_gfxcard_find_driver( CoreDFB *core );
static void dfb_gfxcard_load_driver();

/**********************************************************************************************************************/

typedef struct {
     int                      magic;

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

     /*
      * Points to the current state of the graphics card.
      */
     CardState               *state;
     FusionID                 holder; /* Fusion ID of state owner. */
} DFBGraphicsCoreShared;

struct __DFB_DFBGraphicsCore {
     int                        magic;

     CoreDFB                   *core;

     DFBGraphicsCoreShared     *shared;

     DirectModuleEntry         *module;
     const GraphicsDriverFuncs *driver_funcs;

     void                      *driver_data;
     void                      *device_data; /* copy of shared->device_data */

     CardCapabilities           caps;        /* local caps */
     CardLimitations            limits;      /* local limits */

     GraphicsDeviceFuncs        funcs;
};


DFB_CORE_PART( graphics_core, GraphicsCore );

/**********************************************************************************************************************/

static CoreGraphicsDevice *card;   /* FIXME */

/* Hook for registering additional screen(s) and layer(s) in app or lib initializing DirectFB. */
void (*__DFB_CoreRegisterHook)( CoreDFB *core, CoreGraphicsDevice *device, void *ctx ) = NULL;
void  *__DFB_CoreRegisterHookCtx = NULL;


/** public **/

static DFBResult
dfb_graphics_core_initialize( CoreDFB               *core,
                              DFBGraphicsCore       *data,
                              DFBGraphicsCoreShared *shared )
{
     DFBResult            ret;
     int                  videoram_length;
     int                  auxram_length;
     FusionSHMPoolShared *pool = dfb_core_shmpool( core );

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_initialize( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_ASSERT( shared != NULL );


     card = data;   /* FIXME */

     data->core   = core;
     data->shared = shared;


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

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          data->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

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

          ret = funcs->InitDevice( data, &shared->device_info,
                                   data->driver_data, data->device_data );
          if (ret) {
               funcs->CloseDriver( card, card->driver_data );
               SHFREE( pool, shared->device_data );
               SHFREE( pool, shared->module_name );
               D_FREE( card->driver_data );
               card = NULL;
               return ret;
          }

          if (data->funcs.EngineReset)
               data->funcs.EngineReset( data->driver_data, data->device_data );
     }

     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n",
             shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major,
             shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          if (data->funcs.CheckState) {
               data->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          data->caps   = shared->device_info.caps;
          data->limits = shared->device_info.limits;
     }

     fusion_property_init( &shared->lock, dfb_core_world(core) );

     if (__DFB_CoreRegisterHook)
         __DFB_CoreRegisterHook( core, card, __DFB_CoreRegisterHookCtx );

     D_MAGIC_SET( data, DFBGraphicsCore );
     D_MAGIC_SET( shared, DFBGraphicsCoreShared );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_join( CoreDFB               *core,
                        DFBGraphicsCore       *data,
                        DFBGraphicsCoreShared *shared )
{
     DFBResult          ret;
     GraphicsDriverInfo driver_info;

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_join( %p, %p, %p )\n", core, data, shared );

     D_ASSERT( data != NULL );
     D_MAGIC_ASSERT( shared, DFBGraphicsCoreShared );

     card = data;   /* FIXME */

     data->core   = core;
     data->shared = shared;

     /* Initialize software rasterizer. */
     gGetDriverInfo( &driver_info );

     /* Build a list of available drivers. */
     direct_modules_explore_directory( &dfb_graphics_drivers );

     /* Load driver. */
     if (dfb_system_caps() & CSCAPS_ACCELERATION)
          dfb_gfxcard_load_driver();

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          data->driver_data = D_CALLOC( 1, shared->driver_info.driver_data_size );

          data->device_data = shared->device_data;

          ret = funcs->InitDriver( card, &card->funcs,
                                   card->driver_data, card->device_data, core );
          if (ret) {
               D_FREE( data->driver_data );
               data = NULL;
               return ret;
          }
     }
     else if (shared->module_name) {
          D_ERROR( "DirectFB/Graphics: Could not load driver used by the running session!\n" );
          data = NULL;
          return DFB_UNSUPPORTED;
     }


     D_INFO( "DirectFB/Graphics: %s %s %d.%d (%s)\n",
             shared->device_info.vendor, shared->device_info.name,
             shared->driver_info.version.major,
             shared->driver_info.version.minor, shared->driver_info.vendor );

     if (dfb_config->software_only) {
          if (data->funcs.CheckState) {
               data->funcs.CheckState = NULL;

               D_INFO( "DirectFB/Graphics: Acceleration disabled (by 'no-hardware')\n" );
          }
     }
     else {
          data->caps   = shared->device_info.caps;
          data->limits = shared->device_info.limits;
     }

     D_MAGIC_SET( data, DFBGraphicsCore );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_shutdown( DFBGraphicsCore *data,
                            bool             emergency )
{
     DFBGraphicsCoreShared *shared;
     FusionSHMPoolShared   *pool = dfb_core_shmpool( data->core );

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_shutdown( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     shared = data->shared;


     dfb_gfxcard_lock( GDLF_SYNC );

     if (data->driver_funcs) {
          const GraphicsDriverFuncs *funcs = data->driver_funcs;

          funcs->CloseDevice( data, data->driver_data, data->device_data );
          funcs->CloseDriver( data, data->driver_data );

          direct_module_unref( data->module );

          SHFREE( pool, card->device_data );
          D_FREE( card->driver_data );
     }

     fusion_property_destroy( &shared->lock );

     if (shared->module_name)
          SHFREE( pool, shared->module_name );


     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( shared );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_leave( DFBGraphicsCore *data,
                         bool             emergency )
{
     DFBGraphicsCoreShared *shared;

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_leave( %p, %semergency )\n", data, emergency ? "" : "no " );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     shared = data->shared;


     dfb_gfxcard_sync();

     if (data->driver_funcs) {
          data->driver_funcs->CloseDriver( data, data->driver_data );

          direct_module_unref( data->module );

          D_FREE( data->driver_data );
     }


     D_MAGIC_CLEAR( data );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_suspend( DFBGraphicsCore *data )
{
     DFBGraphicsCoreShared *shared;

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_suspend( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     shared = data->shared;

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     return DFB_OK;
}

static DFBResult
dfb_graphics_core_resume( DFBGraphicsCore *data )
{
     DFBGraphicsCoreShared *shared;

     D_DEBUG_AT( Core_Graphics, "dfb_graphics_core_resume( %p )\n", data );

     D_MAGIC_ASSERT( data, DFBGraphicsCore );
     D_MAGIC_ASSERT( data->shared, DFBGraphicsCoreShared );

     shared = data->shared;

     dfb_gfxcard_unlock();

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_gfxcard_lock( GraphicsDeviceLockFlags flags )
{
     DFBResult              ret;
     DFBGraphicsCoreShared *shared;
     GraphicsDeviceFuncs   *funcs;

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

     if (shared->lock_flags & GDLF_INVALIDATE) {
          if (funcs->InvalidateState)
               funcs->InvalidateState( card->driver_data, card->device_data );
          shared->state = NULL;
     }

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
 * Signal beginning of a sequence of operations using this state.
 * Any number of states can be 'drawing'.
 */
void
dfb_gfxcard_start_drawing( CoreGraphicsDevice *device, CardState *state )
{
     D_ASSERT( device != NULL );
     D_MAGIC_ASSERT( state, CardState );

     if (device->funcs.StartDrawing)
          device->funcs.StartDrawing( device->driver_data, device->device_data, state );
}

/*
 * Signal end of sequence, i.e. destination surface is consistent again.
 */
void
dfb_gfxcard_stop_drawing( CoreGraphicsDevice *device, CardState *state )
{
     D_ASSERT( device != NULL );
     D_MAGIC_ASSERT( state, CardState );

     if (device->funcs.StopDrawing)
          device->funcs.StopDrawing( device->driver_data, device->device_data, state );
}

/*
 * This function returns non zero if acceleration is available
 * for the specific function using the given state.
 */
bool
dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel )
{
     CoreSurface       *dst;
     CoreSurface       *src;
     CoreSurfaceBuffer *dst_buffer;
     CoreSurfaceBuffer *src_buffer;

     int cx2, cy2;

     D_ASSERT( card != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT_IF( state->destination, CoreSurface );
     D_MAGIC_ASSERT_IF( state->source, CoreSurface );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, 0x%08x ) [%d,%d - %d,%d]\n",
                 __FUNCTION__, state, accel, DFB_REGION_VALS( &state->clip ) );

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

     dst = state->destination;

     /* Destination may have been destroyed. */
     if (!dst) {
          D_BUG( "no destination" );
          return false;
     }

     dst_buffer = dfb_surface_get_buffer( dst, state->to );
     D_MAGIC_ASSERT( dst_buffer, CoreSurfaceBuffer );

     D_ASSUME( state->clip.x2 < dst->config.size.w );
     D_ASSUME( state->clip.y2 < dst->config.size.h );

     cx2 = state->destination->config.size.w  - 1;
     cy2 = state->destination->config.size.h - 1;

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

     src = state->source;

     /* Source may have been destroyed. */
     if (DFB_BLITTING_FUNCTION( accel ) && !src) {
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

     /* If destination or blend functions have been changed... */
     if (state->modified & (SMF_DESTINATION | SMF_SRC_BLEND | SMF_DST_BLEND)) {
          /* ...force rechecking for all functions. */
          state->accel   = 0;
          state->checked = 0;
     }
     else {
          /* If source or blitting flags have been changed... */
          if (state->modified & (SMF_SOURCE | SMF_BLITTING_FLAGS)) {
               /* ...force rechecking for all blitting functions. */
               state->accel   &= 0x0000FFFF;
               state->checked &= 0x0000FFFF;
          }

          /* If drawing flags have been changed... */
          if (state->modified & SMF_DRAWING_FLAGS) {
               /* ...force rechecking for all drawing functions. */
               state->accel   &= 0xFFFF0000;
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

     /* Move modification flags to the set for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = 0;

     /*
      * If back_buffer policy is 'system only' there's no acceleration
      * available.
      */
     if (dst_buffer->policy == CSP_SYSTEMONLY) {
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
     if (DFB_BLITTING_FUNCTION( accel )) {
          src_buffer = dfb_surface_get_buffer( src, state->from );

          D_MAGIC_ASSERT( src_buffer, CoreSurfaceBuffer );

          if (src_buffer->policy == CSP_SYSTEMONLY && !(card->caps.flags & CCF_READSYSMEM)) {
               /* Clear 'accelerated blitting functions'. */
               state->accel   &= 0x0000FFFF;
               state->checked |= 0xFFFF0000;
     
               return false;
          }
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
     DFBResult               ret;
     CoreSurface            *dst;
     CoreSurface            *src;
     DFBGraphicsCoreShared  *shared;
     CoreSurfaceAccessFlags  access = CSAF_GPU_WRITE;

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     D_MAGIC_ASSERT( state, CardState );
     D_MAGIC_ASSERT_IF( state->destination, CoreSurface );
     D_MAGIC_ASSERT_IF( state->source, CoreSurface );

     dst    = state->destination;
     src    = state->source;
     shared = card->shared;

     /* find locking flags */
     if (DFB_BLITTING_FUNCTION( accel )) {
          if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                      DSBLIT_BLEND_COLORALPHA   |
                                      DSBLIT_DST_COLORKEY))
               access |= CSAF_GPU_READ;
     }
     else if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_DST_COLORKEY))
          access |= CSAF_GPU_READ;

     /* lock destination */
     ret = dfb_surface_lock_buffer( dst, state->to, access, &state->dst );
     if (ret) {
          D_DEBUG_AT( Core_Graphics, "Could not lock destination for GPU access!\n" );
          return false;
     }

     /* if blitting... */
     if (DFB_BLITTING_FUNCTION( accel )) {
          /* ...lock source for reading */
          ret = dfb_surface_lock_buffer( src, state->from, CSAF_GPU_READ, &state->src );
          if (ret) {
               D_DEBUG_AT( Core_Graphics, "Could not lock source for GPU access!\n" );
               dfb_surface_unlock_buffer( dst, &state->dst );
               return false;
          }

          state->flags |= CSF_SOURCE_LOCKED;
     }

     /*
      * Make sure that state setting with subsequent command execution
      * isn't done by two processes simultaneously.
      *
      * This will timeout if the hardware is locked by another party with
      * the first argument being true (e.g. DRI).
      */
     if (dfb_gfxcard_lock( GDLF_NONE )) {
          D_DERROR( ret, "Core/Graphics: Could not lock GPU!\n" );

          dfb_surface_unlock_buffer( dst, &state->dst );

          if (state->flags & CSF_SOURCE_LOCKED) {
               dfb_surface_unlock_buffer( src, &state->src );
               state->flags &= ~CSF_SOURCE_LOCKED;
          }

          return false;
     }

     /* if we are switching to another state... */
     if (state != shared->state || state->fusion_id != shared->holder) {
          /* ...set all modification bits and clear 'set functions' */
          state->mod_hw |= SMF_ALL;
          state->set     = 0;

          shared->state  = state;
          shared->holder = state->fusion_id;
     }

     dfb_state_update( state, state->flags & CSF_SOURCE_LOCKED );

     /* Move modification flags to the set for drivers. */
     state->mod_hw   |= state->modified;
     state->modified  = SMF_ALL;

     /*
      * If function hasn't been set or state is modified,
      * call the driver function to propagate the state changes.
      */
     if (state->mod_hw || !(state->set & accel))
          card->funcs.SetState( card->driver_data, card->device_data,
                                &card->funcs, state, accel );

     if (state->modified != SMF_ALL)
          D_ONCE( "USING OLD DRIVER! *** Use 'state->mod_hw' NOT 'modified'." );

     state->modified = 0;

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

     /* start command processing if not already running */
     if (card->funcs.EmitCommands)
          card->funcs.EmitCommands( card->driver_data, card->device_data );

     /* Store the serial of the operation. */
#if FIXME_SC_2
     if (card->funcs.GetSerial) {
          card->funcs.GetSerial( card->driver_data, card->device_data, &state->serial );

          state->destination->back_buffer->video.serial = state->serial;
     }
#endif

     /* allow others to use the hardware */
     dfb_gfxcard_unlock();

     /* destination always gets locked during acquisition */
     dfb_surface_unlock_buffer( state->destination, &state->dst );

     /* if source got locked this value is true */
     if (state->flags & CSF_SOURCE_LOCKED) {
          dfb_surface_unlock_buffer( state->source, &state->src );

          state->flags &= ~CSF_SOURCE_LOCKED;
     }
}

/** DRAWING FUNCTIONS **/

void
dfb_gfxcard_fillrectangles( const DFBRectangle *rects, int num, CardState *state )
{
     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, rects, num, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( num > 0 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

     if (!(state->render_options & DSRO_MATRIX)) {
          while (num > 0) {
               if (dfb_rectangle_region_intersects( rects, &state->clip ))
                    break;

               rects++;
               num--;
          }
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
                    if (!(state->render_options & DSRO_MATRIX) &&
                        !dfb_rectangle_region_intersects( &rects[i], &state->clip ))
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
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d - %dx%d, %p )\n", __FUNCTION__, DFB_RECTANGLE_VALS(rect), state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %p )\n", __FUNCTION__, lines, num_lines, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( lines != NULL );
     D_ASSERT( num_lines > 0 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d, %p [%d], %p )\n", __FUNCTION__, y, spans, num_spans, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( spans != NULL );
     D_ASSERT( num_spans > 0 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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
     DDA dda1 = {0}, dda2 = {0};
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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d - %d,%d - %d,%d, %p )\n", __FUNCTION__,
                 tri->x1, tri->y1, tri->x2, tri->y2, tri->x3, tri->y3, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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
     D_ASSERT( rect->x < state->source->config.size.w );
     D_ASSERT( rect->y < state->source->config.size.h );
     D_ASSERT( rect->x + rect->w - 1 < state->source->config.size.w );
     D_ASSERT( rect->y + rect->h - 1 < state->source->config.size.h );

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d - %dx%d -> %d,%d, %p )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS(rect), dx, dy, state );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p, %p [%d], %p )\n", __FUNCTION__, rects, points, num, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rects != NULL );
     D_ASSERT( points != NULL );
     D_ASSERT( num > 0 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d - %d,%d, %p )\n", __FUNCTION__, dx1, dy1, dx2, dy2, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( rect != NULL );

     /* If called with an invalid rectangle, the algorithm goes into an
        infinite loop. This should never happen but it's safer to check. */
     D_ASSERT( rect->w >= 1 );
     D_ASSERT( rect->h >= 1 );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     odx = dx1;

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %d,%d - %dx%d -> %d,%d - %dx%d, %p )\n",
                 __FUNCTION__, DFB_RECTANGLE_VALS(srect), DFB_RECTANGLE_VALS(drect), state );

     if (srect->w == drect->w && srect->h == drect->h) {
          dfb_gfxcard_blit( srect, drect->x, drect->y, state );
          return;
     }

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %s, %p )\n", __FUNCTION__, vertices, num,
                 (formation == DTTF_LIST)  ? "LIST"  :
                 (formation == DTTF_STRIP) ? "STRIP" :
                 (formation == DTTF_FAN)   ? "FAN"   : "unknown formation", state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_ASSERT( vertices != NULL );
     D_ASSERT( num >= 3 );
     D_MAGIC_ASSERT( state, CardState );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

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

     if (state->blittingflags == DSBLIT_INDEX_TRANSLATION) {
          flags = DSBLIT_INDEX_TRANSLATION;

          /* FIXME: Don't set all four indices each time? */
          dfb_state_set_index_translation( &font->state, state->index_translation, state->num_translation );
     }
     else {
          /* set color */
          if (DFB_PIXELFORMAT_IS_INDEXED( state->destination->config.format ))
               dfb_state_set_color_index( &font->state, state->color_index );

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
          if ((DFB_PIXELFORMAT_HAS_ALPHA( state->destination->config.format )
               && (state->destination->config.caps & DSCAPS_PREMULTIPLIED))
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
     }

     /* set blitting flags */
     dfb_state_set_blitting_flags( &font->state, flags );

     /* set render options */
     dfb_state_set_render_options( &font->state, state->render_options );

     /* set matrix? */
     if (state->render_options & DSRO_MATRIX)
          dfb_state_set_matrix( &font->state, state->matrix );

     /* set disabled functions */
     if (state->disabled & DFXL_DRAWSTRING)
          font->state.disabled = DFXL_ALL;
     else
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

     if (encoding == DTEID_UTF8)
          D_DEBUG_AT( Core_GraphicsOps, "%s( '%s' [%d], %d,%d, %p, %p )\n",
                      __FUNCTION__, text, bytes, x, y, font, state );
     else
          D_DEBUG_AT( Core_GraphicsOps, "%s( %p [%d], %d, %d,%d, %p, %p )\n",
                      __FUNCTION__, text, bytes, encoding, x, y, font, state );

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

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );


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

     dfb_state_unlock( state );
}

void dfb_gfxcard_drawglyph( unsigned int index, int x, int y,
                            CoreFont *font, CardState *state )
{
     DFBResult      ret;
     CoreGlyphData *data;
     DFBRectangle   rect;
     bool           hw = false;

     D_DEBUG_AT( Core_GraphicsOps, "%s( %u, %d,%d, %p, %p )\n",
                 __FUNCTION__, index, x, y, font, state );

     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );
     D_MAGIC_ASSERT( state, CardState );
     D_ASSERT( font != NULL );

     /* The state is locked during graphics operations. */
     dfb_state_lock( state );

     /* Signal beginning of sequence of operations if not already done. */
     dfb_state_start_drawing( state, card );

     dfb_font_lock( font );

     ret = dfb_font_get_glyph_data (font, index, &data);
     if (ret)
          D_DEBUG_AT( Core_Graphics, "  -> dfb_font_get_glyph_data() failed! [%s]\n",
                      DirectFBErrorString( ret ) );

     if (ret || !data->width)
          goto out;

     x += data->left;
     y += data->top;

     if (! dfb_clip_blit_precheck( &state->clip, data->width, data->height, x, y ))
          goto out;

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

out:
     dfb_font_unlock( font );

     dfb_state_unlock( state );
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

/* FIXME_SC_2     if (card->funcs.WaitSerial)
          ret = card->funcs.WaitSerial( card->driver_data, card->device_data, serial );
     else*/ if (card->funcs.EngineSync)
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

void dfb_gfxcard_surface_enter( CoreSurfaceBuffer *buffer, DFBSurfaceLockFlags flags )
{
     D_ASSUME( card != NULL );

     if (card && card->funcs.SurfaceEnter)
          card->funcs.SurfaceEnter( card->driver_data, card->device_data, buffer, flags );
}

void dfb_gfxcard_surface_leave( CoreSurfaceBuffer *buffer )
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

//FIXME_SMAN     return dfb_surfacemanager_adjust_heap_offset( card->shared->surface_manager, offset );
     return DFB_OK;
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
dfb_gfxcard_reserve_memory( CoreGraphicsDevice *device, unsigned int size )
{
     DFBGraphicsCoreShared *shared;

     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     shared = device->shared;

     if (shared->device_info.limits.surface_byteoffset_alignment) {
          size += shared->device_info.limits.surface_byteoffset_alignment - 1;
          size -= (size % shared->device_info.limits.surface_byteoffset_alignment);
     }
     else
          D_WARN( "no alignment specified yet?" );

     if (shared->videoram_length < size) {
          D_WARN( "not enough video memory (%u < %u)", shared->videoram_length, size );
          return -1;
     }

     shared->videoram_length -= size;

     return shared->videoram_length;
}

int
dfb_gfxcard_reserve_auxmemory( CoreGraphicsDevice *device, unsigned int size )
{
     DFBGraphicsCoreShared *shared;
     int                    offset;

     D_ASSERT( device != NULL );
     D_ASSERT( device->shared != NULL );

     shared = device->shared;

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
dfb_gfxcard_map_mmio( CoreGraphicsDevice *device,
                      unsigned int        offset,
                      int                 length )
{
     return dfb_system_map_mmio( offset, length );
}

void
dfb_gfxcard_unmap_mmio( CoreGraphicsDevice *device,
                        volatile void      *addr,
                        int                 length )
{
     dfb_system_unmap_mmio( addr, length );
}

int
dfb_gfxcard_get_accelerator( CoreGraphicsDevice *device )
{
     return dfb_system_get_accelerator();
}

void
dfb_gfxcard_get_limits( CoreGraphicsDevice *device,
                        CardLimitations    *ret_limits )
{
     D_ASSERT( device != NULL );
     D_ASSERT( ret_limits != NULL );

     if (!device)
          device = card;

     *ret_limits = device->limits;
}

void
dfb_gfxcard_calc_buffer_size( CoreGraphicsDevice *device,
                              CoreSurfaceBuffer  *buffer,
                              int                *ret_pitch,
                              int                *ret_length )
{
     int          pitch;
     int          length;
     CoreSurface *surface;

     D_ASSERT( device != NULL );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     /* calculate the required length depending on limitations */
     pitch = MAX( surface->config.size.w, surface->config.min_size.w );

     if (pitch < device->limits.surface_max_power_of_two_pixelpitch &&
         surface->config.size.h < device->limits.surface_max_power_of_two_height)
          pitch = 1 << direct_log2( pitch );

     if (device->limits.surface_pixelpitch_alignment > 1) {
          pitch += device->limits.surface_pixelpitch_alignment - 1;
          pitch -= pitch % device->limits.surface_pixelpitch_alignment;
     }

     pitch = DFB_BYTES_PER_LINE( buffer->format, pitch );

     if (pitch < device->limits.surface_max_power_of_two_bytepitch &&
         surface->config.size.h < device->limits.surface_max_power_of_two_height)
          pitch = 1 << direct_log2( pitch );

     if (device->limits.surface_bytepitch_alignment > 1) {
          pitch += device->limits.surface_bytepitch_alignment - 1;
          pitch -= pitch % device->limits.surface_bytepitch_alignment;
     }

     length = DFB_PLANE_MULTIPLY( buffer->format,
                                  MAX( surface->config.size.h, surface->config.min_size.h ) * pitch );

     /* Add extra space for optimized routines which are now allowed to overrun, e.g. prefetching. */
     length += 16;

     if (device->limits.surface_byteoffset_alignment > 1) {
          length += device->limits.surface_byteoffset_alignment - 1;
          length -= length % device->limits.surface_byteoffset_alignment;
     }

     if (ret_pitch)
          *ret_pitch = pitch;

     if (ret_length)
          *ret_length = length;
}

unsigned long
dfb_gfxcard_memory_physical( CoreGraphicsDevice *device,
                             unsigned int        offset )
{
     return dfb_system_video_memory_physical( offset );
}

void *
dfb_gfxcard_memory_virtual( CoreGraphicsDevice *device,
                            unsigned int        offset )
{
     return dfb_system_video_memory_virtual( offset );
}

unsigned long
dfb_gfxcard_auxmemory_physical( CoreGraphicsDevice *device,
                                unsigned int        offset )
{
     return dfb_system_aux_memory_physical( offset );
}

void *
dfb_gfxcard_auxmemory_virtual( CoreGraphicsDevice *device,
                               unsigned int        offset )
{
     return dfb_system_aux_memory_virtual( offset );
}

void *
dfb_gfxcard_get_device_data()
{
     D_ASSERT( card != NULL );
     D_ASSERT( card->shared != NULL );

     return card->shared->device_data;
}

void *
dfb_gfxcard_get_driver_data()
{
     D_ASSERT( card != NULL );

     return card->driver_data;
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

