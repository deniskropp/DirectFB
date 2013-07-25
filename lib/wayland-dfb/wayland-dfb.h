#ifndef WAYLAND_DFB_H
#define WAYLAND_DFB_H

#include <directfb.h>

#include <wayland-server.h>


#ifdef  __cplusplus
extern "C" {
#endif


struct wl_dfb;


struct wl_dfb *
wayland_dfb_init( struct wl_display *display,
                  IDirectFB         *dfb );

void
wayland_dfb_uninit(struct wl_dfb *dfb);

int
wayland_is_dfb_buffer(struct wl_resource *resource);



#ifdef  __cplusplus
}
#endif


#include <direct/Type.h>


namespace WL {

class Types : public Direct::Types<Types>
{
};


class Buffer : public Types::Type<Buffer>
{
     friend class EGLDisplayWayland;

public:
     Buffer();

     virtual ~Buffer();

public:
     struct wl_resource            *resource;

     struct wl_dfb                 *wl_dfb;

     DFBSurfaceID                   surface_id;
     DFBSurfaceBufferID             buffer_id;
     DFBSurfaceAllocationID         allocation_id;

     IDirectFBSurface              *surface;
     DFBDimension                   size;
};


}



#endif

