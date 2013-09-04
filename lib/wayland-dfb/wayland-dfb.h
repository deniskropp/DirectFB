#ifndef WAYLAND_DFB_H
#define WAYLAND_DFB_H

/*

     Wayland Compositor                                                    Wayland Client
          - CreateWindow           <- create_surface
          - GetSurface, AttachEventBuffer
                                                                           - wl_surface id

                                                                      wl_shm              wl_egl_create_window
                                                                      - create pool       eglCreateWindowSurface
                                                                      - create buffer     -> wayland_dfb_get_surface( wl_surface_id )
          - Allocate               <- attach buffer (wl_shm)
                                   <- damage / commit
          - Flip



                                                                                          SurfacePeer
                                                                                          -> GetAllocations

                                                                                          eglSwapBuffers
                                                                                          -> Flip
                                   Update Event             <--
                                   - flags
          - lookup surface         - surface_id
          - get buffer             - buffer_id
                                   - allocation_id
          - release buffer         - rectangle
                                                            -->                           - FrameAck



          GL Renderer (attach buffer)
 
          - eglDestroyImageKHR (prev)
          - eglCreateImageKHR (buffer)
               -> GetAllocation/Allocate
               -> Update?
          - glEGLImageTargetTexture2D




          - wlc-pid                IDirectFBWindows

            pid != wlc-pid         <- window added (window_id)

            - GetWindow
            - GetSurface, AttachEventBuffer
            - create_surface
                                   Update Events
                                   - flags
                                   - surface_id
                                   - buffer_id
                                   - allocation_id
                                   - rectangle






     DirectFB Compositor



*/

#include <directfb.h>

#include <wayland-server.h>


#ifdef  __cplusplus

#include <direct/Type.h>

namespace WL {


extern "C" {
#else

struct wl_dfb;
#endif


typedef void (*wl_dfb_client_callback)( void             *context,
                                        struct wl_client *client );

struct wl_dfb *
wayland_dfb_init( struct wl_display      *display,
                  IDirectFB              *dfb,
                  wl_dfb_client_callback  callback,
                  void                   *callback_context,
                  IDirectFBEventBuffer   *events );

void
wayland_dfb_uninit(struct wl_dfb *dfb);

void
wayland_dfb_handle_surface_event( struct wl_dfb         *dfb,
                                  const DFBSurfaceEvent *event );

int
wayland_is_dfb_buffer(struct wl_resource *resource);



#ifdef  __cplusplus
}



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
     DFBSurfacePixelFormat          format;

     unsigned int                   flip_count;
};


}



#endif


#endif

