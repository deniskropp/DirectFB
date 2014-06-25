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

#ifndef ___DIRECTFB__SDL__EGLDISPLAYSDL__H___
#define ___DIRECTFB__SDL__EGLDISPLAYSDL__H___


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/KHR_image.h>


namespace DirectFB {

namespace SDL {


using namespace DirectFB::EGL;


class Types : public Direct::Types<Types,EGL::Types>
{
};


class EGLDisplaySDL;
class SurfaceSDLSurface;


class EGLCoreModuleSDL : public EGL::CoreModule, public Types::Type<EGLCoreModuleSDL,EGL::Core>
{
protected:
     virtual Direct::String GetName() const
     {
          return "SDL";
     }

     virtual DFBResult Initialise ( EGL::Core              &core );


     DFBResult Display_Probe      ( const EGL::Display     &display,
                                    unsigned int           &ret_score );

     DFBResult Display_Initialise ( EGLDisplaySDL          &display );


public:
     static void
     registry_handle_global(void *data, struct wl_registry *registry, uint32_t name,
                            const char *interface, uint32_t version);

     static void
     registry_handle_global_remove(void *data, struct wl_registry *registry,
                                   uint32_t name);
};


class EGLDisplaySDL : public Types::Type<EGLDisplaySDL,EGL::Display>
{
     friend class EGLCoreModuleSDL;
     friend class SurfaceSDLSurface;

public:
     EGLDisplaySDL( EGL::Display     &display,
                    EGLCoreModuleSDL &module );
     virtual ~EGLDisplaySDL();


     EGLDisplaySDL( EGLDisplaySDL &display ) = delete;



public:
     DFBResult Surface_Initialise( SurfaceSDLSurface &surface );
};


class SurfaceSDLSurface : public Types::Type<SurfaceSDLSurface,EGL::Surface>
{
     friend class EGLDisplaySDL;

public:
     SurfaceSDLSurface( EGL::Surface  &surface,
                        EGLDisplaySDL &display );

     virtual ~SurfaceSDLSurface();

     EGLint SwapBuffers();

private:
     EGLDisplaySDL &display;
     SDL_Surface   *sdl_surface;
};


}

}



#endif


