/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <malloc.h>

#include <pthread.h>

#include <core/fusion/shmalloc.h>

#include "directfb.h"

#include "core.h"
#include "coredefs.h"
#include "coretypes.h"

#include "gfxcard.h"
#include "surfaces.h"
#include "surfacemanager.h"

#include "gfx/util.h"
#include "misc/util.h"
#include "misc/mem.h"

static DFBResult surface_allocate_buffer  ( CoreSurface    *surface,
                                            int             policy,
                                            SurfaceBuffer **buffer );
static DFBResult surface_reallocate_buffer( CoreSurface    *surface,
                                            SurfaceBuffer  *buffer );
static void surface_deallocate_buffer     ( CoreSurface    *surface,
                                            SurfaceBuffer  *buffer );


/** public **/

DFBResult surface_create( int width, int height, int format, int policy,
                          DFBSurfaceCapabilities caps, CoreSurface **surface )
{
     DFBResult    ret;
     CoreSurface *s;

     if (BYTES_PER_PIXEL( format ) == 0) {
          BUG( "unknown pixel format" );
          return DFB_BUG;
     }

     s = (CoreSurface*) shcalloc( 1, sizeof(CoreSurface) );

     s->width   = width;
     s->height  = height;
     s->format  = format;
     s->caps    = caps;

     switch (policy) {
          case CSP_SYSTEMONLY:
               s->caps |= DSCAPS_SYSTEMONLY;
               break;
          case CSP_VIDEOONLY:
               s->caps |= DSCAPS_VIDEOONLY;
               break;
     }

     ret = surface_allocate_buffer( s, policy, &s->front_buffer );
     if (ret) {
          shmfree( s );
          return ret;
     }

     if (caps & DSCAPS_FLIPPING) {
          ret = surface_allocate_buffer( s, policy, &s->back_buffer );
          if (ret) {
               surface_deallocate_buffer( s, s->front_buffer );

               shmfree( s );
               return ret;
          }
     }
     else
          s->back_buffer = s->front_buffer;

     skirmish_init( &s->front_lock );
     skirmish_init( &s->back_lock );

     s->reactor = reactor_new(sizeof(CoreSurfaceNotification));

     surfacemanager_add_surface( gfxcard_surface_manager(), s );

     *surface = s;

     return DFB_OK;
}

DFBResult surface_reformat( CoreSurface *surface, int width, int height,
                            DFBSurfacePixelFormat format )
{
     int old_width, old_height;
     DFBSurfacePixelFormat old_format;
     DFBResult ret;

     skirmish_prevail( &surface->front_lock );
     skirmish_prevail( &surface->back_lock );

     old_width  = surface->width;
     old_height = surface->height;
     old_format = surface->format;

     surface->width = width;
     surface->height = height;
     surface->format = format;

     ret = surface_reallocate_buffer( surface, surface->front_buffer );
     if (ret) {
          surface->width  = old_width;
          surface->height = old_height;
          surface->format = old_format;

          skirmish_dismiss( &surface->front_lock );
          skirmish_dismiss( &surface->back_lock );

          return ret;
     }

     if (surface->caps & DSCAPS_FLIPPING) {
          ret = surface_reallocate_buffer( surface, surface->back_buffer );
          if (ret) {
               surface->width  = old_width;
               surface->height = old_height;
               surface->format = old_format;

               surface_reallocate_buffer( surface, surface->front_buffer );

               skirmish_dismiss( &surface->front_lock );
               skirmish_dismiss( &surface->back_lock );

               return ret;
          }
     }


     surface_notify_listeners( surface, CSNF_SIZEFORMAT |
                                        CSNF_SYSTEM | CSNF_VIDEO );

     skirmish_dismiss( &surface->front_lock );
     skirmish_dismiss( &surface->back_lock );

     return DFB_OK;
}

void surface_flip_buffers( CoreSurface *surface )
{
     SurfaceBuffer *tmp;

     if (surface->back_buffer->policy == surface->front_buffer->policy) {
          skirmish_prevail( &surface->front_lock );
          skirmish_prevail( &surface->back_lock );

          tmp = surface->front_buffer;
          surface->front_buffer = surface->back_buffer;
          surface->back_buffer = tmp;

          surface_notify_listeners( surface, CSNF_FLIP );

          skirmish_dismiss( &surface->front_lock );
          skirmish_dismiss( &surface->back_lock );
     }
     else
          back_to_front_copy( surface, NULL );
}

DFBResult surface_software_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                                 void **data, unsigned int *pitch, int front )
{
     SurfaceBuffer *buffer;

     if (!flags) {
          BUG( "lock without flags" );
          return DFB_INVARG;
     }

     if (front) {
          skirmish_prevail( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          skirmish_prevail( &surface->back_lock );
          buffer = surface->back_buffer;
     }

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               *data = buffer->system.addr;
               *pitch = buffer->system.pitch;
               break;
          case CSP_VIDEOLOW:
               /* read access or no video instance? system lock! */
               if (flags & DSLF_READ  ||  buffer->video.health != CSH_STORED) {
                    surfacemanager_assure_system( surface->manager, buffer );
                    *data = buffer->system.addr;
                    *pitch = buffer->system.pitch;
                    if (flags & DSLF_WRITE &&
                        buffer->video.health == CSH_STORED)
                         buffer->video.health = CSH_RESTORE;
               }
               else {
                    /* ok, write only goes into video directly */
                    buffer->video.locked = 1;
                    *data = gfxcard_memory_virtual( buffer->video.offset );
                    *pitch = buffer->video.pitch;
                    buffer->system.health = CSH_RESTORE;
                    gfxcard_sync();
               }
               break;
          case CSP_VIDEOHIGH:
               /* no video instance yet? system lock! */
               if (buffer->video.health != CSH_STORED) {
                    /* no video health, no fetch */
                    *data = buffer->system.addr;
                    *pitch = buffer->system.pitch;
                    break;
               }
               /* video lock! write access? restore system! */
               if (flags & DSLF_WRITE)
                    buffer->system.health = CSH_RESTORE;
               /* FALL THROUGH, for the rest we have to do a video lock
                  as if it had the policy CSP_VIDEOONLY */
          case CSP_VIDEOONLY:
               buffer->video.locked = 1;
               *data = gfxcard_memory_virtual( buffer->video.offset );
               *pitch = buffer->video.pitch;
               gfxcard_sync();
               break;
          default:
               BUG( "invalid surface policy" );

               if (front)
                    skirmish_dismiss( &surface->front_lock );
               else
                    skirmish_dismiss( &surface->back_lock );

               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult surface_hardware_lock( CoreSurface *surface,
                                 unsigned int flags, int front )
{
     SurfaceBuffer *buffer;

     if (!flags) {
          BUG( "lock without flags" );
          return DFB_INVARG;
     }

     if (front) {
          skirmish_prevail( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          skirmish_prevail( &surface->back_lock );
          buffer = surface->back_buffer;
     }

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               /* never ever! */
               break;

          case CSP_VIDEOHIGH:   // XXX enabled XXX, see below
          case CSP_VIDEOLOW:
               /* no reading? no force? no video instance? no success! ;-) */
               if (!(flags & (DSLF_READ|CSLF_FORCE)) && buffer->video.health != CSH_STORED)
                    break;
               /* fall through */

//          case CSP_VIDEOHIGH: // XXX disabled XXX
               if (surfacemanager_assure_video( surface->manager, buffer ))
                    break;
               buffer->video.locked = 1;
               if (flags & DSLF_WRITE)
                    buffer->system.health = CSH_RESTORE;
               return DFB_OK;

          case CSP_VIDEOONLY:
               buffer->video.locked = 1;
               return DFB_OK;

          default:
               BUG( "invalid surface policy" );

               if (front)
                    skirmish_dismiss( &surface->front_lock );
               else
                    skirmish_dismiss( &surface->back_lock );

               return DFB_BUG;
     }

     if (front)
          skirmish_dismiss( &surface->front_lock );
     else
          skirmish_dismiss( &surface->back_lock );

     return DFB_FAILURE;
}

void surface_unlock( CoreSurface *surface, int front )
{
     gfxcard_flush_texture_cache();

     if (front) {
          surface->front_buffer->video.locked = 0;
          skirmish_dismiss( &surface->front_lock );
     }
     else {
          surface->back_buffer->video.locked = 0;
          skirmish_dismiss( &surface->back_lock );
     }
}

void surface_destroy( CoreSurface *surface )
{
     surface_notify_listeners( surface, CSNF_DESTROY );

     skirmish_destroy( &surface->front_lock );
     skirmish_destroy( &surface->back_lock );

     surface_deallocate_buffer( surface, surface->front_buffer );

     if (surface->back_buffer != surface->front_buffer)
          surface_deallocate_buffer( surface, surface->back_buffer );

     reactor_free( surface->reactor );

     surfacemanager_remove_surface( surface->manager, surface );

     shmfree( surface );
}


/** internal **/

static DFBResult surface_allocate_buffer( CoreSurface *surface, int policy,
                                          SurfaceBuffer **buffer )
{
     SurfaceBuffer *b;

     b = (SurfaceBuffer *) shcalloc( 1, sizeof(SurfaceBuffer) );

     b->policy = policy;
     b->surface = surface;

     switch (policy) {
          case CSP_SYSTEMONLY:
          case CSP_VIDEOLOW:
          case CSP_VIDEOHIGH:
               b->system.health = CSH_STORED;

               b->system.pitch = surface->width *
                                 BYTES_PER_PIXEL(surface->format);
               if (b->system.pitch & 3)
                    b->system.pitch += 4 - (b->system.pitch & 3);

               b->system.addr = shmalloc( surface->height * b->system.pitch );
               break;
          case CSP_VIDEOONLY: {
               DFBResult ret;

               surfacemanager_lock( surface->manager );

               ret = surfacemanager_allocate( surface->manager, b );

               surfacemanager_unlock( surface->manager );

               if (ret) {
                    shmfree( b );
                    return ret;
               }

               b->video.health = CSH_STORED;
               break;
          }
     }

     *buffer = b;

     return DFB_OK;
}

static DFBResult surface_reallocate_buffer( CoreSurface   *surface,
                                            SurfaceBuffer *buffer )
{
     DFBResult    ret;

     if (buffer->system.health) {
          buffer->system.health = CSH_STORED;

          buffer->system.pitch = surface->width *
                                 BYTES_PER_PIXEL(surface->format);
          if (buffer->system.pitch & 3)
               buffer->system.pitch += 4 - (buffer->system.pitch & 3);

          /* HACK HACK HACK */
          shmfree( buffer->system.addr );
          buffer->system.addr = shmalloc( surface->height*buffer->system.pitch );

          /* FIXME: better support video instance reallocation */
          surfacemanager_lock( surface->manager );
          surfacemanager_deallocate( surface->manager, buffer );
          surfacemanager_unlock( surface->manager );
     }
     else {
          /* FIXME: better support video instance reallocation */
          surfacemanager_lock( surface->manager );
          surfacemanager_deallocate( surface->manager, buffer );
          ret = surfacemanager_allocate( surface->manager, buffer );
          surfacemanager_unlock( surface->manager );

          if (ret) {
               CAUTION( "reallocation of video instance failed" );
               return ret;
          }

          buffer->video.health = CSH_STORED;
     }

     return DFB_OK;
}

static void surface_deallocate_buffer( CoreSurface   *surface,
                                       SurfaceBuffer *buffer )
{
     if (buffer->system.health)
          shmfree( buffer->system.addr );

     surfacemanager_lock( surface->manager );

     if (buffer->video.health)
          surfacemanager_deallocate( surface->manager, buffer );

     surfacemanager_unlock( surface->manager );

     shmfree( buffer );
}

