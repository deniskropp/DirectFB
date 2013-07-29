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

#ifndef ___DirectFB__X11__EGLDISPLAYX11__H___
#define ___DirectFB__X11__EGLDISPLAYX11__H___


#ifdef __cplusplus
extern "C" {
#endif

#include "x11.h"

DFBResult dfb_x11_egl_display_register( DFBX11 *x11 );
void      dfb_x11_egl_display_unregister( void );

#ifdef __cplusplus
}


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/image.h>


namespace DirectFB {

namespace X11 {


class Types : public Direct::Types<Types,EGL::Types>
{
};


class EGLDisplayX11;
class SurfaceXWindow;


namespace EGL {

class Image : public Types::Type<Image,DirectFB::EGL::KHR::Image> {
public:
     Pixmap                      pixmap;
     IDirectFBSurfaceAllocation *allocation;

     Image( DirectFB::EGL::KHR::Image &egl_image,
            EGLDisplayX11             &impl );
};

}


class EGLCoreModuleX11 : public DirectFB::EGL::CoreModule, public Types::Type<EGLCoreModuleX11,DirectFB::EGL::Core>
{
protected:
     virtual Direct::String GetName() const
     {
          return "X11";
     }

     virtual DFBResult Initialise   ( DirectFB::EGL::Core             &core );


     DFBResult Display_Probe     ( const DirectFB::EGL::Display    &display,
                                   unsigned int          &ret_score );

     DFBResult Display_Initialise( EGLDisplayX11         &display );
};


class EGLDisplayX11 : public Types::Type<EGLDisplayX11,DirectFB::EGL::Display>
{
     friend class EGLCoreModuleX11;
     friend class EGL::Image;

public:
     EGLDisplayX11( DirectFB::EGL::Display     &display,
                    EGLCoreModuleX11 &module );
     virtual ~EGLDisplayX11();

public:
     DFBResult Surface_Initialise( SurfaceXWindow        &surface );

     DFBResult Image_Initialise( DirectFB::EGL::KHR::Image &image );


private:
     ::Display *x11_display;
};


class SurfaceXWindow : public Types::Type<SurfaceXWindow,DirectFB::EGL::Surface>
{
     friend class EGLDisplayX11;

public:
     SurfaceXWindow( DirectFB::EGL::Surface  &surface,
                     EGLDisplayX11 &display );

     virtual ~SurfaceXWindow();

private:
     ::Window                    window;
     IDirectFBSurfaceAllocation *allocation;
};


}

}


#endif // __cplusplus


#endif


