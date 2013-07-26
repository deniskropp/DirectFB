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

#ifndef ___DirectFB__X11__X11EGLIMPL__H___
#define ___DirectFB__X11__X11EGLIMPL__H___


#ifdef __cplusplus
extern "C" {
#endif

#include "x11.h"

DFBResult dfb_x11_eglimpl_register( DFBX11 *x11 );
void      dfb_x11_eglimpl_unregister( void );

#ifdef __cplusplus
}


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/image.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>


namespace X11 {

using namespace DirectFB;


class Types : public Direct::Types<Types>
{
};

class X11EGLImpl;


namespace EGL {

class Image : public Types::Type<Image,DirectFB::EGL::KHR::Image> {
public:
     Pixmap                      pixmap;
     IDirectFBSurfaceAllocation *allocation;

     Image( DirectFB::EGL::KHR::Image &egl_image,
            X11EGLImpl                &impl );
};

}


class GLeglImage : public Types::Type<GLeglImage,DirectFB::EGL::KHR::Image> {
public:
     GLeglImageOES glEGLImage;

     GLeglImage( DirectFB::EGL::KHR::Image &egl_image,
                 X11EGLImpl                &impl );
};


class X11EGLImpl : public Graphics::Implementation
{
     friend class EGL::Image;
     friend class GLeglImage;
     friend class X11EGLConfig;
     friend class X11EGLContext;
     friend class X11EGLSurfacePeer;

public:
     X11EGLImpl();
     virtual ~X11EGLImpl();

     DFBResult Init( DFBX11 *x11 );

public:
     virtual Graphics::Configs &GetConfigs();

public:
     DirectFB::EGL::Library              lib;
     PFNEGLCREATEIMAGEKHRPROC            eglCreateImageKHR;
     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC glEGLImageTargetTexture2DOES;

     void Context_glEGLImageTargetTexture2D( GL::enum_       &target,
                                             X11::GLeglImage &image );


private:
     DFBX11            *x11;
     Graphics::Configs  configs;

     EGLDisplay         egl_display;
     EGLint             egl_major, egl_minor;
};


class X11EGLConfig : public Graphics::Config
{
     friend class X11EGLImpl;
     friend class X11EGLContext;
     friend class X11EGLSurfacePeer;

protected:
     X11EGLConfig( X11EGLImpl &impl,
                   EGLConfig   egl_config );
     virtual ~X11EGLConfig();

public:
     virtual DFBResult GetOption    ( const Direct::String    &name,
                                      long                    &value );

     virtual DFBResult CheckOptions ( const Graphics::Options &options );

     virtual DFBResult CreateContext( const Direct::String    &api,
                                      Graphics::Context       *share,
                                      Graphics::Options       *options,
                                      Graphics::Context      **ret_context );

     virtual DFBResult CreateSurfacePeer( IDirectFBSurface       *surface,
                                          Graphics::Options      *options,
                                          Graphics::SurfacePeer **ret_peer );

private:
     X11EGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?
     EGLConfig   egl_config;
};


class X11EGLContext : public Graphics::Context
{
     friend class X11EGLConfig;

protected:
     X11EGLContext( X11EGLImpl           &impl,
                    const Direct::String &api,
                    Graphics::Config     *config,
                    Graphics::Context    *share,
                    Graphics::Options    *options );
     virtual ~X11EGLContext();

public:
     virtual DFBResult Init  ();
     virtual DFBResult Bind  ( Graphics::SurfacePeer *draw,
                               Graphics::SurfacePeer *read );
     virtual DFBResult Unbind();

     virtual DFBResult GetProcAddress( const Direct::String  &name,
                                       void                 *&addr );

private:
     X11EGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?
     EGLContext  egl_context;

     // TODO: store draw/read, bind to draw/read, unbind from draw/read
};


class X11EGLSurfacePeer : public Graphics::SurfacePeer
{
     friend class X11EGLContext;

public:
     X11EGLSurfacePeer( X11EGLImpl        &impl,
                        Graphics::Config  *config,
                        Graphics::Options *options,
                        IDirectFBSurface  *surface );

     virtual ~X11EGLSurfacePeer();

     virtual DFBResult    Init();
     virtual DFBResult    Flip( const DFBRegion     *region,
                                DFBSurfaceFlipFlags  flags );

protected:
     X11EGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?

     EGLSurface  getEGLSurface();

     typedef std::map<XID,EGLSurface> SurfaceMap;

     SurfaceMap    surface_map;

     unsigned int                alloc_num;
     IDirectFBSurfaceAllocation *alloc_left[MAX_SURFACE_BUFFERS];
     IDirectFBSurfaceAllocation *alloc_right[MAX_SURFACE_BUFFERS];

     unsigned int                index;

     bool                        is_pixmap;

     // TODO: add context here which is currently bound to the surface(s)
};


}


#endif // __cplusplus


#endif


