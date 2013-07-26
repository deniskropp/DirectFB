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

#include <config.h>

#include <stdio.h>
#include <string.h>

#include <typeindex>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>
#include <egl/image.h>

#include <GLES2/gl2.h>


D_LOG_DOMAIN( DFBEGL_Image, "DFBEGL/Image", "DirectFB EGL Image" );
D_LOG_DOMAIN( DFBEGL_Api,   "DFBEGL/API",   "DirectFB EGL API" );




namespace DirectFB {


namespace GL {

OES::eglImage::eglImage()
{
     D_LOG( DFBEGL_Api, DEBUG_1, "OES::eglImage::%s( %p )\n", __FUNCTION__, this );
}

}



namespace EGL {

using namespace std::placeholders;


/**********************************************************************************************************************/

static EGLImage egl_image;

/**********************************************************************************************************************/

EGLImage::EGLImage()
{
     D_DEBUG_AT( DFBEGL_Image, "EGLImage::%s( %p )\n", __FUNCTION__, this );

     Display::Register< EGLExtension::GetNames >( "EGLImage", [](){ return "EGL_DIRECTFB_idirectfbsurface EGL_KHR_image_base EGL_KHR_image_pixmap EGL_KHR_image"; } );

     Core::Register<
          Core::GetProcAddress
          >( "eglCreateImageKHR", [](const char *){ return (void*) eglCreateImageKHR; } );

     Core::Register<
          Core::GetProcAddress
          >( "eglDestroyImageKHR", [](const char *){ return (void*) eglDestroyImageKHR; } );

     Core::Register<
          Core::GetProcAddress
          >( "glEGLImageTargetTexture2DOES", [](const char *){ return (void*) glEGLImageTargetTexture2DOES; } );

     Core::Register<
          Core::GetProcAddress
          >( "glEGLImageTargetRenderBufferStorageOES", [](const char *){ return (void*) glEGLImageTargetRenderBufferStorageOES; } );
}

EGLImage::~EGLImage()
{
     D_DEBUG_AT( DFBEGL_Image, "EGLImage::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

EGLImageKHR
EGLImage::eglCreateImageKHR( EGLDisplay       dpy,
                             EGLContext       ctx,
                             EGLenum          target,
                             EGLClientBuffer  buffer,
                             const EGLint    *attr_list )
{
     D_DEBUG_AT( DFBEGL_Api, "%s( display %p, ctx %p, target 0x%04x '%s', buffer 0x%08lx, attribs %p )\n",
                 __FUNCTION__, dpy, ctx, target, *ToString<EGLInt>( EGLInt(target) ), (long) buffer, attr_list );

     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_IMAGE_KHR);

     if (!target || !buffer)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_NO_IMAGE_KHR);


     EGL::Display *display = (EGL::Display *) dpy;
     EGL::Context *context = (EGL::Context *) ctx;
     EGL::KHR::Image   *image;

     DFBResult ret = KHR::Image::Create( display, context, target, buffer, attr_list, image );

     if (ret)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_NO_IMAGE_KHR);

//     D_LOG( DFBEGL_Api, VERBOSE, "New DFBSurfaceID %u from EGLImage buffer %p (target %s)\n",
//            surface->GetID(), buffer, *ToString<EGL::EGLInt>( target ) );

     DFB_EGL_RETURN (EGL_SUCCESS, (EGLImageKHR) new Types::TypeHandle( image ) );
}

EGLBoolean
EGLImage::eglDestroyImageKHR( EGLDisplay  dpy,
                              EGLImageKHR img )
{
     D_DEBUG_AT( DFBEGL_Api, "%s( display %p, image %p )\n",
                 __FUNCTION__, dpy, img );

     EGL::TLS *tls = EGLTLS.Get();

     if (!dpy)
          DFB_EGL_RETURN (EGL_BAD_DISPLAY, EGL_FALSE);

     if (!img)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, EGL_FALSE);


     Types::TypeHandle *handle = (Types::TypeHandle *) img;

     KHR::Image *image = &(KHR::Image&) **handle;

     delete image;

     DFB_EGL_RETURN (EGL_SUCCESS, EGL_TRUE);
}


void
EGLImage::glEGLImageTargetTexture2DOES( GLenum      target,
                                        EGLImageKHR img )
{
     D_DEBUG_AT( DFBEGL_Api, "%s( target 0x%04x '%s', image %p )\n", __FUNCTION__, target, *ToString<EGLInt>( EGLInt(target) ), img );

     EGL::TLS     *tls = EGLTLS.Get();
     EGL::Context *ctx = tls->GetContext();

     if (!img)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, );

     if (!ctx)
          DFB_EGL_RETURN (EGL_BAD_CONTEXT, );


     Graphics::Context &context = *(ctx->gfx_context);

     Types::TypeHandle *handle = (Types::TypeHandle *) img;

     KHR::Image &image = (KHR::Image&) **handle;

     context.Call<GL::OES::glEGLImageTargetTexture2D>()( target, image );
}

void
EGLImage::glEGLImageTargetRenderBufferStorageOES( GLenum      target,
                                                  EGLImageKHR img )
{
     D_DEBUG_AT( DFBEGL_Api, "%s( target 0x%04x '%s', image %p )\n", __FUNCTION__, target, *ToString<EGLInt>( EGLInt(target) ), img );

     EGL::TLS     *tls = EGLTLS.Get();
     EGL::Context *ctx = tls->GetContext();

     if (!img)
          DFB_EGL_RETURN (EGL_BAD_PARAMETER, );

     if (!ctx)
          DFB_EGL_RETURN (EGL_BAD_CONTEXT, );


     Graphics::Context &context = *(ctx->gfx_context);

     Types::TypeHandle *handle = (Types::TypeHandle *) img;

     KHR::Image &image = (KHR::Image&) **handle;

     context.Call<GL::OES::glEGLImageTargetRenderBufferStorage>()( target, image );
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
Display::Image_Initialise( DirectFB::EGL::KHR::Image &image )
{
     D_DEBUG_AT( DFBEGL_Image, "EGLImage::InitImage::%s( %p, image %p )\n", __FUNCTION__, this, &image );

     DFBResult         ret;
     IDirectFBSurface *surface = (IDirectFBSurface *) image.buffer;

     ret = (DFBResult) surface->AddRef( surface );
     if (ret) {
          D_DERROR_AT( DFBEGL_Image, ret, "  -> IDirectFBSurface::AddRef() failed!\n" );
          return ret;
     }

     int w, h;

     surface->GetSize( surface, &w, &h );

     D_INFO( "DFBEGL/Image: New EGLImage from IDirectFBSurface (%dx%d)\n", w, h );

     image.surface = surface;

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
KHR::Image::Create( Display          *display,
                    Context          *context,
                    EGLenum           target,
                    EGLClientBuffer   buffer,
                    const EGLint     *attrib_list,
                    Image           *&ret_image )
{
     DFBResult ret;

     D_DEBUG_AT( DFBEGL_Image, "KHR::Image::%s( display %p, context %p, target 0x%04x '%s', buffer 0x%08lx, attribs %p )\n",
                 __FUNCTION__, display, context, target, *ToString<EGLInt>( EGLInt(target) ), (long) buffer, attrib_list );

     if (attrib_list)
          DFB_EGL_ATTRIB_LIST_DEBUG_AT( DFBEGL_Image, attrib_list );


     ToString<EGLInt> target_str( target );
     EGLInt           target_val;

     D_DEBUG_AT( DFBEGL_Image, "  -> target '%s'\n", *target_str );

     // Key 1) context | target
     // Key 2) display | target
     // Key 3) target

     if (FromString<EGLInt>( target_val, target_str )) {
          if (target_val.value == (EGLint) target) {

          }
          else
               D_WARN( "conversion from/to 0x%04x/0x%04x (%s)", target, target_val.value, *target_str );
     }


     Image *image = new Image();

     image->display = display;

     image->context = context;
     image->target  = target;
     image->buffer  = buffer;

     if (attrib_list) {
          ret = EGL::Util::GetOptions( image->gfx_options, attrib_list );
          if (ret)
               D_DERROR( ret, "DFBEGL/Image: Failed to get Options from attrib_list!\n" );
     }

     if (target == EGL_NATIVE_PIXMAP_KHR && display->native_pixmap_target) {
          D_DEBUG_AT( DFBEGL_Image, "  -> NATIVE_PIXMAP_KHR = 0x%04x (%s)\n",
                      display->native_pixmap_target, *ToString<EGLInt>( EGLInt(display->native_pixmap_target) ) );

          target = display->native_pixmap_target;
     }


     KHR::Image::Initialise init;

     if (context) {
          init = KHR::Image::Call<KHR::Image::Initialise>( context->GetName() / *ToString<EGLInt>( EGLInt(target) ) );
     }

     if (!init) {
          init = KHR::Image::Call<KHR::Image::Initialise>( display->GetName() / *ToString<EGLInt>( EGLInt(target) ) );
     }

     if (!init) {
          init = KHR::Image::Call<KHR::Image::Initialise>( ToString<EGLInt>( EGLInt(target) ) );
     }


     if (!init) {
          D_ERROR( "DFBEGL/Image: No implementation for target 0x%04x '%s', display '%s', context '%s'\n",
                   target, *target_str, *display->GetName(), *context->GetName() );
          return DFB_NOIMPL;
     }

     D_INFO( "DFBEGL/Image: Using implementation '%s'\n", *ToString<std::type_info>( init.target_type() ) );

     ret = init( *image );
     if (ret) {
          delete image;
          return ret;
     }

     ret_image = image;

     return DFB_OK;
}

/**********************************************************************************************************************/

KHR::Image::Image()
     :
     display( NULL ),
     context( NULL ),
     target( 0 ),
     buffer( NULL )
{
     D_DEBUG_AT( DFBEGL_Image, "KHR::Image::%s( %p )\n", __FUNCTION__, this );
}

KHR::Image::~Image()
{
     D_DEBUG_AT( DFBEGL_Image, "KHR::Image::%s( %p )\n", __FUNCTION__, this );
}



}

}

