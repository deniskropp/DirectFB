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



#include <config.h>

#include <dlfcn.h>

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
#include "android_surface_pool_gr.h"

D_DEBUG_DOMAIN( Android_FBO, "Android/FBO", "Android FBO Surface Pool GR" );

D_DEBUG_DOMAIN( GL, "GL", "GL" );

#define CHECK_GL_ERROR() {                             \
     int err = glGetError();                           \
     if (err) {                                        \
          D_ERROR("Android/FBO: GL_ERROR(%d)\n", err); \
          return DFB_INCOMPLETE;                       \
     }                                                 \
}

/**********************************************************************************************************************/

static void incRef(struct android_native_base_t* base)
{
}

static void decRef(struct android_native_base_t* base)
{
}

typedef int(*HW_GET_MODULE)( const char *, const hw_module_t **);

static ANativeWindowBuffer_t *
AndroidAllocNativeBuffer( FBOAllocationData *alloc, int width, int height, uint32_t native_pixelformat )
{
     void *hw_handle = dlopen( "/system/lib/libhardware.so", RTLD_NOW );
     if (!hw_handle) {
          D_ERROR( "DirectFB/EGL: dlopen failed (%d)\n", errno );
          return NULL;
     }

     HW_GET_MODULE hw_get_module = dlsym( hw_handle, "hw_get_module" );
     if (!hw_get_module)  {
          D_ERROR( "DirectFB/EGL: dlsym failed (%d)\n", errno );
          dlclose( hw_handle );
          return NULL;
     }

     dlclose( hw_handle );

     int err = (*hw_get_module)( "gralloc", &alloc->hw_mod );
     if (err || !alloc->hw_mod) {
          D_ERROR( "DirectFB/EGL: hw_get_module failed (%d)\n", err );
          return NULL;
     }

     alloc->hw_mod->methods->open( alloc->hw_mod, "gpu0", (struct hw_device_t**)&alloc->alloc_mod );
     if (!alloc->alloc_mod) {
          D_ERROR( "DirectFB/EGL: open alloc failed\n");
          return NULL;
     }

     buffer_handle_t buf_handle = NULL;
     int stride = 0;
     int usage = GRALLOC_USAGE_HW_TEXTURE | GRALLOC_USAGE_HW_RENDER | GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_SW_READ_OFTEN;

     alloc->alloc_mod->alloc( alloc->alloc_mod, width, height, native_pixelformat, usage, &buf_handle, &stride );
     if (!buf_handle) {
          D_ERROR( "DirectFB/EGL: failed to alloc buffer\n");
          return NULL;
     }

     ANativeWindowBuffer_t *wbuf = (ANativeWindowBuffer_t *)malloc( sizeof(ANativeWindowBuffer_t) );
     wbuf->common.magic = ANDROID_NATIVE_BUFFER_MAGIC;
     wbuf->common.version = sizeof(ANativeWindowBuffer_t);
     memset( wbuf->common.reserved, 0, sizeof(wbuf->common.reserved) );
     wbuf->width = width;
     wbuf->height = height;
     wbuf->stride = stride;
     wbuf->format = native_pixelformat;
     wbuf->common.incRef = incRef;
     wbuf->common.decRef = decRef;
     wbuf->usage = usage;
     wbuf->handle = buf_handle;

     alloc->win_buf = wbuf;
     alloc->gralloc_mod = (gralloc_module_t *)alloc->hw_mod;

     return wbuf;
}

void
AndroidFreeNativeBuffer( FBOAllocationData *alloc )
{
     if (!alloc->alloc_mod || !alloc->win_buf) {
          D_WARN(" AndroidFreeNativeBuffer: FBO was never initialized correctly.\n ");
          return;
     }

     alloc->alloc_mod->free( alloc->alloc_mod, alloc->win_buf->handle );

     free( alloc->win_buf );

     alloc->alloc_mod->common.close( (hw_device_t *)alloc->alloc_mod );
}

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
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
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

     ANativeWindowBuffer_t *buf = AndroidAllocNativeBuffer( alloc, buffer->config.size.w, buffer->config.size.h, local->data->shared->native_pixelformat );

     CHECK_GL_ERROR();
     EGLint eglImgAttrs[] = { EGL_IMAGE_PRESERVED_KHR, EGL_TRUE, EGL_NONE, EGL_NONE };
     alloc->image = eglCreateImageKHR( eglGetDisplay( EGL_DEFAULT_DISPLAY ), EGL_NO_CONTEXT,
                                      EGL_NATIVE_BUFFER_ANDROID, buf, 0 );
     CHECK_GL_ERROR();

     alloc->pitch = alloc->win_buf->stride * 4;
     alloc->size  = alloc->win_buf->stride * 4 * buffer->config.size.h;

     int tex, fbo, crb;

     glGetIntegerv( GL_FRAMEBUFFER_BINDING, &fbo );
     CHECK_GL_ERROR();

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );
     CHECK_GL_ERROR();

     glGetIntegerv( GL_RENDERBUFFER_BINDING, &crb );
     CHECK_GL_ERROR();

     glGenTextures( 1, &alloc->texture );
     CHECK_GL_ERROR();

     D_DEBUG_AT( GL, "%s glBindTexture (%d)\n", __FUNCTION__, alloc->texture );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
     CHECK_GL_ERROR();

     glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
     CHECK_GL_ERROR();

     glEGLImageTargetTexture2DOES( GL_TEXTURE_2D, alloc->image );
     CHECK_GL_ERROR();

     glGenRenderbuffers( 1, &alloc->depth_rb );
     CHECK_GL_ERROR();

     D_DEBUG_AT( GL, "%s glBindRenderbuffer (%d)\n", __FUNCTION__, alloc->depth_rb );

     glBindRenderbuffer( GL_RENDERBUFFER, alloc->depth_rb );
     CHECK_GL_ERROR();

     glGenRenderbuffers( 1, &alloc->color_rb );
     CHECK_GL_ERROR();

     D_DEBUG_AT( GL, "%s glBindRenderbuffer (%d)\n", __FUNCTION__, alloc->color_rb );

     glBindRenderbuffer( GL_RENDERBUFFER, alloc->color_rb );
     CHECK_GL_ERROR();

     glEGLImageTargetRenderbufferStorageOES( GL_RENDERBUFFER, alloc->image );
     CHECK_GL_ERROR();

     /*
      * Framebuffer
      */
     glGenFramebuffers( 1, &alloc->fbo );
     CHECK_GL_ERROR();

     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
     CHECK_GL_ERROR();

     if (!alloc->fb_ready) {
          D_DEBUG_AT( GL, "%s glFramebufferRenderbuffer (%d)\n", __FUNCTION__, alloc->color_rb );

          glFramebufferRenderbuffer( GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, alloc->color_rb );
          CHECK_GL_ERROR();

          if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
               D_ERROR( "DirectFB/Mesa: Framebuffer not complete\n" );
          }

          checkFramebufferStatus();

          alloc->fb_ready = 1;
     }

     D_DEBUG_AT( GL, "%s glBindFramebuffer (%d)\n", __FUNCTION__, fbo );

     glBindFramebuffer( GL_FRAMEBUFFER, fbo );
     CHECK_GL_ERROR();

     D_DEBUG_AT( GL, "%s glBindTexture (%d)\n", __FUNCTION__, tex );

     glBindTexture( GL_TEXTURE_2D, tex );

     D_DEBUG_AT( GL, "%s glBindRenderbuffer (%d)\n", __FUNCTION__, crb );

     glBindRenderbuffer( GL_RENDERBUFFER, crb );

     allocation->size = alloc->size;

#ifndef ANDROID_USE_FBO_FOR_PRIMARY
     alloc->layer_flip = 1;
#endif

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

     AndroidFreeNativeBuffer( alloc );

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

     D_DEBUG_AT( Android_FBO, "%s( %p, accessor 0x%02x, access 0x%02x )\n",
                 __FUNCTION__, lock->buffer, lock->accessor, lock->access );

//     if (!dfb_core_is_master(local->core))
//          return DFB_UNSUPPORTED;

     lock->pitch  = alloc->pitch;
     lock->offset = 0;
     lock->addr   = NULL;
     lock->phys   = 0;

     switch (lock->accessor) {
          case CSAID_CPU:
               if (alloc->gralloc_mod->lock(alloc->gralloc_mod, alloc->win_buf->handle, GRALLOC_USAGE_SW_READ_OFTEN | GRALLOC_USAGE_SW_WRITE_OFTEN, 0, 0, allocation->config.size.w, allocation->config.size.h, &lock->addr))
                    return DFB_ACCESSDENIED;
               break;
          case CSAID_GPU:
          case CSAID_LAYER0:
               if (lock->access & CSAF_WRITE) {
                    if (allocation->type & CSTF_LAYER && alloc->layer_flip) {
                         lock->handle = NULL;

                         D_DEBUG_AT( GL, "%s glBindFramebuffer (%d)\n", __FUNCTION__, 0 );

                         glBindFramebuffer( GL_FRAMEBUFFER, 0 );
                    }
                    else {
                         lock->handle = (void*) (long) alloc->fbo;

                         D_DEBUG_AT( GL, "%s glBindFramebuffer (%d)\n", __FUNCTION__, alloc->fbo );

                         glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );
                         CHECK_GL_ERROR();
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

     D_DEBUG_AT( Android_FBO, "  -> offset %lu, pitch %d, addr %p, phys 0x%08lx\n",
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

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, lock->buffer );

     (void) alloc;

     switch (lock->accessor) {
          case CSAID_CPU:
               alloc->gralloc_mod->unlock(alloc->gralloc_mod, alloc->win_buf->handle);
               break;
          case CSAID_GPU:
               if (lock->access & CSAF_WRITE) {
                    //glBindFramebuffer( GL_FRAMEBUFFER, 0 );
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

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, allocation->buffer );

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

     D_DEBUG_AT( GL, "%s glBindFramebuffer (%d)\n", __FUNCTION__, alloc->fbo );
          
     glBindFramebuffer( GL_FRAMEBUFFER, alloc->fbo );

     glReadPixels(rect->x, rect->y,
                  rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, destination);

     D_DEBUG_AT( GL, "%s glBindFramebuffer (%d)\n", __FUNCTION__, fbo );

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

     D_DEBUG_AT( Android_FBO, "%s( %p )\n", __FUNCTION__, allocation->buffer );

     EGLint err;

     int tex;

     glGetIntegerv( GL_TEXTURE_BINDING_2D, &tex );

     D_DEBUG_AT( GL, "%s glBindTexture (%d)\n", __FUNCTION__, alloc->texture );

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
     //glTexImage2D(GL_TEXTURE_2D, 0,
     //             GL_RGBA, rect->w, rect->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, source);


     //D_FREE(buff);

     D_DEBUG_AT( GL, "%s glTexSubImage2D\n", __FUNCTION__ );

     glTexSubImage2D( GL_TEXTURE_2D, 0, rect->x, rect->y, rect->w, rect->h, GL_RGBA, GL_UNSIGNED_BYTE, source );
     if ((err = glGetError()) != 0) {
          D_ERROR( "DirectFB/PVR2D: glTexSubImage2D() failed! (error = %x)\n", err );
          //return DFB_FAILURE;
     }

     D_DEBUG_AT( GL, "%s glBindTexture (%d)\n", __FUNCTION__, tex );

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

