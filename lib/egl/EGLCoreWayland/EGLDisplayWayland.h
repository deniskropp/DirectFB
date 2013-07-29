/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef ___DIRECTFB__WAYLAND__EGLDISPLAYWAYLAND__H___
#define ___DIRECTFB__WAYLAND__EGLDISPLAYWAYLAND__H___


#ifdef __cplusplus
extern "C" {
#endif

//#include "wayland.h"

//DFBResult dfb_wayland_egl_display_register( DFBWayland *wayland );
//void      dfb_wayland_egl_display_unregister( void );

#ifdef __cplusplus
}


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/image.h>

#include <wayland-egl.h>
#include <wayland-egl-priv.h>

#include <wayland-dfb.h>
#include <wayland-dfb-client-protocol.h>
#include <wayland-dfb-server-protocol.h>


namespace DirectFB {

namespace Wayland {


using namespace DirectFB::EGL;


class Types : public Direct::Types<Types,EGL::Types>
{
};


class EGLDisplayWayland;
class SurfaceWLEGLWindow;


class EGLCoreModuleWayland : public EGL::CoreModule, public Types::Type<EGLCoreModuleWayland,EGL::Core>
{
     // no copy
     EGLCoreModuleWayland( const EGLCoreModuleWayland& ) = delete;

     // no assign
     EGLCoreModuleWayland& operator=( const EGLCoreModuleWayland& ) = delete;

protected:
     virtual Direct::String GetName() const
     {
          return "Wayland";
     }

     virtual DFBResult Initialise ( EGL::Core              &core );


     DFBResult Display_Probe      ( const EGL::Display     &display,
                                    unsigned int           &ret_score );

     DFBResult Display_Initialise ( EGLDisplayWayland      &display );


public:
     EGLCoreModuleWayland();

     static void
     registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version);

     static void
     registry_handle_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name);
};


class EGLDisplayWayland : public Types::Type<EGLDisplayWayland,EGL::Display>
{
     friend class EGLCoreModuleWayland;
     friend class WLBindWaylandDisplay;
     friend class SurfaceWLEGLWindow;

public:
     EGLDisplayWayland( EGL::Display         &display,
                        EGLCoreModuleWayland &module );
     virtual ~EGLDisplayWayland();


public:
     DFBResult Surface_Initialise( SurfaceWLEGLWindow        &surface );

     DFBResult Image_Initialise( EGL::KHR::Image &image );


protected:
     int roundtrip();


public:
     struct wl_display     *wl_display;
     struct wl_event_queue *wl_queue;
     struct wl_registry    *wl_registry;

     struct wl_dfb         *wl_server_dfb;
     struct wl_dfb         *wl_dfb;
};


class SurfaceWLEGLWindow : public Types::Type<SurfaceWLEGLWindow,EGL::Surface>
{
     friend class EGLDisplayWayland;

public:
     SurfaceWLEGLWindow( EGL::Surface      &surface,
                         EGLDisplayWayland &display );

     virtual ~SurfaceWLEGLWindow();

     EGLint SwapBuffers();

private:
     EGLDisplayWayland          &display;
     wl_egl_window              *window;
     wl_dfb_buffer              *buffer;
};


}

}




namespace WL {

class BindWaylandDisplay : public Types::Type<BindWaylandDisplay,DirectFB::EGL::EGLExtension>
{
public:
     BindWaylandDisplay();
     virtual ~BindWaylandDisplay();


     static EGLBoolean eglBindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *wl_display);

     static EGLBoolean eglUnbindWaylandDisplayWL(EGLDisplay dpy, struct wl_display *wl_display);

     static EGLBoolean eglQueryWaylandBufferWL(EGLDisplay dpy, struct wl_buffer *buffer, EGLint attribute, EGLint *value);


     typedef std::function<DFBResult( DirectFB::EGL::Display &egl_display,
                                      struct wl_display      *wl_display )>     BindWaylandDisplayWL;

     typedef std::function<DFBResult( DirectFB::EGL::Display &egl_display,
                                      struct wl_display      *wl_display )>     UnbindWaylandDisplayWL;


     DFBResult eglBindWaylandDisplay  ( DirectFB::Wayland::EGLDisplayWayland &display,
                                        struct wl_display                    *wl_display );

     DFBResult eglUnbindWaylandDisplay( DirectFB::Wayland::EGLDisplayWayland &display,
                                        struct wl_display                    *wl_display );


};

}


#endif // __cplusplus


#endif


