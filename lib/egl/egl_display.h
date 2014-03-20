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

#ifndef ___DFBEGL__display__H___
#define ___DFBEGL__display__H___

#include "dfbegl.h"


#ifdef __cplusplus
extern "C" {
#endif


// C wrappers


#ifdef __cplusplus
}

#include <direct/Map.h>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>


namespace DirectFB {


namespace EGL {


/*
 * EGL::Display for wrapping client's native display handle
 */

class Display : public Types::Type<Display>
{
     friend class Core;

public:
     typedef std::function<DFBResult (Display               &display,
                                      unsigned int          &ret_score)>   Probe;

     typedef std::function<DFBResult (Display               &display)>     Initialise;


protected:
     Display();
     virtual ~Display();

public:
     virtual DFBResult        Init();

     virtual void             GetVersion( EGLint &major, EGLint &minor );


     virtual EGLint           eglInitialise();
     virtual EGLint           Terminate();

     virtual EGLint           QueryString( EGLint          name,
                                           const char    *&ret_value );

     virtual EGLint           GetConfigs( Config **configs, EGLint config_size, EGLint *num_configs );

     virtual EGLint           ChooseConfig( const EGLint *attrib_list, Config **configs, EGLint config_size, EGLint *num_configs );


     virtual EGLint           CreateContext( EGLenum api, Config *config, Context *share, const EGLint *attrib_list, Context **ret_context );


     virtual DFBResult        CreateSurface( Config               *config,
                                             NativeHandle          native_handle,
                                             const EGLint         *attrib,
                                             Surface             **ret_surface );

     virtual EGLint           CreatePixmapSurface( Config               *config,
                                                   EGLNativePixmapType   pixmap,
                                                   const EGLint         *attrib,
                                                   Surface             **ret_surface );

     virtual EGLint           CreateWindowSurface( Config               *config,
                                                   EGLNativeWindowType   win,
                                                   const EGLint         *attrib,
                                                   Surface             **ret_surface );

     virtual EGLint           CreatePbufferSurface( Config               *config,
                                                    const EGLint         *attrib,
                                                    Surface             **ret_surface );

     virtual EGLint           CreatePbufferFromClientBuffer( EGLenum           buftype,
                                                             EGLClientBuffer   buffer,
                                                             Config           *config,
                                                             const EGLint     *attrib,
                                                             Surface         **ret_surface );

     virtual EGLint           SwapBuffers( Surface             *surface );

     virtual EGLint           CopyBuffers( Surface             *source,
                                           EGLNativePixmapType  destination );


     IDirectFB *GetDFB() const { return dfb; }





public:
     IDirectFB               *dfb;
     EGLNativeDisplayType     native_display;

     EGLenum                  native_pixmap_target;

private:
     unsigned int             refs;

     Direct::String           apis;
     Direct::String           extensions;
     Direct::String           vendor;
     Direct::String           version;

     std::shared_ptr<Graphics::Core>    gfx_core;


protected:

     // FIXME: Ref: use shared_ptr!
     void addRef() {
          refs++;
     }
};


class DisplayDFB : public Types::Type<DisplayDFB,EGL::Display>
{
public:
     DisplayDFB( EGL::Display &display,
                 Core         &core );
     virtual ~DisplayDFB();

     DFBResult Surface_Initialise( DirectFB::EGL::Surface    &surface );

     DFBResult Image_Initialise  ( DirectFB::EGL::KHR::Image &image );
};


}

}


#endif

#endif

