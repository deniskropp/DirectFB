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

#include <direct/Types++.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

extern "C" {
#include <direct/messages.h>

#include <core/system.h>

#include "drmkms_system.h"
}

#include <direct/ToString.h>

#include <core/CoreSurfaceAllocation.h>
#include <core/Debug.h>

#include "DRMEGLImpl.h"


D_LOG_DOMAIN( DFBDRM_EGLImpl,   "DFBDRM/EGL/Impl",   "DirectFB DRM EGL Implementation" );
D_LOG_DOMAIN( DFBDRM_EGLConfig, "DFBDRM/EGL/Config", "DirectFB DRM EGL Config" );


namespace DRM {

static DRMEGLImpl impl;



using namespace DirectFB;

using namespace std::placeholders;


class Holder : public Types::Type<Holder> {
public:
     GBM::BoSurface *bo;

     Holder( DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> &buffer,
             DRMEGLImpl                                             &impl )
          :
          bo( NULL )
     {
     }

     ~Holder()
     {
     }
};


/**********************************************************************************************************************/

GLeglImage::GLeglImage( EGL::KHR::Image &egl_image,
                        DRMEGLImpl      &impl )
     :
     Type( egl_image ),
     impl( impl ),
     glEGLImage( 0 )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRM::GLeglImage::%s( %p, KHR::Image %p, impl %p )\n", __FUNCTION__, this, &egl_image, &impl );

     D_LOG( DFBDRM_EGLImpl, VERBOSE, "  -> Deriving GLeglImage for DRM GL Context from generic EGLImageKHR (image %p, surface %p)\n", &egl_image, egl_image.dfb_surface );

     if (impl.eglCreateImageKHR) {
          DFBResult                   ret;
          IDirectFBSurfaceAllocation *allocation;

          ret = egl_image.dfb_surface->GetAllocation( egl_image.dfb_surface, DSBR_FRONT, DSSE_LEFT, "drm_gem.name", &allocation );
          if (ret) {
               D_DEBUG_AT( DFBDRM_EGLImpl, "  -> IDirectFBSurface::GetAllocation( FRONT, LEFT, 'drm_gem.name' ) failed! (%s)\n", DirectResultString((DirectResult)ret) );

               ret = egl_image.dfb_surface->Allocate( egl_image.dfb_surface, DSBR_FRONT, DSSE_LEFT, "drm_gem.name", 0, &allocation );
               if (ret) {
                    D_DERROR_AT( DFBDRM_EGLImpl, ret, "  -> IDirectFBSurface::Allocate( FRONT, LEFT, 'drm_gem.name' ) failed!\n" );
                    return;
               }
          }

          u64 handle;

          ret = allocation->GetHandle( allocation, &handle );
          if (ret) {
               D_DERROR_AT( DFBDRM_EGLImpl, ret, "  -> IDirectFBSurface::GetAllocation( FRONT, LEFT, 'drm_gem.name' ) failed!\n" );
               allocation->Release( allocation );
               return;
          }

          allocation->Release( allocation );


          u32 name = (u32) (long) handle;

          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> creating new EGLImage from name 0x%x\n", name );

          EGLint attribs[] = {
              EGL_WIDTH,                      egl_image.size.w,
              EGL_HEIGHT,                     egl_image.size.h,
              EGL_DRM_BUFFER_STRIDE_MESA,     egl_image.size.w,//FIXME
              EGL_DRM_BUFFER_FORMAT_MESA,     EGL_DRM_BUFFER_FORMAT_ARGB32_MESA,
              EGL_DRM_BUFFER_USE_MESA,        EGL_DRM_BUFFER_USE_SHARE_MESA | EGL_DRM_BUFFER_USE_SCANOUT_MESA,
              EGL_NONE
          };

          glEGLImage = impl.eglCreateImageKHR( impl.egl_display,
                                               EGL_NO_CONTEXT, EGL_DRM_BUFFER_MESA,
                                               (EGLClientBuffer)(long) name, attribs );
          if (!glEGLImage) {
               D_ERROR_AT( DFBDRM_EGLImpl, "  -> creating new EGLImage from name 0x%x failed (%s)\n",
                           name, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ) );
               return;
          }

          D_LOG( DFBDRM_EGLImpl, VERBOSE, "  => New EGLImage 0x%08lx from name 0x%x\n", (long) glEGLImage, name );
     }
}

GLeglImage::~GLeglImage()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "RCar::GLeglImage::%s( %p )\n", __FUNCTION__, this );

     if (glEGLImage)
          impl.eglDestroyImageKHR( impl.egl_display, glEGLImage );
}

/**********************************************************************************************************************/

void *
DRMEGLImpl::Context_eglGetProcAddress( const char *name )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLImpl::%s( %p, name '%s' )\n", __FUNCTION__, this, name );

     return (void*) impl.lib.eglGetProcAddress( name );
}

/**********************************************************************************************************************/

DRMEGLImpl::DRMEGLImpl()
     :
     eglCreateImageKHR( NULL ),
     eglDestroyImageKHR( NULL ),
     glEGLImageTargetTexture2DOES( NULL ),
     egl_display( EGL_NO_DISPLAY ),
     egl_major( 0 ),
     egl_minor( 0 )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLImpl::%s( %p )\n", __FUNCTION__, this );

     Register();
}

DRMEGLImpl::~DRMEGLImpl()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLImpl::%s( %p )\n", __FUNCTION__, this );
}

const Direct::String &
DRMEGLImpl::GetName() const
{
     static Direct::String name( "DRMEGL" );

     return name;
}

DirectResult
DRMEGLImpl::Initialise()
{
     DFBResult   ret;
     const char *client_apis;
     const char *egl_vendor;
     const char *egl_extensions;
     EGLConfig   egl_configs[400];
     EGLint      num_configs = 0;

     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLImpl::%s( %p )\n", __FUNCTION__, this );

// TODO: Later we may support loading multiple EGL implementations, system module independent, maybe surface pool needed
     if (dfb_system_type() == CORE_DRMKMS) {
          DRMKMSData *drmkms = (DRMKMSData*) dfb_system_data();

          dev.OpenFd( drmkms->fd );
     }
     else {
          //dev.Open( "/dev/dri/card0" );

          return DR_UNSUPPORTED;
     }

     DRM::GLeglImage::RegisterConversion< DirectFB::EGL::KHR::Image, DRMEGLImpl& >( *this );
     Holder::RegisterConversion< DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer>, DRMEGLImpl& >( *this );

     EGL::Core::Register< EGL::Core::GetProcAddress >( "GetProcAddress",
                                                       std::bind( &DRMEGLImpl::Context_eglGetProcAddress, this, _1 ),
                                                       "" );


     char *vals[10];
     int   num = 0;

     direct_config_get( "drmkms-egl-so", &vals[0], 10, &num );

     if (num) {
          ret = lib.Init( vals[0], true, false );
          if (ret)
               return (DirectResult) ret;
     }
     else {
          ret = lib.Init( "/usr/lib/libEGL.so.1", true, false );
          if (ret) {
               ret = lib.Init( "/usr/lib/x86_64-linux-gnu/mesa-egl/libEGL.so.1", true, false );
               if (ret) {
                    ret = lib.Init( "/usr/lib/i386-linux-gnu/mesa-egl/libEGL.so.1", true, false );
                    if (ret)
                         return (DirectResult) ret;
               }
          }
     }


     char *old_platform = getenv( "EGL_PLATFORM" );

     setenv( "EGL_PLATFORM", "drm", 1 );

     egl_display = lib.eglGetDisplay( (EGLNativeDisplayType) dev.gbm );
     if (!egl_display) {
          D_ERROR( "DRM/EGLImpl: eglGetDisplay() failed\n" );
          return (DirectResult) DFB_FAILURE;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  => EGLDisplay %p\n", egl_display );

     ret = lib.Load();
     if (ret)
          return (DirectResult) ret;

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling eglInitialize()\n" );

     if (!lib.eglInitialize( egl_display, &egl_major, &egl_minor )) {
          D_ERROR( "DRM/EGLImpl: eglInitialize() failed\n" );
          goto failure;
     }

     if (old_platform)
          setenv( "EGL_PLATFORM", old_platform, 1 );
     else
          unsetenv( "EGL_PLATFORM" );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> version %d.%d\n", egl_major, egl_minor );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling eglQueryString:%p\n",
                 lib.eglQueryString );


     egl_vendor = lib.eglQueryString( egl_display, EGL_VENDOR );

     if (!egl_vendor) {
          D_ERROR( "DRM/EGLImpl: eglQueryString( %p, EGL_VENDOR ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> vendor '%s'\n", egl_vendor );


     client_apis = lib.eglQueryString( egl_display, EGL_CLIENT_APIS );

     if (!client_apis) {
          D_ERROR( "DRM/EGLImpl: eglQueryString( %p, EGL_CLIENT_APIS ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> apis '%s'\n", client_apis );


     egl_extensions = lib.eglQueryString( egl_display, EGL_EXTENSIONS );

     if (!egl_extensions) {
          D_ERROR( "DRM/EGLImpl: eglQueryString( %p, EGL_EXTENSIONS ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> extensions '%s'\n", egl_extensions );


     name       = Direct::String::F( "DRM EGL (on %s)", egl_vendor );
     apis       = Direct::String( client_apis ).GetTokens( " " );
     //extensions = Direct::String( egl_extensions ).GetTokens( " " );


     if (!lib.eglGetConfigs( egl_display, egl_configs, D_ARRAY_SIZE(egl_configs), &num_configs )) {
          D_ERROR( "DRM/EGLImpl: eglGetConfigs( %p ) failed\n", egl_display );
          goto failure;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> got %u configs\n", num_configs );

     for (EGLint i=0; i<num_configs; i++)
          configs.push_back( new DRMEGLConfig( *this, egl_configs[i] ) );



     if (strstr( egl_extensions, "EGL_KHR_image" )) {
          eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC) lib.Lookup( "eglCreateImageKHR" );
          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> eglCreateImageKHR = %p\n", eglCreateImageKHR );

          eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC) lib.Lookup( "eglDestroyImageKHR" );
          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> eglDestroyImageKHR = %p\n", eglDestroyImageKHR );

          glEGLImageTargetTexture2DOES = (PFNGLEGLIMAGETARGETTEXTURE2DOESPROC) lib.Lookup( "glEGLImageTargetTexture2DOES" );
          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> glEGLImageTargetTexture2DOES = %p\n", glEGLImageTargetTexture2DOES );

          glEGLImageTargetRenderbufferStorageOES = (PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC) lib.Lookup( "glEGLImageTargetRenderbufferStorageOES" );
          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> glEGLImageTargetRenderbufferStorageOES = %p\n", glEGLImageTargetRenderbufferStorageOES );

          D_INFO( "DRM/EGL: Using EGLImage extension\n" );
     }


     core->RegisterImplementation( this );

     return DR_OK;


failure:
     lib.eglTerminate( egl_display );
     lib.Unload();
     egl_display = NULL;
     return DR_FAILURE;
}

DirectResult
DRMEGLImpl::Finalise()
{
     core->UnregisterImplementation( this );

     for (auto it=configs.begin(); it!=configs.end(); it++)
          delete *it;

     if (egl_display)
          lib.eglTerminate( egl_display );

     return DR_OK;
}

/**********************************************************************************************************************/

DRMEGLConfig::DRMEGLConfig( DRMEGLImpl &impl,
                            EGLConfig   egl_config )
     :
     Config( &impl ),
     impl( impl ),
     egl_config( egl_config )
{
     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p, egl_config 0x%08lx )\n", __FUNCTION__, this, (long) egl_config );

}

DRMEGLConfig::~DRMEGLConfig()
{
     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p )\n", __FUNCTION__, this );

}

DFBResult
DRMEGLConfig::GetOption( const Direct::String &name,
                         long                 &value )
{
     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p, '%s' ) <- egl_config 0x%08lx\n", __FUNCTION__, this, *name, (long) egl_config );

     DirectFB::EGL::EGLInt option;

     if (FromString<DirectFB::EGL::EGLInt>( option, name )) {
          D_DEBUG_AT( DFBDRM_EGLConfig, "  => 0x%04x\n", option.value );

          EGLint v = 0;

          switch (option.value) {
               case EGL_NATIVE_VISUAL_ID:
                    v = 0x21;
                    break;

               default:
                    if (!impl.lib.eglGetConfigAttrib( impl.egl_display, egl_config, option.value, &v)) {
                         D_ERROR( "DRM/EGLConfig: eglGetConfigAttrib( %p, config %p, 0x%04x '%s' ) failed\n",
                                  impl.egl_display, (void*) (long) egl_config, option.value, *name );
                         return DFB_FAILURE;
                    }
          }

          D_DEBUG_AT( DFBDRM_EGLConfig, "  => 0x%x\n", v );

          value = v;

          return DFB_OK;
     }

     return Graphics::Config::GetOption( name, value );
}

DFBResult
DRMEGLConfig::CheckOptions( const Graphics::Options &options )
{
     DFBResult ret = DFB_OK;

     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p, options %p )\n", __FUNCTION__, this, &options );

     for (Graphics::Options::const_iterator it = options.begin();
          it != options.end();
          it++)
     {
          long                  val   = 0;
          long                  check = 0;
          Graphics::OptionBase *base = (*it).second;

          D_DEBUG_AT( DFBDRM_EGLConfig, "  ---> '%s'\n", *base->GetName() );

          GetOption( base->GetName(), val );

          check = dynamic_cast<Graphics::Option<long> *>(base)->GetValue();

          if (base->GetName() == "RENDERABLE_TYPE" || base->GetName() == "SURFACE_TYPE") {
               if ((val & check) == check)
                    D_DEBUG_AT( DFBDRM_EGLConfig, "  =    local '0x%08lx' contains '0x%08lx'\n", val, check );
               else {
                    D_DEBUG_AT( DFBDRM_EGLConfig, "  X    local '0x%08lx' misses '0x%08lx'\n", val, check );

                    ret = DFB_UNSUPPORTED;
               }
          }
          else {
               if (val > check)// && check != 0)
                    D_DEBUG_AT( DFBDRM_EGLConfig, "  >    local '%ld' greater than '%ld'\n", val, check );
               else if (val == check)
                    D_DEBUG_AT( DFBDRM_EGLConfig, "  =    local '%ld' equals\n", val );
               else {
                    D_DEBUG_AT( DFBDRM_EGLConfig, "  X    local '%ld' < '%ld'\n", val, check );

                    ret = DFB_UNSUPPORTED;
               }
          }
     }

     if (ret)
          return ret;

     return Graphics::Config::CheckOptions( options );
}

DFBResult
DRMEGLConfig::CreateContext( const Direct::String  &api,
                             Graphics::Context     *share,
                             Graphics::Options     *options,
                             Graphics::Context    **ret_context )
{
     DFBResult          ret;
     Graphics::Context *context;

     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p, api '%s', share %p, options %p )\n",
                 __FUNCTION__, this, *api, share, options );

     context = new DRMEGLContext( impl, api, this, share, options );

     ret = context->Init();
     if (ret) {
          delete context;
          return ret;
     }

     *ret_context = context;

     return DFB_OK;
}

DFBResult
DRMEGLConfig::CreateSurfacePeer( CoreSurface            *surface,
                                 Graphics::Options      *options,
                                 Graphics::SurfacePeer **ret_peer )
{
     DFBResult              ret;
     Graphics::SurfacePeer *peer;

     D_DEBUG_AT( DFBDRM_EGLConfig, "DRMEGLConfig::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     peer = new DRMEGLSurfacePeer( impl, this, options, surface );

     ret = peer->Init();
     if (ret) {
          delete peer;
          return ret;
     }

     *ret_peer = peer;

     return DFB_OK;
}

/**********************************************************************************************************************/

DRMEGLContext::DRMEGLContext( DRMEGLImpl           &impl,
                              const Direct::String &api,
                              Graphics::Config     *config,
                              Graphics::Context    *share,
                              Graphics::Options    *options )
     :
     Context( api, config, share, options ),
     impl( impl ),
     egl_context( EGL_NO_CONTEXT )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p )\n", __FUNCTION__, this );

     Graphics::Context::Register< GL::OES::glEGLImageTargetTexture2D >( "glEGLImageTargetTexture2D",
                                                                        std::bind( &DRMEGLContext::glEGLImageTargetTexture2D, this, _1, _2 ),
                                                                        "",
                                                                        this );

     Graphics::Context::Register< GL::OES::glEGLImageTargetRenderbufferStorage >( "glEGLImageTargetRenderbufferStorage",
                                                                                  std::bind( &DRMEGLContext::glEGLImageTargetRenderbufferStorage, this, _1, _2 ),
                                                                                  "",
                                                                                  this );
}

DRMEGLContext::~DRMEGLContext()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
DRMEGLContext::Init()
{
     DRMEGLConfig *config;
     EGLContext    egl_share = EGL_NO_CONTEXT;

     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p )\n", __FUNCTION__, this );

     config = GetConfig<DRMEGLConfig>();

     if (share)
          egl_share = dynamic_cast<DRMEGLContext*>( share )->egl_context;

     EGLenum egl_api = DirectFB::EGL::Util::StringToAPI( api );

     if (!egl_api) {
          D_ERROR( "DRM/EGLImpl: Unknown API '%s'!\n", *api );
          return DFB_INVARG;
     }

     impl.lib.eglBindAPI( egl_api );

     EGLint attribs[] = {
          EGL_CONTEXT_CLIENT_VERSION, (EGLint) options->GetValue<long>( "CONTEXT_CLIENT_VERSION", 1 ),
          EGL_NONE
     };

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling eglCreateContext( EGLDisplay %p, EGLConfig %p, share %p, attribs %p )\n",
                 impl.egl_display, config->egl_config, egl_share, attribs );

     egl_context = impl.lib.eglCreateContext( impl.egl_display, config->egl_config, egl_share, attribs );
     if (egl_context == EGL_NO_CONTEXT) {
          D_ERROR( "DRM/EGLImpl: eglCreateContext() failed\n" );
          return DFB_FAILURE;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  => EGLContext %p\n", egl_context );

     return DFB_OK;
}

DFBResult
DRMEGLContext::Bind( Graphics::SurfacePeer *draw,
                     Graphics::SurfacePeer *read )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p )\n", __FUNCTION__, this );

     DRMEGLSurfacePeer *draw_peer = (DRMEGLSurfacePeer *) draw;
     DRMEGLSurfacePeer *read_peer = (DRMEGLSurfacePeer *) read;

     EGLSurface surf_draw = draw_peer->eglSurface;
     EGLSurface surf_read = read_peer->eglSurface;

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling eglMakeCurrent( draw/read 0x%08lx/0x%08lx, context 0x%08lx )...\n",
                 (unsigned long) surf_draw, (unsigned long) surf_read, (unsigned long) egl_context );

     if (!impl.lib.eglMakeCurrent( impl.egl_display, surf_draw, surf_read, egl_context )) {
          D_ERROR( "DRM/EGLImpl: eglMakeCurrent( %p, %p, %p ) failed\n", surf_draw, surf_read, (void*) (long) egl_context );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

DFBResult
DRMEGLContext::Unbind()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p )\n", __FUNCTION__, this );

     if (!impl.lib.eglMakeCurrent( impl.egl_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )) {
          D_ERROR( "DRM/EGLImpl: eglMakeCurrent( EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT ) failed\n" );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

DFBResult
DRMEGLContext::GetProcAddress( const Direct::String  &name,
                               void                 *&addr )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::%s( %p, name '%s' )\n", __FUNCTION__, this, *name );

     __eglMustCastToProperFunctionPointerType result = impl.lib.eglGetProcAddress( *name );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> eglGetProcAddress() -> %p\n", result );

     if (result) {
          addr = (void*) result;
          return DFB_OK;
     }

     return DFB_ITEMNOTFOUND;
}

void
DRMEGLContext::glEGLImageTargetTexture2D( GL::enum_  &target,
                                          GLeglImage &image )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::glEGLImageTargetTexture2D::%s( %p, target 0x%04x, DRM::GLeglImage %p, EGLImage %p )\n",
            __FUNCTION__, this, target, &image, image.glEGLImage );

     if (!image.glEGLImage) {
          void                  *data;
          int                    pitch;
          int                    width;
          int                    height;
          DFBSurfacePixelFormat  format;

          image.parent.dfb_surface->GetPixelFormat( image.parent.dfb_surface, &format );

          image.parent.dfb_surface->GetSize( image.parent.dfb_surface, &width, &height );

          image.parent.dfb_surface->Lock( image.parent.dfb_surface, DSLF_READ, &data, &pitch );

          D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling glTexImage2D( target 0x%04x, data %p )...\n", target, data );
          D_INFO( "  -> calling glTexImage2D( target 0x%04x, %dx%d, data %p )...\n", target, width, height, data );

          glPixelStorei( GL_UNPACK_ROW_LENGTH_EXT, pitch/4 );

          if (format == DSPF_ABGR) {
               glTexImage2D( target, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data );
          }
          else {
               glTexImage2D( target, 0, GL_BGRA_EXT, width, height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, data );
          }

          image.parent.dfb_surface->Unlock( image.parent.dfb_surface );
     }
     else
          impl.glEGLImageTargetTexture2DOES( target, image.glEGLImage );
}

void
DRMEGLContext::glEGLImageTargetRenderbufferStorage( GL::enum_  &target,
                                                    GLeglImage &image )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLContext::glEGLImageTargetRenderbufferStorage::%s( %p, target 0x%04x, DRM::GLeglImage %p, EGLImage %p )\n",
            __FUNCTION__, this, target, &image, image.glEGLImage );

     if (!image.glEGLImage) {
          D_UNIMPLEMENTED();
     }
     else
          impl.glEGLImageTargetRenderbufferStorageOES( target, image.glEGLImage );
}

/**********************************************************************************************************************/

DRMEGLSurfacePeer::DRMEGLSurfacePeer( DRMEGLImpl        &impl,
                                      Graphics::Config  *config,
                                      Graphics::Options *options,
                                      CoreSurface       *surface )
     :
     SurfacePeer( config, options, surface ),
     impl( impl ),
     gbm_surface( NULL ),
     eglSurface( EGL_NO_SURFACE )
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     SurfacePeer::Register< SurfacePeer::Flush >( "Flush",
                                                  std::bind( &DRMEGLSurfacePeer::Flush, this ),
                                                  "",
                                                  this );
}

DRMEGLSurfacePeer::~DRMEGLSurfacePeer()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );
}


DFBResult
DRMEGLSurfacePeer::Init()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLSurfacePeer::%s( %p )\n", __FUNCTION__, this );

     DFBResult ret;

     // refs surface and updates buffers
     ret = SurfacePeer::Init();
     if (ret)
          return ret;

     DRMEGLConfig *config = GetConfig<DRMEGLConfig>();

     gbm_surface = gbm_surface_create( impl.dev.gbm,
                                       GetSurfaceConfig().size.w,
                                       GetSurfaceConfig().size.h,
                                       GBM_FORMAT_ARGB8888,
                                       GBM_BO_USE_SCANOUT |
                                       GBM_BO_USE_RENDERING );
     if (!gbm_surface) {
          D_ERROR( "DRMEGL: failed to create gbm surface\n" );
          return DFB_FAILURE;
     }

     EGLint attrs[] = { EGL_NONE };

     eglSurface = impl.lib.eglCreateWindowSurface( impl.egl_display, config->egl_config, gbm_surface, attrs );
     if (!eglSurface) {
          D_ERROR( "DRM/EGLImpl: eglCreateWindowSurface( %p ) failed ('%s')\n", eglSurface, *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ));
          return DFB_FAILURE;
     }

     return DFB_OK;
}

DFBResult
DRMEGLSurfacePeer::Flush()
{
     D_DEBUG_AT( DFBDRM_EGLImpl, "DRMEGLSurfacePeer::%s( %p ) <- bound %p\n", __FUNCTION__, this, eglSurface );

     if (eglSurface == EGL_NO_SURFACE)
          return DFB_OK;

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> calling eglSwapBuffers( %p )\n", eglSurface );

     DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> &current = getBuffer( 0 );
     DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> &next    = getBuffer( 1 );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> current %s\n", *ToString<CoreSurfaceBuffer>(*current.object) );
     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> next    %s\n", *ToString<CoreSurfaceBuffer>(*next.object) );

     Holder &holder = next;

     if (holder.bo)
          holder.bo->release();

     if (!impl.lib.eglSwapBuffers( impl.egl_display, eglSurface )) {
          D_ERROR( "DRM/EGLImpl: eglSwapBuffers( %p ) failed ('%s')\n", eglSurface,
                   *ToString<DirectFB::EGL::EGLInt>( DirectFB::EGL::EGLInt(impl.lib.eglGetError()) ));
          return DFB_FAILURE;
     }

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> eglSwapBuffers( %p ) done\n", eglSurface );


     DFBResult   ret;

     struct gbm_bo *front = gbm_surface_lock_front_buffer( gbm_surface );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> gbm_surface_lock_front_buffer() returned %p\n", front );

     D_DEBUG_AT( DFBDRM_EGLImpl, "  -> handle    0x%x\n", gbm_bo_get_handle(front).u32 );

     GBM::BoSurface *bo = (GBM::BoSurface *) gbm_bo_get_user_data( front );

     if (!bo) {
          bo = new GBM::BoSurface( impl.dev, gbm_surface, front );

          gbm_bo_set_user_data( front, bo, GBM::BoSurface::destroy_user_data );
     }
     else {
          bo->reclaim();
     }

     {
     Holder &holder = current;

     holder.bo = bo;


     if (!bo->allocation) {
          ret = ::CoreSurface_GetOrAllocate( GetSurface(),
                                             buffer_left(),
                                             "drm_gem.name+handle", strlen("drm_gem.name+handle")+1,
                                             (((u64)bo->handle) << 32) | bo->name,
                                             (DFBSurfaceAllocationOps)(DSAO_HANDLE | DSAO_KEEP | DSAO_UPDATED),
                                             &bo->allocation );
          if (ret)
               D_ERROR_AT( DFBDRM_EGLImpl, "CoreSurface_GetOrAllocate( 0x%x, drm_gem.name+handle 0x%x / 0x%x ) failed! (%s)\n",
                           buffer_left(), bo->name, bo->handle, *ToString<DFBResult>(ret) );
     }
     else {
          ::CoreSurfaceAllocation_Updated( bo->allocation, NULL, 0 );
     }
     }

     return DFB_OK;
}



}

