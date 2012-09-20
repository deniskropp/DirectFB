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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <directfb_util.h>

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


     EGLint                id;
     GLeglImageOES         eglImage;
     GLuint                texture;
     GLuint                fbo;
     void                 *pixeldata;
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
//     ret_desc->access[CSAID_CPU]    = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
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


     alloc->offset = 0;

     D_DEBUG_AT( EGL_Surfaces, "  -> offset %d, pitch %d, size %d\n", alloc->offset, alloc->pitch, alloc->size );


    // EGLContext context = eglGetCurrentContext();

  //   eglMakeCurrent( egl->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, egl->eglContext );

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
      * EGLImage
      */
     
#if 1
     EGLint err;
     int    fbo = 0;
     int    tex = 0;

#if 0
        

     alloc->pixeldata = malloc (alloc->pitch * surface->config.size.h);

     alloc->id = -1;
     eglCreateGlobalImageBRCM( surface->config.size.w, surface->config.size.h,  EGL_PIXEL_FORMAT_ARGB_8888_BRCM, alloc->pixeldata, alloc->pitch, &alloc->id);
     if (alloc->id < 0) {
          D_ERROR( "DirectFB/EGL: eglCreateGlobalImageBRCM() failed! (error = %x)\n", err );
          return DFB_FAILURE;
     }

     EGL_IMAGE_WRAP_BRCM_BC_IMAGE_T brcm_image_wrap;

     brcm_image_wrap.format = BEGL_BufferFormat_eA8B8G8R8_Texture;
     brcm_image_wrap.height = surface->config.size.h;
     brcm_image_wrap.width  = surface->config.size.w;
     brcm_image_wrap.stride = alloc->pitch;
     brcm_image_wrap.storage = alloc->id;

     
     alloc->eglImage = egl->eglCreateImageKHR( egl->eglDisplay, NULL, EGL_IMAGE_WRAP_BRCM_BCG, &brcm_image_wrap, NULL );
     if ((err = eglGetError()) != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: eglCreateImageKHR() failed! (error = %x)\n", err );
          return DFB_FAILURE;
     }

#endif

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glGetIntegerv( GL_FRAMEBUFFER_BINDING ) failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }
     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glGetIntegerv( GL_TEXTURE_BINDING_2D ) failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }


     /*
      * Texture
      */
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: Error before glGenTextures()! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     while (!alloc->texture) {
          glGenTextures( 1, &alloc->texture );
          if ((err = glGetError()) != 0) {
               D_ERROR( "DirectFB/EGL: glGenTextures() failed! (error = %x)\n", err );
               //return DFB_FAILURE;
          }
     }

     glBindTexture( GL_TEXTURE_2D, alloc->texture );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glBindTexture() failed! (error = %x)\n", err );
         return DFB_FAILURE;
     }

     glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, surface->config.size.w, surface->config.size.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL );

//     glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, alloc->eglImage );
     if ((err = eglGetError()) != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: glTexImage2D() failed! (error = %x)\n", err );
          return DFB_FAILURE;
     }

     /*
      * FBO
      */
     glGenFramebuffers( 1, &alloc->fbo );
     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
     glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, alloc->texture, 0 );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glFramebufferTexture2D() failed! (error = %x)\n", err );
         return DFB_FAILURE;
     }

     
     glBindTexture( GL_TEXTURE_2D, tex );
     glBindFramebuffer( GL_FRAMEBUFFER, fbo );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/EGL: glBindFramebuffer() failed! (error = %x)\n", err );
         return DFB_FAILURE;
     }
#endif
     allocation->size   = alloc->size;
     allocation->offset = alloc->offset;

     D_MAGIC_SET( alloc, EGLAllocationData );
     
    // eglMakeCurrent( egl->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, context );

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
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     egl = local->egl;
     D_ASSERT( egl != NULL );

     glDeleteFramebuffers( 1, &alloc->fbo );
     glDeleteTextures( 1, &alloc->texture );
     
     if (allocation->type & CSTF_LAYER) {
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
     lock->addr   = alloc->pixeldata;
//     lock->addr   = alloc->meminfo->pBase;
//     lock->phys   = alloc->meminfo->ui32DevAddr;
//     lock->handle = alloc->meminfo;

     if (lock->accessor == CSAID_GPU) {
          if (lock->access & CSAF_WRITE) {
               if (allocation->type & CSTF_LAYER)
                    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
               else
                    glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
          }
          else
               lock->handle = alloc->texture;
     }

     D_DEBUG_AT( EGL_SurfLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, 0x0 );

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
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     EGLint err;
     int    tex;

     switch (allocation->config.format) {
          case DSPF_RGB32:
          case DSPF_ARGB:
               glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

               glBindTexture( GL_TEXTURE_2D, alloc->texture );

               buff = (GLuint *)D_MALLOC(rect->w * rect->h * 4);
               if (!buff) {
                    D_ERROR("EGL: failed to allocate %d bytes for texture upload!\n",
                            rect->w * rect->h * 4);
                    return D_OOM();
               }

               pixels_per_line = pitch/4;

               sline = (GLuint *)source + rect->x + (rect->y * pixels_per_line);
               dline = buff;

               h = rect->h;
               while (h--) {
                    s = sline;
                    d = dline;
                    w = rect->w;
                    while (w--) {
                         pixel = *s++;
                         *d++ = (pixel & 0xff00ff00) |
                                ((pixel >> 16) & 0x000000ff) |
                                ((pixel << 16) & 0x00ff0000);
                    }
                    sline += pixels_per_line;
                    dline += rect->w;
               }

               //if (rect->w == allocation->config.size.w && rect->h == allocation->config.size.h)
               //     glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, allocation->config.size.w, allocation->config.size.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, source );
               //else
                    glTexSubImage2D( GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, buff );
                    D_FREE(buff);
               if ((err = glGetError()) != 0) {
                    D_ERROR( "DirectFB/EGL: glTexSubImage2D() failed! (error = %x)\n", err );
                    //return DFB_FAILURE;
               }

               glBindTexture( GL_TEXTURE_2D, tex );
               break;
          default:
               break;
     }

     return DFB_OK;
}


static DFBResult
fboRead( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           void                  *destination,
           int                    pitch,
           const DFBRectangle    *rect )
{
     EGLAllocationData *alloc = alloc_data;
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     D_INFO( "%s( %p, %dx%d, type 0x%08x )\n", __FUNCTION__, allocation->buffer, allocation->config.size.w, allocation->config.size.h,
             allocation->type );

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     (void) alloc;


     buff = (GLuint *)D_MALLOC(rect->w * rect->h * 4);
     if (!buff) {
          D_ERROR("EGL: failed to allocate %d bytes for texture download!\n",
                  rect->w * rect->h);
          return D_OOM();
     }


     int fbo;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glReadPixels(rect->x, rect->y,
                  rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, buff);

     glBindFramebuffer( GL_FRAMEBUFFER, fbo );


     pixels_per_line = pitch/4;

     sline = buff;
     dline = (GLuint *)destination;// + rect->x + (rect->y * pixels_per_line);

     h = rect->h;
     while (h--) {
          s = sline;
          d = dline;
          w = rect->w;
          while (w--) {
               pixel = *s++;
               *d++ = (pixel & 0xff00ff00) |
                      ((pixel >> 16) & 0x000000ff) |
                      ((pixel << 16) & 0x00ff0000);
          }
          sline += rect->w;
          dline += pixels_per_line;
     }

     D_FREE(buff);

     return DFB_OK;
}

static DFBResult
fboWrite( CoreSurfacePool       *pool,
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
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, EGLAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

//     D_INFO( "%s( %p, %dx%d, type 0x%08x )\n", __FUNCTION__, allocation->buffer, allocation->config.size.w, allocation->config.size.h,
//             allocation->type );

//     direct_trace_print_stack(NULL);

     D_DEBUG_AT( EGL_SurfLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );
     D_DEBUG_AT( EGL_SurfLock, "  -> " DFB_RECT_FORMAT "\n", DFB_RECTANGLE_VALS(rect) );


     EGLint err;


     int tex;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     glPixelStorei( GL_UNPACK_ALIGNMENT, 8 );

     if (allocation->config.format == DSPF_ABGR) {
          glTexSubImage2D(GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, source);
     }
     else {
          glTexSubImage2D(GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h, GL_BGRA_EXT, GL_UNSIGNED_BYTE, source);
     }

     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/PVR2D: glTexSubImage2D() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     glBindTexture( GL_TEXTURE_2D, tex );

//     D_INFO( "%s( %p, %dx%d, type 0x%08x ) done\n", __FUNCTION__, allocation->buffer, allocation->config.size.w, allocation->config.size.h,
//             allocation->type );

     return DFB_OK;
}


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

     Read:               fboRead,
     Write:              fboWrite,
};

const SurfacePoolFuncs *eglSurfacePoolFuncs = &_eglSurfacePoolFuncs;

