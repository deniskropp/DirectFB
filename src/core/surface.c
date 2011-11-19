/*
   (c) Copyright 2001-2010  The world wide DirectFB Open Source Community (directfb.org)
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

#include <direct/debug.h>

#include <core/core.h>
#include <core/palette.h>
#include <core/surface.h>
#include <core/surface_pool.h>

#include <core/system.h>

#include <fusion/conf.h>
#include <fusion/shmalloc.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

#include <core/CoreDFB.h>
#include <core/CoreSurface.h>

#include <gfx/convert.h>


D_DEBUG_DOMAIN( Core_Surface, "Core/Surface", "DirectFB Core Surface" );

/**********************************************************************************************************************/

static const ReactionFunc dfb_surface_globals[] = {
/* 0 */   _dfb_layer_region_surface_listener,
/* 1 */   _dfb_windowstack_background_image_listener,
          NULL
};

static void
surface_destructor( FusionObject *object, bool zombie, void *ctx )
{
     int          i;
     CoreSurface *surface = (CoreSurface*) object;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "destroying %p (%dx%d%s)\n", surface,
                 surface->config.size.w, surface->config.size.h, zombie ? " ZOMBIE" : "");

     Core_Resource_RemoveSurface( surface );

     CoreSurface_Deinit_Dispatch( &surface->call );

     dfb_surface_lock( surface );

     surface->state |= CSSF_DESTROYED;

     /* announce surface destruction */
     dfb_surface_notify( surface, CSNF_DESTROY );

     /* unlink palette */
     if (surface->palette) {
          dfb_palette_detach_global( surface->palette, &surface->palette_reaction );
          dfb_palette_unlink( &surface->palette );
     }

     /* destroy buffers */
     for (i=0; i<MAX_SURFACE_BUFFERS; i++) {
          if (surface->buffers[i])
               dfb_surface_buffer_decouple( surface->buffers[i] );
     }

     /* release the system driver specific surface data */
     if (surface->data) {
          dfb_system_surface_data_destroy( surface, surface->data );
          SHFREE( surface->shmpool, surface->data );
          surface->data = NULL;
     }

     direct_serial_deinit( &surface->serial );

     dfb_surface_unlock( surface );

     fusion_skirmish_destroy( &surface->lock );

     D_MAGIC_CLEAR( surface );

     fusion_object_destroy( object );
}

FusionObjectPool *
dfb_surface_pool_create( const FusionWorld *world )
{
     FusionObjectPool *pool;

     pool = fusion_object_pool_create( "Surface Pool",
                                       sizeof(CoreSurface),
                                       sizeof(CoreSurfaceNotification),
                                       surface_destructor, NULL, world );

     return pool;
}

/**********************************************************************************************************************/

DFBResult
dfb_surface_create( CoreDFB                  *core,
                    const CoreSurfaceConfig  *config,
                    CoreSurfaceTypeFlags      type,
                    unsigned long             resource_id,
                    CorePalette              *palette,
                    CoreSurface             **ret_surface )
{
     DFBResult           ret = DFB_BUG;
     int                 i;
     int                 buffers;
     CoreSurface *       surface;
     char                buf[64];
     int                 data_size;
     int                 num_eyes;
     DFBSurfaceStereoEye eye;

     D_ASSERT( core != NULL );
     D_FLAGS_ASSERT( type, CSTF_ALL );
     D_MAGIC_ASSERT_IF( palette, CorePalette );
     D_ASSERT( ret_surface != NULL );

     D_DEBUG_AT( Core_Surface, "dfb_surface_create( %p, %p, %p )\n", core, config, ret_surface );

     surface = dfb_core_create_surface( core );
     if (!surface)
          return DFB_FUSION;

     surface->data = NULL;

     if (config) {
          D_FLAGS_ASSERT( config->flags, CSCONF_ALL );

          surface->config.flags = config->flags;

          if (config->flags & CSCONF_SIZE) {
               D_DEBUG_AT( Core_Surface, "  -> %dx%d\n", config->size.w, config->size.h );

               surface->config.size = config->size;
          }

          if (config->flags & CSCONF_FORMAT) {
               D_DEBUG_AT( Core_Surface, "  -> %s\n", dfb_pixelformat_name( config->format ) );

               surface->config.format = config->format;
          }

          if (config->flags & CSCONF_COLORSPACE) {
               D_DEBUG_AT( Core_Surface, "  -> %s\n", dfb_colorspace_name( config->colorspace ) );

               surface->config.colorspace = config->colorspace;
          }

          if (config->flags & CSCONF_CAPS) {
               D_DEBUG_AT( Core_Surface, "  -> caps 0x%08x\n", config->caps );

               if (config->caps & DSCAPS_ROTATED)
                    D_UNIMPLEMENTED();

               surface->config.caps = config->caps & ~DSCAPS_ROTATED;
          }

          if (config->flags & CSCONF_PREALLOCATED) {
               D_DEBUG_AT( Core_Surface, "  -> prealloc %p [%d]\n",
                           config->preallocated[0].addr,
                           config->preallocated[0].pitch );

               direct_memcpy( surface->config.preallocated, config->preallocated, sizeof(config->preallocated) );

               surface->config.preallocated_pool_id = config->preallocated_pool_id;

               type |= CSTF_PREALLOCATED;
          }
     }

     if (surface->config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (surface->config.caps & DSCAPS_VIDEOONLY)
          surface->type = (type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     if (surface->config.caps & DSCAPS_SHARED)
          surface->type |= CSTF_SHARED;

     surface->resource_id = resource_id;

     if (surface->config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (surface->config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else {
          buffers = 1;

          surface->config.caps &= ~DSCAPS_ROTATED;
     }

     surface->notifications = CSNF_ALL & ~CSNF_FLIP;

     surface->alpha_ramp[0] = 0x00;
     surface->alpha_ramp[1] = 0x55;
     surface->alpha_ramp[2] = 0xaa;
     surface->alpha_ramp[3] = 0xff;


     if (surface->config.caps & DSCAPS_STATIC_ALLOC)
          surface->config.min_size = surface->config.size;

     surface->shmpool = dfb_core_shmpool( core );

     direct_serial_init( &surface->serial );

     snprintf( buf, sizeof(buf), "Surface %dx%d %s %s", surface->config.size.w,
               surface->config.size.h, dfb_pixelformat_name(surface->config.format),
               dfb_colorspace_name(surface->config.colorspace) );

     fusion_ref_set_name( &surface->object.ref, buf );

     fusion_skirmish_init2( &surface->lock, buf, dfb_core_world(core), fusion_config->secure_fusion );

//     fusion_skirmish_add_permissions( &surface->lock, 0, FUSION_SKIRMISH_PERMIT_PREVAIL | FUSION_SKIRMISH_PERMIT_DISMISS );

     D_MAGIC_SET( surface, CoreSurface );


     if (dfb_config->warn.flags & DCWF_CREATE_SURFACE &&
         dfb_config->warn.create_surface.min_size.w <= surface->config.size.w &&
         dfb_config->warn.create_surface.min_size.h <= surface->config.size.h)
          D_WARN( "create-surface  %4dx%4d %6s, buffers %d, caps 0x%08x, type 0x%08x",
                  surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(surface->config.format),
                  buffers, surface->config.caps, surface->type );


     if (palette) {
          dfb_surface_set_palette( surface, palette );
     }
     else if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format )) {
          ret = dfb_surface_init_palette( core, surface );
          if (ret)
               goto error;
     }

     /* Create the system driver specific surface data information */
     data_size = dfb_system_surface_data_size();
     if (data_size) {
          surface->data = SHCALLOC( surface->shmpool, 1, data_size );
          if (!surface->data) {
              ret = D_OOSHM();
              goto error;
          }

          dfb_system_surface_data_init( surface, surface->data );
     }


     dfb_surface_lock( surface );

     /* Create the Surface Buffers. */
     num_eyes = config->caps & DSCAPS_STEREO ? 2 : 1;
     for (eye=DSSE_LEFT; num_eyes>0; num_eyes--, eye=DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i=0; i<buffers; i++) {
               ret = dfb_surface_buffer_create( core, surface, CSBF_NONE, &surface->buffers[i] );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
                    dfb_surface_unlock( surface );
                    goto error;
               }

               dfb_surface_buffer_globalize( surface->buffers[i] );
     
               if (eye == DSSE_LEFT)
                    surface->num_buffers++;
     
               switch (i) {
                    case 0:
                         surface->buffer_indices[CSBR_FRONT] = i;
                    case 1:
                         surface->buffer_indices[CSBR_BACK] = i;
                    case 2:
                         surface->buffer_indices[CSBR_IDLE] = i;
               }
          }
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     dfb_surface_unlock( surface );


     CoreSurface_Init_Dispatch( core, surface, &surface->call );

     fusion_object_activate( &surface->object );

     *ret_surface = surface;

     return DFB_OK;

error:
     num_eyes = config->caps & DSCAPS_STEREO ? 2 : 1;
     for (eye=DSSE_LEFT; num_eyes>0; num_eyes--, eye=DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i=0; i<MAX_SURFACE_BUFFERS; i++) {
               if (surface->buffers[i])
                    dfb_surface_buffer_decouple( surface->buffers[i] );
          }
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     /* release the system driver specific surface data */
     if (surface->data) {
         dfb_system_surface_data_destroy( surface, surface->data );
         SHFREE( surface->shmpool, surface->data );
         surface->data = NULL;
     }

     fusion_skirmish_destroy( &surface->lock );

     direct_serial_deinit( &surface->serial );

     D_MAGIC_CLEAR( surface );

     fusion_object_destroy( &surface->object );

     return ret;
}

DFBResult
dfb_surface_create_simple ( CoreDFB                 *core,
                            int                      width,
                            int                      height,
                            DFBSurfacePixelFormat    format,
                            DFBSurfaceColorSpace     colorspace,
                            DFBSurfaceCapabilities   caps,
                            CoreSurfaceTypeFlags     type,
                            unsigned long            resource_id,
                            CorePalette             *palette,
                            CoreSurface            **ret_surface )
{
     CoreSurfaceConfig config;

     D_DEBUG_AT( Core_Surface, "%s( %p, %dx%d %s, %p )\n", __FUNCTION__, core, width, height,
                 dfb_pixelformat_name( format ), ret_surface );

     D_ASSERT( core != NULL );
     D_ASSERT( ret_surface != NULL );

     config.flags        = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_COLORSPACE | CSCONF_CAPS;
     config.size.w       = width;
     config.size.h       = height;
     config.format       = format;
     config.colorspace   = colorspace;
     config.caps         = caps;

     //return dfb_surface_create( core, &config, type, resource_id, palette, ret_surface );
     return CoreDFB_CreateSurface( core, &config, type, resource_id, palette, ret_surface );
}

DFBResult
dfb_surface_init_palette( CoreDFB     *core,
                          CoreSurface *surface )
{
     DFBResult    ret;
     CorePalette *palette;

     ret = dfb_palette_create( core,
                               1 << DFB_COLOR_BITS_PER_PIXEL( surface->config.format ),
                               &palette );
     if (ret) {
          D_DERROR( ret, "Core/Surface: Error creating palette!\n" );
          return ret;
     }

     switch (surface->config.format) {
          case DSPF_LUT8:
               dfb_palette_generate_rgb332_map( palette );
               break;

          case DSPF_ALUT44:
               dfb_palette_generate_rgb121_map( palette );
               break;

          default:
               break;
     }

     dfb_surface_set_palette( surface, palette );

     dfb_palette_unref( palette );

     return DFB_OK;
}


DFBResult
dfb_surface_notify( CoreSurface                  *surface,
                    CoreSurfaceNotificationFlags  flags)
{
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     FUSION_SKIRMISH_ASSERT( &surface->lock );
     D_FLAGS_ASSERT( flags, CSNF_ALL );

     direct_serial_increase( &surface->serial );

     if (!(surface->state & CSSF_DESTROYED)) {
          if (!(surface->notifications & flags))
               return DFB_OK;
     }

     notification.flags   = flags;
     notification.surface = surface;

     return dfb_surface_dispatch( surface, &notification, dfb_surface_globals );
}

DFBResult
dfb_surface_notify_display( CoreSurface       *surface,
                            CoreSurfaceBuffer *buffer )
{
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     FUSION_SKIRMISH_ASSERT( &surface->lock );

     direct_serial_increase( &surface->serial );

     notification.flags   = CSNF_DISPLAY;
     notification.surface = surface;
     notification.index   = dfb_surface_buffer_index( buffer );

     return dfb_surface_dispatch( surface, &notification, dfb_surface_globals );
}

DFBResult
dfb_surface_flip( CoreSurface *surface, bool swap )
{
     unsigned int back, front;

     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     if (surface->num_buffers == 0)
          return DFB_SUSPENDED;
     
     back  = (surface->flips + CSBR_BACK)  % surface->num_buffers;
     front = (surface->flips + CSBR_FRONT) % surface->num_buffers;

     D_ASSERT( surface->buffer_indices[back]  < surface->num_buffers );
     D_ASSERT( surface->buffer_indices[front] < surface->num_buffers );

     if (surface->buffers[surface->buffer_indices[back]]->policy !=
         surface->buffers[surface->buffer_indices[front]]->policy || (surface->config.caps & DSCAPS_ROTATED))
          return DFB_UNSUPPORTED;

     if (swap) {
          int tmp = surface->buffer_indices[back];
          surface->buffer_indices[back] = surface->buffer_indices[front];
          surface->buffer_indices[front] = tmp;
     }
     else
          surface->flips++;

     dfb_surface_notify( surface, CSNF_FLIP );

     return DFB_OK;
}

DFBResult
dfb_surface_reconfig( CoreSurface             *surface,
                      const CoreSurfaceConfig *config )
{
     int i, buffers;
     DFBResult ret;
     DFBSurfaceStereoEye eye;
     int num_eyes;
     CoreSurfaceConfig new_config;

     D_DEBUG_AT( Core_Surface, "%s( %p, %dx%d %s -> %dx%d %s )\n", __FUNCTION__, surface,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name( surface->config.format ),
                 (config->flags & CSCONF_SIZE) ? config->size.w : surface->config.size.w,
                 (config->flags & CSCONF_SIZE) ? config->size.h : surface->config.size.h,
                 (config->flags & CSCONF_FORMAT) ? dfb_pixelformat_name( config->format ) :
                                                   dfb_pixelformat_name( surface->config.format ) );

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( config != NULL );

     if (config->flags & CSCONF_PREALLOCATED)
          return DFB_UNSUPPORTED;

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     if (  (config->flags == CSCONF_SIZE ||
          ((config->flags == (CSCONF_SIZE | CSCONF_FORMAT)) && (config->format == surface->config.format)))  &&
         config->size.w <= surface->config.min_size.w &&
         config->size.h <= surface->config.min_size.h)
     {
          surface->config.size = config->size;

          fusion_skirmish_dismiss( &surface->lock );
          return DFB_OK;
     }

     new_config = surface->config;

     if (config->flags & CSCONF_SIZE)
          new_config.size = config->size;

     if (config->flags & CSCONF_FORMAT)
          new_config.format = config->format;

     if (config->flags & CSCONF_CAPS) {
          if (config->caps & DSCAPS_ROTATED)
               D_UNIMPLEMENTED();

          new_config.caps = config->caps & ~DSCAPS_ROTATED;
     }

     if (new_config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (surface->type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (new_config.caps & DSCAPS_VIDEOONLY)
          surface->type = (surface->type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = surface->type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     if (new_config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (new_config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else {
          buffers = 1;

          new_config.caps &= ~DSCAPS_ROTATED;
     }

     ret = Core_Resource_CheckSurfaceUpdate( surface, &new_config );
     if (ret)
          return ret;

     /* Destroy the Surface Buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye=DSSE_LEFT; num_eyes>0; num_eyes--, eye=DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i=0; i<surface->num_buffers; i++) {
               dfb_surface_buffer_decouple( surface->buffers[i] );
               surface->buffers[i] = NULL;
          }
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     surface->num_buffers = 0;

     Core_Resource_UpdateSurface( surface, &new_config );

     surface->config = new_config;

     /* Recreate the Surface Buffers. */
     num_eyes = new_config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye=DSSE_LEFT; num_eyes>0; num_eyes--, eye=DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i=0; i<buffers; i++) {
               CoreSurfaceBuffer *buffer;
     
               ret = dfb_surface_buffer_create( core_dfb, surface, CSBF_NONE, &buffer );
               if (ret) {
                    D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
                    goto error;
               }

               dfb_surface_buffer_globalize( buffer );

               surface->buffers[i] = buffer;
               if (eye == DSSE_LEFT)
                    surface->num_buffers++;
     
               switch (i) {
                    case 0:
                         surface->buffer_indices[CSBR_FRONT] = i;
                    case 1:
                         surface->buffer_indices[CSBR_BACK] = i;
                    case 2:
                         surface->buffer_indices[CSBR_IDLE] = i;
               }
          }
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     dfb_surface_notify( surface, CSNF_SIZEFORMAT );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;

error:
     D_UNIMPLEMENTED();

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_destroy_buffers( CoreSurface *surface )
{
     int i, num_eyes;
     DFBSurfaceStereoEye eye;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     /* Destroy the Surface Buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i = 0; i < surface->num_buffers; i++) {
               dfb_surface_buffer_decouple( surface->buffers[i] );
               surface->buffers[i] = NULL;
          }
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     surface->num_buffers = 0;

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_deallocate_buffers( CoreSurface *surface )
{
     int i, num_eyes;
     DFBSurfaceStereoEye eye;

     D_DEBUG_AT( Core_Surface, "%s( %p )\n", __FUNCTION__, surface );

     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->type & CSTF_PREALLOCATED) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_UNSUPPORTED;
     }

     /* Deallocate the Surface Buffers. */
     num_eyes = surface->config.caps & DSCAPS_STEREO ? 2 : 1;
     for (eye = DSSE_LEFT; num_eyes > 0; num_eyes--, eye = DSSE_RIGHT) {
          dfb_surface_set_stereo_eye(surface, eye);
          for (i = 0; i < surface->num_buffers; i++)
               dfb_surface_buffer_deallocate( surface->buffers[i] );
     }
     dfb_surface_set_stereo_eye(surface, DSSE_LEFT);

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_lock_buffer( CoreSurface            *surface,
                         CoreSurfaceBufferRole   role,
                         CoreSurfaceAccessorID   accessor,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *ret_lock )
{
     DFBResult              ret;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Core_Surface, "%s( 0x%02x 0x%02x ) <- %dx%d %s [%d]\n", __FUNCTION__, accessor, access,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(surface->config.format),
                 role );

     ret = CoreSurface_PreLockBuffer2( surface, role,
                                       dfb_surface_get_stereo_eye(surface), // FIXME: make argument to dfb_surface_lock_buffer
                                       accessor, access, true, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     /* Lock the allocation. */
     dfb_surface_buffer_lock_init( ret_lock, accessor, access );

     ret = dfb_surface_pool_lock( allocation->pool, allocation, ret_lock );
     if (ret) {
          D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                    allocation->pool->desc.name );
          dfb_surface_buffer_lock_deinit( ret_lock );

          dfb_surface_allocation_unref( allocation );
          return ret;
     }

     return DFB_OK;
}

DFBResult
dfb_surface_unlock_buffer( CoreSurface           *surface,
                           CoreSurfaceBufferLock *lock )
{
     DFBResult ret;

     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = dfb_surface_buffer_unlock( lock );

     return ret;
}

DFBResult
dfb_surface_read_buffer( CoreSurface            *surface,
                         CoreSurfaceBufferRole   role,
                         void                   *destination,
                         int                     pitch,
                         const DFBRectangle     *prect )
{
     DFBResult              ret;
     int                    y;
     int                    bytes;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( destination != NULL );
     D_ASSERT( pitch > 0 );
     DFB_RECTANGLE_ASSERT_IF( prect );

     D_DEBUG_AT( Core_Surface, "%s( %p, %p [%d] )\n", __FUNCTION__, surface, destination, pitch );

     /* Determine area. */
     rect.x = 0;
     rect.y = 0;
     rect.w = surface->config.size.w;
     rect.h = surface->config.size.h;

     if (prect && (!dfb_rectangle_intersect( &rect, prect ) || !DFB_RECTANGLE_EQUAL( rect, *prect )))
          return DFB_INVAREA;

     /* Calculate bytes per read line. */
     format = surface->config.format;
     bytes  = DFB_BYTES_PER_LINE( format, rect.w );

     D_DEBUG_AT( Core_Surface, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( format ) );

     ret = CoreSurface_PreLockBuffer2( surface, role,
                                       dfb_surface_get_stereo_eye(surface), // FIXME: make argument to dfb_surface_read_buffer
                                       CSAID_CPU, CSAF_READ, false, &allocation );
     if (ret == DFB_NOALLOCATION) {
          for (y=0; y<rect.h; y++) {
               memset( destination, 0, bytes );

               destination += pitch;
          }

          return DFB_OK;
     }
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     /* Try reading from allocation directly... */
     ret = dfb_surface_pool_read( allocation->pool, allocation, destination, pitch, &rect );
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_READ) {
               CoreSurfaceBufferLock lock;

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_READ );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                              allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
                    return ret;
               }

               /* Move to start of read. */
               lock.addr += DFB_BYTES_PER_LINE( format, rect.x ) + rect.y * lock.pitch;

               /* Copy the data. */
               for (y=0; y<rect.h; y++) {
                    direct_memcpy( destination, lock.addr, bytes );

                    destination += pitch;
                    lock.addr   += lock.pitch;
               }

               /* Unlock the allocation. */
               ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
               if (ret)
                    D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", allocation->pool->desc.name );

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_write_buffer( CoreSurface            *surface,
                          CoreSurfaceBufferRole   role,
                          const void             *source,
                          int                     pitch,
                          const DFBRectangle     *prect )
{
     DFBResult              ret;
     int                    bytes;
     DFBRectangle           rect;
     DFBSurfacePixelFormat  format;
     CoreSurfaceAllocation *allocation;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( pitch > 0 || source == NULL );
     DFB_RECTANGLE_ASSERT_IF( prect );

     D_DEBUG_AT( Core_Surface, "%s( %p, %p [%d] )\n", __FUNCTION__, surface, source, pitch );

     /* Determine area. */
     rect.x = 0;
     rect.y = 0;
     rect.w = surface->config.size.w;
     rect.h = surface->config.size.h;

     if (prect) {
          if (!dfb_rectangle_intersect( &rect, prect )) {
               D_DEBUG_AT( Core_Surface, "  -> no intersection!\n" );
               return DFB_INVAREA;
          }

          if (!DFB_RECTANGLE_EQUAL( rect, *prect )) {
               D_DEBUG_AT( Core_Surface, "  -> got clipped to %d,%d-%dx%d!\n", DFB_RECTANGLE_VALS(&rect) );
               return DFB_INVAREA;
          }
     }

     /* Calculate bytes per read line. */
     format = surface->config.format;
     bytes  = DFB_BYTES_PER_LINE( format, rect.w );

     D_DEBUG_AT( Core_Surface, "  -> %d,%d - %dx%d (%s)\n", DFB_RECTANGLE_VALS(&rect),
                 dfb_pixelformat_name( format ) );

     ret = CoreSurface_PreLockBuffer2( surface, role,
                                       dfb_surface_get_stereo_eye(surface), // FIXME: make argument to dfb_surface_read_buffer
                                       CSAID_CPU, CSAF_WRITE, false, &allocation );
     if (ret)
          return ret;

     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     D_DEBUG_AT( Core_Surface, "  -> PreLockBuffer returned allocation %p (%s)\n", allocation, allocation->pool->desc.name );

     /* Try writing to allocation directly... */
     ret = source ? dfb_surface_pool_write( allocation->pool, allocation, source, pitch, &rect ) : DFB_UNSUPPORTED;
     if (ret) {
          /* ...otherwise use fallback method via locking if possible. */
          if (allocation->access[CSAID_CPU] & CSAF_WRITE) {
               int                   y;
               int                   bytes;
               DFBSurfacePixelFormat format;
               CoreSurfaceBufferLock lock;

               /* Calculate bytes per written line. */
               format = surface->config.format;
               bytes  = DFB_BYTES_PER_LINE( format, rect.w );

               /* Lock the allocation. */
               dfb_surface_buffer_lock_init( &lock, CSAID_CPU, CSAF_WRITE );

               ret = dfb_surface_pool_lock( allocation->pool, allocation, &lock );
               if (ret) {
                    D_DERROR( ret, "Core/SurfBuffer: Locking allocation failed! [%s]\n",
                              allocation->pool->desc.name );
                    dfb_surface_buffer_lock_deinit( &lock );
                    dfb_surface_allocation_unref( allocation );
                    return ret;
               }

               /* Move to start of write. */
               lock.addr += DFB_BYTES_PER_LINE( format, rect.x ) + rect.y * lock.pitch;

               /* Copy the data. */
               for (y=0; y<rect.h; y++) {
                    if (source) {
                         direct_memcpy( lock.addr, source, bytes );

                         source += pitch;
                    }
                    else
                         memset( lock.addr, 0, bytes );

                    lock.addr += lock.pitch;
               }

               /* Unlock the allocation. */
               ret = dfb_surface_pool_unlock( allocation->pool, allocation, &lock );
               if (ret)
                    D_DERROR( ret, "Core/SurfBuffer: Unlocking allocation failed! [%s]\n", allocation->pool->desc.name );

               dfb_surface_buffer_lock_deinit( &lock );
          }
     }

     dfb_surface_allocation_unref( allocation );

     return DFB_OK;
}

DFBResult
dfb_surface_dump_buffer( CoreSurface           *surface,
                         CoreSurfaceBufferRole  role,
                         const char            *path,
                         const char            *prefix )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( path != NULL );
     D_ASSERT( prefix != NULL );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->num_buffers == 0) {
          fusion_skirmish_dismiss( &surface->lock );
          return DFB_SUSPENDED;
     }

     buffer = dfb_surface_get_buffer( surface, role );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = dfb_surface_buffer_dump( buffer, path, prefix );

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_set_palette( CoreSurface *surface,
                         CorePalette *palette )
{
     D_MAGIC_ASSERT( surface, CoreSurface );
     D_MAGIC_ASSERT_IF( palette, CorePalette );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (surface->palette != palette) {
          if (surface->palette) {
               dfb_palette_detach_global( surface->palette, &surface->palette_reaction );
               dfb_palette_unlink( &surface->palette );
          }

          if (palette) {
               dfb_palette_link( &surface->palette, palette );
               dfb_palette_attach_global( palette, DFB_SURFACE_PALETTE_LISTENER,
                                          surface, &surface->palette_reaction );
          }

          dfb_surface_notify( surface, CSNF_PALETTE_CHANGE );
     }

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_set_field( CoreSurface *surface,
                       int          field )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     surface->field = field;

     dfb_surface_notify( surface, CSNF_FIELD );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

DFBResult
dfb_surface_set_alpha_ramp( CoreSurface *surface,
                            u8           a0,
                            u8           a1,
                            u8           a2,
                            u8           a3 )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     surface->alpha_ramp[0] = a0;
     surface->alpha_ramp[1] = a1;
     surface->alpha_ramp[2] = a2;
     surface->alpha_ramp[3] = a3;

     dfb_surface_notify( surface, CSNF_ALPHA_RAMP );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;
}

ReactionResult
_dfb_surface_palette_listener( const void *msg_data,
                               void       *ctx )
{
     const CorePaletteNotification *notification = msg_data;
     CoreSurface                   *surface      = ctx;

     if (notification->flags & CPNF_DESTROY)
          return RS_REMOVE;

     if (notification->flags & CPNF_ENTRIES) {
          if (fusion_skirmish_prevail( &surface->lock ))
               return RS_OK;

          dfb_surface_notify( surface, CSNF_PALETTE_UPDATE );

          fusion_skirmish_dismiss( &surface->lock );
     }

     return RS_OK;
}

