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

#ifndef ___DirectFB__DRM__DRMEGLIMPL__H___
#define ___DirectFB__DRM__DRMEGLIMPL__H___


#include <direct/String.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>

#include <egl/dfbegl.h>
#include <egl/KHR_image.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include "DFBGBM.h"


namespace DRM {

using namespace DirectFB;


class Types : public Direct::Types<Types>
{
};

class DRMEGLImpl;


class GLeglImage : public Types::Type<GLeglImage,EGL::KHR::Image> {
private:
     DRMEGLImpl &impl;

public:
     GLeglImageOES glEGLImage;

     GLeglImage( DirectFB::EGL::KHR::Image &egl_image,
                 DRMEGLImpl                &impl );
     ~GLeglImage();
};


class DRMEGLImpl : public Graphics::Implementation
{
     friend class GLeglImage;
     friend class DRMEGLConfig;
     friend class DRMEGLContext;
     friend class DRMEGLSurfacePeer;

public:
     DRMEGLImpl();
     virtual ~DRMEGLImpl();

     virtual const Direct::String &GetName() const;

     virtual DirectResult Initialise();// override;
     virtual DirectResult Finalise();// override;

public:
     GBM::Device                                   dev;
     DirectFB::EGL::Library                        lib;
     PFNEGLCREATEIMAGEKHRPROC                      eglCreateImageKHR;
     PFNEGLDESTROYIMAGEKHRPROC                     eglDestroyImageKHR;
     PFNGLEGLIMAGETARGETTEXTURE2DOESPROC           glEGLImageTargetTexture2DOES;
     PFNGLEGLIMAGETARGETRENDERBUFFERSTORAGEOESPROC glEGLImageTargetRenderbufferStorageOES;


     void *Context_eglGetProcAddress( const char *name );

private:
     EGLDisplay         egl_display;
     EGLint             egl_major, egl_minor;
};


class DRMEGLConfig : public Graphics::Config
{
     friend class DRMEGLImpl;
     friend class DRMEGLContext;
     friend class DRMEGLSurfacePeer;

protected:
     DRMEGLConfig( DRMEGLImpl &impl,
                   EGLConfig   egl_config );
     virtual ~DRMEGLConfig();

public:
     virtual DFBResult GetOption    ( const Direct::String    &name,
                                      long                    &value );

     virtual DFBResult CreateContext( const Direct::String    &api,
                                      Graphics::Context       *share,
                                      Graphics::Options       *options,
                                      Graphics::Context      **ret_context );

     virtual DFBResult CreateSurfacePeer( CoreSurface            *surface,
                                          Graphics::Options      *options,
                                          Graphics::SurfacePeer **ret_peer );

private:
     DRMEGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?
     EGLConfig   egl_config;
};


class DRMEGLContext : public Graphics::Context
{
     friend class DRMEGLConfig;

protected:
     DRMEGLContext( DRMEGLImpl           &impl,
                    const Direct::String &api,
                    Graphics::Config     *config,
                    Graphics::Context    *share,
                    Graphics::Options    *options );
     virtual ~DRMEGLContext();

public:
     virtual DFBResult Init  ();
     virtual DFBResult Bind  ( Graphics::SurfacePeer *draw,
                               Graphics::SurfacePeer *read );
     virtual DFBResult Unbind();

     virtual DFBResult GetProcAddress( const Direct::String  &name,
                                       void                 *&addr );

     void glEGLImageTargetTexture2D( GL::enum_  &target,
                                     GLeglImage &image );

     void glEGLImageTargetRenderbufferStorage( GL::enum_  &target,
                                               GLeglImage &image );

private:
     DRMEGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?
     EGLContext  egl_context;

     // TODO: store draw/read, bind to draw/read, unbind from draw/read
};


class DRMEGLSurfacePeer : public Graphics::SurfacePeer
{
     friend class DRMEGLContext;

public:
     DRMEGLSurfacePeer( DRMEGLImpl        &impl,
                        Graphics::Config  *config,
                        Graphics::Options *options,
                        CoreSurface       *surface );

     virtual ~DRMEGLSurfacePeer();


     virtual DFBResult Init();

//     typedef std::function<DFBResult(void)>  Init;

protected:
     DRMEGLImpl &impl;   // FIXME: remove and use Graphics::Config::implementation?

     struct gbm_surface *gbm_surface;
     EGLSurface          eglSurface;

     // TODO: add context here which is currently bound to the surface(s)

     DFBResult Flush();
};


}


#endif

