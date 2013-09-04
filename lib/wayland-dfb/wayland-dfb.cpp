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

#include <directfb_util.h>

#include "wayland-dfb.h"
#include "wayland-dfb-server-protocol.h"


D_LOG_DOMAIN( DFBWayland_wl_dfb,      "DFBWayland/wl_dfb",      "DirectFB Wayland extension" );
D_LOG_DOMAIN( DFBWayland_Buffer,      "DFBWayland/Buffer",      "DirectFB Wayland Buffer" );

/**********************************************************************************************************************/

namespace WL {

struct wl_dfb {
public:
     wl_dfb();
     ~wl_dfb();

     struct wl_global                  *global;

     struct wl_display                 *display;
     IDirectFB                         *dfb;
     IDirectFBEventBuffer              *events;

     wl_dfb_client_callback             callback;
     void                              *callback_context;

     std::map<DFBSurfaceID,Buffer*>     surfaces;

     void HandleSurfaceEvent( const DFBSurfaceEvent &event );
};


wl_dfb::wl_dfb()
     :
     global( NULL ),
     display( NULL ),
     dfb( NULL )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "wl_dfb::%s( %p )\n", __FUNCTION__, this );
}

wl_dfb::~wl_dfb()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "wl_dfb::%s( %p )\n", __FUNCTION__, this );

     if (global)
          wl_global_destroy( global );

     if (dfb)
          dfb->Release( dfb );
}

void
wl_dfb::HandleSurfaceEvent( const DFBSurfaceEvent &event )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "wl_dfb::%s( %p, event %p ) <- type 0x%04x\n", __FUNCTION__, this, &event, event.type );

     switch (event.type) {
          case DSEVT_UPDATE: {
               D_DEBUG_AT( DFBWayland_wl_dfb, "  -> UPDATE %u %d,%d-%dx%d %d\n",
                           event.surface_id, DFB_RECTANGLE_VALS_FROM_REGION( &event.update ), event.flip_count );

               Buffer *surface = surfaces[event.surface_id];

               if (surface) {
                    surface->surface->FrameAck( surface->surface, event.flip_count );

                    surface->flip_count = event.flip_count;
               }
               else
                    D_LOG( DFBWayland_wl_dfb, VERBOSE, "  -> SURFACE WITH ID %u NOT FOUND\n", event.surface_id );

               break;
          }

          default:
               break;
     }
}

/**********************************************************************************************************************/

Buffer::Buffer()
     :
     flip_count( 0 )
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );
}

Buffer::~Buffer()
{
     D_DEBUG_AT( DFBWayland_Buffer, "Buffer::%s( %p )\n", __FUNCTION__, this );

     surface->DetachEventBuffer( surface, wl_dfb->events );
     surface->Release( surface );
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
                  uint32_t            surface_id,
                  uint32_t            buffer_id,
                  uint32_t            allocation_id)
{
     D_DEBUG_AT( DFBWayland_Buffer, "%s( client %p, resource %p, id %u, surface_id %u, buffer_id %u, allocation_id %u )\n",
                 __FUNCTION__, client, resource, id, surface_id, buffer_id, allocation_id );

     struct wl_dfb *wl_dfb = (struct wl_dfb*) resource->data;

     DFBResult  ret;
     Buffer    *buffer = new Buffer();

     buffer->wl_dfb = wl_dfb;
     buffer->surface_id = surface_id;
     buffer->buffer_id = buffer_id;
     buffer->allocation_id = allocation_id;

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

     buffer->surface->AttachEventBuffer( buffer->surface, wl_dfb->events );
     buffer->surface->MakeClient( buffer->surface );
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

     struct wl_dfb      *wl_dfb = (struct wl_dfb *) data;
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

struct wl_dfb *
wayland_dfb_init( struct wl_display      *display,
                  IDirectFB              *directfb,
                  wl_dfb_client_callback  callback,
                  void                   *callback_context,
                  IDirectFBEventBuffer   *events )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( display %p, directfb %p )\n", __FUNCTION__, display, directfb );

     wl_dfb *wldfb = new wl_dfb();

     directfb->AddRef( directfb );
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
wayland_dfb_uninit(wl_dfb *wl_dfb)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s( wl_dfb %p )\n", __FUNCTION__, wl_dfb );

     delete wl_dfb;
}

void
wayland_dfb_handle_surface_event( struct wl_dfb         *dfb,
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



}

