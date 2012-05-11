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
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>


#include "egl_system.h"

D_DEBUG_DOMAIN( EGL_Surfaces, "EGL/Surfaces", "EGL Framebuffer Surface Pool" );
D_DEBUG_DOMAIN( EGL_SurfLock, "EGL/SurfLock", "EGL Framebuffer Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

} EGLPoolData;

typedef struct {
     int             magic;

     CoreDFB        *core;
     EGLData      *egl;
} EGLPoolLocalData;

typedef struct {
     int   magic;

     int   offset;
     int   pitch;
     int   size;


     NATIVE_PIXMAP_STRUCT  nativePixmap;
     GLeglImageOES         eglImage;
     GLuint                texture;
} EGLAllocationData;

/**********************************************************************************************************************/

static int
eglPoolDataSize( void )
{
     return sizeof(EGLPoolData);
}

static int
eglPoolLocalDataSize( void )
{
     return sizeof(EGLPoolLocalData);
}

static int
eglAllocationDataSize( void )
{
     return sizeof(EGLAllocationData);
}

static DFBResult
eglInitPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data,
               CoreSurfacePoolDescription *ret_desc )
{
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;
     EGLData          *egl = system_data;
     EGLDataShared    *shared;

     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( egl != NULL );
     D_ASSERT( ret_desc != NULL );

     shared = egl->shared;
     D_ASSERT( shared != NULL );

     ret_desc->caps                 = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_CPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_GPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;
     ret_desc->types                = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority             = CSPP_DEFAULT;
     ret_desc->size                 = 0;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "EGL Pool" );

     local->core  = core;
     local->egl = egl;

     D_MAGIC_SET( data, EGLPoolData );
     D_MAGIC_SET( local, EGLPoolLocalData );

     return DFB_OK;
}

static DFBResult
eglJoinPool( CoreDFB                    *core,
               CoreSurfacePool            *pool,
               void                       *pool_data,
               void                       *pool_local,
               void                       *system_data )
{
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;
     EGLData          *egl = system_data;
     EGLDataShared    *shared;

     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_ASSERT( local != NULL );
     D_ASSERT( egl != NULL );

     shared = egl->shared;
     D_ASSERT( shared != NULL );

     (void) data;

     local->core  = core;
     local->egl = egl;

     D_MAGIC_SET( local, EGLPoolLocalData );

     return DFB_OK;
}

static DFBResult
eglDestroyPool( CoreSurfacePool *pool,
                  void            *pool_data,
                  void            *pool_local )
{
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;

     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_MAGIC_ASSERT( local, EGLPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
eglLeavePool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;

     D_DEBUG_AT( EGL_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_MAGIC_ASSERT( local, EGLPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
eglTestConfig( CoreSurfacePool         *pool,
                 void                    *pool_data,
                 void                    *pool_local,
                 CoreSurfaceBuffer       *buffer,
                 const CoreSurfaceConfig *config )
{
     DFBResult           ret;
     CoreSurface        *surface;
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_MAGIC_ASSERT( local, EGLPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER)
          return DFB_OK;

     ret = DFB_OK;//dfb_surfacemanager_allocate( local->core, data->manager, buffer, NULL, NULL );

     D_DEBUG_AT( EGL_Surfaces, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
eglAllocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface         *surface;
     EGLPoolData       *data  = pool_data;
     EGLPoolLocalData  *local = pool_local;
     EGLAllocationData *alloc = alloc_data;
     EGLData           *egl;

     (void) data;
     (void) local;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_MAGIC_ASSERT( local, EGLPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     egl = local->egl;
     D_ASSERT( egl != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     if (surface->type & CSTF_LAYER) {
          /*
           * Use Framebuffer
           */
     }
     else {

          /*
           * Allocate EGL Memory
           */
     }

     alloc->offset = 0;

     D_DEBUG_AT( EGL_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );


     long ePixelFormat;

     switch (surface->config.format) {
          case DSPF_RGB16:
               ePixelFormat = 0;
               break;

          case DSPF_ARGB:
               ePixelFormat = 2;
               break;

          default:
               break;
     }


     /*
      * Native Pixmap
      */
     alloc->nativePixmap.ePixelFormat = ePixelFormat;
     alloc->nativePixmap.eRotation    = 0;
     alloc->nativePixmap.lWidth       = surface->config.size.w;
     alloc->nativePixmap.lHeight      = surface->config.size.h;
     alloc->nativePixmap.lStride      = alloc->pitch;
     alloc->nativePixmap.lSizeInBytes = alloc->size;


     /*
      * EGLImage
      */
     EGLint err;

     alloc->eglImage = egl->eglCreateImageKHR( egl->eglDisplay, EGL_NO_CONTEXT, EGL_NATIVE_PIXMAP_KHR, &alloc->nativePixmap, NULL );
     if ((err = eglGetError()) != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: eglCreateImageKHR() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }


     /*
      * Texture
      */
     glGenTextures( 1, &alloc->texture );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     egl->glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, alloc->eglImage );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glEGLImageTargetTexture2DOES() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, EGLAllocationData );

     return DFB_OK;
}

static DFBResult
eglDeallocateBuffer( CoreSurfacePool       *pool,
                       void                  *pool_data,
                       void                  *pool_local,
                       CoreSurfaceBuffer     *buffer,
                       CoreSurfaceAllocation *allocation,
                       void                  *alloc_data )
{
     CoreSurface         *surface;
     EGLPoolData       *data  = pool_data;
     EGLPoolLocalData  *local = pool_local;
     EGLAllocationData *alloc = alloc_data;
     EGLData           *egl;

     (void) data;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, EGLPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     egl = local->egl;
     D_ASSERT( egl != NULL );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER) {
     }
     else {
//          EGLERROR eEGLStatus;

//          eEGLStatus = EGLMemFree( egl->hEGLContext, alloc->meminfo );
//          if (eEGLStatus) {
//               D_ERROR( "DirectFB/EGL: EGLMemFree() failed! (status %d)\n", eEGLStatus );
//               return DFB_INIT;
//          }
     }

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
eglMuckOut( CoreSurfacePool   *pool,
              void              *pool_data,
              void              *pool_local,
              CoreSurfaceBuffer *buffer )
{
     CoreSurface        *surface;
     EGLPoolData      *data  = pool_data;
     EGLPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( EGL_Surfaces, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, VPSMemPoolData );
     D_MAGIC_ASSERT( local, VPSMemPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     return DFB_UNSUPPORTED;//dfb_surfacemanager_displace( local->core, data->manager, buffer );
}

static DFBResult
eglLock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     EGLPoolLocalData  *local = pool_local;
     EGLAllocationData *alloc = alloc_data;
     EGLData           *egl;

     (void) local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     egl = local->egl;
     D_ASSERT( egl != NULL );

     lock->pitch  = alloc->pitch;
     lock->offset = alloc->offset;
//     lock->addr   = alloc->meminfo->pBase;
//     lock->phys   = alloc->meminfo->ui32DevAddr;
//     lock->handle = alloc->meminfo;

     if (lock->accessor == CSAID_GPU) {
//          if (lock->access & CSAF_WRITE)
//               lock->handle = alloc->meminfo;
//          else
               lock->handle = alloc->texture;
     }

     D_DEBUG_AT( EGL_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
eglUnlock( CoreSurfacePool       *pool,
             void                  *pool_data,
             void                  *pool_local,
             CoreSurfaceAllocation *allocation,
             void                  *alloc_data,
             CoreSurfaceBufferLock *lock )
{
     EGLAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     return DFB_OK;
}

#if 0
static DFBResult
eglRead( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           void                  *destination,
           int                    pitch,
           const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     (void) alloc;

     return DFB_OK;
}

static DFBResult
eglWrite( CoreSurfacePool       *pool,
            void                  *pool_data,
            void                  *pool_local,
            CoreSurfaceAllocation *allocation,
            void                  *alloc_data,
            const void            *source,
            int                    pitch,
            const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;
     CoreSurface         *surface;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, MesaAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );


     EGLint err;

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, surface->config.size.w, surface->config.size.h, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, source );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glTexSubImage2D() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     return DFB_OK;
}
#endif

static const SurfacePoolFuncs _eglSurfacePoolFuncs = {
     PoolDataSize:       eglPoolDataSize,
     PoolLocalDataSize:  eglPoolLocalDataSize,
     AllocationDataSize: eglAllocationDataSize,

     InitPool:           eglInitPool,
     JoinPool:           eglJoinPool,
     DestroyPool:        eglDestroyPool,
     LeavePool:          eglLeavePool,

     TestConfig:         eglTestConfig,
     AllocateBuffer:     eglAllocateBuffer,
     DeallocateBuffer:   eglDeallocateBuffer,
     MuckOut:            eglMuckOut,

     Lock:               eglLock,
     Unlock:             eglUnlock,

//     Read:               eglRead,
//     Write:              eglWrite,
};

const SurfacePoolFuncs *eglSurfacePoolFuncs = &_eglSurfacePoolFuncs;

