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

#define EGL_EGLEXT_PROTOTYPES

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <misc/conf.h>

#include <idirectfb.h>


#include "android_system.h"

D_DEBUG_DOMAIN( Android_FBO,     "Android/FBO",     "Android FBO Surface Pool" );
D_DEBUG_DOMAIN( Android_FBOLock, "Android/FBOLock", "Android FBO Surface Pool Locks" );

/**********************************************************************************************************************/

typedef struct {
     int             magic;

} FBOPoolData;

typedef struct {
     int                  magic;

     CoreDFB             *core;

     AndroidData         *data;
} FBOPoolLocalData;

typedef struct {
     int            magic;

     int            pitch;
     int            size;

     GLuint                texture;
     GLuint                fbo;
} FBOAllocationData;

/**********************************************************************************************************************/

static inline bool
TestEGLError( const char* pszLocation )
{
     EGLint iErr = eglGetError();
     if (iErr != EGL_SUCCESS) {
          D_ERROR( "DirectFB/EGL: %s failed (%d).\n", pszLocation, iErr );
          return false;
     }

     return true;
}

/**********************************************************************************************************************/

static int
fboPoolDataSize( void )
{
     return sizeof(FBOPoolData);
}

static int
fboPoolLocalDataSize( void )
{
     return sizeof(FBOPoolLocalData);
}

static int
fboAllocationDataSize( void )
{
     return sizeof(FBOAllocationData);
}

static DFBResult
fboInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     AndroidData      *android_data = system_data;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( data != NULL );
     D_ASSERT( local != NULL );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_PHYSICAL | CSPCAPS_VIRTUAL;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->access[CSAID_LAYER0] = CSAF_READ | CSAF_SHARED;
     ret_desc->types             = CSTF_WINDOW | CSTF_LAYER | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_ULTIMATE;
     ret_desc->size              = 0;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "FBO Pool" );

     local->core = core;
     local->data = android_data;

     D_MAGIC_SET( data, FBOPoolData );
     D_MAGIC_SET( local, FBOPoolLocalData );

     return DFB_OK;
}

static DFBResult
fboJoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     AndroidData      *android_data = system_data;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_ASSERT( local != NULL );

     (void) data;

     local->core = core;
     local->data = android_data;

     D_MAGIC_SET( local, FBOPoolLocalData );

     return DFB_OK;
}

static DFBResult
fboDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );

     D_MAGIC_CLEAR( data );
     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fboLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     D_DEBUG_AT( Android_FBO, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );

     (void) data;

     D_MAGIC_CLEAR( local );

     return DFB_OK;
}

static DFBResult
fboTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     DFBResult         ret = DFB_OK;
     CoreSurface      *surface;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );


     D_DEBUG_AT( Android_FBO, "  -> %s\n", DirectFBErrorString(ret) );

     return ret;
}

static DFBResult
checkFramebufferStatus( void )
{
     GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

     switch (status) {
          case 0:
               D_WARN( "zero status" );
          case GL_FRAMEBUFFER_COMPLETE:
               return DFB_OK;

          case GL_FRAMEBUFFER_UNSUPPORTED:
               D_ERROR( "%s(): Unsupported!\n", __FUNCTION__);
               return DFB_UNSUPPORTED;

          case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
               D_ERROR( "%s(): Incomplete attachment!\n", __FUNCTION__);
               return DFB_INVARG;

          case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
               D_ERROR( "%s(): Incomplete missing attachment!\n", __FUNCTION__);
               return DFB_INVARG;

          case GL_FRAMEBUFFER_INCOMPLETE_DIMENSIONS:
               D_ERROR( "%s(): Incomplete dimensions!\n", __FUNCTION__);
               return DFB_INVARG;

          //case GL_FRAMEBUFFER_INCOMPLETE_FORMATS:
          //     D_ERROR( "%s(): Incomplete formats!\n", __FUNCTION__);
          //     return DFB_INVARG;

          default:
               D_ERROR( "%s(): Failure! (0x%04x)\n", __FUNCTION__, status );
               return DFB_FAILURE;
     }
}

static DFBResult
fboAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface       *surface;
     FBOPoolData       *data  = pool_data;
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     dfb_surface_calc_buffer_size( surface, 8, 1, &alloc->pitch, &alloc->size );

     D_INFO("FBO %dx%d\n", buffer->config.size.w, buffer->config.size.h);


     int tex, fbo;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );


     glGenTextures( 1, &alloc->texture );
     glBindTexture( GL_TEXTURE_2D, alloc->texture );
     glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, buffer->config.size.w,
                   buffer->config.size.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, 0 );

     glGenFramebuffers( 1, &alloc->fbo );
     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glFramebufferTexture2D( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, alloc->texture, 0 );


     checkFramebufferStatus();



     glBindFramebuffer( GL_FRAMEBUFFER, fbo );
     glBindTexture( GL_TEXTURE_2D, tex );


     allocation->size = alloc->size;

     D_MAGIC_SET( alloc, FBOAllocationData );

     return DFB_OK;
}

static DFBResult
fboDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     FBOPoolData       *data  = pool_data;
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) data;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     glDeleteTextures( 1, &alloc->texture );
     glDeleteFramebuffers( 1, &alloc->fbo );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
fboMuckOut( CoreSurfacePool   *pool,
            void              *pool_data,
            void              *pool_local,
            CoreSurfaceBuffer *buffer )
{
     CoreSurface      *surface;
     FBOPoolData      *data  = pool_data;
     FBOPoolLocalData *local = pool_local;

     (void) data;
     (void) local;

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, buffer );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( data, FBOPoolData );
     D_MAGIC_ASSERT( local, FBOPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     return DFB_UNSUPPORTED;
}

static DFBResult
fboLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     FBOPoolLocalData  *local = pool_local;
     FBOAllocationData *alloc = alloc_data;

     (void) local;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_FBOLock, "%s( %p, accessor 0x%02x, access 0x%02x )\n",
                 __FUNCTION__, lock->buffer, lock->accessor, lock->access );

//     if (!dfb_core_is_master(local->core))
//          return DFB_UNSUPPORTED;

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_GPU:
          case CSAID_LAYER0:
               if (lock->access & CSAF_WRITE) {
                    if (allocation->type & CSTF_LAYER) {
                         lock->handle = NULL;

                         glBindFramebuffer( GL_FRAMEBUFFER, 0 );
                    }
                    else {
                         lock->handle = (void*) (long) alloc->fbo;
     
                         glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
                    }
               }
               else {
                    lock->handle = (void*) (long) alloc->texture;
               }
               break;

          default:
               D_BUG( "unsupported accessor %d", lock->accessor );
               return DFB_BUG;
     }

     D_DEBUG_AT( Android_FBOLock, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
                 lock->offset, lock->pitch, lock->addr, lock->phys );

     return DFB_OK;
}

static DFBResult
fboUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     FBOAllocationData *alloc = alloc_data;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     switch (lock->accessor) {
          case CSAID_GPU:
               if (lock->access & CSAF_WRITE) {
                    glBindFramebuffer( GL_FRAMEBUFFER, 0 );
               }
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
     FBOAllocationData *alloc = alloc_data;
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     (void) alloc;

/*
     buff = (GLuint *)D_MALLOC(rect->w * rect->h * 4);
     if (!buff) {
          D_ERROR("EGL: failed to allocate %d bytes for texture download!\n",
                  rect->w * rect->h);
          return D_OOM();
     }
*/

     int fbo;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glReadPixels(rect->x, rect->y,
                  rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, destination);

     glBindFramebuffer( GL_FRAMEBUFFER, fbo );

/*
     pixels_per_line = pitch/4;

     sline = buff;
     dline = (GLuint *)destination + rect->x + (rect->y * pixels_per_line);

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
*/
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
     FBOAllocationData *alloc = alloc_data;
     CoreSurface       *surface;
     GLuint            *buff, *sline, *dline, *s, *d;
     GLuint             pixel, w, h, pixels_per_line;

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, FBOAllocationData );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     D_DEBUG_AT( Android_FBOLock, "%s( %p )\n", __FUNCTION__, allocation->buffer );


     EGLint err;

     int tex;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );


     glBindTexture( GL_TEXTURE_2D, alloc->texture );

/*
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
*/
     //glTexSubImage2D(GL_TEXTURE_2D, 0,
     //                rect->x, rect->y,
     //                rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, buff);
     // glTexImage2D(GL_TEXTURE_2D, 0,
     //              GL_RGBA, rect->w, rect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, buff);
     glPixelStorei( GL_UNPACK_ALIGNMENT, 8);
     glTexImage2D(GL_TEXTURE_2D, 0,
                  GL_RGBA, rect->w, rect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);


     //D_FREE(buff);



//     glTexSubImage2D( GL_TEXTURE_2D, 0, 0, 0, allocation->config.size.w, allocation->config.size.h, GL_RGBA, GL_UNSIGNED_BYTE, source );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/PVR2D: glTexSubImage2D() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }


     glBindTexture( GL_TEXTURE_2D, tex );


     return DFB_OK;
}

const SurfacePoolFuncs androidSurfacePoolFuncs = {
     PoolDataSize:       fboPoolDataSize,
     PoolLocalDataSize:  fboPoolLocalDataSize,
     AllocationDataSize: fboAllocationDataSize,

     InitPool:           fboInitPool,
     JoinPool:           fboJoinPool,
     DestroyPool:        fboDestroyPool,
     LeavePool:          fboLeavePool,

     TestConfig:         fboTestConfig,
     AllocateBuffer:     fboAllocateBuffer,
     DeallocateBuffer:   fboDeallocateBuffer,
     MuckOut:            fboMuckOut,

     Lock:               fboLock,
     Unlock:             fboUnlock,

     Read:               fboRead,
     Write:              fboWrite,
};

const SurfacePoolFuncs *fboSurfacePoolFuncs = &androidSurfacePoolFuncs;

