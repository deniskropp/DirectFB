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
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include "android_system.h"

D_DEBUG_DOMAIN( Android_Surfaces, "Android/Surfaces", "Android Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( Android_SurfLock, "Android/SurfLock", "Android Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;
} AndroidPoolData;

typedef struct {
     int             magic;

     AndroidData       *android;
} AndroidPoolLocalData;

typedef struct {
     int   magic;

     int   pitch;
     int   size;

     EGLImageKHR     image;
     EGLint          handle;
//     struct gbm_bo  *bo;
     GLuint          fbo;
     GLuint          color_rb;
     GLuint          texture;

     uint32_t    fb_id;
} AndroidAllocationData;

/**********************************************************************************************************************/

static int
androidPoolDataSize( void )
{
     return sizeof(AndroidPoolData);
}

static int
androidPoolLocalDataSize( void )
{
     return sizeof(AndroidPoolLocalData);
}

static int
androidAllocationDataSize( void )
{
     return sizeof(AndroidAllocationData);
}

static DFBResult
androidInitPool( CoreDFB                    *core,
              CoreSurfacePool            *pool,
              void                       *pool_data,
              void                       *pool_local,
              void                       *system_data,
              CoreSurfacePoolDescription *ret_desc )
{
     AndroidPoolData      *data   = pool_data;
     AndroidPoolLocalData *local  = pool_local;
     AndroidData          *android = system_data;

     D_DEBUG_AT( Android_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( android != NULL );
     D_ASSERT( android->shared != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_VIRTUAL;
//     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_ACCEL1] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
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

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "/dev/mem" );

     local->android = android;

     D_MAGIC_SET( data, AndroidPoolData );
     D_MAGIC_SET( local, AndroidPoolLocalData );

     return DFB_OK;
}

static DFBResult
androidJoinPool( CoreDFB                    *core,
              CoreSurfacePool            *pool,
              void                       *pool_data,
              void                       *pool_local,
              void                       *system_data )
{
     AndroidPoolData      *data  = pool_data;
     AndroidPoolLocalData *local = pool_local;
     AndroidData          *android  = system_data;

     D_DEBUG_AT( Android_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( android != NULL );
     D_ASSERT( android->shared != NULL );

     (void) data;

     local->android = android;

     D_MAGIC_SET( local, AndroidPoolLocalData );

     return DFB_OK;
}

static DFBResult
androidDestroyPool( CoreSurfacePool *pool,
                 void            *pool_data,
                 void            *pool_local )
{
     AndroidPoolData      *data  = pool_data;
     AndroidPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_MAGIC_ASSERT( local, AndroidPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
androidLeavePool( CoreSurfacePool *pool,
               void            *pool_data,
               void            *pool_local )
{
     AndroidPoolData      *data  = pool_data;
     AndroidPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_MAGIC_ASSERT( local, AndroidPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
androidTestConfig( CoreSurfacePool         *pool,
                void                    *pool_data,
                void                    *pool_local,
                CoreSurfaceBuffer       *buffer,
                const CoreSurfaceConfig *config )
{
     CoreSurface       *surface;
     AndroidPoolData      *data  = pool_data;
     AndroidPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_MAGIC_ASSERT( local, AndroidPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->config.format != DSPF_ARGB)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
androidAllocateBuffer( CoreSurfacePool       *pool,
                    void                  *pool_data,
                    void                  *pool_local,
                    CoreSurfaceBuffer     *buffer,
                    CoreSurfaceAllocation *allocation,
                    void                  *alloc_data )
{
     int                 ret;
     CoreSurface        *surface;
     AndroidPoolData       *data  = pool_data;
     AndroidPoolLocalData  *local = pool_local;
     AndroidAllocationData *alloc = alloc_data;
     AndroidData           *android;

     D_DEBUG_AT( Android_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_MAGIC_ASSERT( local, AndroidPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     android = local->android;
     D_ASSERT( android != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );
// !!!! FIXME !!!!
     return DFB_OK;

     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, android->ctx );


     GLint texture, fbo, rbo;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture );
     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     glGetIntegerv( GL_RENDERBUFFER_BINDING, &rbo );

//     alloc->bo = gbm_bo_create( android->gbm, surface->config.size.w, surface->config.size.h, GBM_BO_FORMAT_ARGB8888,
//                                                                            GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING );

//     alloc->handle = gbm_bo_get_handle( alloc->bo ).u32;
//     alloc->pitch  = gbm_bo_get_pitch( alloc->bo );

//     alloc->image  = eglCreateImageKHR( android->dpy, NULL, EGL_NATIVE_PIXMAP_KHR, alloc->bo, NULL );

     alloc->size = alloc->pitch * surface->config.size.h;

     D_DEBUG_AT( Android_Surfaces, "  -> pitch %d, size %d\n", alloc->pitch, alloc->size );

     allocation->size = alloc->size;


     /*
      * Color Render Buffer
      */
     glGenRenderbuffers( 1, &alloc->color_rb );

     glBindRenderbuffer( GL_RENDERBUFFER, alloc->color_rb );

     glEGLImageTargetRenderbufferStorageOES( GL_RENDERBUFFER, alloc->image );


     /*
      * Framebuffer
      */
     glGenFramebuffers( 1, &alloc->fbo );

     glBindFramebuffer( GL_RENDERBUFFER, alloc->fbo );

     glFramebufferRenderbuffer( GL_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER,
                                alloc->color_rb );

     if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
          D_ERROR( "DirectFB/Android: Framebuffer not complete\n" );
     }


     /*
      * Texture
      */
     glGenTextures( 1, &alloc->texture );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, alloc->image );


     /*
      * Restore
      */
     glBindRenderbuffer( GL_RENDERBUFFER, rbo );
     glBindFramebuffer( GL_RENDERBUFFER, fbo );
     glBindTexture( GL_TEXTURE_2D, texture );

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );



     D_MAGIC_SET( alloc, AndroidAllocationData );

     return DFB_OK;
}

static DFBResult
androidDeallocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
          // !!!!! FIXME !!!!!
     return DFB_OK;
     AndroidPoolData       *data  = pool_data;
     AndroidAllocationData *alloc = alloc_data;
     AndroidPoolLocalData  *local = pool_local;

     D_DEBUG_AT( Android_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, AndroidPoolData );
     D_MAGIC_ASSERT( alloc, AndroidAllocationData );

//     drmModeRmFB( local->android->fd,  alloc->fb_id );
     eglDestroyImageKHR( local->android->dpy, alloc->image );
//     gbm_bo_destroy( alloc->bo );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
androidLock( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          CoreSurfaceBufferLock *lock )
{
     AndroidPoolLocalData  *local = pool_local;
     AndroidAllocationData *alloc = alloc_data;
     AndroidData           *android;

     // !!!!! FIXME !!!!!
     return DFB_OK;
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, AndroidAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     android = local->android;
     D_ASSERT( android != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_GPU:
               if (lock->access & CSAF_WRITE) {
                    eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, android->ctx );

                    lock->handle = (void*) (long) alloc->color_rb;
               }
               else
                    lock->handle = (void*) (long) alloc->texture;
               break;

          case CSAID_ACCEL1:
               if (lock->access & CSAF_WRITE)
                    lock->handle = (void*) (long) alloc->color_rb;
               else
                    lock->handle = (void*) (long) alloc->image;
               break;

          case CSAID_LAYER0:
               lock->handle = (void*) (long) alloc->fb_id;
               break;
#if 0
          case CSAID_CPU:
               {
                    struct drm_i915_gem_mmap_gtt arg;
                    memset(&arg, 0, sizeof(arg));
                    arg.handle = alloc->handle;

                    drmCommandWriteRead( local->android->fd, DRM_I915_GEM_MMAP_GTT, &arg, sizeof( arg ) );
                    lock->addr = mmap( 0, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, local->android->fd, arg.offset );
               }
               break;
#endif
          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               break;
     }

     D_DEBUG_AT( Android_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
androidUnlock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     AndroidAllocationData *alloc = alloc_data;

     // !!!!! FIXME !!!!!
     return DFB_OK;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, AndroidAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

static DFBResult
androidRead( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          void                  *destination,
          int                    pitch,
          const DFBRectangle    *rect )
{
     AndroidPoolLocalData  *local = pool_local;
     AndroidAllocationData *alloc = alloc_data;
     AndroidData           *android;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, AndroidAllocationData );

     D_DEBUG_AT( Android_SurfLock, "%s( %p )\n", __FUNCTION__, allocation );

     android = local->android;
     D_ASSERT( android != NULL );


     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, android->ctx );


     GLint fbo;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );


     glBindFramebuffer( GL_RENDERBUFFER, alloc->fbo );

     glFramebufferRenderbuffer( GL_FRAMEBUFFER,
                                GL_COLOR_ATTACHMENT0,
                                GL_RENDERBUFFER,
                                alloc->color_rb );

//     glReadPixels( rect->x, rect->y, rect->w, rect->h, GL_BGRA, GL_UNSIGNED_BYTE, destination );


     glBindFramebuffer( GL_RENDERBUFFER, fbo );

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );

     return DFB_OK;
}

static DFBResult
androidWrite( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           const void            *source,
           int                    pitch,
           const DFBRectangle    *rect )
{
     AndroidPoolLocalData  *local = pool_local;
     AndroidAllocationData *alloc = alloc_data;
     CoreSurface        *surface;
     AndroidData           *android;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, AndroidAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Android_SurfLock, "%s( %p )\n", __FUNCTION__, allocation );

     android = local->android;
     D_ASSERT( android != NULL );


     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, android->ctx );


     GLint texture;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture );


     glBindTexture( GL_TEXTURE_2D, alloc->texture );

//     glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, surface->config.size.w, surface->config.size.h, GL_BGRA, GL_UNSIGNED_BYTE, source );


     glBindTexture( GL_TEXTURE_2D, texture );

     eglMakeCurrent( android->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );

     return DFB_OK;
}

const SurfacePoolFuncs androidSurfacePoolFuncs = {
     .PoolDataSize       = androidPoolDataSize,
     .PoolLocalDataSize  = androidPoolLocalDataSize,
     .AllocationDataSize = androidAllocationDataSize,

     .InitPool           = androidInitPool,
     .JoinPool           = androidJoinPool,
     .DestroyPool        = androidDestroyPool,
     .LeavePool          = androidLeavePool,

     .TestConfig         = androidTestConfig,
     .AllocateBuffer     = androidAllocateBuffer,
     .DeallocateBuffer   = androidDeallocateBuffer,

     .Lock               = androidLock,
     .Unlock             = androidUnlock,

     .Read               = androidRead,
     .Write              = androidWrite,
};

