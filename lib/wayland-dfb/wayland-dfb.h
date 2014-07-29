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

#include <wayland-client-protocol.h>

#include <wayland-dfb-client-protocol.h>


#ifdef  __cplusplus

#include <direct/Type.h>

#include <queue>

namespace WL {

class WLDFB;

#define WL_WLDFB WL::WLDFB

extern "C" {
#else

typedef void WL_WLDFB;
#endif


#ifndef DFBPP_H
#define IDirectFB_C                IDirectFB
#define IDirectFBEventBuffer_C     IDirectFBEventBuffer
#define IDirectFBSurface_C         IDirectFBSurface
#endif


typedef void (*wl_dfb_client_callback)( void             *context,
                                        struct wl_client *client );

WL_WLDFB *
wayland_dfb_init( struct wl_display      *display,
                  IDirectFB_C            *dfb,
                  wl_dfb_client_callback  callback,
                  void                   *callback_context,
                  IDirectFBEventBuffer_C *events );

void
wayland_dfb_uninit( WL_WLDFB *dfb );

void
wayland_dfb_handle_surface_event( WL_WLDFB              *dfb,
                                  const DFBSurfaceEvent *event );

int
wayland_is_dfb_buffer( struct wl_resource *resource );



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

     WLDFB                         *wl_dfb;

     DFBSurfaceID                   surface_id;
     DFBSurfaceBufferID             buffer_id;
     DFBSurfaceAllocationID         allocation_id;

     IDirectFBSurface_C            *surface;
     DFBDimension                   size;
     DFBSurfacePixelFormat          format;

     std::queue<Update>             updates;

     long long                      last_got;
     long long                      last_ack;

     Listener                      *listener;

     struct wl_signal               destroy_signal;
};


class Server
{
public:
     Server( IDirectFB_C *dfb, IDirectFBEventBuffer_C *events, const char *socket_name )
     {
          display = wl_display_create();

          compositor_interface.create_surface = create_surface;

          if (!wl_global_create( display, &wl_compositor_interface, 3, this, compositor_bind ))
               throw std::runtime_error( "wl_global_create failed" );

          wl_display_init_shm( display );

          wl_dfb = wayland_dfb_init( display, dfb, client_callback, NULL, events );

          struct wl_event_loop *loop = wl_display_get_event_loop( display );

          timeout_source = wl_event_loop_add_timer( loop, timeout_handler, this );

          if (wl_display_add_socket( display, socket_name ))
               throw std::runtime_error( Direct::String::F( "failed to add socket '%s': %m", socket_name ) );
     }

     ~Server()
     {
          wl_event_source_remove( timeout_source );

          wayland_dfb_uninit( wl_dfb );
     }

     void
     run( long millis = -1 )
     {
          if (millis != -1)
               wl_event_source_timer_update( timeout_source, millis );

          wl_display_run( display );
     }

     static int
     timeout_handler( void *data )
     {
          Server *server = (Server*) data;

          wl_display_terminate( server->display );

          return 1;
     }

     static void
     client_callback( void             *context,
                      struct wl_client *client )
     {
          pid_t pid;
          uid_t uid;
          gid_t gid;

          wl_client_get_credentials( client, &pid, &uid, &gid );

          D_INFO( "DFBWayland/Server: New Client PID %d\n", pid );
     }



public:
     class SurfaceResource;

     class BufferHandle
     {
     public:
          WL::Buffer           *dfb_buffer;
          struct wl_shm_buffer *shm_buffer;

          BufferHandle( WL::Buffer           *dfb_buffer,
                        struct wl_shm_buffer *shm_buffer )
               :
               dfb_buffer( dfb_buffer ),
               shm_buffer( shm_buffer )
          {
          }
     };

     typedef std::function<void(SurfaceResource &, BufferHandle &)> BufferHandler;

     void
     SetBufferHandler( BufferHandler handler )
     {
          buffer_handler = handler;
     }


private:
     struct wl_display              *display;
     WLDFB                          *wl_dfb;
     struct wl_compositor_interface  compositor_interface;
     struct wl_event_source         *timeout_source;
protected:
     BufferHandler                   buffer_handler;

public:
     class SurfaceResource
     {
     public:
          SurfaceResource( Server             &server,
                           struct wl_client   *client,
                           struct wl_resource *resource, uint32_t id );

     private:
          static void
          destroy_surface( struct wl_resource *resource )
          {
               SurfaceResource *surface = (SurfaceResource*) wl_resource_get_user_data( resource );

               /* Set the resource to NULL, since we don't want to leave a
                * dangling pointer if the surface was refcounted and survives
                * the weston_surface_destroy() call. */
               surface->resource = NULL;
               delete surface;
          }

          static void
          surface_destroy( struct wl_client *client, struct wl_resource *resource )
          {
               wl_resource_destroy( resource );
          }

          static void
          surface_attach(struct wl_client   *client,
                         struct wl_resource *resource,
                         struct wl_resource *buffer_resource, int32_t sx, int32_t sy);

          Server                      &server;
          wl_resource                 *resource;
          struct wl_surface_interface  surface_interface;
          struct BufferHandle         *pending_buffer;
          struct wl_listener           pending_buffer_destroy_listener;
     };

private:
     static void
     create_surface( struct wl_client   *client,
                     struct wl_resource *resource, uint32_t id )
     {
          Server *server = (Server *) wl_resource_get_user_data( resource );

          new SurfaceResource( *server, client, resource, id );
     }

     static void
     compositor_bind( struct wl_client *client,
                      void *data, uint32_t version, uint32_t id )
     {
          Server             *server = (Server *) data;
          struct wl_resource *resource;

          resource = wl_resource_create( client, &wl_compositor_interface, MIN(version, 3), id );
          if (resource == NULL) {
               wl_client_post_no_memory( client );
               throw std::runtime_error( "wl_resource_create for compositor failed" );
          }

          wl_resource_set_implementation( resource, &server->compositor_interface, server, NULL );
     }
};


class Client
{
public:
     Client( const char *socket_name );

     ~Client();

     void run();

     struct wl_display    *getDisplay() const { return display; }
     struct wl_compositor *getCompositor() const { return compositor; }
     struct wl_dfb        *getWLDFB() const { return wl_dfb; }


private:
     struct wl_display           *display;
     struct wl_registry          *registry;
     struct wl_compositor        *compositor;
	uint32_t                     formats;
	struct wl_shm               *shm;
	struct wl_dfb               *wl_dfb;
     struct wl_registry_listener  registry_listener;
     struct wl_shm_listener       shm_listener;


     static void
     registry_handle_global(void *data, struct wl_registry *registry,
                            uint32_t id, const char *interface, uint32_t version);

     static void
     registry_handle_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name)
     {
     }


     static void
     shm_handle_format(void *data, struct wl_shm *wl_shm, uint32_t format);
};



}


#ifndef DFBPP_H
#undef IDirectFB
#undef IDirectFBEventBuffer
#undef IDirectFBSurface
#endif


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

