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

#ifndef ___DFBEGL__dfbegl__H___
#define ___DFBEGL__dfbegl__H___


#ifdef DFBEGL_ENABLE_MANGLE
#ifdef __egl_h_
#error Must include dfbegl.h before egl.h!
#endif

#include "EGL/egldfbmangle.h"
#endif


#include <directfb.h>    // MUST INCLUDE BEFORE <EGL/egl.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>



#ifdef __cplusplus


#include <direct/Map.h>
#include <direct/String.h>
#include <direct/TLSObject.h>

#include <core/CoreSurface.h>
#include <core/Graphics.h>


#include <list>
#include <map>






/*

  EGL (Core)


  = Display Modules
    - Native/X11 (X11 system module)
    - Native/XDirectFB (app module)
    - Native/Wayland

    = Extensions
      - WL/BindDisplay

  = Implementations (see more in core/Graphics.h)
    - DRM/EGL   (DRM system module + Mesa egl library)
    - X11/EGL   (X11 system module + Mesa egl library)
    - DirectVG  (Independent module)


  = Surfaces

     Functions

          eglCreatePixmapSurface( *config*, handle, attr )
          eglCreateWindowSurface( *config*, handle, attr )

          eglCreatePbufferSurface( *config*, attr )


     Native Handles
                         DFB                 X11                 DRM       Wayland
               PIXMAP    IDirectFBSurface    Pixmap (XID)        bo        ?
               WINDOW    IDirectFBSurface    Window (XID)        bo        wl_egl_window



  = Client Buffers

     Functions

          eglCreateImage( *context*, target, buffer, attr )

          eglCreatePbufferFromClientBuffer( *config*, buftype, buffer, attr )


     Types

          target                                                 buffer type

             (core)
             - EGL_IDIRECTFBSURFACE_DIRECTFB

             - EGL_WAYLAND_BUFFER_WL
             - EGL_WAYLAND_PLANE_WL

             - EGL_DRM_BUFFER_MESA

             - EGL_FRAMEBUFFER_TARGET_ANDROID
             - EGL_NATIVE_BUFFER_ANDROID


             (display)
             - EGL_NATIVE_PIXMAP_KHR
                    / DirectFB
                    / X11

             (implementation)  <context>
             - VG_PARENT_IMAGE_KHR                               OPENVG_IMAGE
                    / DVG

             - GL_RENDERBUFFER_KHR
                    / X11EGL

             - GL_TEXTURE_2D_KHR
                    / X11EGL


             - ...
               GL_TEXTURE_3D_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_X_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_X_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_Y_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Y_KHR
               GL_TEXTURE_CUBE_MAP_POSITIVE_Z_KHR
               GL_TEXTURE_CUBE_MAP_NEGATIVE_Z_KHR


    Attributes

          GL_TEXTURE_LEVEL_KHR
          GL_TEXTURE_ZOFFSET_KHR
          EGL_IMAGE_PRESERVED_KHR



  = Extensions (important/interesting to use/support)

          EXT_buffer_age
          EXT_swap_buffers_with_damage

          EXT_image_dma_buf_import

          EXT_client_extensions
          EXT_platform_base
          EXT_platform_x11

          ANGLE_query_surface_pointer

          KHR_reusable_sync
          KHR_wait_sync

          KHR_surfaceless_context

          ANDROID_native_fence_sync
*/




namespace DirectFB {


namespace EGL {


class Types : public Direct::Types<Types,Graphics::Core>
{
};



class Core;
class CoreExtension;
class CoreModule;
class DisplayImplementation;

class Display;
class Config;
class Context;
class Surface;


DECLARE_MODULE_DIRECTORY( core_modules );

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFBEGL_CORE_ABI_VERSION      1




class CoreModule
{
     friend class Core;

protected:
     virtual Direct::String GetName() const = 0;

     virtual DFBResult Initialise   ( Core                  &core ) = 0;
};




class Core : public EGL::Types::Type<Core>
{
     friend class Display;


public:
     DFBResult Display_Probe     ( Display      &display,
                                   unsigned int &ret_score );

     DFBResult Display_Initialise( Display      &display );


private:
     Core();
     ~Core();

     DFBResult LoadModules();


public:
     static Core &GetInstance();

     // FIXME: Result: Add EGL errors to DirectResult (use DFBEGLResult for API calls)
     EGLint    GetDisplay( EGLNativeDisplayType   native_display,
                           Display              *&ret_display );

     DFBResult PutDisplay( Display               *display );


private:
     typedef std::map<DirectModuleEntry*,CoreModule*>            CoreModules;
     typedef std::map<EGLNativeDisplayType,Display*>             DisplayMap;


     CoreModules            modules;
     DisplayMap             displays;

public:
     typedef std::function<void *( const char * )>     GetProcAddress;
};



class EGLExtension : public Types::Type<EGLExtension>
{
public:
     typedef std::function<Direct::Strings (void)>     GetNames;
};



class EGLInt {
public:
     EGLint value;

     EGLInt( const EGLint &value = 0 )
          :
          value( value )
     {
     }

     typedef std::map<const EGLint,Direct::String> ToStringMap;     // move To/FromStringMap stuff to Direct::
     typedef std::map<const Direct::String,EGLint> FromStringMap;

     class Maps {
     public:
          Maps() {}

          ToStringMap     toString;
          FromStringMap   fromString;
     };

     operator Direct::String ();
     operator std::string ();

     Direct::String operator * () {
          return (Direct::String) *this;
     }

public:
     static Maps &GetMaps();
     static void Register( const EGLint         &egl_int,
                           const Direct::String &egl_str );
};





class NativeHandle {
public:
     typedef unsigned long Type;

     typedef enum {
          CLASS_NONE,
          CLASS_PIXMAP,
          CLASS_WINDOW
     } Class;

     Class clazz;

     union {
          unsigned long        value;
          void                *ptr;
          EGLNativePixmapType  pixmap;
          EGLNativeWindowType  window;
     };

     NativeHandle()
          :
          clazz( CLASS_NONE ),
          value( 0 )
     {
     }

     NativeHandle( Class clazz, unsigned long value = 0 )
          :
          clazz( clazz ),
          value( value )
     {
     }
};





namespace KHR {
     class Image;
}


/*
 * EGL::Display for wrapping client's native display handle
 */

class Display : public Types::Type<Display>
{
     friend class Core;

public:
     typedef std::function<DFBResult (Display               &display,
                                      unsigned int          &ret_score)>   Probe;

     typedef std::function<DFBResult (Display               &display)>     Initialise;


     DFBResult Image_Initialise  ( DirectFB::EGL::KHR::Image &image );
     DFBResult Surface_Initialise( DirectFB::EGL::Surface    &surface );


protected:
     Display();
     virtual ~Display();

public:
     virtual DFBResult        Init();

     virtual void             GetVersion( EGLint &major, EGLint &minor );


     virtual EGLint           eglInitialise();
     virtual EGLint           Terminate();

     virtual EGLint           QueryString( EGLint          name,
                                           const char    *&ret_value );

     virtual EGLint           GetConfigs( Config **configs, EGLint config_size, EGLint *num_configs );

     virtual EGLint           ChooseConfig( const EGLint *attrib_list, Config **configs, EGLint config_size, EGLint *num_configs );


     virtual EGLint           CreateContext( EGLenum api, Config *config, Context *share, const EGLint *attrib_list, Context **ret_context );


     virtual DFBResult        CreateSurface( Config               *config,
                                             NativeHandle          native_handle,
                                             const EGLint         *attrib,
                                             Surface             **ret_surface );

     virtual EGLint           CreatePixmapSurface( Config               *config,
                                                   EGLNativePixmapType   pixmap,
                                                   const EGLint         *attrib,
                                                   Surface             **ret_surface );

     virtual EGLint           CreateWindowSurface( Config               *config,
                                                   EGLNativeWindowType   win,
                                                   const EGLint         *attrib,
                                                   Surface             **ret_surface );

     virtual EGLint           CreatePbufferSurface( Config               *config,
                                                    const EGLint         *attrib,
                                                    Surface             **ret_surface );

     virtual EGLint           CreatePbufferFromClientBuffer( EGLenum           buftype,
                                                             EGLClientBuffer   buffer,
                                                             Config           *config,
                                                             const EGLint     *attrib,
                                                             Surface         **ret_surface );

     virtual EGLint           SwapBuffers( Surface             *surface );

     virtual EGLint           CopyBuffers( Surface             *source,
                                           EGLNativePixmapType  destination );


     IDirectFB *GetDFB() const { return dfb; }





public:
     IDirectFB               *dfb;
     EGLNativeDisplayType     native_display;

     EGLenum                  native_pixmap_target;

private:
     unsigned int             refs;

     Direct::String           apis;
     Direct::String           extensions;
     Direct::String           vendor;
     Direct::String           version;


protected:

     // FIXME: Ref: use shared_ptr!
     void addRef() {
          refs++;
     }
};


/*
 * EGL::Surface for wrapping client's native handles
 */

class Surface : public Types::Type<Surface>
{
     friend class Context;
     friend class Display;

public:
     typedef std::function<DFBResult (Surface &surface)> Initialise;
     typedef std::function<EGLint (void)> SwapBuffersFunc;


     NativeHandle           native_handle;

     Config                *config;          // config
     Graphics::Config      *gfx_config;

     Graphics::Options      gfx_options;     // attribs

     IDirectFBSurface      *surface;         // Init

     Surface();

public:
     Surface( Config           *config,
              const EGLint     *attrib_list );
     virtual ~Surface();

     virtual DFBResult      Init();
     virtual DFBResult      Copy( Surface *source );

     virtual DFBSurfaceID   GetID();

     virtual EGLint         SwapBuffers();

     virtual EGLint         GetAttrib( EGLint attribute, EGLint &value );
     virtual EGLint         SetAttrib( EGLint attribute, EGLint value );

     Config                  *GetConfig() const { return config; }
     const Graphics::Options &GetOptions() const { return gfx_options; }
     IDirectFBSurface        *GetSurface() const { return surface; }
     Graphics::SurfacePeer   *GetPeer() const { return gfx_peer; }

protected:
     Graphics::SurfacePeer *gfx_peer;        // Convert

};


/*
 * EGL::Context for wrapping implementations' contexts
 */

class Context : public Types::Type<Context>
{
     friend class Display;

protected:
     Context( EGLenum       api,
              Config       *config,
              Context      *share,
              const EGLint *attrib_list );

public:
     virtual           ~Context();

     virtual DFBResult  Init();

     virtual EGLint     Bind( Surface *draw, Surface *read );
     virtual void       Unbind();

     virtual EGLint     GetAttrib( EGLint attribute, EGLint *value );
     virtual Surface   *GetSurface( EGLint which );

     virtual DFBResult  GetProcAddress( const char  *name,
                                        void       **result );

     Config            *GetConfig() const { return config; }
     Context           *GetShareContext() const { return share; }

private:
     EGLenum              api;
     Config              *config;
     Context             *share;
     Graphics::Config    *gfx_config;
public:
     Graphics::Context   *gfx_context;
private:
     Graphics::Options    gfx_options;

     class Binding {
     public:
          bool            active;
          Surface        *draw;
          Surface        *read;

          Binding()
               :
               active( false ),
               draw( NULL ),
               read( NULL )
          {
          }

          void Set( Surface *draw, Surface *read );
          void Reset();
     };


     Binding binding;
};


/*
 * EGL::Config for wrapping implementations' configs
 */

class Config : public Types::Type<Config>
{
     friend class Context;
     friend class Surface;

public:
     Config( Display          *display,
             Graphics::Config *gfx_config );
     virtual ~Config();

     virtual EGLint  GetAttrib( EGLint attribute, EGLint *value );

     Display        *GetDisplay() const { return display; }

public:
     Display             *display;
     Graphics::Config    *gfx_config;
};


/*
 * Graphics::Option implementation for EGLint value based options
 */

class Option : public Graphics::Option<long>
{
public:
     Option( const EGLint option,
             long         value );
     virtual ~Option();

     virtual Direct::String GetName() const;
     virtual Direct::String GetString() const;

     virtual const long    &GetValue() const;

private:
     const EGLint option;
     long         value;
};


/*
 * Utils
 */

class Util {
public:
     static Direct::String APIToString( EGLenum api, EGLint version );
     static EGLenum        StringToAPI( const Direct::String &api );

     static DFBResult      GetOptions( Graphics::Options &options,
                                       const EGLint      *attrib_list );

     static DFBResult      GetSurfaceAttribs( Graphics::Options   &options,
                                              std::vector<EGLint> &attribs );

     static DFBResult      GetSurfaceDescription( Graphics::Options     &options,
                                                  DFBSurfaceDescription &desc );
};

class TLS
{
     friend class Direct::TLSObject2<TLS>;


     static TLS *create( void *ctx, void *params )
     {
          return new TLS();
     }

     static void destroy( void *ctx, TLS *tls )
     {
          delete tls;
     }

     TLS();

public:
     EGLint GetError();
     void   SetError( EGLint egl_error );

     EGLenum        GetAPI();
     void           SetAPI( EGLenum api );

     Context       *GetContext();
     void           SetContext( Context *context );

private:
     EGLint         egl_error;
     EGLenum        api;
     Context       *context;
};


class Library
{
private:
     void *handle;

public:
     Library();
     virtual ~Library();

     DFBResult  Init( const Direct::String &filename, bool global = false, bool now = false );
     DFBResult  Load();
     void      *Lookup( const Direct::String &symbol );


     EGLint (*eglGetError)(void);

     EGLDisplay (*eglGetDisplay)(EGLNativeDisplayType display_id);
     EGLBoolean (*eglInitialize)(EGLDisplay dpy, EGLint *major, EGLint *minor);
     EGLBoolean (*eglTerminate)(EGLDisplay dpy);

     const char * (*eglQueryString)(EGLDisplay dpy, EGLint name);

     EGLBoolean (*eglGetConfigs)(EGLDisplay dpy, EGLConfig *configs,
                                 EGLint config_size, EGLint *num_config);
     EGLBoolean (*eglChooseConfig)(EGLDisplay dpy, const EGLint *attrib_list,
                                   EGLConfig *configs, EGLint config_size,
                                   EGLint *num_config);
     EGLBoolean (*eglGetConfigAttrib)(EGLDisplay dpy, EGLConfig config,
                                      EGLint attribute, EGLint *value);

     EGLSurface (*eglCreateWindowSurface)(EGLDisplay dpy, EGLConfig config,
                                          EGLNativeWindowType win,
                                          const EGLint *attrib_list);
     EGLSurface (*eglCreatePbufferSurface)(EGLDisplay dpy, EGLConfig config,
                                           const EGLint *attrib_list);
     EGLSurface (*eglCreatePixmapSurface)(EGLDisplay dpy, EGLConfig config,
                                          EGLNativePixmapType pixmap,
                                          const EGLint *attrib_list);
     EGLBoolean (*eglDestroySurface)(EGLDisplay dpy, EGLSurface surface);
     EGLBoolean (*eglQuerySurface)(EGLDisplay dpy, EGLSurface surface,
                                   EGLint attribute, EGLint *value);

     EGLBoolean (*eglBindAPI)(EGLenum api);
     EGLenum (*eglQueryAPI)(void);

     EGLBoolean (*eglWaitClient)(void);

     EGLBoolean (*eglReleaseThread)(void);

     EGLSurface (*eglCreatePbufferFromClientBuffer)(EGLDisplay dpy, EGLenum buftype, EGLClientBuffer buffer,
                                                    EGLConfig config, const EGLint *attrib_list);

     EGLBoolean (*eglSurfaceAttrib)(EGLDisplay dpy, EGLSurface surface,
                                    EGLint attribute, EGLint value);
     EGLBoolean (*eglBindTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);
     EGLBoolean (*eglReleaseTexImage)(EGLDisplay dpy, EGLSurface surface, EGLint buffer);


     EGLBoolean (*eglSwapInterval)(EGLDisplay dpy, EGLint interval);


     EGLContext (*eglCreateContext)(EGLDisplay dpy, EGLConfig config,
                                    EGLContext share_context,
                                    const EGLint *attrib_list);
     EGLBoolean (*eglDestroyContext)(EGLDisplay dpy, EGLContext ctx);
     EGLBoolean (*eglMakeCurrent)(EGLDisplay dpy, EGLSurface draw,
                                  EGLSurface read, EGLContext ctx);

     EGLContext (*eglGetCurrentContext)(void);
     EGLSurface (*eglGetCurrentSurface)(EGLint readdraw);
     EGLDisplay (*eglGetCurrentDisplay)(void);
     EGLBoolean (*eglQueryContext)(EGLDisplay dpy, EGLContext ctx,
                                   EGLint attribute, EGLint *value);

     EGLBoolean (*eglWaitGL)(void);
     EGLBoolean (*eglWaitNative)(EGLint engine);
     EGLBoolean (*eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface);
     EGLBoolean (*eglCopyBuffers)(EGLDisplay dpy, EGLSurface surface,
                                  EGLNativePixmapType target);

     EGLImageKHR (*eglCreateImageKHR) (EGLDisplay dpy, EGLContext ctx, EGLenum target, EGLClientBuffer buffer, const EGLint *attrib_list);
     EGLBoolean  (*eglDestroyImageKHR) (EGLDisplay dpy, EGLImageKHR image);

     /* This is a generic function pointer type, whose name indicates it must
      * be cast to the proper type *and calling convention* before use.
      */
     typedef void (*__eglMustCastToProperFunctionPointerType)(void);

     /* Now, define eglGetProcAddress using the generic function ptr. type */
     __eglMustCastToProperFunctionPointerType (*eglGetProcAddress)(const char *procname);
};


#define DFB_EGL_ATTRIB_LIST_DEBUG_AT(domain,__v)                                                                                            \
     do {                                                                                                                                   \
          if ((__v) != NULL) {                                                                                                              \
               for (const EGLint *v = (__v); *v != EGL_NONE; v++) {                                                                    \
                    D_DEBUG_AT( domain, "  -> %02ld: 0x%08x (%d) '%s'\n",                                                                   \
                                (long)(v - (__v)), *v, *v, (*v >= 0x3000 && *v < 0x4000) ? *ToString<EGL::EGLInt>( EGL::EGLInt(*v) ) : "" );\
               }                                                                                                                            \
          }                                                                                                                                 \
     } while (0)


#define DFB_EGL_RETURN(error, val) { tls->SetError( (error) ); return val; }

}



extern Direct::TLSObject2<EGL::TLS> EGLTLS;

}


#endif

#endif

