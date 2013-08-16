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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <wayland-client.h>

extern "C" {
#include <direct/messages.h>

#include <core/system.h>

#include <idirectfb.h>
}

#include <direct/ToString.h>

#include <core/CoreDFB.h>

#include "EGLDisplayWayland.h"


D_LOG_DOMAIN( DFBWayland_EGLCoreModule,    "DFBWayland/EGLCoreModule",    "DirectFB Wayland EGL Core Module" );
D_LOG_DOMAIN( DFBWayland_EGLDisplay,       "DFBWayland/EGLDisplay",       "DirectFB Wayland EGL Display" );

D_LOG_DOMAIN( DFBWayland_BindDisplay,      "DFBWayland/BindDisplay",      "DirectFB Wayland EGL BindDisplay extension" );


using namespace std::placeholders;


namespace DirectFB {

namespace Wayland {


__attribute__((constructor))
void dfb_wayland_egl_core_register( void );


void
dfb_wayland_egl_core_register()
{
     static EGLCoreModuleWayland egl_core_module;

     direct_modules_register( &EGL::core_modules,
                              DFBEGL_CORE_ABI_VERSION,
                              "dfbegl_core_wayland", &egl_core_module );
}

/**********************************************************************************************************************/

EGLCoreModuleWayland::EGLCoreModuleWayland()
{
     D_DEBUG_AT( DFBWayland_EGLCoreModule, "EGLCoreModuleWayland::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
EGLCoreModuleWayland::Initialise( DirectFB::EGL::Core &core )
{
     D_DEBUG_AT( DFBWayland_EGLCoreModule, "EGLCoreModuleWayland::%s( %p, core %p )\n",
                 __FUNCTION__, this, &core );

     Core::Register< Display::Probe >     ( EGLDisplayWayland::GetTypeInstance().GetName(), std::bind( &EGLCoreModuleWayland::Display_Probe, this, _1, _2 ) );
     Core::Register< Display::Initialise >( EGLDisplayWayland::GetTypeInstance().GetName(), std::bind( &EGLCoreModuleWayland::Display_Initialise, this, _1 ) );

     EGLDisplayWayland::RegisterConversion< EGL::Display, EGLCoreModuleWayland& >( *this );

     Display::Register< EGLExtension::GetNames >( GetName(), [](){ return "DIRECTFB_display_wayland";} );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
EGLCoreModuleWayland::Display_Probe( const EGL::Display &display,
                                     unsigned int       &ret_score )
{
     D_DEBUG_AT( DFBWayland_EGLCoreModule, "EGLCoreModuleWayland::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     if (display.native_display == idirectfb_singleton) {
          D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> is IDirectFB singleton = %p!\n", idirectfb_singleton );
          return DFB_UNSUPPORTED;
     }

     ret_score = 0;

     void *deref = NULL;

     if (direct_pointer_is_accessible( display.native_display ))
          deref = *(void **) display.native_display;
     else
          D_DEBUG_AT( DFBWayland_EGLCoreModule, "  -> pointer not accessible\n" );

     if (deref == &wl_display_interface) {
          D_DEBUG_AT( DFBWayland_EGLCoreModule, "  -> *display.native_display == &wl_display_interface (%p)\n", &wl_display_interface );

          ret_score = 100;
     }
     else {
          D_DEBUG_AT( DFBWayland_EGLCoreModule, "  -> *display.native_display (%p) != &wl_display_interface (%p)\n", deref, &wl_display_interface );

          if (display.native_display != 0) {
               char *env = getenv("EGL_PLATFORM");

               if (env && !strcmp( env, "wayland" )) {
                    D_DEBUG_AT( DFBWayland_EGLCoreModule, "  -> EGL_PLATFORM is set to 'wayland'\n" );

                    ret_score = 40;
               }
          }
     }

     return DFB_OK;
}


void
EGLCoreModuleWayland::registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
                                             const char *interface, uint32_t version)
{
     EGLDisplayWayland *display = (EGLDisplayWayland*) data;

     if (version > 1)
          version = 2;

     if (strcmp(interface, "wl_dfb") == 0) {
          display->wl_dfb = (struct wl_dfb*) wl_registry_bind(registry, name, &wl_dfb_interface, version);

//          wl_dfb_add_listener(dri2_dpy->wl_dfb, &dfb_listener, display);
     }
}

void
EGLCoreModuleWayland::registry_handle_global_remove(void *data, struct wl_registry *registry,
                                                    uint32_t name)
{
}

static const struct wl_registry_listener registry_listener = {
     EGLCoreModuleWayland::registry_handle_global,
     EGLCoreModuleWayland::registry_handle_global_remove
};

DFBResult
EGLCoreModuleWayland::Display_Initialise( EGLDisplayWayland &display )
{
     D_DEBUG_AT( DFBWayland_EGLCoreModule, "EGLCoreModuleWayland::%s( %p, display %p, wayland_display %p )\n",
                 __FUNCTION__, this, &display, display.wl_display );

     // Wayland Client Initialisation

     display.parent.native_pixmap_target = EGL_WAYLAND_BUFFER_WL;

     display.wl_display = (::wl_display*) display.parent.native_display;

     EGL::Surface::Register< EGL::Surface::Initialise >( display.GetName(),
                                                         std::bind( &EGLDisplayWayland::Surface_Initialise, &display, _1 ) );

     SurfaceWLEGLWindow::RegisterConversion< EGL::Surface, EGLDisplayWayland& >( display );



     display.wl_queue = wl_display_create_queue( display.wl_display );

     wl_display_dispatch_pending( display.wl_display );

     display.wl_registry = wl_display_get_registry( display.wl_display );

     wl_proxy_set_queue( (struct wl_proxy *) display.wl_registry, display.wl_queue );

     wl_registry_add_listener( display.wl_registry, &registry_listener, &display );

     if (display.roundtrip() < 0 || display.wl_dfb == NULL)
          goto error;

     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_dfb %p\n",  display.wl_dfb );
     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_queue %p\n",  display.wl_queue );
     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_registry %p\n",  display.wl_registry );

     return DFB_OK;


error:
     // FIXME: cleanup

     return DFB_FAILURE;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

EGLDisplayWayland::EGLDisplayWayland( EGL::Display         &display,
                                      EGLCoreModuleWayland &module )
     :
     Type( display ),
     wl_display( NULL ),
     wl_queue( NULL ),
     wl_registry( NULL ),
     wl_server_dfb( NULL ),
     wl_dfb( NULL )
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "EGLDisplayWayland::%s( %p, native_display 0x%08lx )\n", __FUNCTION__, this, (unsigned long) display.native_display );

     KHR::Image::Register< KHR::Image::Initialise >( EGLInt(EGL_WAYLAND_BUFFER_WL),
                                                     std::bind( &EGLDisplayWayland::Image_Initialise, this, _1 ) );
}

EGLDisplayWayland::~EGLDisplayWayland()
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "EGLDisplayWayland::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

DFBResult
EGLDisplayWayland::Surface_Initialise( SurfaceWLEGLWindow &surface )
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "EGLDisplayWayland::%s( %p ) <- window %p\n",
                 __FUNCTION__, this, surface.window );

     DFBResult ret;


     wl_egl_window *window = surface.window;



     DFBSurfaceDescription desc;

     desc.width  = window->width;
     desc.height = window->height;

     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> size %dx%d\n", desc.width, desc.height );

     desc.flags       = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH       | DSDESC_HEIGHT |
                                                     /*DSDESC_PIXELFORMAT |*/ DSDESC_CAPS   |
                                                     DSDESC_RESOURCE_ID | DSDESC_HINTS);
     desc.pixelformat = DSPF_ARGB;
     desc.caps        = DSCAPS_SHARED;
     desc.resource_id = wl_proxy_get_id( (wl_proxy*) window->surface );
     desc.hints       = DSHF_NONE;

     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wayland surface_id %lu\n", desc.resource_id );

     //if (WINDOW)
     D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );
     D_FLAGS_SET( desc.caps, DSCAPS_FLIPPING );
     D_FLAGS_SET( desc.hints, DSHF_WINDOW );

     IDirectFB *dfb = parent.GetDFB();

     dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );

     ret = dfb->CreateSurface( dfb, &desc, &surface.parent.surface );
     if (ret) {
          D_DERROR( ret, "DFBEGL/SurfaceWLEGLWindow: IDirectFB::CreateSurface() failed!\n" );
          return ret;
     }

     surface.parent.surface->AllowAccess( surface.parent.surface, "*" );

     return DFB_OK;
}

DFBResult
EGLDisplayWayland::Image_Initialise( DirectFB::EGL::KHR::Image &image )
{

     D_DEBUG_AT( DFBWayland_EGLDisplay, "EGLDisplayWayland::%s( %p, image %p )\n", __FUNCTION__, this, &image );

     D_ASSERT( image.target == EGL_WAYLAND_BUFFER_WL );

     DFBResult         ret;
     WL::Buffer       *buffer  = (WL::Buffer *) wl_resource_get_user_data( (struct wl_resource*) image.buffer );
     IDirectFBSurface *surface = buffer->surface;

     ret = (DFBResult) surface->AddRef( surface );
     if (ret) {
          D_DERROR_AT( DFBWayland_EGLDisplay, ret, "  -> IDirectFBSurface::AddRef() failed!\n" );
          return ret;
     }

     int w, h;

     surface->GetSize( surface, &w, &h );

     D_INFO( "DFBEGL/Image: New EGLImage from WL::Buffer (%dx%d)\n", w, h );

     image.surface = surface;

     return DFB_OK;
}



static void
sync_callback(void *data, struct wl_callback *callback, uint32_t serial)
{
   int *done = (int*) data;

   *done = 1;
   wl_callback_destroy(callback);
}

static const struct wl_callback_listener sync_listener = {
   sync_callback
};

int
EGLDisplayWayland::roundtrip()
{
     struct wl_callback *callback;
     int done = 0, ret = 0;

     callback = wl_display_sync( wl_display );

     wl_callback_add_listener( callback, &sync_listener, &done );

     wl_proxy_set_queue( (struct wl_proxy *) callback, wl_queue );

     while (ret != -1 && !done)
          ret = wl_display_dispatch_queue( wl_display, wl_queue );

     if (!done)
          wl_callback_destroy(callback);

     return ret;
}

/**********************************************************************************************************************/

SurfaceWLEGLWindow::SurfaceWLEGLWindow( EGL::Surface      &surface,
                                        EGLDisplayWayland &display )
     :
     Type( surface ),
     display( display ),
     window( (wl_egl_window*) surface.native_handle.value )
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "SurfaceWLEGLWindow::%s( %p, window %p, display %p )\n",
                 __FUNCTION__, this, window, &display );

     EGL::Surface::Register< EGL::Surface::SwapBuffersFunc >( GetName(), std::bind( &SurfaceWLEGLWindow::SwapBuffers, this ) );

     surface.surface->AllowAccess( surface.surface, "*" );
}

SurfaceWLEGLWindow::~SurfaceWLEGLWindow()
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "SurfaceWLEGLWindow::%s( %p )\n",  __FUNCTION__, this );
}

EGLint
SurfaceWLEGLWindow::SwapBuffers()
{
     D_DEBUG_AT( DFBWayland_EGLDisplay, "SurfaceWLEGLWindow::%s( %p, display %p )\n",  __FUNCTION__, this, &display );

     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_dfb %p\n",  display.wl_dfb );
     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_queue %p\n",  display.wl_queue );
     D_DEBUG_AT( DFBWayland_EGLDisplay, "  -> wl_registry %p\n",  display.wl_registry );

     buffer = wl_dfb_create_buffer( display.wl_dfb, parent.GetID(), 0, 0 );

     wl_surface_attach( window->surface, (struct wl_buffer*) buffer, 0, 0 );
     wl_surface_damage( window->surface, 0, 0, window->width, window->height );
     wl_surface_commit( window->surface );

     wl_dfb_buffer_destroy( buffer );

     return EGL_SUCCESS;
}


}

}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

namespace WL {

static BindWaylandDisplay bind_wayland_display;

/**********************************************************************************************************************/
/**********************************************************************************************************************/

BindWaylandDisplay::BindWaylandDisplay()
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "BindWaylandDisplay::%s( %p )\n", __FUNCTION__, this );

     DirectFB::EGL::Display::Register< DirectFB::EGL::EGLExtension::GetNames >( GetName(), [](){ return "EGL_WL_bind_wayland_display";} );

     DirectFB::EGL::Core::Register<
     DirectFB::EGL::Core::GetProcAddress
     >( "eglBindWaylandDisplayWL", [](const char *){ return (void*) eglBindWaylandDisplayWL;} );

     DirectFB::EGL::Core::Register<
     DirectFB::EGL::Core::GetProcAddress
     >( "eglUnbindWaylandDisplayWL", [](const char *){ return (void*) eglUnbindWaylandDisplayWL;} );

     DirectFB::EGL::Core::Register<
     DirectFB::EGL::Core::GetProcAddress
     >( "eglQueryWaylandBufferWL", [](const char *){ return (void*) eglQueryWaylandBufferWL;} );

     DirectFB::EGL::Display::Register< BindWaylandDisplayWL >( "", std::bind( &BindWaylandDisplay::eglBindWaylandDisplay, this, _1, _2 ) );
//     DirectFB::EGL::Display::Register< UnbindWaylandDisplayWL >( "", std::bind( &BindWaylandDisplay::eglUnbindWaylandDisplay, this, _1, _2 ) );
}

BindWaylandDisplay::~BindWaylandDisplay()
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "BindWaylandDisplay::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

EGLBoolean
BindWaylandDisplay::eglBindWaylandDisplayWL( EGLDisplay         dpy,
                                               struct wl_display *wl_display )
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "%s( display %p, wl_display %p )\n", __FUNCTION__, dpy, wl_display );

     DirectFB::EGL::TLS *tls = DirectFB::EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!wl_display)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     DirectFB::EGL::Display *display = (DirectFB::EGL::Display*) dpy;

     DirectFB::EGL::Display::Call< BindWaylandDisplayWL >()( *display, wl_display );


     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean
BindWaylandDisplay::eglUnbindWaylandDisplayWL( EGLDisplay         dpy,
                                                 struct wl_display *wl_display )
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "%s( display %p, wl_display %p )\n", __FUNCTION__, dpy, wl_display );

     DirectFB::EGL::TLS *tls = DirectFB::EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!wl_display)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     DirectFB::EGL::Display *display = (DirectFB::EGL::Display*) dpy;

     DirectFB::EGL::Display::Call< UnbindWaylandDisplayWL >()( *display, wl_display );


     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}

EGLBoolean
BindWaylandDisplay::eglQueryWaylandBufferWL( EGLDisplay        dpy,
                                               struct wl_buffer *wl_buffer,
                                               EGLint            attribute,
                                               EGLint           *value )
{
     WL::Buffer *buffer;

     D_DEBUG_AT( DFBWayland_BindDisplay, "%s( display %p, wl_buffer %p, attribute 0x%04x '%s' )\n",
                 __FUNCTION__, dpy, wl_buffer, attribute, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(attribute) ) );

     DirectFB::EGL::TLS     *tls = DirectFB::EGLTLS.Get();
//     EGL::Context *ctx = tls->GetContext();

     if (!wl_buffer)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);

     buffer = (WL::Buffer *) wl_resource_get_user_data( (struct wl_resource*) wl_buffer );

     switch (attribute) {
     case EGL_TEXTURE_FORMAT:
          *value = EGL_TEXTURE_RGBA;
          DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);

     case EGL_WIDTH:
          *value = buffer->size.w;
          DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);

     case EGL_HEIGHT:
          *value = buffer->size.h;
          DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
     }

     DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);
}

DFBResult
BindWaylandDisplay::eglBindWaylandDisplay( DirectFB::Wayland::EGLDisplayWayland &display,
                                           struct wl_display *wl_display )
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "EGLDisplayWayland::%s( %p, display %p, wl_display %p ) <- display.wl_display %p\n",
                 __FUNCTION__, this, &display, wl_display, display.wl_display );

     // Wayland Server Initialisation

     if (display.wl_display && display.wl_display != wl_display) {
          D_ERROR( "WL/EGL/BindWaylandDisplay: display.wl_display (%p) != wl_display (%p)\n",
                   display.wl_display, wl_display );
     }

     display.wl_display    = wl_display;
     //display.wl_server_dfb = wayland_dfb_init( wl_display, display.parent.dfb );

     return DFB_OK;
}

DFBResult
BindWaylandDisplay::eglUnbindWaylandDisplay( DirectFB::Wayland::EGLDisplayWayland &display,
                                             struct wl_display                    *wl_display )
{
     D_DEBUG_AT( DFBWayland_BindDisplay, "EGLDisplayWayland::%s( %p, display %p, wl_display %p ) <- display.wl_display %p\n",
                 __FUNCTION__, this, &display, wl_display, display.wl_display );

     if (display.wl_display && display.wl_display != wl_display)
          D_ERROR( "WL/EGL/BindWaylandDisplay: display.wl_display (%p) != wl_display (%p)\n",
                   display.wl_display, wl_display );

     display.wl_display = NULL;

     return DFB_OK;
}



}

