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

#include <direct/debug.h>

#include <core/core.h>
#include <core/palette.h>
#include <core/surface.h>

#include <core/layers_internal.h>
#include <core/windows_internal.h>

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
                 surface->config.size.h, surface->config.size.h, zombie ? " ZOMBIE" : "");

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
               dfb_surface_buffer_destroy( surface->buffers[i] );
     }

     direct_serial_deinit( &surface->serial );

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
     DFBResult    ret = DFB_BUG;
     int          i;
     int          buffers;
     CoreSurface *surface;
     char         buf[64];

     D_ASSERT( core != NULL );
     D_FLAGS_ASSERT( type, CSTF_ALL );
     D_MAGIC_ASSERT_IF( palette, CorePalette );
     D_ASSERT( ret_surface != NULL );

     D_DEBUG_AT( Core_Surface, "dfb_surface_create( %p, %p, %p )\n", core, config, ret_surface );

     surface = dfb_core_create_surface( core );
     if (!surface)
          return DFB_FUSION;

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

          if (config->flags & CSCONF_CAPS) {
               D_DEBUG_AT( Core_Surface, "  -> caps 0x%08x\n", config->caps );

               surface->config.caps = config->caps;
          }

          if (config->flags & CSCONF_PREALLOCATED) {
               D_DEBUG_AT( Core_Surface, "  -> prealloc %p [%d]\n",
                           config->preallocated[0].addr,
                           config->preallocated[0].pitch );

               direct_memcpy( surface->config.preallocated, config->preallocated, sizeof(config->preallocated) );

               type |= CSTF_PREALLOCATED;
          }
     }

     if (surface->config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (surface->config.caps & DSCAPS_VIDEOONLY)
          surface->type = (type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     surface->resource_id = resource_id;

     if (surface->config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (surface->config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else
          buffers = 1;

     surface->notifications = CSNF_ALL & ~CSNF_FLIP;

     surface->alpha_ramp[0] = 0x00;
     surface->alpha_ramp[1] = 0x55;
     surface->alpha_ramp[2] = 0xaa;
     surface->alpha_ramp[3] = 0xff;


     if (surface->config.caps & DSCAPS_STATIC_ALLOC)
          surface->config.min_size = surface->config.size;

     surface->shmpool = dfb_core_shmpool( core );

     direct_serial_init( &surface->serial );

     snprintf( buf, sizeof(buf), "Surface %dx%d %s", surface->config.size.w,
               surface->config.size.h, dfb_pixelformat_name(surface->config.format) );
     fusion_skirmish_init( &surface->lock, buf, dfb_core_world(core) );

     fusion_object_set_lock( &surface->object, &surface->lock );

     D_MAGIC_SET( surface, CoreSurface );


     if (palette) {
          dfb_surface_set_palette( surface, palette );
     }
     else if (DFB_PIXELFORMAT_IS_INDEXED( surface->config.format )) {
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( core,
                                    1 << DFB_COLOR_BITS_PER_PIXEL( surface->config.format ),
                                    &palette );
          if (ret) {
               D_DERROR( ret, "Core/Surface: Error creating palette!\n" );
               goto error;
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
     }

     /* Create the Surface Buffers. */
     for (i=0; i<buffers; i++) {
          CoreSurfaceBuffer *buffer;

          ret = dfb_surface_buffer_new( surface, CSBF_NONE, &buffer );
          if (ret) {
               D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
               goto error;
          }

          surface->buffers[surface->num_buffers++] = buffer;

          switch (i) {
               case 0:
                    surface->buffer_indices[CSBR_FRONT] = i;
               case 1:
                    surface->buffer_indices[CSBR_BACK] = i;
               case 2:
                    surface->buffer_indices[CSBR_IDLE] = i;
          }
     }

     fusion_object_activate( &surface->object );

     *ret_surface = surface;

     return DFB_OK;

error:
     D_MAGIC_CLEAR( surface );

     for (i=0; i<MAX_SURFACE_BUFFERS; i++) {
          if (surface->buffers[i])
               dfb_surface_buffer_destroy( surface->buffers[i] );
     }

     fusion_skirmish_destroy( &surface->lock );

     direct_serial_deinit( &surface->serial );

     fusion_object_destroy( &surface->object );

     return ret;
}

DFBResult
dfb_surface_create_simple ( CoreDFB                 *core,
                            int                      width,
                            int                      height,
                            DFBSurfacePixelFormat    format,
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

     config.flags  = CSCONF_SIZE | CSCONF_FORMAT | CSCONF_CAPS;
     config.size.w = width;
     config.size.h = height;
     config.format = format;
     config.caps   = caps;

     return dfb_surface_create( core, &config, type, resource_id, palette, ret_surface );
}

DFBResult
dfb_surface_notify( CoreSurface                  *surface,
                    CoreSurfaceNotificationFlags  flags)
{
     CoreSurfaceNotification notification;

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_FLAGS_ASSERT( flags, CSNF_ALL );

     direct_serial_increase( &surface->serial );

     if (!(surface->state & CSSF_DESTROYED)) {
          FUSION_SKIRMISH_ASSERT( &surface->lock );

          if (!(surface->notifications & flags))
               return DFB_OK;
     }

     notification.flags   = flags;
     notification.surface = surface;

     return dfb_surface_dispatch( surface, &notification, dfb_surface_globals );
}

DFBResult
dfb_surface_flip( CoreSurface *surface, bool swap )
{
     D_MAGIC_ASSERT( surface, CoreSurface );

     FUSION_SKIRMISH_ASSERT( &surface->lock );

     D_ASSERT( surface->buffer_indices[CSBR_BACK]  < surface->num_buffers );
     D_ASSERT( surface->buffer_indices[CSBR_FRONT] < surface->num_buffers );

     if (surface->buffers[surface->buffer_indices[CSBR_BACK]]->policy !=
         surface->buffers[surface->buffer_indices[CSBR_FRONT]]->policy)
          return DFB_UNSUPPORTED;

     if (swap) {
          int tmp = surface->buffer_indices[CSBR_BACK];

          surface->buffer_indices[CSBR_BACK]  = surface->buffer_indices[CSBR_FRONT];
          surface->buffer_indices[CSBR_FRONT] = tmp;
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

     D_MAGIC_ASSERT( surface, CoreSurface );
     D_ASSERT( config != NULL );

     if (surface->type & CSTF_PREALLOCATED)
          return DFB_UNSUPPORTED;

     if (config->flags & CSCONF_PREALLOCATED)
          return DFB_UNSUPPORTED;

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     if (  (config->flags == CSCONF_SIZE ||
          ((config->flags == (CSCONF_SIZE | CSCONF_FORMAT)) && (config->format == surface->config.format)))  &&
         config->size.w <= surface->config.min_size.w &&
         config->size.h <= surface->config.min_size.h)
     {
          surface->config.size = config->size;

          fusion_skirmish_dismiss( &surface->lock );
          return DFB_OK;
     }

     /* Destroy the Surface Buffers. */
     for (i=0; i<surface->num_buffers; i++) {
          dfb_surface_buffer_destroy( surface->buffers[i] );
          surface->buffers[i] = NULL;
     }

     surface->num_buffers = 0;

     if (config->flags & CSCONF_SIZE)
          surface->config.size = config->size;

     if (config->flags & CSCONF_FORMAT)
          surface->config.format = config->format;

     if (config->flags & CSCONF_CAPS)
          surface->config.caps = config->caps;

     if (surface->config.caps & DSCAPS_SYSTEMONLY)
          surface->type = (surface->type & ~CSTF_EXTERNAL) | CSTF_INTERNAL;
     else if (surface->config.caps & DSCAPS_VIDEOONLY)
          surface->type = (surface->type & ~CSTF_INTERNAL) | CSTF_EXTERNAL;
     else
          surface->type = surface->type & ~(CSTF_INTERNAL | CSTF_EXTERNAL);

     if (surface->config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (surface->config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else
          buffers = 1;

     /* Recreate the Surface Buffers. */
     for (i=0; i<buffers; i++) {
          CoreSurfaceBuffer *buffer;

          ret = dfb_surface_buffer_new( surface, CSBF_NONE, &buffer );
          if (ret) {
               D_DERROR( ret, "Core/Surface: Error creating surface buffer!\n" );
               goto error;
          }

          surface->buffers[surface->num_buffers++] = buffer;

          switch (i) {
               case 0:
                    surface->buffer_indices[CSBR_FRONT] = i;
               case 1:
                    surface->buffer_indices[CSBR_BACK] = i;
               case 2:
                    surface->buffer_indices[CSBR_IDLE] = i;
          }
     }

     dfb_surface_notify( surface, CSNF_SIZEFORMAT );

     fusion_skirmish_dismiss( &surface->lock );

     return DFB_OK;

error:
     D_UNIMPLEMENTED();

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}


DFBResult
dfb_surface_lock_buffer( CoreSurface            *surface,
                         CoreSurfaceBufferRole   role,
                         CoreSurfaceAccessFlags  access,
                         CoreSurfaceBufferLock  *ret_lock )
{
     DFBResult          ret;
     CoreSurfaceBuffer *buffer;

     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     buffer = dfb_surface_get_buffer( surface, role );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     ret = dfb_surface_buffer_lock( buffer, access, ret_lock );

     fusion_skirmish_dismiss( &surface->lock );

     return ret;
}

DFBResult
dfb_surface_unlock_buffer( CoreSurface           *surface,
                           CoreSurfaceBufferLock *lock )
{
     DFBResult ret;

     D_MAGIC_ASSERT( surface, CoreSurface );

     if (fusion_skirmish_prevail( &surface->lock ))
          return DFB_FUSION;

     ret = dfb_surface_buffer_unlock( lock );

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
                            __u8         a0,
                            __u8         a1,
                            __u8         a2,
                            __u8         a3 )
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

