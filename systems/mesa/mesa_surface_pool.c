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

#include <xf86drm.h>  
#include <i915_drm.h>

#include "mesa_system.h"

D_DEBUG_DOMAIN( Mesa_Surfaces, "Mesa/Surfaces", "Mesa Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( Mesa_SurfLock, "Mesa/SurfLock", "Mesa Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;
} MesaPoolData;

typedef struct {
     int             magic;

     MesaData       *mesa;
} MesaPoolLocalData;

typedef struct {
     int   magic;

     int   pitch;
     int   size;

     EGLImageKHR image;
     EGLint      name;
     EGLint      handle;

     GLuint      fbo;
     GLuint      color_rb;
     GLuint      texture;

     uint32_t    fb_id;
} MesaAllocationData;

/**********************************************************************************************************************/

static int
mesaPoolDataSize( void )
{
     return sizeof(MesaPoolData);
}

static int
mesaPoolLocalDataSize( void )
{
     return sizeof(MesaPoolLocalData);
}

static int
mesaAllocationDataSize( void )
{
     return sizeof(MesaAllocationData);
}

static DFBResult
mesaInitPool( CoreDFB                    *core,
              CoreSurfacePool            *pool,
              void                       *pool_data,
              void                       *pool_local,
              void                       *system_data,
              CoreSurfacePoolDescription *ret_desc )
{
     MesaPoolData      *data   = pool_data;
     MesaPoolLocalData *local  = pool_local;
     MesaData          *mesa = system_data;

     D_DEBUG_AT( Mesa_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( mesa != NULL );
     D_ASSERT( mesa->shared != NULL );
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

     local->mesa = mesa;

     D_MAGIC_SET( data, MesaPoolData );
     D_MAGIC_SET( local, MesaPoolLocalData );

     return DFB_OK;
}

static DFBResult
mesaJoinPool( CoreDFB                    *core,
              CoreSurfacePool            *pool,
              void                       *pool_data,
              void                       *pool_local,
              void                       *system_data )
{
     MesaPoolData      *data  = pool_data;
     MesaPoolLocalData *local = pool_local;
     MesaData          *mesa  = system_data;

     D_DEBUG_AT( Mesa_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( mesa != NULL );
     D_ASSERT( mesa->shared != NULL );

     (void) data;

     local->mesa = mesa;

     D_MAGIC_SET( local, MesaPoolLocalData );

     return DFB_OK;
}

static DFBResult
mesaDestroyPool( CoreSurfacePool *pool,
                 void            *pool_data,
                 void            *pool_local )
{
     MesaPoolData      *data  = pool_data;
     MesaPoolLocalData *local = pool_local;

     D_DEBUG_AT( Mesa_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_MAGIC_ASSERT( local, MesaPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
mesaLeavePool( CoreSurfacePool *pool,
               void            *pool_data,
               void            *pool_local )
{
     MesaPoolData      *data  = pool_data;
     MesaPoolLocalData *local = pool_local;

     D_DEBUG_AT( Mesa_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_MAGIC_ASSERT( local, MesaPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
mesaTestConfig( CoreSurfacePool         *pool,
                void                    *pool_data,
                void                    *pool_local,
                CoreSurfaceBuffer       *buffer,
                const CoreSurfaceConfig *config )
{
     CoreSurface       *surface;
     MesaPoolData      *data  = pool_data;
     MesaPoolLocalData *local = pool_local;

     D_DEBUG_AT( Mesa_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_MAGIC_ASSERT( local, MesaPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->config.format != DSPF_ARGB)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
mesaAllocateBuffer( CoreSurfacePool       *pool,
                    void                  *pool_data,
                    void                  *pool_local,
                    CoreSurfaceBuffer     *buffer,
                    CoreSurfaceAllocation *allocation,
                    void                  *alloc_data )
{
     int                 ret;
     CoreSurface        *surface;
     MesaPoolData       *data  = pool_data;
     MesaPoolLocalData  *local = pool_local;
     MesaAllocationData *alloc = alloc_data;
     MesaData           *mesa;

     D_DEBUG_AT( Mesa_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_MAGIC_ASSERT( local, MesaPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     mesa = local->mesa;
     D_ASSERT( mesa != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, mesa->ctx );


     GLint texture, fbo, rbo;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture );
     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     glGetIntegerv( GL_RENDERBUFFER_BINDING, &rbo );


     EGLint image_attribs[] = {
          EGL_WIDTH,                    surface->config.size.w,
          EGL_HEIGHT,                   surface->config.size.h,
          EGL_DRM_BUFFER_FORMAT_MESA,   EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
          EGL_DRM_BUFFER_USE_MESA,      EGL_DRM_BUFFER_USE_SCANOUT_MESA,
          EGL_NONE
     };

     alloc->image = eglCreateDRMImageMESA( local->mesa->dpy, image_attribs );

     eglExportDRMImageMESA( local->mesa->dpy, alloc->image, &alloc->name, &alloc->handle, &alloc->pitch );

     alloc->size = alloc->pitch * surface->config.size.h;

     D_DEBUG_AT( Mesa_Surfaces, "  -> pitch %d, size %d\n", alloc->pitch, alloc->size );

     allocation->size = alloc->size;


     /*
      * Color Render Buffer
      */
     glGenRenderbuffers( 1, &alloc->color_rb );

     glBindRenderbuffer( GL_RENDERBUFFER_EXT, alloc->color_rb );

     glEGLImageTargetRenderbufferStorageOES( GL_RENDERBUFFER, alloc->image );


     /*
      * Framebuffer
      */
     glGenFramebuffers( 1, &alloc->fbo );

     glBindFramebuffer( GL_RENDERBUFFER_EXT, alloc->fbo );

     glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT,
                                   GL_COLOR_ATTACHMENT0_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   alloc->color_rb );

     if (glCheckFramebufferStatusEXT(GL_FRAMEBUFFER_EXT) != GL_FRAMEBUFFER_COMPLETE) {
          D_ERROR( "DirectFB/Mesa: Framebuffer not complete\n" );
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
     glBindRenderbuffer( GL_RENDERBUFFER_EXT, rbo );
     glBindFramebuffer( GL_RENDERBUFFER_EXT, fbo );
     glBindTexture( GL_TEXTURE_2D, texture );

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );


     /*
      * Mode Framebuffer
      */
     ret = drmModeAddFB( local->mesa->fd,
                         surface->config.size.w, surface->config.size.h,
                         32, 32, alloc->pitch, alloc->handle, &alloc->fb_id );
     if (ret) {
          D_ERROR( "DirectFB/Mesa: drmModeAddFB() failed!\n" );
//          return DFB_FAILURE;
     }


     D_MAGIC_SET( alloc, MesaAllocationData );

     return DFB_OK;
}

static DFBResult
mesaDeallocateBuffer( CoreSurfacePool       *pool,
                      void                  *pool_data,
                      void                  *pool_local,
                      CoreSurfaceBuffer     *buffer,
                      CoreSurfaceAllocation *allocation,
                      void                  *alloc_data )
{
     MesaPoolData       *data  = pool_data;
     MesaAllocationData *alloc = alloc_data;

     D_DEBUG_AT( Mesa_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, MesaPoolData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );


     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
mesaLock( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          CoreSurfaceBufferLock *lock )
{
     MesaPoolLocalData  *local = pool_local;
     MesaAllocationData *alloc = alloc_data;
     MesaData           *mesa;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Mesa_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     mesa = local->mesa;
     D_ASSERT( mesa != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_GPU:
               if (lock->access & CSAF_WRITE) {
                    eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, mesa->ctx );

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

          case CSAID_CPU:
               {
                    struct drm_i915_gem_mmap_gtt arg;
                    memset(&arg, 0, sizeof(arg));
                    arg.handle = alloc->handle;
                          
                    drmCommandWriteRead( local->mesa->fd, DRM_I915_GEM_MMAP_GTT, &arg, sizeof( arg ) );
                    lock->addr = mmap( 0, alloc->size, PROT_READ | PROT_WRITE, MAP_SHARED, local->mesa->fd, arg.offset );
               }                                   
               break;

          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               break;
     }

     D_DEBUG_AT( Mesa_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
mesaUnlock( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            CoreSurfaceBufferLock *lock )
{
     MesaAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Mesa_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

static DFBResult
mesaRead( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          void                  *destination,
          int                    pitch,
          const DFBRectangle    *rect )
{
     MesaPoolLocalData  *local = pool_local;
     MesaAllocationData *alloc = alloc_data;
     MesaData           *mesa;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );

     D_DEBUG_AT( Mesa_SurfLock, "%s( %p )\n", __FUNCTION__, allocation );

     mesa = local->mesa;
     D_ASSERT( mesa != NULL );


     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, mesa->ctx );


     GLint fbo;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );


     glBindFramebuffer( GL_RENDERBUFFER_EXT, alloc->fbo );

     glFramebufferRenderbufferEXT( GL_FRAMEBUFFER_EXT,
                                   GL_COLOR_ATTACHMENT0_EXT,
                                   GL_RENDERBUFFER_EXT,
                                   alloc->color_rb );

     glReadPixels( rect->x, rect->y, rect->w, rect->h, GL_BGRA, GL_UNSIGNED_BYTE, destination );


     glBindFramebuffer( GL_RENDERBUFFER_EXT, fbo );

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );

     return DFB_OK;
}

static DFBResult
mesaWrite( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           const void            *source,
           int                    pitch,
           const DFBRectangle    *rect )
{
     MesaPoolLocalData  *local = pool_local;
     MesaAllocationData *alloc = alloc_data;
     CoreSurface        *surface;
     MesaData           *mesa;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Mesa_SurfLock, "%s( %p )\n", __FUNCTION__, allocation );

     mesa = local->mesa;
     D_ASSERT( mesa != NULL );


     EGLContext context = eglGetCurrentContext();

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, mesa->ctx );


     GLint texture;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &texture );


     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, surface->config.size.w, surface->config.size.h, GL_BGRA, GL_UNSIGNED_BYTE, source );


     glBindTexture( GL_TEXTURE_2D, texture );

     eglMakeCurrent( mesa->dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, context );

     return DFB_OK;
}

const SurfacePoolFuncs mesaSurfacePoolFuncs = {
     .PoolDataSize       = mesaPoolDataSize,
     .PoolLocalDataSize  = mesaPoolLocalDataSize,
     .AllocationDataSize = mesaAllocationDataSize,

     .InitPool           = mesaInitPool,
     .JoinPool           = mesaJoinPool,
     .DestroyPool        = mesaDestroyPool,
     .LeavePool          = mesaLeavePool,

     .TestConfig         = mesaTestConfig,
     .AllocateBuffer     = mesaAllocateBuffer,
     .DeallocateBuffer   = mesaDeallocateBuffer,

     .Lock               = mesaLock,
     .Unlock             = mesaUnlock,

     .Read               = mesaRead,
     .Write              = mesaWrite,
};

