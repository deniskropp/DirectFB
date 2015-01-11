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

/************************************************************************** 
 * Config compare and sort from Mesa 
 **************************************************************************
 *
 * Copyright 2008 VMware, Inc.
 * Copyright 2009-2010 Chia-I Wu <olvaffe@gmail.com>
 * Copyright 2010-2011 LunarG, Inc.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sub license, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial portions
 * of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 **************************************************************************/

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
#include <egl/KHR_image.h>


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


     gfx_core->LoadModules();


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

     auto map = Map< EGLExtension::GetNames >( "GetNames" );

     for (auto f = map.begin(); f != map.end(); f++) {
          if (!(*f).second().empty()) {
               extensions.PrintF( "%s%s", comma, *(*f).second().Concatenated( " " ) );

               comma = " ";
          }
     }

//     extensions += " (Display)";

//     comma = "   ";

     // FIXME: filter extensions / provide/bridge them
     for (Graphics::Implementations::const_iterator it = gfx_core->implementations.begin();
          it != gfx_core->implementations.end();
          it++)
     {
//          extensions.PrintF( "%s%s (%s)", comma, *(*it)->GetExtensions().Concatenated( " " ), *(*it)->GetName()  );
          if (!(*it)->GetExtensions().empty()) {
               extensions.PrintF( "%s%s", comma, *(*it)->GetExtensions().Concatenated( " " )  );

               comma = " ";
          }
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
          try {
               sortConfigs( configs.data(), configs.size(), fallbackCompareConfigs, &options );
          }
          catch (std::runtime_error &e) {
               return EGLTLS.Get()->GetError();   // FIXME: avoid double SetError by using TLS more intelligently
          }

          for (EGLint i=0; i<(EGLint) configs.size() && i < config_size; i++) {
               confs[i] = configs[i];

               Direct::String v;

               confs[i]->gfx_config->DumpValues( {
                    "BUFFER_SIZE",
                    "ALPHA_SIZE",
                    "RED_SIZE",
                    "GREEN_SIZE",
                    "BLUE_SIZE",
                    "DEPTH_SIZE",
                    "STENCIL_SIZE",
                    "NATIVE_VISUAL_ID",
                    "NATIVE_VISUAL_TYPE",
                    "NATIVE_RENDERABLE",
                    "RENDERABLE_TYPE",
                    "SURFACE_TYPE",
                    "COLOR_BUFFER_TYPE",
                    "CONFIG_ID",
                    }, v );

               D_INFO( "EGLDisplay: Choose EGLConfig %p %s\n", confs[i], *v );
          }
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

     Direct::String name = (native_handle.clazz == NativeHandle::CLASS_NONE) ? DisplayDFB::GetTypeInstance().GetName() : GetName();    // FIXME

     auto init = Surface::Call< Surface::Initialise >( "Initialise", "", this );
     if (!init) {
          D_ERROR( "DFBEGL/Display: No Surface::Call< Surface::Initialise >( %s )!\n", *name );
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
               D_UNIMPLEMENTED();
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

//     EGL::Config  *config = source->GetConfig();
//     EGL::Surface *dest;
//
//     EGLint error = CreatePixmapSurface( config, destination,
//                                         NULL,     // TODO: Copy: take attribs from source?
//                                         &dest );
//     if (error != EGL_SUCCESS)
//          return error;
//
//     DFBResult ret = dest->Copy( source );
//
//     if (ret)
//          error = EGL_BAD_SURFACE;
//
//     delete dest;
//
//     return error;

     IDirectFBSurface *src = source->GetSurface();
     IDirectFBSurface *dst = (IDirectFBSurface *) destination;

     dst->SetBlittingFlags( dst, DSBLIT_NOFX );
     dst->Blit( dst, src, NULL, 0, 0 );

     return EGL_SUCCESS;
}

void
Display::sortConfigs( Config **configs, EGLint count,
                      EGLint (*compare)(const Config &, const Config &, const Graphics::Options *),
                      const Graphics::Options *criteria )
{
     const EGLint pivot = 0;
     EGLint i, j;

     if (count <= 1)
          return;

     std::swap( configs[pivot], configs[count / 2] );
     i = 1;
     j = count - 1;
     do {
          while (i < count && compare( *configs[i], *configs[pivot], criteria ) < 0)
               i++;
          while (compare( *configs[j], *configs[pivot], criteria ) > 0)
               j--;
          if (i < j) {
               std::swap( configs[i], configs[j] );
               i++;
               j--;
          }
          else if (i == j) {
               i++;
               j--;
               break;
          }
     } while (i <= j);
     std::swap( configs[pivot], configs[j] );

     sortConfigs( configs, j, compare, criteria );
     sortConfigs( configs + i, count - i, compare, criteria );
}

int
Display::fallbackCompareConfigs( const Config &conf1, const Config &conf2, const Graphics::Options *criteria )
{
     return compareConfigs( conf1, conf2, criteria, true );
}

int
Display::compareConfigs( const Config &conf1, const Config &conf2, const Graphics::Options *criteria, bool compare_id )
{
     const EGLint compare_attribs[] = {
          EGL_BUFFER_SIZE,
          EGL_SAMPLE_BUFFERS,
          EGL_SAMPLES,
          EGL_DEPTH_SIZE,
          EGL_STENCIL_SIZE,
          EGL_ALPHA_MASK_SIZE,
     };
     EGLint val1, val2;
     EGLint i;

     if (&conf1 == &conf2)
          return 0;

     EGL::TLS *tls = EGLTLS.Get();


#define GET_ATTRIB( conf, attrib )                \
     ({                                           \
          EGLint v;                               \
          EGLint err;                             \
          err = conf.GetAttrib( attrib, &v );     \
          if (err != EGL_SUCCESS) {               \
               tls->SetError( err );              \
               throw std::runtime_error("");      \
          }                                       \
          v;                                      \
     })

     /* the enum values have the desired ordering */
     D_ASSERT( EGL_NONE < EGL_SLOW_CONFIG );
     D_ASSERT( EGL_SLOW_CONFIG < EGL_NON_CONFORMANT_CONFIG );

     val1 = GET_ATTRIB( conf1, EGL_CONFIG_CAVEAT ) - GET_ATTRIB( conf2, EGL_CONFIG_CAVEAT );
     if (val1)
          return val1;

     /* the enum values have the desired ordering */
     D_ASSERT( EGL_RGB_BUFFER < EGL_LUMINANCE_BUFFER );
     val1 = GET_ATTRIB( conf1, EGL_COLOR_BUFFER_TYPE ) - GET_ATTRIB( conf2, EGL_COLOR_BUFFER_TYPE );
     if (val1)
          return val1;

     if (criteria) {
          val1 = val2 = 0;
          if (GET_ATTRIB( conf1, EGL_COLOR_BUFFER_TYPE ) == EGL_RGB_BUFFER) {
               if (criteria->GetValue<long>( "RED_SIZE", 0 ) > 0) {
                    val1 += GET_ATTRIB( conf1, EGL_RED_SIZE );
                    val2 += GET_ATTRIB( conf2, EGL_RED_SIZE );
               }
               if (criteria->GetValue<long>( "GREEN_SIZE", 0 ) > 0) {
                    val1 += GET_ATTRIB( conf1, EGL_GREEN_SIZE );
                    val2 += GET_ATTRIB( conf2, EGL_GREEN_SIZE );
               }
               if (criteria->GetValue<long>( "BLUE_SIZE", 0 ) > 0) {
                    val1 += GET_ATTRIB( conf1, EGL_BLUE_SIZE );
                    val2 += GET_ATTRIB( conf2, EGL_BLUE_SIZE );
               }
          }
          else {
               if (criteria->GetValue<long>( "LUMINANCE_SIZE", 0 ) > 0) {
                    val1 += GET_ATTRIB( conf1, EGL_LUMINANCE_SIZE );
                    val2 += GET_ATTRIB( conf2, EGL_LUMINANCE_SIZE );
               }
          }
          if (criteria->GetValue<long>( "ALPHA_SIZE", 0 ) > 0) {
               val1 += GET_ATTRIB( conf1, EGL_ALPHA_SIZE );
               val2 += GET_ATTRIB( conf2, EGL_ALPHA_SIZE );
          }
     }
     else {
          /* assume the default criteria, which gives no specific ordering */
          val1 = val2 = 0;
     }

     /* for color bits, larger one is preferred */
     if (val1 != val2)
          return(val2 - val1);

     for (i = 0; i < D_ARRAY_SIZE(compare_attribs); i++) {
          val1 = GET_ATTRIB( conf1, compare_attribs[i] );
          val2 = GET_ATTRIB( conf2, compare_attribs[i] );

          if (val1 != val2)
               return(val1 - val2);
     }

     /* EGL_NATIVE_VISUAL_TYPE cannot be compared here */

     return (compare_id) ? (GET_ATTRIB( conf1, EGL_CONFIG_ID ) - GET_ATTRIB( conf2, EGL_CONFIG_ID )) : 0;
}

/**********************************************************************************************************************/

DisplayDFB::DisplayDFB( EGL::Display &display,
                        Core         &core )
     :
     Type( display )
{
     D_DEBUG_AT( DFBEGL_Display, "DisplayDFB::%s( %p )\n", __FUNCTION__, this );

     Surface::Register< Surface::Initialise >( "Initialise",
                                               std::bind( &DisplayDFB::Surface_Initialise, this, _1 ),
                                               "",
                                               &display );

     KHR::Image::Register< KHR::Image::Initialise >( "Initialise",
                                                     std::bind( &DisplayDFB::Image_Initialise, this, _1 ),
                                                     (Direct::String) EGLInt(EGL_IMAGE_IDIRECTFBSURFACE_DIRECTFB),
                                                     &display );

     KHR::Image::Register< KHR::Image::Initialise >( "Initialise",
                                                     std::bind( &DisplayDFB::Image_Initialise, this, _1 ),
                                                     (Direct::String) EGLInt(EGL_NATIVE_PIXMAP_KHR),
                                                     &display );
}

DisplayDFB::~DisplayDFB()
{
     D_DEBUG_AT( DFBEGL_Display, "DisplayDFB::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

DFBResult
DisplayDFB::Image_Initialise( DirectFB::EGL::KHR::Image &image )
{
     D_DEBUG_AT( DFBEGL_Display, "EGLDisplayDFB::%s( %p )\n", __FUNCTION__, this );

     DFBResult         ret;
     IDirectFBSurface *surface = (IDirectFBSurface *) image.buffer;

     ret = (DFBResult) surface->AddRef( surface );
     if (ret) {
          D_DERROR_AT( DFBEGL_Display, ret, "  -> IDirectFBSurface::AddRef() failed!\n" );
          return ret;
     }

     int w, h;

     surface->GetSize( surface, &w, &h );

     D_INFO( "DFBEGL/Image: New EGLImage from IDirectFBSurface (%dx%d)\n", w, h );

     image.dfb_surface = surface;
     image.size.w = w;
     image.size.h = h;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
DisplayDFB::Surface_Initialise( DirectFB::EGL::Surface &surface )
{
     D_DEBUG_AT( DFBEGL_Display, "EGLDisplayDFB::%s( %p )\n", __FUNCTION__, this );

     DFBResult ret;

     if (surface.native_handle.value) {
          surface.surface = (IDirectFBSurface*) surface.native_handle.ptr;
     }
     else {
          DFBSurfaceDescription desc;

          Util::GetSurfaceDescription( surface.gfx_options, desc );

          if (surface.native_handle.clazz == NativeHandle::CLASS_WINDOW) {
               D_FLAGS_SET( desc.caps, DSCAPS_PRIMARY );

               parent.dfb->SetCooperativeLevel( parent.dfb, DFSCL_FULLSCREEN );
          }

          ret = parent.dfb->CreateSurface( parent.dfb, &desc, &surface.surface );
          if (ret) {
               D_DERROR( ret, "DFBEGL/Display: IDirectFB::CreateSurface() failed!\n" );
               return ret;
          }
     }

     int w, h;

     surface.surface->GetSize( surface.surface, &w, &h );

     D_INFO( "DFBEGL/Surface: New EGLSurface from %s IDirectFBSurface (%dx%d) with ID %u\n",
             surface.native_handle.value ? "existing" : "new", w, h, surface.GetID() );

     return DFB_OK;
}

}

}

