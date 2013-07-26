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

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <egl/dfbegl.h>

#include <GLES2/gl2.h>




namespace DirectFB {




namespace GL {


class Core : public Direct::Types<Core>
{
public:

};


typedef unsigned int enum_;



namespace OES {

class Core : public Direct::Types<Core,GL::Core>
{
public:

};


class eglImage : public Core::Type<eglImage>
{
public:
     ::GLeglImageOES image;

     eglImage();
};

typedef std::function<void ( GL::enum_     &target,
                             OES::eglImage &image )> glEGLImageTargetTexture2D;

typedef std::function<void ( GL::enum_     &target,
                             OES::eglImage &image )> glEGLImageTargetRenderBufferStorage;

}

}




namespace EGL {


/*
 * EGL::Image for wrapping
 */

namespace KHR {

class Types : public Direct::Types<Types,EGL::Types>
{
};


class Image : public Types::Type<Image>
{
     friend class Context;
     friend class Display;

protected:

public:
     Image();
     virtual ~Image();

     static DFBResult Create( Display          *display,
                              Context          *context,
                              EGLenum           target,
                              EGLClientBuffer   buffer,
                              const EGLint     *attrib_list,
                              Image           *&ret_image );

     Display             *GetDisplay() const { return display; }
     Context             *GetContext() const { return context; }
     EGLenum              GetTarget() const { return target; }
     EGLClientBuffer      GetBuffer() const { return buffer; }


     typedef std::function<DFBResult (KHR::Image &image)> Initialise;


public:
     Display               *display;
     Context               *context;
     EGLenum                target;
     EGLClientBuffer        buffer;

     Graphics::Options      gfx_options;

     IDirectFBSurface      *surface;
};



}



class EGLImage : public Types::Type<EGLImage,EGLExtension>
{
public:
     EGLImage();
     virtual ~EGLImage();


     static EGLImageKHR eglCreateImageKHR ( EGLDisplay       dpy,
                                            EGLContext       ctx,
                                            EGLenum          target,
                                            EGLClientBuffer  buffer,
                                            const EGLint    *attr_list );

     static EGLBoolean  eglDestroyImageKHR( EGLDisplay       dpy,
                                            EGLImageKHR      image );

     static void        glEGLImageTargetTexture2DOES( GLenum       target,
                                                      EGLImageKHR  image );

     static void        glEGLImageTargetRenderBufferStorageOES( GLenum       target,
                                                                EGLImageKHR  image );
};


}

}

