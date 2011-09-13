/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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
D_DEBUG_DOMAIN( GLX_Pixmaps,  "GLX/Pixmaps",  "GLX Surface Pool Pixmaps" );

/**********************************************************************************************************************/

typedef void (*GLXBindTexImageEXTProc)   ( Display *dpy, GLXDrawable drawable, int buffer, const int *attrib_list );
typedef void (*GLXReleaseTexImageEXTProc)( Display *dpy, GLXDrawable drawable, int buffer );

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

     DirectHash                   *pixmaps;

     /* Every thread needs its own context! */
     pthread_key_t                 context_key;
     pthread_key_t                 context_key2;
} glxPoolLocalData;

typedef struct {
} glxPoolData;

/**********************************************************************************************************************/

static void
destroy_context( void *arg )
{
     ThreadContext *ctx = arg;

     XLockDisplay( ctx->display );

     glXDestroyContext( ctx->display, ctx->context );

     XUnlockDisplay( ctx->display );

     D_FREE( ctx );
}

/**********************************************************************************************************************/

static DFBResult
InitLocal( glxPoolLocalData *local,
           DFBX11           *x11 )
{
     DFBResult ret;

     int i;
     int attribs[] = {
          GLX_DOUBLEBUFFER,
          False,

          GLX_DRAWABLE_TYPE,
          GLX_PIXMAP_BIT,

          GLX_X_RENDERABLE,
          True,

          GLX_RED_SIZE,
          8,

          GLX_GREEN_SIZE,
          8,

          GLX_BLUE_SIZE,
          8,

          GLX_DEPTH_SIZE,
          16,

          GLX_X_VISUAL_TYPE,
          GLX_TRUE_COLOR,

          None
     };


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


     ret = direct_hash_create( 7, &local->pixmaps );
     if (ret)
          return ret;


     XLockDisplay( local->display );


     local->configs = glXChooseFBConfig( local->display, DefaultScreen(local->display), attribs, &local->num_configs );

     D_DEBUG_AT( GLX_Surfaces, "  -> found %d configs\n", local->num_configs );

     for (i=0; i<local->num_configs; i++) {
          int          red_size;
          int          green_size;
          int          blue_size;
          int          alpha_size;
          int          buffer_size;
          
          XVisualInfo *info = glXGetVisualFromFBConfig( local->display, local->configs[i] );
          
          /* 
           * According to the GLX specification, GLX_BUFFER_SIZE is the sum of 
           * RED_SIZE, GREEN_SIZE, BLUE_SIZE and ALPHA_SIZE.
           * Since GLX_BUFFER_SIZE is often reported as 32 bit even if ALPHA_SIZE is 0,
           * lets calculate the value by ourselves.
           */             
          glXGetFBConfigAttrib( local->display, local->configs[i], GLX_RED_SIZE, &red_size );
          glXGetFBConfigAttrib( local->display, local->configs[i], GLX_GREEN_SIZE, &green_size );
          glXGetFBConfigAttrib( local->display, local->configs[i], GLX_BLUE_SIZE, &blue_size );
          glXGetFBConfigAttrib( local->display, local->configs[i], GLX_ALPHA_SIZE, &alpha_size );

          buffer_size = red_size + green_size + blue_size + alpha_size;

          D_DEBUG_AT( GLX_Surfaces, "     [%2d] ID 0x%02lx, buffer_size %d, RGB 0x%06lx/0x%06lx/0x%06lx {%d}, class %d, z %d\n",
                      i, info->visualid, buffer_size,
                      info->red_mask, info->green_mask, info->blue_mask,
                      info->bits_per_rgb, info->class, info->depth );

          if (info->class == TrueColor) {
               switch (buffer_size) {
                    case 32:
                         local->config32 = local->configs[i];
                         local->visual32 = info->visual;
                         break;

                    case 24:
                         local->config24 = local->configs[i];
                         local->visual24 = info->visual;
                         break;
               }
          }
     }

     if (!local->config24 || !local->config32) {
          D_ERROR( "GLX/Surfaces: Could not find useful visuals!\n" );
          direct_hash_destroy( local->pixmaps );
          XUnlockDisplay( local->display );
          return DFB_UNSUPPORTED;
     }

     XVisualInfo *info24 = glXGetVisualFromFBConfig( local->display, local->config24 );
     XVisualInfo *info32 = glXGetVisualFromFBConfig( local->display, local->config32 );

     D_INFO( "GLX/Surfaces: Using visual 0x%02lx (24bit) and 0x%02lx (32bit)\n", info24->visualid, info32->visualid );

     XUnlockDisplay( local->display );


     pthread_key_create( &local->context_key,  destroy_context );
     pthread_key_create( &local->context_key2, destroy_context );

     D_MAGIC_SET( local, glxPoolLocalData );

     return DFB_OK;
}

static DFBResult
GetLocalPixmap( glxPoolLocalData       *local,
                glxAllocationData      *alloc,
                CoreSurfaceAllocation  *allocation,
                LocalPixmap           **ret_pixmap )
{
     LocalPixmap       *pixmap;
     CoreSurface       *surface;
     CoreSurfaceBuffer *buffer;

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     buffer = allocation->buffer;
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     pixmap = direct_hash_lookup( local->pixmaps, alloc->pixmap );
     if (!pixmap) {
          pixmap = D_CALLOC( 1, sizeof(LocalPixmap) );
          if (!pixmap)
               return D_OOM();

          pixmap->pixmap = alloc->pixmap;
          pixmap->config = (alloc->depth == 24) ? local->config24 : local->config32;

          /*
           * Create a GLXPixmap
           */
          int attribs[] = {
               GLX_TEXTURE_FORMAT_EXT,  (alloc->depth == 24) ? GLX_TEXTURE_FORMAT_RGB_EXT : GLX_TEXTURE_FORMAT_RGBA_EXT,
               GLX_TEXTURE_TARGET_EXT,  GLX_TEXTURE_RECTANGLE_EXT,
               None
          };


          XLockDisplay( local->display );

          pixmap->drawable = glXCreatePixmap( local->display, pixmap->config, alloc->pixmap, attribs );
          if (!pixmap->drawable) {
               D_ERROR( "GLX/Surfaces: Could not create %dx%d (depth %d) GLXPixmap!\n",
                        surface->config.size.w, surface->config.size.h, alloc->depth );
               XUnlockDisplay( local->display );
               D_FREE( pixmap );
               return DFB_FAILURE;
          }

          D_DEBUG_AT( GLX_Surfaces, "  -> drawable 0x%lx\n", pixmap->drawable );

          /*
           * Create a GC (for writing to pixmap)
           */
          pixmap->gc = XCreateGC( local->display, alloc->pixmap, 0, NULL );

          D_DEBUG_AT( GLX_Surfaces, "  -> gc 0x%lx\n", pixmap->drawable );

          XUnlockDisplay( local->display );


          /*
           * Create a texture object
           */
          glGenTextures( 1, &pixmap->buffer.texture );


          D_DEBUG_AT( GLX_Pixmaps, "  NEW GLXPixmap 0x%lx for 0x%lx [%4dx%4d] %-10s\n", pixmap->drawable, alloc->pixmap,
                      surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format) );
          D_DEBUG_AT( GLX_Surfaces, "  -> GLXPixmap 0x%lx [%4dx%4d] %-10s (%u)\n", pixmap->drawable,
                      surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format),
                      pixmap->buffer.texture );

          D_MAGIC_SET( pixmap, LocalPixmap );
          D_MAGIC_SET( &pixmap->buffer, GLBufferData );

          direct_hash_insert( local->pixmaps, alloc->pixmap, pixmap );
     }
     else
          D_MAGIC_ASSERT( pixmap, LocalPixmap );

     *ret_pixmap = pixmap;

     return DFB_OK;
}

static void
ReleasePixmap( glxPoolLocalData *local,
               LocalPixmap      *pixmap )
{
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( pixmap, LocalPixmap );

     if (pixmap->bound) {
          D_DEBUG_AT( GLX_Pixmaps, "  RELEASE 0x%08lx from %p\n", pixmap->drawable, pixmap->bound );

          local->ReleaseTexImageEXT( local->display, pixmap->drawable, GLX_FRONT_EXT );

          pixmap->bound = NULL;
     }
}

static void
DestroyPixmap( glxPoolLocalData *local,
               LocalPixmap      *pixmap )
{
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( pixmap, LocalPixmap );

     D_DEBUG_AT( GLX_Pixmaps, "  DESTROY 0x%08lx (%d)\n", pixmap->drawable, pixmap->buffer.texture );

     glXWaitGL();

     ReleasePixmap( local, pixmap );

     glXWaitX();

//FIXME: crashes without proper context     glDeleteTextures( 1, &pixmap->buffer.texture );

     XFreeGC( local->display, pixmap->gc );

     glXDestroyPixmap( local->display, pixmap->drawable );

     direct_hash_remove( local->pixmaps, pixmap->pixmap );

     D_MAGIC_CLEAR( pixmap );
     D_MAGIC_CLEAR( &pixmap->buffer );

     D_FREE( pixmap );
}

/**********************************************************************************************************************/

static int
glxPoolDataSize( void )
{
     return sizeof(glxPoolData);
}

static int
glxPoolLocalDataSize( void )
{
     return sizeof(glxPoolLocalData);
}

static int
glxAllocationDataSize( void )
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

     /* For user contexts via DirectFBGL */
     ret_desc->access[CSAID_ACCEL1] = CSAF_READ | CSAF_WRITE;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "GLX Drawables" );


     return InitLocal( local, x11 );
}

static DFBResult
glxJoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     glxPoolLocalData *local = pool_local;
     DFBX11           *x11   = system_data;

     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );


     return InitLocal( local, x11 );
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

     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     XLockDisplay( local->display );

     alloc->depth  = DFB_COLOR_BITS_PER_PIXEL( buffer->format ) + DFB_ALPHA_BITS_PER_PIXEL( buffer->format );

     /*
      * Create a pixmap
      */
     alloc->pixmap = XCreatePixmap( local->display, DefaultRootWindow( local->display ),
                                    surface->config.size.w, surface->config.size.h, alloc->depth );
     if (!alloc->pixmap) {
          D_ERROR( "GLX/Surfaces: Could not create %dx%d (depth %d) pixmap!\n",
                   surface->config.size.w, surface->config.size.h, alloc->depth );

          XUnlockDisplay( local->display );

          return DFB_FAILURE;
     }

     D_DEBUG_AT( GLX_Surfaces, "  -> pixmap 0x%lx\n", alloc->pixmap );

     D_DEBUG_AT( GLX_Pixmaps, "  NEW Pixmap 0x%lx [%4dx%4d] %-10s\n", alloc->pixmap,
                 surface->config.size.w, surface->config.size.h, dfb_pixelformat_name(buffer->format) );

     XUnlockDisplay( local->display );


     /* Pseudo calculation */
     dfb_surface_calc_buffer_size( surface, 8, 2, NULL, &allocation->size );

     D_MAGIC_SET( alloc, glxAllocationData );

     return DFB_OK;
}

static DFBResult
glxDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     LocalPixmap       *pixmap;
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, glxAllocationData );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     XLockDisplay( local->display );

     pixmap = direct_hash_lookup( local->pixmaps, alloc->pixmap );
     if (pixmap)
          DestroyPixmap( local, pixmap );

     XFreePixmap( local->display, alloc->pixmap );

     XUnlockDisplay( local->display );

     D_MAGIC_CLEAR( alloc );

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
     DFBResult          ret;
     LocalPixmap       *pixmap;
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     ret = GetLocalPixmap( local, alloc, allocation, &pixmap );
     if (ret)
          return ret;

     if (lock->accessor == CSAID_GPU || lock->accessor == CSAID_ACCEL1) {
          ThreadContext *ctx;

          ctx = pthread_getspecific( (lock->accessor == CSAID_GPU) ? local->context_key : local->context_key2 );
          if (!ctx) {
               ctx = D_CALLOC( 1, sizeof(ThreadContext) );
               if (!ctx)
                    return D_OOM();

               ctx->display = local->display;

               XLockDisplay( local->display );

               ctx->context = glXCreateNewContext( local->display, pixmap->config, GLX_RGBA_TYPE, NULL, GL_TRUE );
               if (!ctx->context) {
                    D_ERROR( "GLX: Could not create GLXContext!\n" );
                    XUnlockDisplay( local->display );
                    D_FREE( ctx );
                    return DFB_FAILURE;
               }

               XUnlockDisplay( local->display );

               pthread_setspecific( (lock->accessor == CSAID_GPU) ? local->context_key : local->context_key2, ctx );

               D_DEBUG_AT( GLX_Surfaces, "  -> NEW CONTEXT %p\n", ctx->context );
          }

          if (lock->access & CSAF_WRITE) {
               if (ctx->context != glXGetCurrentContext() || ctx->drawable != pixmap->drawable) {
                    D_DEBUG_AT( GLX_Surfaces, "  -> MAKE CURRENT 0x%08lx <- 0x%08lx\n", pixmap->drawable, glXGetCurrentDrawable() );

                    if (ctx->drawable != pixmap->drawable) {
                         ctx->drawable = pixmap->drawable;

                         pixmap->buffer.flags |= GLBF_UPDATE_TARGET;
                    }

                    XLockDisplay( local->display );

                    glXMakeContextCurrent( local->display, pixmap->drawable, pixmap->drawable, ctx->context );
                    pixmap->current = ctx->context;

                    ReleasePixmap( local, pixmap );

                    XUnlockDisplay( local->display );
               }
          }
          else {
               if (pixmap->bound != ctx->context) {
                    D_DEBUG_AT( GLX_Surfaces, "  -> BIND TEXTURE 0x%08lx (%d)\n", pixmap->drawable, pixmap->buffer.texture );

                    XLockDisplay( local->display );

                    ReleasePixmap( local, pixmap );

                    glEnable( GL_TEXTURE_RECTANGLE_ARB );
                    glBindTexture( GL_TEXTURE_RECTANGLE_ARB, pixmap->buffer.texture );

                    local->BindTexImageEXT( local->display, pixmap->drawable, GLX_FRONT_EXT, NULL );
                    pixmap->bound = ctx->context;

                    XUnlockDisplay( local->display );

                    pixmap->buffer.flags |= GLBF_UPDATE_TEXTURE;
               }
          }

          lock->handle = &pixmap->buffer;
     }
     else
          lock->handle = pixmap;

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
#if 0
     DFBResult          ret;
     LocalPixmap       *pixmap;
     glxPoolLocalData  *local = pool_local;
     glxAllocationData *alloc = alloc_data;

     D_DEBUG_AT( GLX_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( local, glxPoolLocalData );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, glxAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     ret = GetLocalPixmap( local, alloc, allocation, &pixmap );
     if (ret)
          return ret;

     if (lock->accessor == CSAID_GPU) {
          XLockDisplay( local->display );

          if (lock->access & CSAF_WRITE) {
//               D_DEBUG_AT( GLX_Surfaces, "  -> UNMAKE CURRENT 0x%08lx <- 0x%08lx\n", pixmap->drawable, glXGetCurrentDrawable() );

//               glXMakeContextCurrent( local->display, None, None, NULL );
//               pixmap->current = NULL;
          }
          else {
               D_DEBUG_AT( GLX_Surfaces, "  -> UNBIND TEXTURE 0x%08lx (%d)\n", pixmap->drawable, pixmap->buffer.texture );

               ReleasePixmap( local, pixmap );
          }

          XUnlockDisplay( local->display );
     }

#endif
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

     image = XCreateImage( local->display, (alloc->depth == 24) ? local->visual24 : local->visual32,
                           alloc->depth, ZPixmap, 0, destination, rect->w, rect->h, 32, pitch );
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
     DFBResult          ret;
     LocalPixmap       *pixmap;
     CoreSurface       *surface;
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

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     ret = GetLocalPixmap( local, alloc, allocation, &pixmap );
     if (ret)
          return ret;

     XLockDisplay( local->display );

     image = XCreateImage( local->display, (alloc->depth == 24) ? local->visual24 : local->visual32,
                           alloc->depth, ZPixmap, 0, (void*) source, rect->w, rect->h, 32, pitch );
     if (!image) {
          D_ERROR( "GLX/Surfaces: XCreateImage( %dx%d, depth %d ) failed!\n", rect->w, rect->h, alloc->depth );
          XUnlockDisplay( local->display );
          return DFB_FAILURE;
     }

     glXWaitGL();

     XPutImage( local->display, alloc->pixmap, pixmap->gc, image, 0, 0, rect->x, rect->y, rect->w, rect->h );

     glXWaitX();

     /* FIXME: Why the X-hell is XDestroyImage() freeing *MY* data? */
     image->data = NULL;
     XDestroyImage( image );

     XUnlockDisplay( local->display );

     return DFB_OK;
}

const SurfacePoolFuncs glxSurfacePoolFuncs = {
     .PoolDataSize       = glxPoolDataSize,
     .PoolLocalDataSize  = glxPoolLocalDataSize,
     .AllocationDataSize = glxAllocationDataSize,

     .InitPool           = glxInitPool,
     .JoinPool           = glxJoinPool,
     .DestroyPool        = glxDestroyPool,
     .LeavePool          = glxLeavePool,

     .TestConfig         = glxTestConfig,

     .AllocateBuffer     = glxAllocateBuffer,
     .DeallocateBuffer   = glxDeallocateBuffer,

     .Lock               = glxLock,
     .Unlock             = glxUnlock,

     .Read               = glxRead,
     .Write              = glxWrite,
};

