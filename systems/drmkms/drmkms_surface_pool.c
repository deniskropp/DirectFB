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


#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/debug.h>
#include <direct/mem.h>

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

typedef struct {
     int             magic;
} DRMKMSPoolData;

typedef struct {
     int             magic;

     DRMKMSData     *drmkms;
} DRMKMSPoolLocalData;

typedef struct {
     int   magic;

     unsigned int        pitch;
     int                 size;
     int                 offset;

     unsigned int        handle;
     int                 prime_fd;

#ifdef USE_GBM
     struct gbm_bo      *bo;
     struct gbm_surface *gs;
#else
     struct kms_bo      *bo;
#endif

     uint32_t    fb_id;
} DRMKMSAllocationData;

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
     ret_desc->priority          = CSPP_DEFAULT;
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

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );

     (void) data;

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
     int                 ret;
     CoreSurface        *surface;
     DRMKMSPoolData       *data  = pool_data;
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;

     u32 drm_format        = 0;
     u32 drm_bo_offsets[4] = {0,0,0,0};
     u32 drm_bo_handles[4] = {0,0,0,0};
     u32 drm_bo_pitches[4] = {0,0,0,0};

     u32 drm_fake_width;
     u32 drm_fake_height;

     (void)data;
     (void)local;

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( local, DRMKMSPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     drm_fake_width  = surface->config.size.w;
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
               drm_fake_width = (surface->config.size.w + 1) >> 1;
               break;
          case DSPF_ARGB1555:
               drm_format = DRM_FORMAT_ARGB1555;
               drm_fake_width = (surface->config.size.w + 1) >> 1;
               break;
          case DSPF_RGB332:
               drm_format = DRM_FORMAT_RGB332;
               drm_fake_width = (surface->config.size.w + 3) >> 2;
               break;
          case DSPF_RGB24:
               drm_format = DRM_FORMAT_RGB888;
               drm_fake_width = (surface->config.size.w * 3 + 3) >> 2;
               break;
          case DSPF_A8:
               drm_format = DRM_FORMAT_C8;
               drm_fake_width = (surface->config.size.w + 3) >> 2;
               break;
          case DSPF_UYVY:
               drm_format = DRM_FORMAT_UYVY;
               drm_fake_width = (surface->config.size.w + 1) >> 1;
               break;
          case DSPF_YUY2:
               drm_format = DRM_FORMAT_YUYV;
               drm_fake_width = (surface->config.size.w + 1) >> 1;
               break;
          case DSPF_NV12:
               drm_format = DRM_FORMAT_NV12;
               drm_fake_width  = (surface->config.size.w + 3) >> 2;
               drm_fake_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV21:
               drm_format = DRM_FORMAT_NV21;
               drm_fake_width  = (surface->config.size.w + 3) >> 2;
               drm_fake_height = (surface->config.size.h * 3 + 1) >> 1;
               break;
          case DSPF_NV16:
               drm_format = DRM_FORMAT_NV16;
               drm_fake_width  = (surface->config.size.w + 3) >> 2;
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
     if( kms_bo_create(drmkms->kms, attr, &alloc->bo))
            D_ERROR( "DirectFB/DRMKMS: kms_bo_create() failed!\n" );


     kms_bo_get_prop(alloc->bo, KMS_HANDLE, &alloc->handle);
     kms_bo_get_prop(alloc->bo, KMS_PITCH, &alloc->pitch);
#endif
     drm_bo_handles[0] = drm_bo_handles[1] = drm_bo_handles[2] = drm_bo_handles[3] = alloc->handle;
     drm_bo_pitches[0] = drm_bo_pitches[1] = drm_bo_pitches[2] = drm_bo_pitches[3] = alloc->pitch;
     drm_bo_offsets[1] = surface->config.size.h * alloc->pitch;

     if (drmkms->shared->use_prime_fd) {

          // this seems to render the handle unusable on radeon
          ret = drmPrimeHandleToFD( drmkms->fd, alloc->handle, DRM_CLOEXEC , &alloc->prime_fd );
          if (ret)
               D_WARN( "DirectFB/DRMKMS: drmPrimeHandleToFD() failed!\n" );
     }

     alloc->size = alloc->pitch * surface->config.size.h;

     D_DEBUG_AT( DRMKMS_Surfaces, "  -> pitch %d, size %d\n", alloc->pitch, alloc->size );

     allocation->size = alloc->size;


     /*
      * Mode Framebuffer
      */


     ret = drmModeAddFB2( drmkms->fd,
                          surface->config.size.w, surface->config.size.h, drm_format,
                          drm_bo_handles, drm_bo_pitches, drm_bo_offsets, &alloc->fb_id, 0 );

     if (ret) {
          D_PERROR( "DirectFB/DRMKMS: drmModeAddFB2() failed!\n" );
          return DFB_FAILURE;
     }

     D_MAGIC_SET( alloc, DRMKMSAllocationData );

     return DFB_OK;
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

     D_DEBUG_AT( DRMKMS_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, DRMKMSPoolData );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );

     drmModeRmFB( local->drmkms->fd,  alloc->fb_id );

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

static DFBResult
drmkmsLock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     DRMKMSPoolLocalData  *local = pool_local;
     DRMKMSAllocationData *alloc = alloc_data;
     DRMKMSData           *drmkms;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, DRMKMSAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     drmkms = local->drmkms;
     D_ASSERT( drmkms != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

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
               {
#ifdef USE_GBM
                    //FIXME use gbm instead of ioctl
                    struct drm_i915_gem_mmap_gtt arg;
                    memset(&arg, 0, sizeof(arg));
                    arg.handle = alloc->handle;

                    drmCommandWriteRead( local->drmkms->fd, DRM_I915_GEM_MMAP_GTT, &arg, sizeof( arg ) );
                    lock->addr = mmap( 0, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, local->drmkms->fd, arg.offset );
#else
                    kms_bo_map( alloc->bo, &lock->addr );
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

     D_DEBUG_AT( DRMKMS_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     switch (lock->accessor) {
          case CSAID_LAYER0:
          case CSAID_ACCEL0:
               lock->handle = (void*) (long) 0;
               break;

          case CSAID_CPU:
#ifdef USE_GBM
               // FIXME: unmap in GBM case
#else
               kms_bo_unmap( alloc->bo );
#endif
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

