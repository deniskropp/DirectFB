/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>

#include <core/Debug.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <xf86drm.h>
#include <i915_drm.h>
#include <drm_fourcc.h>

#include "drmkms_system.h"

D_DEBUG_DOMAIN( DRMKMS_Surfaces, "DRMKMS/Surfaces", "DRMKMS Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( DRMKMS_SurfLock, "DRMKMS/SurfLock", "DRMKMS Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

struct kms_bo
{
	struct kms_driver *kms;
	void *ptr;
	size_t size;
	size_t offset;
	size_t pitch;
	unsigned handle;
};


typedef struct {
     int             magic;
} DRMKMSPoolData;

typedef struct {
     int             magic;

     DRMKMSData     *drmkms;

     DirectHash     *hash;
     DirectMutex     lock;
} DRMKMSPoolLocalData;

typedef struct {
     int   magic;

     unsigned int        pitch;
     int                 size;
     int                 offset;

     unsigned int        handle;
     int                 prime_fd;
     u32                 name;

#ifdef USE_GBM
     struct gbm_bo      *bo;
     struct gbm_surface *gs;
#else
     struct kms_bo      *bo;
#endif

     uint32_t    fb_id;
     void       *addr;
} DRMKMSAllocationData;

typedef struct {
     int                   magic;

     DRMKMSPoolLocalData  *pool_local;
     DRMKMSAllocationData *alloc;

     FusionObjectID        alloc_id;

     unsigned int          pitch;
     int                   size;
     int                   offset;

     unsigned int          handle;

     void                 *addr;

     Reaction              reaction;

#ifdef USE_GBM
     struct gbm_bo        *bo;
     struct gbm_surface   *gs;
#else
     struct kms_bo         bo;
#endif
} DRMKMSAllocationLocalData;

/**********************************************************************************************************************/

static int
drmkmsPoolDataSize( void )
{
     return sizeof(DRMKMSPoolData);
}

static int
drmkmsPoolLocalDataSize( void )
{
     return sizeof(DRMKMSPoolLocalData);
}

static int
drmkmsAllocationDataSize( void )
{
     return sizeof(DRMKMSAllocationData);
}

static DFBResult
InitLocal( DRMKMSPoolLocalData *local,
           CoreDFB             *core )
{
     direct_hash_create( 17, &local->hash );
     direct_mutex_init( &local->lock );

     return DFB_OK;
}

static void
DeinitLocal( DRMKMSPoolLocalData *local )
{
     direct_mutex_init( &local->lock );
     direct_hash_destroy( local->hash );
}

static DFBResult
drmkmsInitPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data,
                CoreSurfacePoolDescription *ret_desc )
{
     DRMKMSPoolData      *data   = pool_data;
     DRMKMSPoolLocalData *local  = pool_local;
     DRMKMSData          *drmkms = system_data;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_ULTIMATE;
     ret_desc->size              = dfb_config->video_length;

     /* For hardware layers */
     ret_desc->access[CSAID_LAYER0] = CSAF_READ;
     ret_desc->access[CSAID_LAYER1] = CSAF_READ;
     ret_desc->access[CSAID_LAYER2] = CSAF_READ;
     ret_desc->access[CSAID_LAYER3] = CSAF_READ;
     ret_desc->access[CSAID_LAYER4] = CSAF_READ;
     ret_desc->access[CSAID_LAYER5] = CSAF_READ;
     ret_desc->access[CSAID_LAYER6] = CSAF_READ;
     ret_desc->access[CSAID_LAYER7] = CSAF_READ;
     ret_desc->access[CSAID_LAYER8] = CSAF_READ;
     ret_desc->access[CSAID_LAYER9] = CSAF_READ;
     ret_desc->access[CSAID_LAYER10] = CSAF_READ;
     ret_desc->access[CSAID_LAYER11] = CSAF_READ;
     ret_desc->access[CSAID_LAYER12] = CSAF_READ;
     ret_desc->access[CSAID_LAYER13] = CSAF_READ;
     ret_desc->access[CSAID_LAYER14] = CSAF_READ;
     ret_desc->access[CSAID_LAYER15] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "DRMKMS" );

     local->drmkms = drmkms;


     InitLocal( local, core );


     D_MAGIC_SET( data, DRMKMSPoolData );
     D_MAGIC_SET( local, DRMKMSPoolLocalData );

     return DFB_OK;
}

static DFBResult
drmkmsJoinPool( CoreDFB                    *core,
                CoreSurfacePool            *pool,
                void                       *pool_data,
                void                       *pool_local,
                void                       *system_data )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;
     DRMKMSData          *drmkms  = system_data;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( drmkms != NULL );
     D_ASSERT( drmkms->shared != NULL );

     (void) data;

     local->drmkms = drmkms;

     InitLocal( local, core );

     D_MAGIC_SET( local, DRMKMSPoolLocalData );

     return DFB_OK;
}

static DFBResult
drmkmsDestroyPool( CoreSurfacePool *pool,
                   void            *pool_data,
                   void            *pool_local )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );

     DeinitLocal( local );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
drmkmsLeavePool( CoreSurfacePool *pool,
                 void            *pool_data,
                 void            *pool_local )
{
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s()\n", __FUNCTION__ );

     (void) data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );

     DeinitLocal( local );

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
drmkmsTestConfig( CoreSurfacePool         *pool,
                  void                    *pool_data,
                  void                    *pool_local,
                  CoreSurfaceBuffer       *buffer,
                  const CoreSurfaceConfig *config )
{
     CoreSurface         *surface;
     DRMKMSPoolData      *data  = pool_data;
     DRMKMSPoolLocalData *local = pool_local;

     (void)data;
     (void)local;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     switch (surface->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB16:
          case DSPF_RGB555:
          case DSPF_ARGB1555:
          case DSPF_RGB332:
          case DSPF_RGB24:
          case DSPF_A8:
          case DSPF_UYVY:
          case DSPF_YUY2:
          case DSPF_NV12:
          case DSPF_NV21:
          case DSPF_NV16:
               break;
          default:
               D_ERROR( "DirectFB/DRMKMS: unsupported pixelformat!\n" );
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
drmkmsAllocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     int                   ret;
     int                   pitch;
     int                   length;
     CoreGraphicsDevice   *device;
     CoreSurface          *surface;
     DRMKMSPoolData       *data  = pool_data;
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;

     u32 drm_format;
     u32 drm_fake_width;
     u32 drm_fake_height;

     (void)data;
     (void)local;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( allocation %p )\n", __FUNCTION__, allocation );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> %s\n", ToString_CoreSurfaceAllocation( allocation ) );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     /* FIXME: Only one global device at the moment. */
     device = dfb_core_get_part( core_dfb, DFCP_GRAPHICS );
     D_ASSERT( device != NULL );

     dfb_gfxcard_calc_buffer_size( device, buffer, &pitch, &length );

     drm_fake_width  = (pitch + 3) >> 2;
     drm_fake_height = surface->config.size.h;

     switch (surface->config.format) {
          case DSPF_ARGB:
               drm_format = DRM_FORMAT_ARGB8888;
               break;
          case DSPF_RGB32:
               drm_format = DRM_FORMAT_XRGB8888;
               break;
          case DSPF_RGB16:
               drm_format = DRM_FORMAT_RGB565;
               break;
          case DSPF_RGB555:
               drm_format = DRM_FORMAT_XRGB1555;
               break;
          case DSPF_ARGB1555:
               drm_format = DRM_FORMAT_ARGB1555;
               break;
          case DSPF_RGB332:
               drm_format = DRM_FORMAT_RGB332;
               break;
          case DSPF_RGB24:
               drm_format = DRM_FORMAT_RGB888;
               break;
          case DSPF_A8:
               drm_format = DRM_FORMAT_C8;
               break;
          case DSPF_UYVY:
               drm_format = DRM_FORMAT_UYVY;
               break;
          case DSPF_YUY2:
               drm_format = DRM_FORMAT_YUYV;
               break;
          case DSPF_NV12:
               drm_format = DRM_FORMAT_NV12;
               drm_fake_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV21:
               drm_format = DRM_FORMAT_NV21;
               drm_fake_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV16:
               drm_format = DRM_FORMAT_NV16;
               drm_fake_height = surface->config.size.h << 2;
               break;

          default:
               D_ERROR( "DirectFB/DRMKMS: unsupported pixelformat!\n" );
               return DFB_FAILURE;
     }

#ifdef USE_GBM
     alloc->bo = gbm_bo_create( drmkms->gbm, drm_fake_width, drm_Fake_height, GBM_BO_FORMAT_ARGB8888,
                                                                            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );
     alloc->handle = gbm_bo_get_handle( alloc->bo ).u32;
     alloc->pitch  = gbm_bo_get_stride( alloc->bo );
#else
     unsigned attr[] = { KMS_BO_TYPE, KMS_BO_TYPE_SCANOUT_X8R8G8B8, KMS_WIDTH, drm_fake_width, KMS_HEIGHT, drm_fake_height, KMS_TERMINATE_PROP_LIST };

     ret = kms_bo_create(drmkms->kms, attr, &alloc->bo);
     if (ret) {
          D_ERROR( "DirectFB/DRMKMS: kms_bo_create( '%s' ) failed (%s)!\n", ToString_CoreSurfaceAllocation( allocation ), strerror(-ret) );
          ret = errno2result( -ret );
          goto error;
     }

     kms_bo_get_prop(alloc->bo, KMS_HANDLE, &alloc->handle);
     kms_bo_get_prop(alloc->bo, KMS_PITCH, &alloc->pitch);
#endif

     alloc->size = alloc->pitch * drm_fake_height;

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo        %p\n", alloc->bo );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo.handle %u\n", alloc->handle );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo.pitch  %u\n", alloc->pitch );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> size      %u\n", alloc->size );

     if (drmkms->shared->use_prime_fd) {
          // this seems to render the handle unusable on radeon
          ret = drmPrimeHandleToFD( drmkms->fd, alloc->handle, DRM_CLOEXEC, &alloc->prime_fd );
          if (ret) {
               ret = errno2result( errno );
               D_ERROR( "DirectFB/DRMKMS: drmPrimeHandleToFD( %u '%s' ) failed (%s)!\n",
                        alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
               goto error;
          }

          D_DEBUG_AT( DRMKMS_Surfaces, "  -> prime_fd  %d\n", alloc->prime_fd );
     }


     struct drm_gem_flink fl;

     fl.handle = alloc->handle;
     fl.name   = 0;

     ret = drmIoctl( drmkms->fd, DRM_IOCTL_GEM_FLINK, &fl );
     if (ret) {
          ret = errno2result( errno );
          D_ERROR( "DirectFB/DRMKMS: DRM_IOCTL_GEM_FLINK( %u '%s' ) failed (%s)!\n",
                   alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
          goto error;
     }

     alloc->name = fl.name;

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> name      %d\n", alloc->name );


     allocation->size   = alloc->size;
     allocation->offset = (unsigned long) alloc->prime_fd;


     /*
      * Mode Framebuffer
      */

     if (surface->type & (CSTF_LAYER | CSTF_WINDOW)) {
          u32 drm_bo_offsets[4] = {0,0,0,0};
          u32 drm_bo_handles[4] = {0,0,0,0};
          u32 drm_bo_pitches[4] = {0,0,0,0};

          drm_bo_handles[0] = drm_bo_handles[1] = drm_bo_handles[2] = drm_bo_handles[3] = alloc->handle;
          drm_bo_pitches[0] = drm_bo_pitches[1] = drm_bo_pitches[2] = drm_bo_pitches[3] = alloc->pitch;
          drm_bo_offsets[1] = surface->config.size.h * alloc->pitch;

          ret = drmModeAddFB2( drmkms->fd,
                               surface->config.size.w, surface->config.size.h, drm_format,
                               drm_bo_handles, drm_bo_pitches, drm_bo_offsets, &alloc->fb_id, 0 );

          if (ret) {
               ret = errno2result( errno );
               D_ERROR( "DirectFB/DRMKMS: drmModeAddFB2( %u '%s' ) failed (%s)!\n",
                        alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
               goto error;
          }

          D_DEBUG_AT( DRMKMS_Surfaces, "  -> fb_id     %u\n", alloc->fb_id );
     }


#ifdef USE_GBM
     //FIXME use gbm instead of ioctl
     struct drm_i915_gem_mmap_gtt arg;
     memset(&arg, 0, sizeof(arg));
     arg.handle = alloc->handle;

     drmCommandWriteRead( drmkms->fd, DRM_I915_GEM_MMAP_GTT, &arg, sizeof( arg ) );
     alloc->addr = mmap( 0, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, drmkms->fd, arg.offset );
#else
     ret = kms_bo_map( alloc->bo, &alloc->addr );
     if (ret) {
          ret = errno2result( errno );
          D_ERROR( "DirectFB/DRMKMS: kms_bo_map( %u '%s' ) failed (%s)!\n",
                   alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
          goto error;
     }
#endif

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr      %p (mapped)\n", alloc->addr );



     D_MAGIC_SET( alloc, DRMKMSAllocationData );

     return DFB_OK;


error:
#ifdef USE_GBM
     // FIXME: unmap in GBM case
#else
     if (alloc->addr)
          kms_bo_unmap( alloc->addr );
#endif

     if (alloc->fb_id)
          drmModeRmFB( drmkms->fd,  alloc->fb_id );

     if (alloc->prime_fd)
          close( alloc->prime_fd );

#ifdef USE_GBM
     gbm_bo_destroy( alloc->bo );
#else
     kms_bo_destroy( &alloc->bo );
#endif

     return ret;
}

static DFBResult
drmkmsDeallocateBuffer( CoreSurfacePool       *pool,
                        void                  *pool_data,
                        void                  *pool_local,
                        CoreSurfaceBuffer     *buffer,
                        CoreSurfaceAllocation *allocation,
                        void                  *alloc_data )
{
     DRMKMSPoolData       *data  = pool_data;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSPoolLocalData  *local = pool_local;

     (void)data;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( allocation %p )\n", __FUNCTION__, allocation );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> %s\n", ToString_CoreSurfaceAllocation( allocation ) );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );

     if (!dfb_config->task_manager)
          dfb_gfxcard_sync();

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo        %p\n", alloc->bo );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo.handle %u\n", alloc->handle );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo.pitch  %u\n", alloc->pitch );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> size      %u\n", alloc->size );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> prime_fd  %d\n", alloc->prime_fd );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> fb_id     %u\n", alloc->fb_id );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> addr      %p (mapped)\n", alloc->addr );

#ifdef USE_GBM
     // FIXME: unmap in GBM case
#else
     if (alloc->addr)
          kms_bo_unmap( alloc->bo );
#endif

     if (alloc->fb_id)
          drmModeRmFB( local->drmkms->fd, alloc->fb_id );

     if (alloc->prime_fd)
          close( alloc->prime_fd );

#ifdef USE_GBM
     gbm_bo_destroy( alloc->bo );
#else
     kms_bo_destroy( &alloc->bo );
#endif

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static ReactionResult
drmkmsAllocationReaction( const void *msg_data,
                          void       *ctx )
{
     const CoreSurfaceAllocationNotification *notification = msg_data;
     DRMKMSAllocationLocalData               *local        = ctx;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( local %p )\n", __FUNCTION__, local );

     D_MAGIC_ASSERT( local, DRMKMSAllocationLocalData );

     if (notification->flags & CSANF_DEALLOCATED) {
          D_DEBUG_AT( DRMKMS_Surfaces, "  -> DEALLOCATED\n" );

          struct drm_gem_close cl;

          cl.handle = local->handle;

          drmIoctl( local->pool_local->drmkms->fd, DRM_IOCTL_GEM_CLOSE, &cl );


#ifdef USE_GBM
          // FIXME: unmap in GBM case
#else
          if (local->addr)
               kms_bo_unmap( &local->bo );
#endif

          direct_mutex_lock( &local->pool_local->lock );
          direct_hash_remove( local->pool_local->hash, (unsigned long) local->alloc_id );
          direct_mutex_unlock( &local->pool_local->lock );

          D_MAGIC_CLEAR( local );
          D_FREE( local );

          return RS_REMOVE;
     }

     return RS_OK;
}

static DFBResult
drmkmsLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     DFBResult             ret;
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( allocation %p )\n", __FUNCTION__, allocation );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> %s\n", ToString_CoreSurfaceAllocation( allocation ) );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = ~0;
     lock->addr   = dfb_core_is_master( core_dfb ) ? alloc->addr : NULL;
     lock->phys   = 0;

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( allocation %p )\n", __FUNCTION__, allocation );
     switch (lock->accessor) {
          case CSAID_LAYER0:
               lock->handle = (void*) (long) alloc->fb_id;
               break;

          case CSAID_GPU:
               if (drmkms->shared->use_prime_fd)
                    lock->offset = (unsigned long) alloc->prime_fd;

               lock->handle = (void*) (long) alloc->handle;
               break;

          case CSAID_CPU:
               if (!dfb_core_is_master( core_dfb )) {
#ifdef USE_GBM
                    //FIXME use gbm instead of ioctl
                    struct drm_i915_gem_mmap_gtt arg;
                    memset(&arg, 0, sizeof(arg));
                    arg.handle = alloc->handle;

                    drmCommandWriteRead( local->drmkms->fd, DRM_I915_GEM_MMAP_GTT, &arg, sizeof( arg ) );
                    lock->addr = mmap( 0, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, local->drmkms->fd, arg.offset );
#else
                    DRMKMSAllocationLocalData *alloc_local;

                    alloc_local = direct_hash_lookup( local->hash, (unsigned long) allocation->object.id );
                    if (!alloc_local) {
                         alloc_local = D_CALLOC( 1, sizeof(DRMKMSAllocationLocalData) + 1024 /* for kms driver private data */ );
                         if (!alloc_local)
                              return D_OOM();

                         alloc_local->bo.kms    = drmkms->kms;
                         alloc_local->bo.size   = alloc->size;
                         alloc_local->bo.offset = alloc->offset;
                         alloc_local->bo.pitch  = alloc->pitch;

                         alloc_local->pool_local = local;
                         alloc_local->alloc      = alloc;
                         alloc_local->alloc_id   = allocation->object.id;
                         alloc_local->pitch      = alloc->pitch;
                         alloc_local->size       = alloc->size;
                         alloc_local->offset     = alloc->offset;


                         struct drm_gem_open op;

                         op.name   = alloc->name;
                         op.handle = 0;

                         ret = drmIoctl( drmkms->fd, DRM_IOCTL_GEM_OPEN, &op );
                         if (ret) {
                              ret = errno2result( errno );
                              D_ERROR( "DirectFB/DRMKMS: DRM_IOCTL_GEM_OPEN( %u '%s' ) failed (%s)!\n",
                                       alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
                              D_FREE( alloc_local );
                              return ret;
                         }

                         alloc_local->bo.handle = op.handle;

                         D_DEBUG_AT( DRMKMS_Surfaces, "  -> bo.handle %u\n", alloc_local->bo.handle );

                         ret = kms_bo_map( &alloc_local->bo, &alloc_local->addr );
                         if (ret) {
                              ret = errno2result( errno );
                              D_ERROR( "DirectFB/DRMKMS: kms_bo_map( %u '%s' ) failed (%s)!\n",
                                       alloc->handle, ToString_CoreSurfaceAllocation( allocation ), strerror(errno) );
                              struct drm_gem_close cl;
                              cl.handle = alloc_local->bo.handle;
                              drmIoctl( drmkms->fd, DRM_IOCTL_GEM_CLOSE, &cl );
                              D_FREE( alloc_local );
                              return ret;
                         }

                         D_DEBUG_AT( DRMKMS_Surfaces, "  -> mapped to %p\n", alloc_local->addr );

                         D_MAGIC_SET( alloc_local, DRMKMSAllocationLocalData );

                         direct_hash_insert( local->hash, (unsigned long) allocation->object.id, alloc_local );

                         dfb_surface_allocation_attach( allocation, drmkmsAllocationReaction, alloc_local, &alloc_local->reaction );
                    }
                    else
                         D_MAGIC_ASSERT( alloc_local, DRMKMSAllocationLocalData );

                    lock->addr = alloc_local->addr;
#endif
               }
               break;

          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               break;
     }

     D_DEBUG_AT( DRMKMS_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
drmkmsUnlock( CoreSurfacePool       *pool,
              void                  *pool_data,
              void                  *pool_local,
              CoreSurfaceAllocation *allocation,
              void                  *alloc_data,
              CoreSurfaceBufferLock *lock )
{
     DRMKMSAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     (void) alloc;

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( allocation %p )\n", __FUNCTION__, allocation );
     D_DEBUG_AT( DRMKMS_Surfaces, "  -> %s\n", ToString_CoreSurfaceAllocation( allocation ) );

     switch (lock->accessor) {
          case CSAID_LAYER0:
          case CSAID_GPU:
               break;

          case CSAID_CPU:
               if (!dfb_core_is_master( core_dfb )) {
#ifdef USE_GBM
                    // FIXME: unmap in GBM case
#else
#endif
               }
               break;

          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               break;
     }

     return DFB_OK;
}


const SurfacePoolFuncs drmkmsSurfacePoolFuncs = {
     .PoolDataSize       = drmkmsPoolDataSize,
     .PoolLocalDataSize  = drmkmsPoolLocalDataSize,
     .AllocationDataSize = drmkmsAllocationDataSize,

     .InitPool           = drmkmsInitPool,
     .JoinPool           = drmkmsJoinPool,
     .DestroyPool        = drmkmsDestroyPool,
     .LeavePool          = drmkmsLeavePool,

     .TestConfig         = drmkmsTestConfig,
     .AllocateBuffer     = drmkmsAllocateBuffer,
     .DeallocateBuffer   = drmkmsDeallocateBuffer,

     .Lock               = drmkmsLock,
     .Unlock             = drmkmsUnlock
};

