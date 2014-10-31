/*
 * Copyright © 2011 Kristian Høgsberg
 * Copyright © 2011 Benjamin Franzke
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Authors:
 *    Kristian Høgsberg <krh@bitplanet.net>
 *    Benjamin Franzke <benjaminfranzke@googlemail.com>
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <unistd.h>

#include <wayland-server.h>

#include <direct/EvLog.h>
#include <directfb_util.h>

#include "wayland-dfb.h"
#include "wayland-dfb-server-protocol.h"


D_LOG_DOMAIN( DFBWayland_wl_dfb,      "DFBWayland/wl_dfb",      "DirectFB Wayland extension" );
D_LOG_DOMAIN( DFBWayland_Buffer,      "DFBWayland/Buffer",      "DirectFB Wayland Buffer" );

/**********************************************************************************************************************/

namespace WL {

class WLDFB {
public:
     WLDFB();
     ~WLDFB();

     struct wl_global                  *global;

     struct wl_display                 *display;
     IDirectFB                         *dfb;
     IDirectFBEventBuffer              *events;

     wl_dfb_client_callback             callback;
     void                              *callback_context;

     std::map<DFBSurfaceID,Buffer*>     surfaces;

     void HandleSurfaceEvent( const DFBSurfaceEvent &event );
};


WLDFB::WLDFB()
     :
     global( NULL ),
     display( NULL ),
     dfb( NULL )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "WLDFB::%s( %p )\n", __FUNCTION__, this );
}

WLDFB::~WLDFB()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "WLDFB::%s( %p )\n", __FUNCTION__, this );

     if (global)
          wl_global_destroy( global );

     if (dfb)
          dfb->Release( dfb );
}

void
WLDFB::HandleSurfaceEvent( const DFBSurfaceEvent &event )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "WLDFB::%s( %p, event %p ) <- type 0x%04x\n", __FUNCTION__, this, &event, event.type );

     switch (event.type) {
          case DSEVT_UPDATE: {
               D_DEBUG_AT( DFBWayland_wl_dfb, "  -> UPDATE %u %d,%d-%dx%d %u\n",
                           event.surface_id, DFB_RECTANGLE_VALS_FROM_REGION( &event.update ), event.flip_count );

               Buffer *surface = surfaces[event.surface_id];

               if (surface) {
                    D_EVLOG( "WaylandDFB/Surface", surface,
                             "UPDATE", *Direct::String::F("[%3u] %d,%d-%dx%d (%lld us -> %lld diff)",
                                                          event.flip_count,
                                                          DFB_RECTANGLE_VALS_FROM_REGION( &event.update ),
                                                          event.time_stamp, event.time_stamp - surface->last_got ) );

                    surface->last_got = event.time_stamp;

                    if (!(event.flip_flags & DSFLIP_NOWAIT))
                         surface->ScheduleUpdate( event );
               }
               else
                    D_LOG( DFBWayland_wl_dfb, VERBOSE, "  -> SURFACE WITH ID %u NOT FOUND\n", event.surface_id );

//               surface->flip_count = event.flip_count;

               break;
          }

          default:
               break;
     }
}

/**********************************************************************************************************************/

Buffer::Buffer()
     :
//     flip_count( 0 ),
//     updating( false ),
     last_got( 0 ),
     last_ack( 0 ),
     listener( NULL )
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );

     wl_signal_init( &destroy_signal );
}

Buffer::~Buffer()
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );

     surface->DetachEventBuffer( surface, wl_dfb->events );
     surface->Release( surface );

     wl_signal_emit( &destroy_signal, this );
}

//void
//Buffer::AddUpdate( const DFBSurfaceEvent &event )
//{
//     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p ) <- flip count %u\n", __FUNCTION__, this, event.flip_count );
//
//     updates.push( event );
//
//     ScheduleUpdate( event );
//}
//

void
Buffer::ScheduleUpdate( const DFBSurfaceEvent &event )
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );

     updates.push( Update( event ) );

     if (listener)
          listener->scheduleUpdate( event );
}

void
Buffer::ProcessUpdates( long long timestamp )
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );

     while (!updates.empty()) {
          Update &update = updates.front();

          if (update.event.time_stamp > timestamp) {
               D_DEBUG_AT( DFBWayland_Buffer, "  -> next update still in future (by %lld us)\n", update.event.time_stamp - timestamp );

               break;
          }

          D_DEBUG_AT( DFBWayland_Buffer, "  -> processing flip count %u\n", update.event.flip_count );
          D_DEBUG_AT( DFBWayland_Buffer, "  -> timestamp %lld us (within %lld us)\n",
                      update.event.time_stamp, update.event.time_stamp - timestamp );

          D_EVLOG( "WaylandDFB/Surface", this,
                   "Schedule/Ack", *Direct::String::F("[%3u] %d,%d-%dx%d (%lld us -> %lld since got) -> %lld since last ack",
                                                      update.event.flip_count,
                                                      DFB_RECTANGLE_VALS_FROM_REGION( &update.event.update ),
                                                      update.event.time_stamp, timestamp - last_got, timestamp - last_ack ) );

          last_ack = update.event.time_stamp;

          surface->FrameAck( surface, update.event.flip_count );

          if (listener)
               listener->processUpdate( update );

          updates.pop();

//          break;
     }
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static void
destroy_buffer( struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_Buffer, "%s( resource %p )\n", __FUNCTION__, resource );

     Buffer *buffer = (Buffer *) resource->data;

     buffer->wl_dfb->surfaces.erase( buffer->surface_id );

     delete buffer;
}

/**********************************************************************************************************************/

static void
buffer_destroy( struct wl_client *client, struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_Buffer, "%s( client %p, resource %p )\n", __FUNCTION__, client, resource );

     wl_resource_destroy( resource );
}

const struct wl_buffer_interface dfb_buffer_interface = {
     buffer_destroy
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static void
destroy_dfb( struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( resource %p )\n", __FUNCTION__, resource );
}

/**********************************************************************************************************************/

static void
dfb_create_buffer(struct wl_client   *client,
                  struct wl_resource *resource,
                  uint32_t            id,
                  uint32_t            surface_id)
{
     D_DEBUG_AT( DFBWayland_Buffer, "%s( client %p, resource %p, id %u, surface_id %u )\n",
                 __FUNCTION__, client, resource, id, surface_id );

     WLDFB *wl_dfb = (WLDFB*) resource->data;

     DFBResult  ret;
     Buffer    *buffer = new Buffer();

     buffer->wl_dfb = wl_dfb;
     buffer->surface_id = surface_id;

     ret = wl_dfb->dfb->GetSurface( wl_dfb->dfb, surface_id, &buffer->surface );
     if (ret) {
          D_DERROR( ret, "DFBWayland/Buffer: IDirectFB::GetSurface( %u ) failed!\n", surface_id );
          return;
     }

     D_DEBUG_AT( DFBWayland_Buffer, "  -> surface %p\n", buffer->surface );

     ret = buffer->surface->GetSize( buffer->surface, &buffer->size.w, &buffer->size.h );
     if (ret) {
          D_DERROR( ret, "DFBWayland/Buffer: IDirectFBSurface::GetSize( surface_id %u ) failed!\n", surface_id );
          return;
     }

     ret = buffer->surface->GetPixelFormat( buffer->surface, &buffer->format );
     if (ret) {
          D_DERROR( ret, "DFBWayland/Buffer: IDirectFBSurface::GetPixelFormat( surface_id %u ) failed!\n", surface_id );
          return;
     }

     buffer->resource = wl_resource_create( client, &wl_buffer_interface, 1, id );
     if (buffer->resource == NULL) {
          wl_client_post_no_memory(client);
          delete buffer;
          return;
     }

     wl_resource_set_implementation( buffer->resource, &dfb_buffer_interface, buffer, destroy_buffer );


     wl_dfb->surfaces[surface_id] = buffer;

     if (wl_dfb->events) {
          buffer->surface->AttachEventBuffer( buffer->surface, wl_dfb->events );
          buffer->surface->MakeClient( buffer->surface );
     }
}

const static struct wl_dfb_interface dfb_interface = {
     dfb_create_buffer
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static void
bind_dfb(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( client %p, data %p, version %u, id %u )\n", __FUNCTION__, client, data, version, id );

     WLDFB              *wl_dfb = (WLDFB *) data;
     struct wl_resource *resource;

     resource = wl_resource_create( client, &wl_dfb_interface, 1, id );
     if (resource == NULL) {
          wl_client_post_no_memory(client);
          return;
     }

     wl_resource_set_implementation( resource, &dfb_interface, wl_dfb, destroy_dfb );

     if (wl_dfb->callback)
          wl_dfb->callback( wl_dfb->callback_context, client );
}

/**********************************************************************************************************************/

extern "C" {

WLDFB *
wayland_dfb_init( struct wl_display      *display,
                  IDirectFB              *directfb,
                  wl_dfb_client_callback  callback,
                  void                   *callback_context,
                  IDirectFBEventBuffer   *events )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( display %p, directfb %p )\n", __FUNCTION__, display, directfb );

     WLDFB *wldfb = new WLDFB();

     directfb->AddRef( directfb );

     if (events)
          events->AddRef( events );

     wldfb->display          = display;
     wldfb->dfb              = directfb;
     wldfb->events           = events;
     wldfb->callback         = callback;
     wldfb->callback_context = callback_context;
     wldfb->global           = wl_global_create( display, &wl_dfb_interface, 1, wldfb, bind_dfb );

     if (!wldfb->global) {
          delete wldfb;
          return NULL;
     }

     return wldfb;
}

void
wayland_dfb_uninit(WLDFB *wl_dfb)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( wl_dfb %p )\n", __FUNCTION__, wl_dfb );

     delete wl_dfb;
}

void
wayland_dfb_handle_surface_event( WLDFB                 *dfb,
                                  const DFBSurfaceEvent *event )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( wl_dfb %p, event %p )\n", __FUNCTION__, dfb, event );

     dfb->HandleSurfaceEvent( *event );
}

int
wayland_is_dfb_buffer(struct wl_resource *resource)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( resource %p )\n", __FUNCTION__, resource );

     return resource->object.implementation == (void (**)(void)) &dfb_buffer_interface;
}

}




Server::SurfaceResource::SurfaceResource( Server             &server,
                                          struct wl_client   *client,
                                          struct wl_resource *resource, uint32_t id )
     :
     server( server )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Server::SurfaceResource::%s( %p, client %p, resource %p, id %u )\n", __FUNCTION__, this, client, resource, id );

     resource = wl_resource_create( client, &wl_surface_interface, wl_resource_get_version(resource), id );
     if (resource == NULL) {
          wl_resource_post_no_memory( resource );
          throw std::runtime_error( "wl_resource_create for surface failed" );
     }

     surface_interface.destroy = surface_destroy;
     surface_interface.attach  = surface_attach;

     wl_resource_set_implementation( resource, &surface_interface, this, destroy_surface );
}

void
Server::SurfaceResource::surface_attach(struct wl_client   *client,
                                        struct wl_resource *resource,
                                        struct wl_resource *buffer_resource, int32_t sx, int32_t sy)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Server::SurfaceResource::%s( client %p, resource %p, buffer_resource %p, xy %d,%d )\n",
                 __FUNCTION__, client, resource, buffer_resource, sx, sy );

     SurfaceResource      *surface    = (SurfaceResource*) wl_resource_get_user_data( resource );
     WL::Buffer           *dfb_buffer = NULL;
     struct wl_shm_buffer *shm_buffer = NULL;

     if (buffer_resource) {
          shm_buffer = wl_shm_buffer_get( buffer_resource );

          if (!shm_buffer && WL::wayland_is_dfb_buffer(buffer_resource)) {
               dfb_buffer = (WL::Buffer *) wl_resource_get_user_data( buffer_resource );
               if (dfb_buffer == NULL) {
                    wl_client_post_no_memory(client);
                    throw std::runtime_error( "wl_resource_get_user_data for buffer failed" );
               }
          }
     }
     else
          throw std::runtime_error( "no buffer resource" );


     /* Attach, attach, without commit in between does not send
      * wl_buffer.release. */
     if (surface->pending_buffer)
          wl_list_remove( &surface->pending_buffer_destroy_listener.link );

     surface->pending_buffer = new BufferHandle( dfb_buffer, shm_buffer );

     if (dfb_buffer)
          wl_signal_add( &dfb_buffer->destroy_signal,
                         &surface->pending_buffer_destroy_listener );

     if (surface->server.buffer_handler)
          surface->server.buffer_handler( *surface, *surface->pending_buffer );
}


Client::Client( const char *socket_name )
     :
     formats( 0 ),
     shm( NULL )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Client::%s( %p )\n", __FUNCTION__, this );

     display  = wl_display_connect( socket_name );
     registry = wl_display_get_registry( display );

     registry_listener.global        = registry_handle_global;
     registry_listener.global_remove = registry_handle_global_remove;

     shm_listener.format             = shm_handle_format;

     wl_registry_add_listener( registry, &registry_listener, this );

     D_DEBUG_AT( DFBWayland_wl_dfb, "  -> roundtrip...\n" );
     wl_display_roundtrip( display );

     if (wl_dfb == NULL)
          throw std::runtime_error( "No wl_dfb global" );
     if (shm == NULL)
          throw std::runtime_error( "No wl_shm global" );
     if (compositor == NULL)
          throw std::runtime_error( "No wl_compositor global" );

     D_DEBUG_AT( DFBWayland_wl_dfb, "  -> roundtrip...\n" );
//     wl_display_roundtrip( display );
     D_DEBUG_AT( DFBWayland_wl_dfb, "  -> roundtrip done.\n" );

//     if (!(formats & (1 << WL_SHM_FORMAT_XRGB8888)))
//          throw std::runtime_error( "WL_SHM_FORMAT_XRGB32 not available" );

     D_DEBUG_AT( DFBWayland_wl_dfb, "  -> getting fd...\n" );

     int fd = wl_display_get_fd( display );

     (void) fd;

     D_DEBUG_AT( DFBWayland_wl_dfb, "  -> done, fd = %d\n", fd );
}

Client::~Client()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Client::%s( %p )\n", __FUNCTION__, this );

     wl_display_destroy( display );
}

void
Client::run()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Client::%s( %p )\n", __FUNCTION__, this );

     wl_display_run( display );
}

void
Client::registry_handle_global(void *data, struct wl_registry *registry,
                               uint32_t id, const char *interface, uint32_t version)
{
     Client *client = (Client*) data;

     D_DEBUG_AT( DFBWayland_wl_dfb, "Client::%s( %p, %u, '%s' %d )\n", __FUNCTION__, data, id, interface, version );

     if (strcmp(interface, "wl_compositor") == 0) {
          client->compositor = (struct wl_compositor*) wl_registry_bind( registry, id, &wl_compositor_interface, version );
     }
     else if (strcmp(interface, "wl_shm") == 0) {
          client->shm = (struct wl_shm*) wl_registry_bind( registry, id, &wl_shm_interface, version );
          wl_shm_add_listener( client->shm, &client->shm_listener, client );
     }
     else
          client->wl_dfb = (struct wl_dfb*) wl_registry_bind( registry, id, &wl_dfb_interface, version );
}


void
Client::shm_handle_format(void *data, struct wl_shm *wl_shm, uint32_t format)
{
     Client *client = (Client*) data;

     D_DEBUG_AT( DFBWayland_wl_dfb, "Client::%s( %p, %p, %u )\n", __FUNCTION__, data, wl_shm, format );

     client->formats |= (1 << format);
}



}

