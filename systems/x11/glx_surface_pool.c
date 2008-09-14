/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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
#include <direct/hash.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/surface_pool.h>

#include <GL/glx.h>
#include <GL/glxext.h>

#include "x11.h"

#include "glx_surface_pool.h"
#include "x11_surface_pool.h"

D_DEBUG_DOMAIN( GLX_Surfaces, "GLX/Surfaces", "GLX Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
} glxPoolData;

typedef struct {
     int                           magic;

     Display                      *display;

     GLXFBConfig                  *configs;
     int                           num_configs;

     GLXFBConfig                   config24;
     Visual                       *visual24;

     GLXFBConfig                   config32;
     Visual                       *visual32;

     GLXBindTexImageEXTProc        BindTexImageEXT;
     GLXReleaseTexImageEXTProc     ReleaseTexImageEXT;
} glxPoolLocalData;

/**********************************************************************************************************************/

static int
glxPoolDataSize()
{
     return sizeof(glxPoolData);
}

static int
glxPoolLocalDataSize()
{
     return sizeof(glxPoolLocalData);
}

static int
glxAllocationDataSize()
{
     return sizeof(glxAllocationData);
}

static DFBResult
glxInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     glxPoolLocalData *local = pool_local;
     DFBX11           *x11   = system_data;

     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_NONE;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_DEFAULT;

     /* For showing our X11 window */
     ret_desc->access[CSAID_LAYER0] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "GLX Drawables" );

     local->display = x11->display;

     local->BindTexImageEXT = (GLXBindTexImageEXTProc) glXGetProcAddress( (unsigned char*) "glXBindTexImageEXT" );
     if (!local->BindTexImageEXT) {
          D_ERROR( "glXGetProcAddress( 'glXBindTexImageEXT' ) failed!\n" );
          return DFB_UNSUPPORTED;
     }

     local->ReleaseTexImageEXT = (GLXReleaseTexImageEXTProc) glXGetProcAddress( (unsigned char*) "glXReleaseTexImageEXT" );
     if (!local->ReleaseTexImageEXT) {
          D_ERROR( "glXGetProcAddress( 'glXReleaseTexImageEXT' ) failed!\n" );
          return DFB_UNSUPPORTED;
     }


     int index = 0;
     int attribs[32];

     attribs[index++] = GLX_DOUBLEBUFFER;
     attribs[index++] = False;

     attribs[index++] = GLX_DRAWABLE_TYPE;
     attribs[index++] = GLX_PIXMAP_BIT;

     attribs[index++] = GLX_X_RENDERABLE;
     attribs[index++] = True;

     attribs[index++] = GLX_RED_SIZE;
     attribs[index++] = 8;

     attribs[index++] = GLX_GREEN_SIZE;
     attribs[index++] = 8;

     attribs[index++] = GLX_BLUE_SIZE;
     attribs[index++] = 8;

     attribs[index++] = GLX_ALPHA_SIZE;
     attribs[index++] = 8;

     attribs[index++] = GLX_DEPTH_SIZE;
     attribs[index++] = 0;

     attribs[index++] = GLX_X_VISUAL_TYPE;
     attribs[index++] = GLX_TRUE_COLOR;

     attribs[index++] = None;


     XLockDisplay( local->display );


     local->configs = glXChooseFBConfig( local->display, DefaultScreen(local->display), attribs, &local->num_configs );

     D_DEBUG_AT( GLX_Surfaces, "  -> found %d configs\n", local->num_configs );

     for (index=0; index<local->num_configs; index++) {
          int          depth;
          XVisualInfo *info = glXGetVisualFromFBConfig( local->display, local->configs[index] );

          glXGetFBConfigAttrib( local->display, local->configs[index], GLX_DEPTH_SIZE, &depth );
          
          D_DEBUG_AT( GLX_Surfaces, "     [%2d] ID 0x%02lx, depth %d, RGB 0x%06lx/0x%06lx/0x%06lx {%d}, class %d, z %d\n",
                      index, info->visualid, info->depth,
                      info->red_mask, info->green_mask, info->blue_mask,
                      info->bits_per_rgb, info->class, depth );

          if (depth == 0 && info->class == TrueColor) {
               switch (info->depth) {
                    case 32:
                         local->config32 = local->configs[index];
                         local->visual32 = info->visual;
                         break;

                    case 24:
                         local->config24 = local->configs[index];
                         local->visual24 = info->visual;
                         break;
               }
          }
     }

     XVisualInfo *info24 = glXGetVisualFromFBConfig( local->display, local->config24 );
     XVisualInfo *info32 = glXGetVisualFromFBConfig( local->display, local->config32 );

     D_INFO( "GLX/Surfaces: Using visual 0x%02lx (24bit) and 0x%02lx (32bit)\n", info24->visualid, info32->visualid );


     XUnlockDisplay( local->display );


     D_MAGIC_SET( local, glxPoolLocalData );

     return DFB_OK;
}

static DFBResult
glxJoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

static DFBResult
glxDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
glxLeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     D_UNIMPLEMENTED();

     return DFB_OK;
}

static DFBResult
glxTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     glxPoolLocalData *local = pool_local;

     D_MAGIC_ASSERT( local, glxPoolLocalData );

     if (!local->configs)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
glxAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     CoreSurface          *surface;
     glxPoolLocalData     *local = pool_local;
     glxAllocationData    *alloc = alloc_data;
#if GLX_POOL_WINDOW
     XSetWindowAttributes  attr  = { 0 };
#endif

     D_DEBUG_AT( GLX_Surfaces, "%s( %p | 0x%lx )...\n", __FUNCTION__,
                 dfb_x11->xw, dfb_x11->xw ? dfb_x11->xw->window : 0 );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     XLockDisplay( local->display );

     alloc->screen = DefaultScreenOfDisplay( local->display );
     alloc->depth  = DFB_COLOR_BITS_PER_PIXEL( buffer->format ) + DFB_ALPHA_BITS_PER_PIXEL( buffer->format );

     alloc->visual = (alloc->depth == 24) ? local->visual24 : local->visual32;// DefaultVisualOfScreen( alloc->screen );
     alloc->config = (alloc->depth == 24) ? local->config24 : local->config32;

     /*
      * Create an input only window
      */
#if GLX_POOL_WINDOW
     alloc->window = XCreateWindow( local->display, RootWindowOfScreen( alloc->screen ),
                                    0, 0, 1, 1, 0, alloc->depth, InputOutput, alloc->visual, CWEventMask, &attr );
     if (!alloc->window) {
          D_ERROR( "GLX/Surfaces: Could not create input only window!\n" );
          goto error_window;
     }

     D_DEBUG_AT( GLX_Surfaces, "  -> window 0x%lx\n", alloc->window );
#endif


     /*
      * Create a pixmap
      */
     alloc->pixmap = XCreatePixmap( local->display, RootWindowOfScreen( alloc->screen ),// alloc->window,
                                    surface->config.size.w, surface->config.size.h, alloc->depth );
     if (!alloc->pixmap) {
          D_ERROR( "GLX/Surfaces: Could not create %dx%d (depth %d) pixmap!\n",
                   surface->config.size.w, surface->config.size.h, alloc->depth );
          goto error_pixmap;
     }

     D_DEBUG_AT( GLX_Surfaces, "  -> pixmap 0x%lx\n", alloc->pixmap );


     /*
      * Create a GC (for writing to pixmap)
      */
     alloc->gc = XCreateGC( local->display, alloc->pixmap, 0, NULL );


     /*
      * Create a GLXPixmap
      */
     int attribs[] = {
          GLX_TEXTURE_FORMAT_EXT,
          (alloc->depth == 24) ? GLX_TEXTURE_FORMAT_RGB_EXT : GLX_TEXTURE_FORMAT_RGBA_EXT,

          GLX_TEXTURE_TARGET_EXT,
          GLX_TEXTURE_RECTANGLE_EXT,

          None
     };

     alloc->drawable = glXCreatePixmap( local->display, alloc->config, alloc->pixmap, attribs );
     if (!alloc->drawable) {
          D_ERROR( "GLX/Surfaces: Could not create %dx%d (depth %d) GLXPixmap!\n",
                   surface->config.size.w, surface->config.size.h, alloc->depth );
          goto error_glxpixmap;
     }

     D_DEBUG_AT( GLX_Surfaces, "  -> drawable 0x%lx\n", alloc->drawable );


     /*
      * Create a GLXContext
      */
#if GLX_POOL_CONTEXT
     alloc->context = glXCreateNewContext( local->display, alloc->config, GLX_RGBA_TYPE, NULL, GL_TRUE );

     D_DEBUG_AT( GLX_Surfaces, "  -> context %p\n", alloc->context );

     if (!alloc->context) {
          D_ERROR( "GLX/Surfaces: Could not create GLXContext!\n" );
          goto error_context;
     }
#endif

     alloc->BindTexImageEXT    = local->BindTexImageEXT;
     alloc->ReleaseTexImageEXT = local->ReleaseTexImageEXT;


     /*
      * Create a texture object
      */
     glGenTextures( 1, &alloc->texture );

#if GLX_POOL_BIND_TEXTURE
     glEnable( GL_TEXTURE_2D );

     glBindTexture( GL_TEXTURE_2D, alloc->texture );

     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );

     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
     glTexParameterf( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );

     /*
      * Bind the pixmap to the texture
      */
     alloc->BindTexImageEXT( local->display, alloc->drawable, GLX_FRONT_EXT, NULL );

     glBindTexture( GL_TEXTURE_2D, 0 );
     glDisable( GL_TEXTURE_2D );
#endif

     XUnlockDisplay( local->display );


     D_DEBUG_AT( GLX_Surfaces, "  -> GLXPixmap 0x%lx [%4dx%4d] %-10s (%u)\n", alloc->drawable,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format), alloc->texture );


     /* Pseudo calculation */
     dfb_surface_calc_buffer_size( surface, 8, 2, NULL, &allocation->size );

     D_MAGIC_SET( alloc, glxAllocationData );

     return DFB_OK;


#if GLX_POOL_CONTEXT
     glXDestroyContext( local->display, alloc->context );

error_context:
#endif
     glXDestroyPixmap( local->display, alloc->drawable );

error_glxpixmap:
     XFreePixmap( local->display, alloc->pixmap );

error_pixmap:
#if GLX_POOL_WINDOW
     XDestroyWindow( local->display, alloc->window );

error_window:
#endif
     XUnlockDisplay( local->display );

     return DFB_FAILURE;
}

static DFBResult
glxDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, glxAllocationData );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     XLockDisplay( local->display );

#if GLX_POOL_CONTEXT
     glXDestroyContext( local->display, alloc->context );
#endif

     if (alloc->bound) {
          D_DEBUG_AT( GLX_Surfaces, "  -> RELEASE %p from %p\n", alloc, alloc->bound );

          glXWaitGL();

          alloc->ReleaseTexImageEXT( local->display, alloc->drawable, GLX_FRONT_EXT );
     }

     glXDestroyPixmap( local->display, alloc->drawable );

     XFreePixmap( local->display, alloc->pixmap );

#if GLX_POOL_WINDOW
     XDestroyWindow( local->display, alloc->window );
#endif

     glXWaitX();

     XUnlockDisplay( local->display );

     return DFB_OK;
}

static DFBResult
glxLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->handle = alloc;
     lock->pitch  = 1;

     return DFB_OK;
}

static DFBResult
glxUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     (void) alloc;

     return DFB_OK;
}

static DFBResult
glxRead( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         void                  *destination,
         int                    pitch,
         const DFBRectangle    *rect )
{
     XImage            *image;
     XImage            *sub;
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_ASSERT( destination != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( GLX_Surfaces, "  => %p 0x%08lx [%4d,%4d-%4dx%4d]\n", alloc, alloc->pixmap, DFB_RECTANGLE_VALS(rect) );

     XLockDisplay( local->display );

     image = XCreateImage( local->display, alloc->visual, alloc->depth,
                           ZPixmap, 0, destination, rect->w, rect->h, 32, pitch );
     if (!image) {
          D_ERROR( "GLX/Surfaces: XCreateImage( %dx%d, depth %d ) failed!\n", rect->w, rect->h, alloc->depth );
          XUnlockDisplay( local->display );
          return DFB_FAILURE;
     }

     glXWaitGL();

     sub = XGetSubImage( local->display, alloc->pixmap, rect->x, rect->y, rect->w, rect->h, ~0, ZPixmap, image, 0, 0 );

     glXWaitX();

     /* FIXME: Why the X-hell is XDestroyImage() freeing *MY* data? */
     image->data = NULL;
     XDestroyImage( image );

     XUnlockDisplay( local->display );

     if (!sub) {
          D_ERROR( "GLX/Surfaces: XGetSubImage( %d,%d-%dx%d ) failed!\n", DFB_RECTANGLE_VALS(rect) );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

static DFBResult
glxWrite( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          const void            *source,
          int                    pitch,
          const DFBRectangle    *rect )
{
     XImage            *image;
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_ASSERT( source != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( GLX_Surfaces, "  <= %p 0x%08lx [%4d,%4d-%4dx%4d]\n", alloc, alloc->pixmap, DFB_RECTANGLE_VALS(rect) );

     XLockDisplay( local->display );

     image = XCreateImage( local->display, alloc->visual, alloc->depth,
                           ZPixmap, 0, (void*) source, rect->w, rect->h, 32, pitch );
     if (!image) {
          D_ERROR( "GLX/Surfaces: XCreateImage( %dx%d, depth %d ) failed!\n", rect->w, rect->h, alloc->depth );
          XUnlockDisplay( local->display );
          return DFB_FAILURE;
     }

     glXWaitGL();

     XPutImage( local->display, alloc->pixmap, alloc->gc, image, 0, 0, rect->x, rect->y, rect->w, rect->h );

     glXWaitX();

     /* FIXME: Why the X-hell is XDestroyImage() freeing *MY* data? */
     image->data = NULL;
     XDestroyImage( image );

     XUnlockDisplay( local->display );

     return DFB_OK;
}

const SurfacePoolFuncs glxSurfacePoolFuncs = {
     PoolDataSize:       glxPoolDataSize,
     PoolLocalDataSize:  glxPoolLocalDataSize,
     AllocationDataSize: glxAllocationDataSize,

     InitPool:           glxInitPool,
     JoinPool:           glxJoinPool,
     DestroyPool:        glxDestroyPool,
     LeavePool:          glxLeavePool,

     TestConfig:         glxTestConfig,

     AllocateBuffer:     glxAllocateBuffer,
     DeallocateBuffer:   glxDeallocateBuffer,

     Lock:               glxLock,
     Unlock:             glxUnlock,

     Read:               glxRead,
     Write:              glxWrite
};

