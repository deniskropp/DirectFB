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

#ifndef ___DirectFB__core__Graphics__H___
#define ___DirectFB__core__Graphics__H___


#include <direct/String.h>
#include <direct/Type.h>

#include <core/CoreSurface.h>

#include <list>
#include <map>
#include <vector>


namespace DirectFB {

namespace Graphics {



/*

 Graphics (Core)


 = Implementations
    - DRM/EGL   (DRM system module + Mesa egl library)
    - X11/EGL   (X11 system module + Mesa egl library)
    - X11/GLX   (X11 system module + GLX wrapper)
    - FBDev     (FBDev system module + Mesa egl library)
    - Android   (Android system module + Android egl library)

    - DirectVG  (Independent module)



 Config

   - key / value pairs (at least as strings)
   - also integers, complex objects (values)

     = Common set of options (should be supported if equivalent attribute exists)

          - WIDTH
          - HEIGHT

     = Extended options (e.g. from EGL or XWindows)

          - RENDERABLE_TYPE
          - NATIVE_VISUAL_ID

     = More complex options (above are just single integers)





 Surface : = SurfaceTypes
               TYPE   (MODULE)

               Pixmap (X11)
               Window (X11)
               SHM    (X11)
               SHM    (DFB)


         + Config


     SurfaceBuffer : = Config
     SurfaceBuffer : = Config

          SurfaceAllocation : = SurfaceType Config
          SurfaceAllocation : = SurfaceType Config


*/



class OptionBase
{
protected:
     OptionBase() {};
     virtual ~OptionBase() {};

public:
     virtual Direct::String GetName() const = 0;
     virtual Direct::String GetString() const = 0;
};


template <typename _T>
class Option : public OptionBase
{
public:
     virtual const _T &GetValue() const = 0;
};


class Options : public std::map<std::string,OptionBase*>
{
public:
     Options() {}

     void Add( OptionBase *option )
     {
          insert( std::pair<std::string,OptionBase*>( option->GetName(), option ) );
     }

     OptionBase *Get( const Direct::String &name )
     {
          std::map<std::string,OptionBase*>::iterator it = find( name );

          if (it != end())
               return (*it).second;

          return NULL;
     }

     template <class _Option>
     _Option *
     Get( const Direct::String &name ) const
     {
          std::map<std::string,OptionBase*>::const_iterator it = find( name );

          if (it != end()) {
               OptionBase *base = (*it).second;

               _Option *option = dynamic_cast<_Option*>( base );

               return option;
          }

          return NULL;
     }

     template <class _T>
     _T
     GetValue( const Direct::String &name,
               const _T             &default_value ) const
     {
          std::map<std::string,OptionBase*>::const_iterator it = find( name );

          if (it != end()) {
               OptionBase *base = (*it).second;

               Graphics::Option<_T> *option = dynamic_cast<Graphics::Option<_T> *>( base );

               return option->GetValue();
          }

          return default_value;
     }

     template <class _T>
     bool
     Get( const Direct::String &name,
          _T                   &ret_value ) const
     {
          std::map<std::string,OptionBase*>::const_iterator it = find( name );

          if (it != end()) {
               OptionBase *base = (*it).second;

               Graphics::Option<_T> *option = dynamic_cast<Graphics::Option<_T> *>( base );

               ret_value = option->GetValue();

               return true;
          }

          return false;
     }
};


class Config;
class Context;
class Implementation;
class SurfacePeer;


typedef std::vector<Config*> Configs;

typedef std::list<Implementation*> Implementations;


class Core : public Direct::Types<Core>
{
public:
     static Implementations implementations;

     static void RegisterImplementation( Implementation *implementation );
     static void UnregisterImplementation( Implementation *implementation );
};


//class Buffer : public Core::Type<Buffer>
//{
//};





class Implementation
{
protected:
     Implementation();
     virtual ~Implementation() {};

public:
     const Direct::String  &GetName() const { return name; }
     const Direct::Strings &GetAPIs() const { return apis; }
     const Direct::Strings &GetExtensions() const { return extensions; }

     virtual Configs   &GetConfigs() = 0;

protected:
     Direct::String  name;
     Direct::Strings apis;
     Direct::Strings extensions;
};


class Config
{
protected:
     Config( Implementation *implementation ) : implementation( implementation ) {};
     virtual ~Config() {};

public:
     Implementation *GetImplementation() const { return implementation; }

     virtual DFBResult GetOption( const Direct::String &name,
                                  long                 &value );// FIXME: replace by template

     virtual DFBResult CheckOptions( const Graphics::Options &options );

     virtual DFBResult CreateContext( const Direct::String  &api,
                                      Context               *share,
                                      Options               *options,
                                      Context              **ret_context );

     virtual DFBResult CreateSurfacePeer( IDirectFBSurface  *surface,
                                          Options           *options,
                                          SurfacePeer      **ret_peer );

private:
     Implementation *implementation;
};


class Context : public Core::Type<Context>
{
protected:
     Context( const Direct::String &api,
              Config               *config,
              Context              *share,
              Options              *options );

public:
     virtual ~Context() {};

     const Direct::String &GetAPI() const { return api; }

     virtual DFBResult Init  () = 0;
     virtual DFBResult Bind  ( Graphics::SurfacePeer *draw,
                               Graphics::SurfacePeer *read ) = 0;
     virtual DFBResult Unbind() = 0;

     virtual DFBResult GetOption( const Direct::String &name,
                                  long                 &value );// FIXME: replace by template

     virtual DFBResult GetProcAddress( const Direct::String  &name,
                                       void                 *&addr );

protected:
     Direct::String  api;
     Config         *config;
     Context        *share;
     Options        *options;

     template <class _Config>
     _Config *
     GetConfig() const
     {
          return dynamic_cast<_Config*>( config );
     }
};



class SurfacePeer
{
     friend class Context;

public:
     SurfacePeer( Graphics::Config *config,
                  Options          *options,
                  IDirectFBSurface *surface = NULL );

     virtual ~SurfacePeer();

     virtual DFBResult    Init();
     virtual DFBResult    Flip( const DFBRegion     *region,
                                DFBSurfaceFlipFlags  flags );

     virtual DFBResult    GetOption( const Direct::String &name,
                                     long                 &value );// FIXME: replace by template

     IDirectFBSurface    *GetSurface() const { return surface; }

protected:
     Config           *config;
     Options          *options;
     IDirectFBSurface *surface;

     template <class _Config>
     _Config *
     GetConfig() const
     {
          return dynamic_cast<_Config*>( config );
     }
};



}

}


#endif


