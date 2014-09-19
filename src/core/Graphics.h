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
#include <core/SurfaceTask.h>

#include <list>
#include <map>
#include <vector>

#include <initializer_list>

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

     = Global IDs for configs from different vendors

          - CONFIG_ID will be generated from Core

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


class SurfaceAllocationKey
{
public:
     DFBSurfaceID             surfaceID;
     CoreSurfaceBufferRole    role;
     DFBSurfaceStereoEye      eye;
     u32                      flips;

     SurfaceAllocationKey( DFBSurfaceID             surfaceID,
                           CoreSurfaceBufferRole    role,
                           DFBSurfaceStereoEye      eye,
                           u32                      flips )
          :
          surfaceID( surfaceID ),
          role( role ),
          eye( eye ),
          flips( flips )
     {
     }

     bool operator < (const SurfaceAllocationKey &other) const {
          if (surfaceID == other.surfaceID) {
               if (role == other.role) {
                    if (eye == other.eye)
                         return flips < other.flips;

                    return eye < other.eye;
               }

               return role < other.role;
          }

          return surfaceID < other.surfaceID;
     }
};

typedef std::map<SurfaceAllocationKey,CoreSurfaceAllocation*>  SurfaceAllocationMap;
typedef std::pair<SurfaceAllocationKey,CoreSurfaceAllocation*> SurfaceAllocationMapPair;


class OptionBase
{
protected:
     OptionBase() {}
public:
     virtual ~OptionBase() {}

public:
     virtual Direct::String GetName() const = 0;
     virtual Direct::String GetString() const = 0;
};


template <typename _T>
class Option : public OptionBase
{
public:
     virtual const _T &GetValue() const = 0;
     virtual void      SetValue( const _T & ) = 0;
};


class SimpleOption : public Graphics::Option<Direct::String>
{
public:
     SimpleOption( const Direct::String &name,
                   const Direct::String &value )
          :
          name( name ),
          value( value )
     {
     }

     virtual ~SimpleOption() {}

     virtual Direct::String GetName() const { return name; }
     virtual Direct::String GetString() const { return value; }

     virtual const Direct::String &GetValue() const { return value; }
     virtual void                  SetValue( const Direct::String &value ) { this->value = value; }

private:
     Direct::String name;
     Direct::String value;
};


class Options : public std::map<std::string,OptionBase*>
{
public:
     Options() {}

     ~Options()
     {
          for (auto it=begin(); it!=end(); it++)
               delete (*it).second;
     }

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



class Core : public Direct::Types<Core>, public Direct::Singleton<Core>
{
public:
     Core();
     virtual ~Core();

     Direct::Modules modules;

     Implementations implementations;

     void RegisterImplementation( Implementation *implementation );
     void UnregisterImplementation( Implementation *implementation );

     void LoadModules()
     {
          modules.Load();
     }
};


class Implementation : public Direct::Module
{
protected:
     Implementation( std::shared_ptr<Core> core = Graphics::Core::GetInstance() );
     virtual ~Implementation();

public:
     virtual const Direct::String &GetName() const = 0;

     const Direct::String  &GetName() { /*Init();*/ return name; }
     const Direct::Strings &GetAPIs() { /*Init();*/ return apis; }
     const Direct::Strings &GetExtensions() { /*Init();*/ return extensions; }

     const Configs         &GetConfigs() { /*Init();*/ return configs; }


protected:
     std::shared_ptr<Core>    core;
     Direct::String           name;
     Direct::Strings          apis;
     Direct::Strings          extensions;
     Graphics::Configs        configs;
};


class Config : public Core::Type<Config>
{
protected:
     Config( Implementation *implementation );

public:
     virtual ~Config() {};

public:
     Implementation *GetImplementation() const { return implementation; }

     virtual DFBResult GetOption( const Direct::String &name,
                                  long                 &value );// FIXME: replace by template

     virtual Direct::String GetOption( const Direct::String &name );

     virtual DFBResult CheckOptions( const Graphics::Options &options );

     virtual DFBResult CreateContext( const Direct::String  &api,
                                      Context               *share,
                                      Options               *options,
                                      Context              **ret_context );

     /*
        Create interface for EGLSurface like producer side
     */
     virtual DFBResult CreateSurfacePeer( CoreSurface       *surface,
                                          Options           *options,
                                          SurfacePeer      **ret_peer );

     virtual void DumpValues( std::initializer_list<Direct::String>  names,
                              Direct::String                        &out_str );

private:
     Implementation *implementation;
};



class RenderTask : public Task
{
public:
     RenderTask() {}

     virtual ~RenderTask() {}

     virtual DFBResult Bind  ( CoreSurfaceBuffer *draw,
                               CoreSurfaceBuffer *read ) { return DFB_UNIMPLEMENTED; }
     virtual DFBResult Unbind() { return DFB_OK; }
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

     virtual DFBResult GetOption( const Direct::String &name,
                                  long                 &value );// FIXME: replace by template

     virtual DFBResult GetProcAddress( const Direct::String  &name,
                                       void                 *&addr );

     // sync api
     virtual DFBResult Bind  ( Graphics::SurfacePeer *draw,
                               Graphics::SurfacePeer *read ) = 0;
     virtual DFBResult Unbind() = 0;

     // async api
     virtual DFBResult CreateTask( Graphics::RenderTask *&task );


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

/*
   Producer side for CoreSurface (later Stream)

   Sends out events upon Flip (SwapBuffers)
*/
class SurfacePeer : public Core::Type<SurfacePeer>
{
     friend class Context;

public:
     SurfacePeer( Graphics::Config *config,
                  Options          *options,
                  CoreSurface      *surface );

     virtual ~SurfacePeer();

     virtual DFBResult Init();

     // FIXME: replace by template
     virtual DFBResult GetOption ( const Direct::String &name,
                                   long                 &value );

     // FIXME: Add new region/update class including stereo support
     virtual DFBResult Flip      ( const DFBRegion     *region    = NULL,
                                   DFBSurfaceFlipFlags  flags     = DSFLIP_NONE,
                                   long long            timestamp = 0 );

     CoreSurface                *GetSurface() const { return surface; }
     const CoreSurfaceConfig    &GetSurfaceConfig() const { return surface_config; }
     CoreSurfaceTypeFlags        GetSurfaceType() const { return surface_type; }

protected:
     typedef std::function<DFBResult(void)>  Flush;

     virtual DFBResult                                               updateBuffers();
     virtual DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> &getBuffer( int offset = 0 );

public:
     Config                                                *config;
     Options                                               *options;
     CoreSurface                                           *surface;

private:
     u32                                                    flips;
     u32                                                    index;

     CoreSurfaceTypeFlags                                   surface_type;
     DirectSerial                                           surface_serial;

     CoreSurfaceConfig                                      surface_config;

     u32                                                    buffer_num;
     DFBSurfaceBufferID                                     buffer_ids[MAX_SURFACE_BUFFERS*2];
     DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> buffer_objects[MAX_SURFACE_BUFFERS*2];

protected:
     u32                         index_left()  const { return index+0; }
     u32                         index_right() const { return index+1; }

     DFBSurfaceBufferID          buffer_left()  const { return buffer_ids[index_left()]; }
     DFBSurfaceBufferID          buffer_right() const { return buffer_ids[index_right()]; }

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


