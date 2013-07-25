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
#include "wayland-dfb.h"
#include "wayland-dfb-server-protocol.h"


D_LOG_DOMAIN( DFBWayland_wl_dfb,      "DFBWayland/wl_dfb",      "DirectFB Wayland extension" );


struct wl_dfb {
     struct wl_global  *global;

     struct wl_display *display;
     IDirectFB         *dfb;
};


namespace WL {



Buffer::Buffer()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Buffer::%s()\n", __FUNCTION__ );
}

Buffer::~Buffer()
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "Buffer::%s()\n", __FUNCTION__ );

     surface->Release( surface );
}


static void
destroy_buffer( struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     Buffer        *buffer = (Buffer *) resource->data;

     delete buffer;
}

static void
buffer_destroy( struct wl_client *client, struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     wl_resource_destroy( resource );
}

const struct wl_buffer_interface dfb_buffer_interface = {
     buffer_destroy
};



static void
dfb_create_buffer(struct wl_client   *client,
                  struct wl_resource *resource,
                  uint32_t            id,
                  uint32_t            surface_id,
                  uint32_t            buffer_id,
                  uint32_t            allocation_id)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     struct wl_dfb *wl_dfb = (struct wl_dfb*) resource->data;

     DFBResult  ret;
     Buffer    *buffer = new Buffer();

     buffer->wl_dfb = wl_dfb;
     buffer->surface_id = surface_id;
     buffer->buffer_id = buffer_id;
     buffer->allocation_id = allocation_id;

     ret = wl_dfb->dfb->GetSurface( wl_dfb->dfb, surface_id, &buffer->surface );
     if (ret) {
          D_DERROR( ret, "DFBWayland/wl_dfb: IDirectFB::GetSurface( %u ) failed!\n", surface_id );
          return;
     }

     ret = buffer->surface->GetSize( buffer->surface, &buffer->size.w, &buffer->size.h );
     if (ret) {
          D_DERROR( ret, "DFBWayland/wl_dfb: IDirectFBSurface::GetSize( surface_id %u ) failed!\n", surface_id );
          return;
     }

     buffer->resource = wl_resource_create( client, &wl_buffer_interface, 1, id );
     if (buffer->resource == NULL) {
          wl_client_post_no_memory(client);
          delete buffer;
          return;
     }

     wl_resource_set_implementation( buffer->resource, &dfb_buffer_interface, buffer, destroy_buffer );
}

const static struct wl_dfb_interface dfb_interface = {
     dfb_create_buffer
};




static void
destroy_dfb( struct wl_resource *resource )
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     struct wl_dfb *wl_dfb = (struct wl_dfb *) resource->data;

     delete wl_dfb;
}

static void
bind_dfb(struct wl_client *client, void *data, uint32_t version, uint32_t id)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     struct wl_dfb      *wl_dfb = (struct wl_dfb *) data;
     struct wl_resource *resource;

     resource = wl_resource_create( client, &wl_dfb_interface, 1, id );
     if (resource == NULL) {
          wl_client_post_no_memory(client);
          return;
     }

     wl_resource_set_implementation( resource, &dfb_interface, wl_dfb, destroy_dfb );
}



extern "C" {

struct wl_dfb *
wayland_dfb_init(struct wl_display *display,
                 IDirectFB         *directfb)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     struct wl_dfb *wl_dfb;

     wl_dfb = new struct wl_dfb;

     wl_dfb->display = display;
     wl_dfb->dfb     = directfb;
     wl_dfb->global  = wl_global_create( display, &wl_dfb_interface, 1, wl_dfb, bind_dfb );

     if (!wl_dfb->global) {
          delete wl_dfb;
          return NULL;
     }

     return wl_dfb;
}

void
wayland_dfb_uninit(struct wl_dfb *wl_dfb)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     wl_global_destroy( wl_dfb->global );

     delete wl_dfb;
}

int
wayland_is_dfb_buffer(struct wl_resource *resource)
{
     D_DEBUG_AT( DFBWayland_wl_dfb, "%s()\n", __FUNCTION__ );

     return resource->object.implementation == (void (**)(void)) &dfb_buffer_interface;
}

}



}

