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

#include <directfb.h>

#include <gfx/util.h>

#include "core.h"
#include "coredefs.h"
#include "surfaces.h"

#include "gfxcard.h"
#include "surfacemanager.h"


static CoreSurface *surfaces = NULL;

void surfaces_cleanup()
{
     while (surfaces)
          surface_destroy( surfaces );
}

DFBResult surface_allocate_buffer( CoreSurface *surface, int policy,
                                   SurfaceBuffer **buffer )
{
     SurfaceBuffer *b;

     b = (SurfaceBuffer *) calloc( 1, sizeof(SurfaceBuffer) );

     b->policy = policy;
     b->surface = surface;

     switch (policy) {
          case CSP_SYSTEMONLY:
          case CSP_VIDEOLOW:
          case CSP_VIDEOHIGH:
               b->system.health = CSH_STORED;
               b->system.pitch = surface->width *
                                 BYTES_PER_PIXEL(surface->format);
               b->system.addr = malloc( surface->height * b->system.pitch );
               break;
          case CSP_VIDEOONLY: {
               DFBResult ret;

               ret = surfacemanager_allocate( b );
               if (ret) {
                    free( b );
                    return ret;
               }

               b->video.health = CSH_STORED;
               break;
          }
     }

     *buffer = b;

     return DFB_OK;
}

DFBResult surface_reallocate_buffer( SurfaceBuffer *buffer )
{
     CoreSurface *surface = buffer->surface;

     if (buffer->system.health) {
          buffer->system.health = CSH_STORED;
          buffer->system.pitch = surface->width *
                                 BYTES_PER_PIXEL(surface->format);
          buffer->system.addr = realloc( buffer->system.addr,
                                         surface->height*buffer->system.pitch );

          /* FIXME: support video instance reallocation */
          surfacemanager_deallocate( buffer );
     }
     else {
          /* FIXME: support video instance reallocation */
          surfacemanager_deallocate( buffer );
          surfacemanager_allocate( buffer );

          buffer->video.health = CSH_STORED;
     }

     return DFB_OK;
}

void surface_destroy_buffer( SurfaceBuffer *buffer )
{
     if (buffer->system.health)
          free( buffer->system.addr );

     if (buffer->video.health)
          surfacemanager_deallocate( buffer );

     free( buffer );
}

/*
 * surface functions
 */
DFBResult surface_create( int width, int height, int format, int policy,
                          DFBSurfaceCapabilities caps, CoreSurface **surface )
{
     DFBResult    ret;
     CoreSurface *s;

     if (BYTES_PER_PIXEL( format ) == 0) {
          BUG( "unknown pixel format" );
          return DFB_BUG;
     }

     s = (CoreSurface*) calloc( 1, sizeof(CoreSurface) );

     s->width = width;
     s->height = height;
     s->format = format;
     s->caps = caps;

     ret = surface_allocate_buffer( s, policy, &s->front_buffer );
     if (ret) {
          free( s );
          return ret;
     }

     if (caps & DSCAPS_FLIPPING) {
          ret = surface_allocate_buffer( s, policy, &s->back_buffer );
          if (ret) {
               surface_destroy_buffer( s->front_buffer );

               free( s );
               return ret;
          }
     }
     else
          s->back_buffer = s->front_buffer;

     pthread_mutex_init( &s->front_lock, NULL );
     pthread_mutex_init( &s->back_lock, NULL );
     pthread_mutex_init( &s->listeners_mutex, NULL );

     if (!surfaces) {
          s->next = NULL;
          s->prev = NULL;
          surfaces = s;

          core_cleanup_last( surfaces_cleanup );
     }
     else {
          s->prev = NULL;
          s->next = surfaces;
          surfaces->prev = s;
          surfaces = s;
     }

     *surface = s;

     return DFB_OK;
}

DFBResult surface_reformat( CoreSurface *surface, int width, int height,
                            DFBSurfacePixelFormat format )
{
     int old_width, old_height;
     DFBSurfacePixelFormat old_format;
     DFBResult ret;

     pthread_mutex_lock( &surface->front_lock );
     pthread_mutex_lock( &surface->back_lock );

     old_width  = surface->width;
     old_height = surface->height;
     old_format = surface->format;

     surface->width = width;
     surface->height = height;
     surface->format = format;

     ret = surface_reallocate_buffer( surface->front_buffer );
     if (ret) {
          surface->width  = old_width;
          surface->height = old_height;
          surface->format = old_format;

          pthread_mutex_unlock( &surface->front_lock );
          pthread_mutex_unlock( &surface->back_lock );

          return ret;
     }

     if (surface->caps & DSCAPS_FLIPPING) {
          ret = surface_reallocate_buffer( surface->back_buffer );
          if (ret) {
               surface->width  = old_width;
               surface->height = old_height;
               surface->format = old_format;

               surface_reallocate_buffer( surface->front_buffer );

               pthread_mutex_unlock( &surface->front_lock );
               pthread_mutex_unlock( &surface->back_lock );

               return ret;
          }
     }


     surface_notify_listeners( surface, CSN_SIZEFORMAT |
                                        CSN_SYSTEM | CSN_VIDEO );

     pthread_mutex_unlock( &surface->front_lock );
     pthread_mutex_unlock( &surface->back_lock );

     return DFB_OK;
}

void surface_flip_buffers( CoreSurface *surface )
{
     SurfaceBuffer *tmp;

     if (surface->back_buffer->policy == surface->front_buffer->policy) {
          pthread_mutex_lock( &surface->front_lock );
          pthread_mutex_lock( &surface->back_lock );

          tmp = surface->front_buffer;
          surface->front_buffer = surface->back_buffer;
          surface->back_buffer = tmp;

          surface_notify_listeners( surface, CSN_FLIP );

          pthread_mutex_unlock( &surface->front_lock );
          pthread_mutex_unlock( &surface->back_lock );
     }
     else
          back_to_front_copy( surface, NULL );
}

void surface_install_listener( CoreSurface *surface,
                               SurfaceListenerReceive receive,
                               unsigned int filter, void *ctx )
{
     SurfaceListener *l = (SurfaceListener*)malloc( sizeof(SurfaceListener) );

     l->filter = filter;
     l->receive = receive;
     l->ctx = ctx;
     l->next = NULL;

     pthread_mutex_lock( &surface->listeners_mutex );

     if (surface->listeners) {
          l->next = surface->listeners;
          surface->listeners = l;
     }
     else
          surface->listeners = l;

     pthread_mutex_unlock( &surface->listeners_mutex );
}

void surface_remove_listener( CoreSurface *surface,
                              SurfaceListenerReceive receive,
                              void *ctx )
{
     SurfaceListener *l, *pl;

     pthread_mutex_lock( &surface->listeners_mutex );

     pl = NULL;
     l = surface->listeners;
     while (l) {
          if (l->receive == receive  &&  l->ctx == ctx) {
               if (pl)
                    pl->next = l->next;
               else
                    surface->listeners = l->next;

               free( l );

               break;
          }

          pl = l;
          l = l->next;
     }

     pthread_mutex_unlock( &surface->listeners_mutex );
}

DFBResult surface_soft_lock( CoreSurface *surface, DFBSurfaceLockFlags flags,
                             void **data, unsigned int *pitch, int front )
{
     SurfaceBuffer *buffer;

     if (!flags) {
          BUG( "lock without flags" );
          return DFB_INVARG;
     }

     if (front) {
          pthread_mutex_lock( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          pthread_mutex_lock( &surface->back_lock );
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
                    surfacemanager_assure_system( buffer );
                    *data = buffer->system.addr;
                    *pitch = buffer->system.pitch;
                    if (flags & DSLF_WRITE &&
                        buffer->video.health == CSH_STORED)
                         buffer->video.health = CSH_RESTORE;
               }
               else {
                    /* ok, write only goes into video directly */
                    *data = card->framebuffer.base + buffer->video.offset;
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
               *data = card->framebuffer.base + buffer->video.offset;
               *pitch = buffer->video.pitch;
               gfxcard_sync();
               break;
          default:
               BUG( "invalid surface policy" );

               if (front)
                    pthread_mutex_unlock( &surface->front_lock );
               else
                    pthread_mutex_unlock( &surface->back_lock );

               return DFB_BUG;
     }

     return DFB_OK;
}

DFBResult surface_hard_lock( CoreSurface *surface,
                             unsigned int flags, int front )
{
     SurfaceBuffer *buffer;

     if (!flags) {
          BUG( "lock without flags" );
          return DFB_INVARG;
     }

     if (front) {
          pthread_mutex_lock( &surface->front_lock );
          buffer = surface->front_buffer;
     }
     else {
          pthread_mutex_lock( &surface->back_lock );
          buffer = surface->back_buffer;
     }

     switch (buffer->policy) {
          case CSP_SYSTEMONLY:
               /* never ever! */
               break;

          case CSP_VIDEOHIGH:   // XXX enabled XXX, see below
          case CSP_VIDEOLOW:
               /* no reading? no video instance? no success! ;-) */
               if (!(flags & DSLF_READ) && buffer->video.health != CSH_STORED)
                    break;
               /* fall through */

//          case CSP_VIDEOHIGH: // XXX disabled XXX
               if (surfacemanager_assure_video( buffer ))
                    break;
               if (flags & DSLF_WRITE)
                    buffer->system.health = CSH_RESTORE;
               return DFB_OK;

          case CSP_VIDEOONLY:
               return DFB_OK;

          default:
               BUG( "invalid surface policy" );

               if (front)
                    pthread_mutex_unlock( &surface->front_lock );
               else
                    pthread_mutex_unlock( &surface->back_lock );

               return DFB_BUG;
     }

     if (front)
          pthread_mutex_unlock( &surface->front_lock );
     else
          pthread_mutex_unlock( &surface->back_lock );

     return DFB_FAILURE;
}

void surface_unlock( CoreSurface *surface, int front )
{
     if (front)
          pthread_mutex_unlock( &surface->front_lock );
     else
          pthread_mutex_unlock( &surface->back_lock );
}

void surface_destroy( CoreSurface *surface )
{
     surface_notify_listeners( surface, CSN_DESTROY );

     pthread_mutex_lock( &surface->front_lock );
     pthread_mutex_lock( &surface->back_lock );

     pthread_mutex_lock( &surface->listeners_mutex );
     while (surface->listeners) {
          SurfaceListener *l = surface->listeners;

          surface->listeners = l->next;

          free( l );
     }
     pthread_mutex_unlock( &surface->listeners_mutex );
     pthread_mutex_destroy( &surface->listeners_mutex );

     surface_destroy_buffer( surface->front_buffer );

     if (surface->caps & DSCAPS_FLIPPING)
          surface_destroy_buffer( surface->back_buffer );

     /* FIXME: mutex for surface list */
     if (surface->prev)
          surface->prev->next = surface->next;
     else
          surfaces = surface->next;

     if (surface->next)
          surface->next->prev = surface->prev;

     pthread_mutex_unlock( &surface->front_lock );
     pthread_mutex_destroy( &surface->front_lock );

     pthread_mutex_unlock( &surface->back_lock );
     pthread_mutex_destroy( &surface->back_lock );

     free( surface );
}

void surface_notify_listeners( CoreSurface *s, unsigned int flags )
{
     SurfaceListener *l, *pl;

     pthread_mutex_lock( &s->listeners_mutex );

     pl = NULL;
     l = s->listeners;

     while (l) {
          if (l->filter & flags &&
              l->receive( s, flags, l->ctx ) == SL_REMOVE)
          {
               SurfaceListener *tmp  = l;

               if (l == s->listeners)
                    s->listeners = l->next;
               else
                    pl->next = l->next;

               l = l->next;

               free( tmp );
          }
          else {
               pl = l;
               l = l->next;
          }
     }

     pthread_mutex_unlock( &s->listeners_mutex );
}


