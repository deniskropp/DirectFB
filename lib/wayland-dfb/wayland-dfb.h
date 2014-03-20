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

#include <queue>

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
     class Update {
     public:
          DFBSurfaceEvent event;

          Update( const DFBSurfaceEvent &event )
               :
               event( event )
          {
          }
     };

     class Listener {
     public:
          virtual void scheduleUpdate( const Update &update ) = 0;
          virtual void processUpdate ( const Update &update ) = 0;
     };

public:
     Buffer();

     virtual ~Buffer();


     void ScheduleUpdate( const DFBSurfaceEvent &event );
     void ProcessUpdates( long long              timestamp );

     void SetListener( Listener *listener )
     {
          this->listener = listener;
     }

public:
     struct wl_resource            *resource;

     struct wl_dfb                 *wl_dfb;

     DFBSurfaceID                   surface_id;
     DFBSurfaceBufferID             buffer_id;
     DFBSurfaceAllocationID         allocation_id;

     IDirectFBSurface              *surface;
     DFBDimension                   size;
     DFBSurfacePixelFormat          format;

     std::queue<Update>             updates;

     long long                      last_got;
     long long                      last_ack;

     Listener                      *listener;
};


}



/*

  WestonDFB                                                      Wayland EGL/DFB Client


                                   = wl_connection =

     weston_surface                                              <- wl_compositor_create_surface( wl_surface_id )
                                                                           = wl_surface

          WL::Surface                                            <- wl_dfb_create_surface( DFBSurfaceID )
                                                                           = wl_dfb_surface

                                                                 <- wl_surface_attach( wl_dfb_surface )
                                                                              /commit

     -> directfb_surface_attach( weston_surface, WL::Surface )

               -> attach event buffer / make client



                                                                 IDirectFBSurface::Flip()

                                   = event buffer =

                                                            <- - - flip_count / region
     -> HandleSurfaceEvent( event )
          -> lookup WL::Surface( event.DFBSurfaceID )

               -> WL::Surface::AddUpdate( count, region )

                    + WL::Updates
                              - flip_count
                              - region

                         -> schedule Update (if not pending in WL::Surface)

                              -> weston_surface_damage( region )
                              -> weston_surface_commit()
                                   -> weston_output_schedule_repaint()


          -> directfb_output_repaint()

                    -> WL::Surface::CheckUpdate()

                         -> check if Update was scheduled
                              -> IDirectFBSurface::FrameAck()    - - ->    wake up WaitForBackBuffer
                              -> renderer->attach()
                                   -> eglCreateImageKHR()
 
                              -> schedule next Frame if queue not empty...

                                   -> weston_surface_damage( region )
                                   -> weston_surface_commit()
                                        -> weston_output_schedule_repaint()

*/



#endif


#endif

