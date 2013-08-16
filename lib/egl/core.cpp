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


#include <idirectfb.h>

#include <direct/ToString.h>

#include <egl/dfbegl.h>
#include <egl/dfbegl_int_names.h>
#include <egl/image.h>


D_LOG_DOMAIN( DFBEGL_Core, "DFBEGL/Core", "DirectFB EGL Core" );



namespace DirectFB {

namespace EGL {

using namespace std::placeholders;


DEFINE_MODULE_DIRECTORY( core_modules, "dfbegl_core", DFBEGL_CORE_ABI_VERSION );


//D_TYPE_DEFINE_( GL::OES::eglImage, eglImage );
//D_TYPE_DEFINE_( GL::OES::glEGLImageTargetTexture2D, glEGLImageTargetTexture2D );
//D_TYPE_DEFINE_( GL::OES::glEGLImageTargetRenderBufferStorage, glEGLImageTargetRenderBufferStorage );

//D_TYPE_DEFINE( Context );
//D_TYPE_DEFINE( Display );

//D_TYPE_DEFINE_( KHR::Image, Image );


Core &
Core::GetInstance()
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s()\n", __FUNCTION__ );

     static Core core;

     D_DEBUG_AT( DFBEGL_Core, "  => %p\n", &core );

     return core;
}



DFBResult
Core::Display_Probe( DirectFB::EGL::Display &display,
                     unsigned int           &ret_score )
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     ret_score = 0;

     if (!display.native_display)
          ret_score = 1;

     if (display.native_display == idirectfb_singleton) {
          D_DEBUG_AT( DFBEGL_Core, "  -> is IDirectFB singleton = %p!\n", idirectfb_singleton );

          ret_score = 100;
     }

     return DFB_OK;
}

DFBResult
Core::Display_Initialise( DirectFB::EGL::Display &display )
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p, display %p, native_display 0x%08lx )\n",
                 __FUNCTION__, this, &display, (unsigned long) display.native_display );

     DFBResult  ret;
     IDirectFB *dfb;

     if (display.native_display) {
          D_DEBUG_AT( DFBEGL_Core, "  ===> Creating native DirectFB display (%p)\n", display.native_display );

          dfb = (IDirectFB*) display.native_display;

          ret = (DFBResult) dfb->AddRef( dfb );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Core: IDirectFB::AddRef() failed!\n" );
               return ret;
          }

          display.dfb = (IDirectFB*) display.native_display;
     }
     else
          D_DEBUG_AT( DFBEGL_Core, "  ===> Creating native DirectFB display (new)\n" );

     KHR::Image::Register< KHR::Image::Initialise >( (Direct::String) EGLInt(EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB),
                                                     std::bind( &Display::Image_Initialise, &display, _1 ) );

     Surface::Register< Surface::Initialise >( Display::GetTypeInstance().GetName(),
                                               std::bind( &Display::Surface_Initialise, &display, _1 ) );

     return DFB_OK;
}




Core::Core()
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p )\n", __FUNCTION__, this );

     Register< Display::Probe >     ( Display::GetTypeInstance().GetName(), std::bind( &Core::Display_Probe, this, _1, _2 ) );
     Register< Display::Initialise >( Display::GetTypeInstance().GetName(), std::bind( &Core::Display_Initialise, this, _1 ) );
}

Core::~Core()
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p )\n", __FUNCTION__, this );
}



DFBResult
Core::LoadModules()
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p )\n", __FUNCTION__, this );

     D_DEBUG_AT( DFBEGL_Core, "  -> loading modules...\n" );

     direct_modules_explore_directory( &core_modules );

     DirectModuleEntry *entry;

     direct_list_foreach( entry, core_modules.entries ) {
          D_DEBUG_AT( DFBEGL_Core, "  -> checking module %p '%s' (refs %d)\n", entry, entry->name, entry->refs );

          CoreModule *module;

          CoreModules::iterator it = modules.find( entry );

          if (it != modules.end()) {
               module = (*it).second;

               D_DEBUG_AT( DFBEGL_Core, "  ---> module already loaded (%p)\n", module );
               continue;
          }


          module = (CoreModule *) direct_module_ref( entry );
          if (!module) {
               D_DEBUG_AT( DFBEGL_Core, "  -> direct_module_ref() returned NULL, module disabled?\n" );
               continue;
          }

          D_DEBUG_AT( DFBEGL_Core, "  ===> %s\n", *module->GetName() );

          module->Initialise( *this );

          modules[entry] = module;
     }

     return DFB_OK;
}



EGLint
Core::GetDisplay( EGLNativeDisplayType   native_display,
                  Display              *&ret_display )
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p, native_display 0x%08lx )\n", __FUNCTION__, this, (unsigned long) native_display );

     DFBResult  ret;
     Display   *display = NULL;

     DisplayMap::iterator it = displays.find( native_display );

     if (it != displays.end()) {
          display = (*it).second;

          D_DEBUG_AT( DFBEGL_Core, "  -> using existing display (%u refs)\n", display->refs );

          display->addRef();

          ret_display = display;

          D_DEBUG_AT( DFBEGL_Core, "  => %p\n", display );

          return EGL_SUCCESS;
     }


     LoadModules();

     Direct::Base::GetInstance().__HandleLists();


     D_DEBUG_AT( DFBEGL_Core, "  -> creating new display...\n" );

     display = new Display();
     D_ASSERT( display != NULL );

     display->native_display = native_display;


     Direct::String scored_impl;     // change to actual Type
     unsigned int   scored_value = 0;

     D_DEBUG_AT( DFBEGL_Core, "  -> probing modules...\n" );

//     for (auto f : Map< Display::Probe >()) {
     std::map<std::string,Display::Probe> &map = Map< Display::Probe >();
     for (std::map<std::string,Display::Probe>::iterator f = map.begin(); f != map.end(); f++) {
          D_DEBUG_AT( DFBEGL_Core, "  -> probing %s...\n", (*f).first.c_str() );

          unsigned int score = 0;

          ret = (*f).second( *display, score );
          if (ret) {
               D_DEBUG_AT( DFBEGL_Core, "  => %s\n", *ToString<DFBResult>( ret ) );
          }
          else if (score > scored_value) {
               D_DEBUG_AT( DFBEGL_Core, "  => KEEPING AS BEST %u > %u\n", score, scored_value );

               scored_impl  = (*f).first;
               scored_value = score;

               continue;
          }
          else
               D_DEBUG_AT( DFBEGL_Core, "  => SKIPPING (%u <= %u)\n", score, scored_value );
     }

     if (scored_impl) {
          D_DEBUG_AT( DFBEGL_Core, "  ===> Creating %s display\n", *scored_impl );

          ret = Call<Display::Initialise>(scored_impl)( *display );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Core: DisplayModule(%s)::CreateDisplay( 0x%08lx ) failed!\n",
                         *scored_impl, (unsigned long) native_display );
               // FIXME: Core: unref module?
               delete display;
               return EGL_BAD_DISPLAY;
          }
     }


     ret = display->Init();
     if (ret) {
          delete display;
          return EGL_BAD_DISPLAY;
     }

     const char *apis       = "";
     const char *extensions = "";
     const char *vendor     = "";
     const char *version    = "";

     display->QueryString( EGL_CLIENT_APIS, apis );
     display->QueryString( EGL_EXTENSIONS, extensions );
     display->QueryString( EGL_VENDOR, vendor );
     display->QueryString( EGL_VERSION, version );

     D_INFO( "DFBEGL/Core: New EGLDisplay (%s %s) -=- %s -=- %s -=- %s\n",
             *display->GetName(), version, vendor, apis, extensions );

     displays[native_display] = display;

     ret_display = display;

     D_DEBUG_AT( DFBEGL_Core, "  => %p\n", display );

     return EGL_SUCCESS;
}

DFBResult
Core::PutDisplay( Display *display )
{
     D_DEBUG_AT( DFBEGL_Core, "Core::%s( %p, display %p ) <- native_display 0x%08lx\n",
                 __FUNCTION__, this, display, (unsigned long) display->native_display );

     DisplayMap::iterator it = displays.find( display->native_display );

     if (it != displays.end()) {
          displays.erase( it );

          return DFB_OK;
     }

     return DFB_NOSUCHINSTANCE;
}


}

}

