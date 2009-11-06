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

#include <config.h>

#include <SDL.h>

#include <direct/debug.h>
#include <direct/mem.h>

#include <core/surface_pool.h>

#include <gfx/convert.h>

#include <directfb_util.h>

#include "sdl.h"

D_DEBUG_DOMAIN( SDL_Pool, "SDL/Pool", "SDL Surface Pool" );

/**********************************************************************************************************************/

typedef struct {
} SDLPoolData;

typedef struct {
     int          magic;

     SDL_Surface *sdl_surf;
} SDLAllocationData;

/**********************************************************************************************************************/

static int
sdlPoolDataSize( void )
{
     return sizeof(SDLPoolData);
}

static int
sdlAllocationDataSize( void )
{
     return sizeof(SDLAllocationData);
}

static DFBResult
sdlInitPool( CoreDFB                    *core,
             CoreSurfacePool            *pool,
             void                       *pool_data,
             void                       *pool_local,
             void                       *system_data,
             CoreSurfacePoolDescription *ret_desc )
{
     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_ASSERT( ret_desc != NULL );

     ret_desc->caps              = CSPCAPS_NONE;
     ret_desc->access[CSAID_CPU] = CSAF_READ | CSAF_WRITE;
     ret_desc->access[CSAID_GPU] = CSAF_READ | CSAF_WRITE;
     ret_desc->types             = CSTF_LAYER | CSTF_WINDOW | CSTF_CURSOR | CSTF_FONT | CSTF_SHARED | CSTF_EXTERNAL;
     ret_desc->priority          = CSPP_PREFERED;

     ret_desc->access[CSAID_LAYER0] = CSAF_READ;

     snprintf( ret_desc->name, DFB_SURFACE_POOL_DESC_NAME_LENGTH, "SDL" );

     return DFB_OK;
}

static DFBResult
sdlDestroyPool( CoreSurfacePool *pool,
                void            *pool_data,
                void            *pool_local )
{
     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );

     return DFB_OK;
}

static DFBResult
sdlTestConfig( CoreSurfacePool         *pool,
               void                    *pool_data,
               void                    *pool_local,
               CoreSurfaceBuffer       *buffer,
               const CoreSurfaceConfig *config )
{
     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     switch (config->format) {
          case DSPF_A8:
          case DSPF_RGB16:
          case DSPF_RGB32:
          case DSPF_ARGB:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

static DFBResult
sdlAllocateBuffer( CoreSurfacePool       *pool,
                   void                  *pool_data,
                   void                  *pool_local,
                   CoreSurfaceBuffer     *buffer,
                   CoreSurfaceAllocation *allocation,
                   void                  *alloc_data )
{
     DFBResult          ret;
     CoreSurface       *surface;
     SDLAllocationData *alloc = alloc_data;

     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     surface = buffer->surface;
     D_MAGIC_ASSERT( surface, CoreSurface );

     if (surface->type & CSTF_LAYER) {
          dfb_sdl->screen = NULL; /* clear? */

          ret = dfb_sdl_set_video_mode( dfb_sdl_core, &surface->config );
          if (ret) {
               D_DERROR( ret, "SDL/Surface: dfb_sdl_set_video_mode() failed!\n" );
               return ret;
          }

          D_ASSERT( dfb_sdl->screen != NULL );

          if (!dfb_sdl->screen) {
               D_ERROR( "SDL/Surface: No screen surface!?\n" );
               return DFB_BUG;
          }

          alloc->sdl_surf = dfb_sdl->screen;

          D_DEBUG_AT( SDL_Pool, "  -> screen surface  %dx%d, %d, 0x%08x, pitch %d\n",
                      dfb_sdl->screen->w, dfb_sdl->screen->h, dfb_sdl->screen->format->BitsPerPixel,
                      dfb_sdl->screen->flags, dfb_sdl->screen->pitch );
     }
     else {
          DFBSurfacePixelFormat  format = surface->config.format;
          Uint32                 flags  = SDL_HWSURFACE;// | SDL_ASYNCBLIT | SDL_FULLSCREEN;
          Uint32                 rmask;
          Uint32                 gmask;
          Uint32                 bmask;
          Uint32                 amask;

          if (surface->config.caps & DSCAPS_FLIPPING)
               flags |= SDL_DOUBLEBUF;

          switch (format) {
               case DSPF_A8:
                    rmask = 0x00;
                    gmask = 0x00;
                    bmask = 0x00;
                    amask = 0xff;
                    break;

               case DSPF_RGB16:
                    rmask = 0xf800;
                    gmask = 0x07e0;
                    bmask = 0x001f;
                    amask = 0x0000;
                    break;

               case DSPF_RGB32:
                    rmask = 0x00ff0000;
                    gmask = 0x0000ff00;
                    bmask = 0x000000ff;
                    amask = 0x00000000;
                    break;

               case DSPF_ARGB:
                    rmask = 0x00ff0000;
                    gmask = 0x0000ff00;
                    bmask = 0x000000ff;
                    amask = 0xff000000;
                    break;

               default:
                    D_ERROR( "SDL/Surface: %s() has no support for %s!\n",
                             __FUNCTION__, dfb_pixelformat_name(format) );
                    return DFB_UNSUPPORTED;
          }

          D_DEBUG_AT( SDL_Pool, "  -> SDL_CreateRGBSurface( 0x%08x, "
                      "%dx%d, %d, 0x%08x, 0x%08x, 0x%08x, 0x%08x )\n",
                      flags, surface->config.size.w, surface->config.size.h,
                      DFB_BITS_PER_PIXEL(format), rmask, gmask, bmask, amask );

          alloc->sdl_surf = SDL_CreateRGBSurface( flags,
                                                  surface->config.size.w,
                                                  surface->config.size.h,
                                                  DFB_BITS_PER_PIXEL(format),
                                                  rmask, gmask, bmask, amask );
          if (!alloc->sdl_surf) {
               D_ERROR( "SDL/Surface: SDL_CreateRGBSurface( 0x%08x, "
                        "%dx%d, %d, 0x%08x, 0x%08x, 0x%08x, 0x%08x ) failed!\n",
                        flags, surface->config.size.w, surface->config.size.h,
                        DFB_BITS_PER_PIXEL(format), rmask, gmask, bmask, amask );

               return DFB_FAILURE;
          }
     }

     /* approximation */
     allocation->size = surface->config.size.h * surface->config.size.w;

     D_MAGIC_SET( alloc, SDLAllocationData );

     return DFB_OK;
}

static DFBResult
sdlDeallocateBuffer( CoreSurfacePool       *pool,
                     void                  *pool_data,
                     void                  *pool_local,
                     CoreSurfaceBuffer     *buffer,
                     CoreSurfaceAllocation *allocation,
                     void                  *alloc_data )
{
     SDLAllocationData *alloc = alloc_data;

     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );
     D_MAGIC_ASSERT( alloc, SDLAllocationData );

     SDL_FreeSurface( alloc->sdl_surf );

     D_MAGIC_CLEAR( alloc );

     return DFB_OK;
}

static DFBResult
sdlLock( CoreSurfacePool       *pool,
         void                  *pool_data,
         void                  *pool_local,
         CoreSurfaceAllocation *allocation,
         void                  *alloc_data,
         CoreSurfaceBufferLock *lock )
{
     SDLAllocationData *alloc = alloc_data;
     SDL_Surface       *sdl_surf;

//     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, SDLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     sdl_surf = alloc->sdl_surf;
     D_ASSERT( sdl_surf != NULL );

     if (SDL_MUSTLOCK( sdl_surf ) && SDL_LockSurface( sdl_surf )) {
          D_ERROR( "SDL/Surface: SDL_LockSurface() on a %dx%dx surface failed!\n", sdl_surf->w, sdl_surf->h );
          return DFB_FAILURE;
     }

     D_ASSUME( sdl_surf->pixels != NULL );
     if (!sdl_surf->pixels)
          return DFB_UNSUPPORTED;

     D_ASSERT( sdl_surf->pitch > 0 );

     lock->addr   = sdl_surf->pixels;
     lock->pitch  = sdl_surf->pitch;
     lock->offset = sdl_surf->offset;
     lock->handle = sdl_surf;

     return DFB_OK;
}

static DFBResult
sdlUnlock( CoreSurfacePool       *pool,
           void                  *pool_data,
           void                  *pool_local,
           CoreSurfaceAllocation *allocation,
           void                  *alloc_data,
           CoreSurfaceBufferLock *lock )
{
     SDLAllocationData *alloc = alloc_data;
     SDL_Surface       *sdl_surf;

//     D_DEBUG_AT( SDL_Pool, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( pool, CoreSurfacePool );
     D_MAGIC_ASSERT( allocation, CoreSurfaceAllocation );
     D_MAGIC_ASSERT( alloc, SDLAllocationData );
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     sdl_surf = alloc->sdl_surf;
     D_ASSERT( sdl_surf != NULL );

     if (SDL_MUSTLOCK( sdl_surf ))
          SDL_UnlockSurface( sdl_surf );

     return DFB_OK;
}

const SurfacePoolFuncs sdlSurfacePoolFuncs = {
     .PoolDataSize       = sdlPoolDataSize,
     .AllocationDataSize = sdlAllocationDataSize,
     .InitPool           = sdlInitPool,
     .DestroyPool        = sdlDestroyPool,

     .TestConfig         = sdlTestConfig,

     .AllocateBuffer     = sdlAllocateBuffer,
     .DeallocateBuffer   = sdlDeallocateBuffer,

     .Lock               = sdlLock,
     .Unlock             = sdlUnlock,
};

