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

#include <X11/extensions/Xcomposite.h>

#include "x11.h"
#include "x11image.h"
#include "x11_surface_pool.h"

D_DEBUG_DOMAIN( X11_Surfaces, "X11/Surfaces", "X11 System Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
} x11PoolData;

typedef struct {
     pthread_mutex_t  lock;
     DirectHash      *hash;

     DFBX11          *x11;
} x11PoolLocalData;

/**********************************************************************************************************************/

static int
x11PoolDataSize( void )
{
     return sizeof(x11PoolData);
}

static int
x11PoolLocalDataSize( void )
{
     return sizeof(x11PoolLocalData);
}

static int
x11AllocationDataSize( void )
{
     return sizeof(x11AllocationData);
}

static DFBResult
x11InitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     DFBResult         ret;
     x11PoolLocalData *local = pool_local;
     DFBX11           *x11   = system_data;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     local->x11 = x11;

     ret_desc->caps     = CSPCAPS_NONE;
     ret_desc->types    = CSTF_LAYER | CSTF_WINDOW | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority = CSPP_ULTIMATE;

     /* For showing our X11 window */
     ret_desc->access[CSAID_LAYER0] = CSAF_READ;
     ret_desc->access[CSAID_LAYER1] = CSAF_READ;
     ret_desc->access[CSAID_LAYER2] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "X11 Windows" );

     ret = direct_hash_create( 7, &local->hash );
     if (ret) {
          D_DERROR( ret, "X11/Surfaces: Could not create local hash table!\n" );
          return ret;
     }

     pthread_mutex_init( &local->lock, NULL );

     int event_base_return, error_base_return;
     XLockDisplay( x11->display );
     XCompositeQueryExtension( x11->display, &event_base_return, &error_base_return );
     XSync( x11->display, False );
     XUnlockDisplay( x11->display );

     return DFB_OK;
}

static DFBResult
x11JoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     DFBResult         ret;
     x11PoolLocalData *local = pool_local;
     DFBX11           *x11   = system_data;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     local->x11 = x11;

     ret = direct_hash_create( 7, &local->hash );
     if (ret) {
          D_DERROR( ret, "X11/Surfaces: Could not create local hash table!\n" );
          return ret;
     }

     pthread_mutex_init( &local->lock, NULL );

     return DFB_OK;
}

static DFBResult
x11DestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pthread_mutex_destroy( &local->lock );

     direct_hash_destroy( local->hash );

     return DFB_OK;
}

static DFBResult
x11LeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     x11PoolLocalData *local = pool_local;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     pthread_mutex_destroy( &local->lock );

     direct_hash_destroy( local->hash );

     return DFB_OK;
}

static DFBResult
x11TestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     x11PoolLocalData *local = pool_local;
     DFBX11           *x11   = local->x11;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     if (!x11->visuals[DFB_PIXELFORMAT_INDEX(config->format)]) {
          D_DEBUG_AT( X11_Surfaces, "  -> NO VISUAL for %s\n", dfb_pixelformat_name(config->format) );
          return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
x11CheckKey( CoreSurfacePool   *pool,
             void              *pool_data,
             void              *pool_local,
             CoreSurfaceBuffer *buffer,
             const char        *key,
             u64                handle )
{
     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     if (!strcmp( key, "Pixmap/X11" ))
          return DFB_OK;

     if (!strcmp( key, "Window/X11" ))
          return DFB_OK;

     return DFB_UNSUPPORTED;
}

static DFBResult
x11AllocateKey( CoreSurfacePool       *pool,
                void                  *pool_data,
                void                  *pool_local,
                CoreSurfaceBuffer     *buffer,
                const char            *key,
                u64                    handle,
                CoreSurfaceAllocation *allocation,
                void                  *alloc_data )
{
     CoreSurface       *surface;
     x11AllocationData *alloc = alloc_data;
     x11PoolLocalData  *local = pool_local;
     DFBX11            *x11   = local->x11;

     D_DEBUG_AT( X11_Surfaces, "%s( %s, 0x%08llx )\n", __FUNCTION__, key, (unsigned long long) handle );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (!strcmp( key, "Pixmap/X11" )) {
          D_DEBUG_AT( X11_Surfaces, "  -> Pixmap/X11\n" );

          alloc->type = X11_ALLOC_PIXMAP;
     }
     else if (!strcmp( key, "Window/X11" )) {
          D_DEBUG_AT( X11_Surfaces, "  -> Window/X11\n" );

          alloc->type = X11_ALLOC_WINDOW;
     }
     else {
          D_BUG( "unexpected key '%s'", key );
          return DFB_BUG;
     }

     dfb_surface_calc_buffer_size( surface, 8, 8, &alloc->pitch, &allocation->size );

     alloc->visual = x11->visuals[DFB_PIXELFORMAT_INDEX(buffer->format)];
     alloc->depth  = DFB_COLOR_BITS_PER_PIXEL( buffer->format ) + DFB_ALPHA_BITS_PER_PIXEL( buffer->format );

     D_DEBUG_AT( X11_Surfaces, "  -> visual %p (id %lu), depth %d\n", alloc->visual, alloc->visual->visualid, alloc->depth );

     if (handle) {
          switch (alloc->type) {
               case X11_ALLOC_PIXMAP:
                    XLockDisplay( x11->display );

                    alloc->xid = (unsigned long) handle;
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> pixmap 0x%08lx\n", (long) alloc->xid );

                    XUnlockDisplay( x11->display );

                    D_INFO( "X11/Windows: Import Pixmap 0x%08lx\n", alloc->xid );
                    break;

               case X11_ALLOC_WINDOW: {
                    XLockDisplay( x11->display );

                    alloc->window = (unsigned long) handle;
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> window 0x%08lx\n", (long) alloc->window );

//                    XCompositeRedirectWindow( x11->display, alloc->window, CompositeRedirectManual );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> redirected\n" );

//                    alloc->xid = XCompositeNameWindowPixmap( x11->display, alloc->window );
                    alloc->xid = alloc->window;
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> pixmap 0x%08lx\n", (long) alloc->xid );

                    XUnmapWindow( x11->display, alloc->window );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> unmapped\n" );

                    XUnlockDisplay( x11->display );

                    D_INFO( "X11/Windows: Import Window 0x%08lx with Pixmap handle 0x%08lx\n", alloc->window, alloc->xid );
                    break;
               }

               default:
                    D_BUG( "unexpected allocation type %d\n", alloc->type );
                    return DFB_BUG;
          }
     }
     else {
          alloc->type = X11_ALLOC_PIXMAP;

          switch (alloc->type) {
               case X11_ALLOC_PIXMAP:
                    XLockDisplay( x11->display );

                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> creating pixmap...\n" );

                    alloc->xid = XCreatePixmap( x11->display, DefaultRootWindow(x11->display),
                                                allocation->config.size.w, allocation->config.size.h, alloc->depth );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> pixmap 0x%08lx\n", (long) alloc->xid );

                    XUnlockDisplay( x11->display );

                    D_INFO( "X11/Windows: New Pixmap 0x%08lx\n", alloc->xid );

                    alloc->created = true;
                    break;

               case X11_ALLOC_WINDOW: {
                    Window w = (Window) (long) buffer->surface->data;


                    XSetWindowAttributes attr;

                    attr.event_mask =
                           ButtonPressMask
                         | ButtonReleaseMask
                         | PointerMotionMask
                         | KeyPressMask
                         | KeyReleaseMask
                         | ExposureMask
                         | StructureNotifyMask;

                    attr.background_pixmap = 0;

                    XLockDisplay( x11->display );

                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> creating window...\n" );

                    alloc->window = w?:XCreateWindow( x11->display,
                                                   DefaultRootWindow(x11->display),
                                                   600, 200, allocation->config.size.w, allocation->config.size.h, 0,
                                                   alloc->depth, InputOutput,
                                                   alloc->visual, CWEventMask, &attr );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> window 0x%08lx\n", (long) alloc->window );
//                    buffer->surface->data = (void*) (long) alloc->window;

//                    XCompositeRedirectWindow( x11->display, alloc->window, CompositeRedirectManual );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> redirected\n" );

                    XMapRaised( x11->display, alloc->window );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> raised\n" );

//                    alloc->xid = XCompositeNameWindowPixmap( x11->display, alloc->window );
                    alloc->xid = alloc->window;
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> pixmap 0x%08lx\n", (long) alloc->xid );

//                    XUnmapWindow( x11->display, alloc->window );
                    XSync( x11->display, False );
                    D_DEBUG_AT( X11_Surfaces, "  -> unmapped\n" );

                    XUnlockDisplay( x11->display );

                    D_INFO( "X11/Windows: New Window 0x%08lx with Pixmap handle 0x%08lx\n", alloc->window, alloc->xid );

                    alloc->created = !w;
                    break;
               }

               default:
                    D_BUG( "unexpected allocation type %d\n", alloc->type );
                    return DFB_BUG;
          }
     }

     allocation->offset = alloc->type;

     return DFB_OK;
}

static DFBResult
x11AllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     return x11AllocateKey( pool, pool_data, pool_local, buffer, "Pixmap/X11", 0, allocation, alloc_data );
}

static DFBResult
x11DeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     x11AllocationData *alloc = alloc_data;
     x11PoolLocalData  *local = pool_local;
     DFBX11            *x11   = local->x11;

     D_DEBUG_AT( X11_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     switch (alloc->type) {
          case X11_ALLOC_WINDOW:
               D_ASSUME( x11->showing != alloc->window );

               if (x11->showing == alloc->window) {
                    D_LOG( X11_Surfaces, VERBOSE, "  -> Hiding window 0x%08lx that is to be destroyed!!!\n", x11->showing );
                    XUnmapWindow( x11->display, x11->showing );
                    x11->showing = 0;
               }

          case X11_ALLOC_PIXMAP:
               if (allocation->type & CSTF_PREALLOCATED) {
                    // don't delete
               }
               else if (alloc->created) {
                    XLockDisplay( x11->display );

                    if (alloc->xid != alloc->window) {
                         D_INFO( "X11/Windows: Free Pixmap 0x%08lx\n", alloc->xid );
                         XFreePixmap( x11->display, alloc->xid );
                    }

                    if (alloc->window) {
                         D_INFO( "X11/Windows: Destroy Window 0x%08lx\n", alloc->window );
                         XDestroyWindow( x11->display, alloc->window );
                    }

                    XUnlockDisplay( x11->display );
               }
               break;

          default:
               D_BUG( "unexpected allocation type %d\n", alloc->type );
               return DFB_BUG;
     }

     return DFB_OK;
}

static DFBResult
x11Lock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     x11PoolLocalData  *local = pool_local;
     x11AllocationData *alloc = alloc_data;

     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     D_ASSERT( local->hash != NULL );

     pthread_mutex_lock( &local->lock );

     switch (alloc->type) {
          case X11_ALLOC_WINDOW:
               if (lock->accessor != CSAID_GPU)
                    lock->handle = (void*)(long) alloc->window;
               else
          case X11_ALLOC_PIXMAP:
                    lock->handle = (void*)(long) alloc->xid;
               break;

          default:
               D_BUG( "unexpected allocation type %d\n", alloc->type );
               pthread_mutex_unlock( &local->lock );
               return DFB_BUG;
     }

     lock->offset = alloc->type;
     lock->pitch  = alloc->pitch;

     pthread_mutex_unlock( &local->lock );

     return DFB_OK;
}

static DFBResult
x11Unlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     return DFB_OK;
}

static DFBResult
x11Read( CoreSurfacePool       *pool,
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
     x11PoolLocalData  *local = pool_local;
     x11AllocationData *alloc = alloc_data;
     DFBX11            *x11   = local->x11;

     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_ASSERT( destination != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( X11_Surfaces, "  => %p 0x%08lx [%4d,%4d-%4dx%4d]\n", alloc, alloc->xid, DFB_RECTANGLE_VALS(rect) );

     XLockDisplay( x11->display );

#if 1
     image = XCreateImage( x11->display, alloc->visual, alloc->depth, ZPixmap, 0, destination, rect->w, rect->h, 32, pitch );
     if (!image) {
          D_ERROR( "X11/Surfaces: XCreateImage( %dx%d, depth %d ) failed!\n", rect->w, rect->h, alloc->depth );
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     sub = XGetSubImage( x11->display, alloc->xid, rect->x, rect->y, rect->w, rect->h, ~0, ZPixmap, image, 0, 0 );
#else
     image = XGetImage( x11->display, alloc->window ? alloc->window : alloc->xid,
                        rect->x, rect->y, rect->w, rect->h, ~0, ZPixmap );
#endif

     if (image) {
          /* FIXME: Why the X-hell is XDestroyImage() freeing *MY* data? */
          image->data = NULL;
          XDestroyImage( image );
     }

     XUnlockDisplay( x11->display );

#if 1
     if (!sub) {
          D_ERROR( "X11/Surfaces: XGetSubImage( %d,%d-%dx%d ) failed!\n", DFB_RECTANGLE_VALS(rect) );
          return DFB_FAILURE;
     }
#endif
     return DFB_OK;
}

static DFBResult
x11Write( CoreSurfacePool       *pool,
          void                  *pool_data,
          void                  *pool_local,
          CoreSurfaceAllocation *allocation,
          void                  *alloc_data,
          const void            *source,
          int                    pitch,
          const DFBRectangle    *rect )
{
     XImage            *image;
     x11PoolLocalData  *local = pool_local;
     x11AllocationData *alloc = alloc_data;
     DFBX11            *x11   = local->x11;

     D_DEBUG_AT( X11_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_ASSERT( source != NULL );
     D_ASSERT( pitch >= 0 );
     DFB_RECTANGLE_ASSERT( rect );

     D_DEBUG_AT( X11_Surfaces, "  <= %p 0x%08lx [%4d,%4d-%4dx%4d]\n", alloc, alloc->xid, DFB_RECTANGLE_VALS(rect) );

     XLockDisplay( x11->display );

     image = XCreateImage( x11->display, alloc->visual, alloc->depth, ZPixmap, 0, (void*) source, rect->w, rect->h, 32, pitch );
     if (!image) {
          D_ERROR( "X11/Surfaces: XCreateImage( %dx%d, depth %d ) failed!\n", rect->w, rect->h, alloc->depth );
          XUnlockDisplay( x11->display );
          return DFB_FAILURE;
     }

     XPutImage( x11->display, alloc->xid, DefaultGC( x11->display, DefaultScreen( x11->display ) ), image, 0, 0, rect->x, rect->y, rect->w, rect->h );

     /* FIXME: Why the X-hell is XDestroyImage() freeing *MY* data? */
     image->data = NULL;
     XDestroyImage( image );

     XUnlockDisplay( x11->display );

     return DFB_OK;
}

const SurfacePoolFuncs x11WindowPoolFuncs = {
     .PoolDataSize       = x11PoolDataSize,
     .PoolLocalDataSize  = x11PoolLocalDataSize,
     .AllocationDataSize = x11AllocationDataSize,

     .InitPool           = x11InitPool,
     .JoinPool           = x11JoinPool,
     .DestroyPool        = x11DestroyPool,
     .LeavePool          = x11LeavePool,

     .TestConfig         = x11TestConfig,

     .CheckKey           = x11CheckKey,
     .AllocateKey        = x11AllocateKey,
     .AllocateBuffer     = x11AllocateBuffer,
     .DeallocateBuffer   = x11DeallocateBuffer,

     .Lock               = x11Lock,
     .Unlock             = x11Unlock,

     .Read               = x11Read,
     .Write              = x11Write,
};

