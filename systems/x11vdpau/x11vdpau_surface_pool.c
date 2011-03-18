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

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>

#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include "x11.h"
#include "x11vdpau_surface_pool.h"

D_DEBUG_DOMAIN( X11VDPAU_Surfaces, "X11/VDPAU/Surfaces", "X11 VDPAU Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
} x11vdpauPoolData;

typedef struct {
     DFBX11          *x11;
     DFBX11VDPAU     *vdp;
} x11vdpauPoolLocalData;

/**********************************************************************************************************************/

static int
x11vdpauPoolDataSize( void )
{
     return sizeof(x11vdpauPoolData);
}

static int
x11vdpauPoolLocalDataSize( void )
{
     return sizeof(x11vdpauPoolLocalData);
}

static int
x11vdpauAllocationDataSize( void )
{
     return sizeof(x11vdpauAllocationData);
}

static DFBResult
x11InitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     x11vdpauPoolLocalData *local = pool_local;
     DFBX11                *x11   = system_data;
     DFBX11VDPAU           *vdp   = &x11->vdp;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     local->x11 = x11;
     local->vdp = vdp;

     ret_desc->caps              = CSPCAPS_NONE;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE | CSAF_SHARED;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL | CSTF_PREALLOCATED;
     ret_desc->priority          = CSPP_ULTIMATE;

     /* For showing our X11 window */
     ret_desc->access[CSAID_LAYER0] = CSAF_READ;
     ret_desc->access[CSAID_LAYER1] = CSAF_READ;
     ret_desc->access[CSAID_LAYER2] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "VDPAU Output Surface" );

     return DFB_OK;
}

static DFBResult
x11JoinPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data )
{
     x11vdpauPoolLocalData *local = pool_local;
     DFBX11                *x11   = system_data;
     DFBX11VDPAU           *vdp   = &x11->vdp;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     local->x11 = x11;
     local->vdp = vdp;

     return DFB_OK;
}

static DFBResult
x11DestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
x11LeavePool( CoreSurfacePool *pool,
              void            *pool_data,
              void            *pool_local )
{
     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
x11TestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     switch (config->format) {
          case DSPF_ARGB:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

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
     CoreSurface            *surface;
     x11vdpauAllocationData *alloc = alloc_data;
     x11vdpauPoolLocalData  *local = pool_local;
     DFBX11                 *x11   = local->x11;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_PREALLOCATED) {
          alloc->surface = surface->config.preallocated[0].handle;

          D_DEBUG_AT( X11VDPAU_Surfaces, "  -> preallocated from output surface %u\n", alloc->surface );
     }
     else {
          DirectResult                  ret;
          int                           retval;
          DFBX11CallOutputSurfaceCreate create;

          create.rgba_format = VDP_RGBA_FORMAT_B8G8R8A8;
          create.width       = surface->config.size.w;
          create.height      = surface->config.size.h;

          ret = fusion_call_execute2( &x11->shared->call, FCEF_NONE, X11_VDPAU_OUTPUT_SURFACE_CREATE, &create, sizeof(create), &retval );
          if (ret) {
               D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
               return ret;
          }

          if (!retval) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceCreate( ARGB %dx%d ) failed!\n",
                        surface->config.size.w, surface->config.size.h );
               return DFB_FAILURE;
          }

          alloc->surface = (VdpOutputSurface) retval;

          D_DEBUG_AT( X11VDPAU_Surfaces, "  -> created output surface %u\n", alloc->surface );
     }

     dfb_surface_calc_buffer_size( surface, 8, 2, &alloc->pitch, &allocation->size );

     return DFB_OK;
}

static DFBResult
x11DeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     CoreSurface            *surface;
     x11vdpauAllocationData *alloc = alloc_data;
     x11vdpauPoolLocalData  *local = pool_local;
     DFBX11                 *x11   = local->x11;
     DFBX11VDPAU            *vdp   = local->vdp;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     CORE_SURFACE_ALLOCATION_ASSERT( allocation );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_PREALLOCATED) {
     }
     else {
          DirectResult                   ret;
          int                            retval;
          DFBX11CallOutputSurfaceDestroy destroy;

          destroy.surface = alloc->surface;

          ret = fusion_call_execute2( &x11->shared->call, FCEF_NONE, X11_VDPAU_OUTPUT_SURFACE_DESTROY, &destroy, sizeof(destroy), &retval );
          if (ret) {
               D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
               return ret;
          }

          if (retval) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceDestroy( %u ) failed (status %d, '%s'!\n",
                        alloc->surface, retval, vdp->GetErrorString( retval ) );
               return DFB_FAILURE;
          }
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
     x11vdpauAllocationData *alloc = alloc_data;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->handle = (void*) (unsigned long) alloc->surface;
     lock->pitch  = alloc->pitch;

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
     D_DEBUG_AT( X11VDPAU_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

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
     x11vdpauAllocationData *alloc = alloc_data;
     x11vdpauPoolLocalData  *local = pool_local;
     DFBX11                 *x11   = local->x11;
     DFBX11VDPAU            *vdp   = local->vdp;

     CoreSurface *surface;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );


     if (dfb_core_is_master( x11->core )) {
          VdpStatus  status;
          VdpRect    src;
          void      *ptrs[3];
          uint32_t   pitches[3];

          ptrs[0]    = destination;
          pitches[0] = pitch;

          src.x0 = rect->x;
          src.y0 = rect->y;
          src.x1 = rect->x + rect->w;
          src.y1 = rect->y + rect->h;

          XLockDisplay( x11->display );
          status = vdp->OutputSurfaceGetBitsNative( alloc->surface, &src, ptrs, pitches );
          XUnlockDisplay( x11->display );
          if (status) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative() failed (status %d, '%s')!\n",
                        status, vdp->GetErrorString( status ) );
               return DFB_FAILURE;
          }
     }
     else {
          DirectResult                         ret;
          int                                  retval;
          DFBX11CallOutputSurfaceGetBitsNative get;
     
          get.surface        = alloc->surface;
          get.ptr            = destination;
          get.pitch          = pitch;
     
          get.source_rect.x0 = rect->x;
          get.source_rect.y0 = rect->y;
          get.source_rect.x1 = rect->x + rect->w;
          get.source_rect.y1 = rect->y + rect->h;
     
     
          ret = fusion_call_execute2( &x11->shared->call, FCEF_NONE, X11_VDPAU_OUTPUT_SURFACE_GET_BITS_NATIVE, &get, sizeof(get), &retval );
          if (ret) {
               D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
               return ret;
          }
     
          if (retval) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfaceGetBitsNative( %u ) failed (status %d, '%s'!\n",
                        alloc->surface, retval, vdp->GetErrorString( retval ) );
               return DFB_FAILURE;
          }
     }

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
     x11vdpauAllocationData *alloc = alloc_data;
     x11vdpauPoolLocalData  *local = pool_local;
     DFBX11                 *x11   = local->x11;
     DFBX11VDPAU            *vdp   = local->vdp;

     CoreSurface *surface;

     D_DEBUG_AT( X11VDPAU_Surfaces, "%s( %p )\n", __FUNCTION__, allocation );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );

     surface = allocation->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );


     if (dfb_core_is_master( x11->core )) {
          VdpStatus   status;
          VdpRect     dest;
          const void *ptrs[3];
          uint32_t    pitches[3];

          ptrs[0]    = source;
          pitches[0] = pitch;

          dest.x0 = rect->x;
          dest.y0 = rect->y;
          dest.x1 = rect->x + rect->w;
          dest.y1 = rect->y + rect->h;

          XLockDisplay( x11->display );
          status = vdp->OutputSurfacePutBitsNative( alloc->surface, ptrs, pitches, &dest );
          XUnlockDisplay( x11->display );
          if (status) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfacePutBitsNative() failed (status %d, '%s')!\n",
                        status, vdp->GetErrorString( status ) );
               return DFB_FAILURE;
          }
     }
     else {
          DirectResult                         ret;
          int                                  retval;
          DFBX11CallOutputSurfacePutBitsNative put;
     
          put.surface             = alloc->surface;
          put.ptr                 = source;
          put.pitch               = pitch;
     
          put.destination_rect.x0 = rect->x;
          put.destination_rect.y0 = rect->y;
          put.destination_rect.x1 = rect->x + rect->w;
          put.destination_rect.y1 = rect->y + rect->h;
     
     
          ret = fusion_call_execute2( &x11->shared->call, FCEF_NONE, X11_VDPAU_OUTPUT_SURFACE_PUT_BITS_NATIVE, &put, sizeof(put), &retval );
          if (ret) {
               D_DERROR( ret, "DirectFB/X11/VDPAU: fusion_call_execute2() failed!\n" );
               return ret;
          }
     
          if (retval) {
               D_ERROR( "DirectFB/X11/VDPAU: OutputSurfacePutBitsNative( %u ) failed (status %d, '%s'!\n",
                        alloc->surface, retval, vdp->GetErrorString( retval ) );
               return DFB_FAILURE;
          }
     }

     return DFB_OK;
}

static const SurfacePoolFuncs _x11vdpauSurfacePoolFuncs = {
     .PoolDataSize       = x11vdpauPoolDataSize,
     .PoolLocalDataSize  = x11vdpauPoolLocalDataSize,
     .AllocationDataSize = x11vdpauAllocationDataSize,

     .InitPool           = x11InitPool,
     .JoinPool           = x11JoinPool,
     .DestroyPool        = x11DestroyPool,
     .LeavePool          = x11LeavePool,

     .TestConfig         = x11TestConfig,

     .AllocateBuffer     = x11AllocateBuffer,
     .DeallocateBuffer   = x11DeallocateBuffer,

     .Lock               = x11Lock,
     .Unlock             = x11Unlock,

     .Read               = x11Read,
     .Write              = x11Write,
};

const SurfacePoolFuncs *x11vdpauSurfacePoolFuncs = &_x11vdpauSurfacePoolFuncs;

