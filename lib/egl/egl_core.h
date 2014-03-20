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

#ifndef ___DFBEGL__core__H___
#define ___DFBEGL__core__H___

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


DECLARE_MODULE_DIRECTORY( core_modules );

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFBEGL_CORE_ABI_VERSION      1


class CoreModule
{
     friend class Core;

protected:
     virtual Direct::String GetName() const = 0;

     virtual DFBResult Initialise   ( Core                  &core ) = 0;
};


class Core : public EGL::Types::Type<Core>
{
     friend class Display;


public:
     DFBResult Display_Probe     ( Display      &display,
                                   unsigned int &ret_score );

     DFBResult Display_Initialise( DisplayDFB   &display );


private:
     Core();
     ~Core();

     DFBResult LoadModules();


public:
     static Core &GetInstance();

     // FIXME: Result: Add EGL errors to DirectResult (use DFBEGLResult for API calls)
     EGLint    GetDisplay( EGLNativeDisplayType   native_display,
                           Display              *&ret_display );

     DFBResult PutDisplay( Display               *display );


private:
     typedef std::map<DirectModuleEntry*,CoreModule*>            CoreModules;
     typedef std::map<EGLNativeDisplayType,Display*>             DisplayMap;


     CoreModules            modules;
     DisplayMap             displays;

public:
     typedef std::function<void *( const char * )>     GetProcAddress;
};



class EGLExtension : public Types::Type<EGLExtension>
{
public:
     typedef std::function<Direct::Strings (void)>     GetNames;
};



class EGLInt {
public:
     EGLint value;

     EGLInt( EGLint value = 0 )
          :
          value( value )
     {
     }

     typedef std::map<const EGLint,Direct::String> ToStringMap;     // move To/FromStringMap stuff to Direct::
     typedef std::map<const Direct::String,EGLint> FromStringMap;

     class Maps {
     public:
          Maps() {}

          ToStringMap     toString;
          FromStringMap   fromString;
     };

     operator Direct::String ();
     operator std::string ();

     Direct::String operator * () {
          return (Direct::String) *this;
     }

public:
     static Maps &GetMaps();
     static void Register( const EGLint         &egl_int,
                           const Direct::String &egl_str );
};





class NativeHandle {
public:
     typedef unsigned long Type;

     typedef enum {
          CLASS_NONE,
          CLASS_PIXMAP,
          CLASS_WINDOW
     } Class;

     Class clazz;

     union {
          unsigned long        value;
          void                *ptr;
          EGLNativePixmapType  pixmap;
          EGLNativeWindowType  window;
     };

     NativeHandle()
          :
          clazz( CLASS_NONE ),
          value( 0 )
     {
     }

     NativeHandle( Class clazz, unsigned long value = 0 )
          :
          clazz( clazz ),
          value( value )
     {
     }
};


}

}


#endif

#endif

