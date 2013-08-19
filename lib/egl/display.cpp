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

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>
#include <direct/Utils.h>

#include <egl/dfbegl.h>


D_LOG_DOMAIN( DFBEGL_Display, "DFBEGL/Display", "DirectFB EGL Display" );


namespace DirectFB {

namespace EGL {

using namespace std::placeholders;


Display::Display()
     :
     dfb( NULL ),
     native_display( NULL ),
     native_pixmap_target( 0 ),
     refs( 1 ),
     gfx_core( Graphics::Core::GetInstance() )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, dfb %p, native_display %p )\n",
                 __FUNCTION__, this, dfb, (void*) (long) native_display );

     gfx_core->LoadModules();
}

Display::~Display()
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n",
                 __FUNCTION__, this );

     if (dfb)
          dfb->Release( dfb );
}

DFBResult
Display::Init()
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n", __FUNCTION__, this );

     DFBResult ret;

     if (!dfb) {
          ret = DirectFBInit( NULL, NULL );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Display: DirectFBInit() failed!\n" );
               return ret;
          }

          ret = DirectFBCreate( &dfb );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Display: DirectFBCreate() failed!\n" );
               return ret;
          }
     }


     const char *comma = "";

     for (Graphics::Implementations::const_iterator it = gfx_core->implementations.begin();
           it != gfx_core->implementations.end();
           it++)
     {
          apis.PrintF( "%s%s (%s)", comma, *(*it)->GetAPIs().Concatenated( " " ), *(*it)->GetName()  );

          comma = "   ";
     }


     extensions = "";

     comma = "";

//     for (auto f : Map< EGLExtension::GetNames >()) {
     std::map<std::string,EGLExtension::GetNames> &map = Map< EGLExtension::GetNames >();
     for (std::map<std::string,EGLExtension::GetNames>::iterator f = map.begin(); f != map.end(); f++) {
          extensions.PrintF( "%s%s", comma, *(*f).second().Concatenated( " " ) );

          comma = " ";
     }

     extensions += " (Display)";

     comma = "   ";

     // FIXME: filter extensions / provide/bridge them
     for (Graphics::Implementations::const_iterator it = gfx_core->implementations.begin();
           it != gfx_core->implementations.end();
           it++)
     {
          extensions.PrintF( "%s%s (%s)", comma, *(*it)->GetExtensions().Concatenated( " " ), *(*it)->GetName()  );

          comma = "   ";
     }


     vendor = "DirectFB EGL United [";

     comma = "";

     for (Graphics::Implementations::const_iterator it = gfx_core->implementations.begin();
           it != gfx_core->implementations.end();
           it++)
     {
          vendor.PrintF( "%s%s", comma, *(*it)->GetName() );

          comma = "][";
     }

     vendor += "]";


     EGLint major, minor;

     GetVersion( major, minor );

     version.PrintF( "%d.%d", major, minor );


     return DFB_OK;
}

//Direct::String
//Display::GetName()
//{
//     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n", __FUNCTION__, this );
//
//     return name;
//}

void
Display::GetVersion( EGLint &major,
                     EGLint &minor )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n", __FUNCTION__, this );

     major = 1;
     minor = 4;
}

EGLint
Display::eglInitialise()
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n", __FUNCTION__, this );

     return EGL_SUCCESS;
}

EGLint
Display::Terminate()
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p ) <- refs %u\n", __FUNCTION__, this, refs );

     D_ASSERT( refs > 0 );

     // FIXME: move to api.cpp?
     if (!--refs) {
          Core::GetInstance().PutDisplay( this );

          delete this;
     }

     return EGL_SUCCESS;
}

EGLint
Display::QueryString( EGLint       name,
                      const char *&ret_value )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, name 0x%04x )\n", __FUNCTION__, this, name );

     D_DEBUG_AT( DFBEGL_Display, "  -> name: '%s'\n", *ToString<EGLInt>( name ) );

     switch (name) {
          case EGL_CLIENT_APIS:
               ret_value = *apis;
               break;

          case EGL_EXTENSIONS:
               ret_value = *extensions;
               break;

          case EGL_VENDOR:
               ret_value = *vendor;
               break;

          case EGL_VERSION:
               ret_value = *version;
               break;

          default:
               D_DEBUG_AT( DFBEGL_Display, "  -> UNKNOWN!\n" );
               return EGL_BAD_PARAMETER;
     }

     D_DEBUG_AT( DFBEGL_Display, "  => '%s'\n", ret_value );

     return EGL_SUCCESS;
}

EGLint
Display::GetConfigs( Config **configs, EGLint config_size, EGLint *num_configs )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p )\n", __FUNCTION__, this );

     return ChooseConfig( NULL, configs, config_size, num_configs );
}

EGLint
Display::ChooseConfig( const EGLint *attrib_list, Config **confs, EGLint config_size, EGLint *num_configs )
{
     DFBResult            ret;
     std::vector<Config*> configs;
     Graphics::Options    options;

     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, attrib_list %p, confs %p, config_size %d )\n", __FUNCTION__, this, attrib_list, confs, config_size );

     DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib_list );

     if (attrib_list) {
          ret = Util::GetOptions( options, attrib_list );
          if (ret)
               D_DERROR( ret, "DFBEGL/Display: Failed to get Options from attrib_list!\n" );
     }

     for (Graphics::Implementations::const_iterator it = gfx_core->implementations.begin();
           it != gfx_core->implementations.end();
           it++)
     {
          D_DEBUG_AT( DFBEGL_Display, "  -> Implementation '%s'\n", *(*it)->GetName() );

          const Graphics::Configs &gfx_confs = (*it)->GetConfigs();

          D_DEBUG_AT( DFBEGL_Display, "  -> %zu configs\n", gfx_confs.size() );

          for (Graphics::Configs::const_iterator it2 = gfx_confs.begin();
                it2 != gfx_confs.end();
                it2++)
          {
               Graphics::Config *gfx_config = *it2;

               D_DEBUG_AT( DFBEGL_Display, "  == config %p\n", gfx_config );

               if (attrib_list) {
                    ret = gfx_config->CheckOptions( options );
                    if (ret) {
                         D_DEBUG_AT( DFBEGL_Display, " ==> ERROR <======== (%s)\n", DirectFBErrorString(ret) );
                         continue;
                    }

                    D_DEBUG_AT( DFBEGL_Display, " ==> OK <========\n" );
               }

               configs.push_back( new Config( this, gfx_config ) );
          }
     }

     if (configs.empty())
          return EGL_BAD_MATCH;

     if (num_configs) {
          if (config_size > 0)
               *num_configs = ((EGLint)configs.size() < config_size) ? (EGLint) configs.size() : config_size;
          else
               *num_configs = (EGLint)configs.size();
     }

     if (confs != NULL && config_size > 0) {
          for (EGLint i=0; i<(EGLint) configs.size() && i < config_size; i++)
               confs[i] = configs[i];
     }

     return EGL_SUCCESS;
}

EGLint
Display::CreateContext( EGLenum api, Config *config, Context *share, const EGLint *attrib_list, Context **ret_context )
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, api 0x%04x '%s', config %p, share %p, attrib_list %p )\n",
                 __FUNCTION__, this, api, *ToString<EGLInt>( EGLInt(api) ), config, share, attrib_list );

     DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib_list );

     EGL::Context *context = new Context( api, config, share, attrib_list );

     ret = context->Init();
     if (ret) {
          delete context;
          return EGL_BAD_DISPLAY;
     }

     *ret_context = context;

     return EGL_SUCCESS;
}

DFBResult
Display::CreateSurface( Config        *config,
                        NativeHandle   native_handle,
                        const EGLint  *attrib,
                        Surface      **ret_surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, config %p, handle 0x%08lx (class %d), attrib %p )\n",
                 __FUNCTION__, this, config, native_handle.value, native_handle.clazz, attrib );

     DFBResult ret;

     EGL::Surface *surface = new EGL::Surface();

     surface->native_handle = native_handle;

     surface->config        = config;
     surface->gfx_config    = config->gfx_config;

     if (attrib)
          Util::GetOptions( surface->gfx_options, attrib );

     auto init = Surface::Call< Surface::Initialise >( GetName() );
     if (!init) {
          D_ERROR( "DFBEGL/Display: No Surface::Call< Surface::Initialise >( %s )!\n", *GetName() );
          return DFB_NOIMPL;
     }

     ret = init( *surface );
     if (ret) {
          D_DERROR( ret, "DFBEGL/Display: Surface::Call< Surface::Initialise > failed!\n" );
          delete surface;
          return ret;
     }

     *ret_surface = surface;

     return DFB_OK;
}

EGLint
Display::CreatePixmapSurface( Config               *config,
                              EGLNativePixmapType   pixmap,
                              const EGLint         *attrib,
                              Surface             **ret_surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, config %p, handle 0x%08lx, attrib %p )\n", __FUNCTION__, this, config, (long) pixmap, attrib );

     if (attrib)
          DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib );


     DFBResult     ret;
     EGL::Surface *surface;

     ret = CreateSurface( config, NativeHandle( NativeHandle::CLASS_PIXMAP, (NativeHandle::Type) pixmap ),
                          attrib, &surface );
     if (ret)
          return EGL_BAD_DISPLAY;

     ret = surface->Init();
     if (ret) {
          delete surface;
          return EGL_BAD_SURFACE;
     }

     *ret_surface = surface;

     return EGL_SUCCESS;
}

EGLint
Display::CreateWindowSurface( Config               *config,
                              EGLNativeWindowType   win,
                              const EGLint         *attrib,
                              Surface             **ret_surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, config %p, handle 0x%08lx, attrib %p )\n", __FUNCTION__, this, config, (long) win, attrib );

     if (attrib)
          DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib );


     DFBResult     ret;
     EGL::Surface *surface;

     ret = CreateSurface( config, NativeHandle( NativeHandle::CLASS_WINDOW, (NativeHandle::Type) win ), attrib, &surface );
     if (ret)
          return EGL_BAD_DISPLAY;

     ret = surface->Init();
     if (ret) {
          delete surface;
          return EGL_BAD_SURFACE;
     }

     *ret_surface = surface;

     return EGL_SUCCESS;
}

EGLint
Display::CreatePbufferSurface( Config        *config,
                               const EGLint  *attrib,
                               Surface      **ret_surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, config %p, attrib %p )\n", __FUNCTION__, this, config, attrib );

     if (attrib)
          DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib );


     DFBResult     ret;
     EGL::Surface *surface;

     ret = CreateSurface( config, NativeHandle( NativeHandle::CLASS_NONE ), attrib, &surface );
     if (ret)
          return EGL_BAD_DISPLAY;

     ret = surface->Init();
     if (ret) {
          delete surface;
          return EGL_BAD_SURFACE;
     }

     *ret_surface = surface;

     return EGL_SUCCESS;
}

EGLint
Display::CreatePbufferFromClientBuffer( EGLenum           buftype,
                                        EGLClientBuffer   buffer,
                                        Config           *config,
                                        const EGLint     *attrib,
                                        Surface         **ret_surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, config %p, buftype 0x%04x, buffer 0x%08lx, attrib %p )\n", __FUNCTION__, this, config, buftype, (long) buffer, attrib );

     if (attrib)
          DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Display, attrib );


     DFBResult     ret;
     EGL::Surface *surface;

     switch (buftype) {
          case EGL_NATIVE_PIXMAP_KHR:
               ret = CreateSurface( config, NativeHandle( NativeHandle::CLASS_PIXMAP, (NativeHandle::Type) buffer ), attrib, &surface );
               if (ret)
                    return EGL_BAD_DISPLAY;
               break;

          case EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB:
          //     surface = new EGL::Surface( config, attrib, (IDirectFBSurface*) buffer );
               break;

          default:
               return EGL_BAD_PARAMETER;
     }

     ret = surface->Init();
     if (ret) {
          delete surface;
          return EGL_BAD_SURFACE;
     }

     *ret_surface = surface;

     return EGL_SUCCESS;
}

EGLint
Display::SwapBuffers( Surface *surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     return surface->SwapBuffers();
}

EGLint
Display::CopyBuffers( Surface             *source,
                      EGLNativePixmapType  destination )
{
     D_DEBUG_AT( DFBEGL_Display, "EGL::Display::%s( %p, source %p, destination %p )\n",
                 __FUNCTION__, this, source, destination );

     EGL::Config  *config = source->GetConfig();
     EGL::Surface *dest;

     EGLint error = CreatePixmapSurface( config, destination,
                                         NULL,     // TODO: Copy: take attribs from source?
                                         &dest );
     if (error != EGL_SUCCESS)
          return error;

     DFBResult ret = dest->Copy( source );

     if (ret)
          error = EGL_BAD_SURFACE;

     delete dest;

     return error;
}


}

}

