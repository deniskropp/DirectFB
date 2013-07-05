/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <stdarg.h>

#include <dlfcn.h>

#include <directfbgl.h>

#include <direct/debug.h>

#include <core/surface.h>

#include <display/idirectfbsurface.h>

D_DEBUG_DOMAIN( IDFBGL_GLX, "IDirectFBGL/GLX", "IDirectFBGL GLX Implementation" );

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBGL, GLX )

/*
 * private data struct of IDirectFBGL
 */
typedef struct {
     int                      ref;       /* reference counter */

     IDirectFBSurface        *surface;
     CoreSurface             *core_surface;

     DFBGLAttributes          attributes;


     bool                     locked;

     CoreSurfaceBufferLock    lock;
} IDirectFBGL_data;


static void
IDirectFBGL_Destruct( IDirectFBGL *thiz )
{
     IDirectFBGL_data *data = thiz->priv;

     if (data->locked)
          dfb_surface_unlock_buffer( data->core_surface, &data->lock );

     data->surface->Release( data->surface );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBGL_AddRef( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBGL_Release( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL)

     if (--data->ref == 0)
          IDirectFBGL_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBGL_Lock( IDirectFBGL *thiz )
{
     DFBResult    ret;
     CoreSurface *surface;

     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     if (data->locked)
          return DFB_LOCKED;

     surface = data->core_surface;
     D_ASSERT( surface != NULL );

     /* Lock destination buffer */
     ret = dfb_surface_lock_buffer( surface, CSBR_BACK, CSAID_ACCEL1, CSAF_READ | CSAF_WRITE, &data->lock );
     if (ret) {
          D_DERROR( ret, "IDirectFBGL/GLX: Could not lock destination buffer!\n" );
          return ret;
     }

     data->locked = true;

     return DFB_OK;
}

static DFBResult
IDirectFBGL_Unlock( IDirectFBGL *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     if (!data->locked)
          return DFB_BUFFEREMPTY;

     dfb_surface_unlock_buffer( data->core_surface, &data->lock );

     data->locked = false;

     return DFB_OK;
}

static DFBResult
IDirectFBGL_GetAttributes( IDirectFBGL     *thiz,
                           DFBGLAttributes *attributes )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     if (!attributes)
          return DFB_INVARG;

     *attributes = data->attributes;

     return DFB_OK;
}

static DFBResult
IDirectFBGL_GetProcAddress( IDirectFBGL  *thiz,
                            const char   *name,
                            void        **ret_address )
{
     void *handle;

     DIRECT_INTERFACE_GET_DATA (IDirectFBGL);

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     if (!name)
          return DFB_INVARG;

     if (!ret_address)
          return DFB_INVARG;

     handle = dlopen( NULL, RTLD_LAZY );
     if (!handle)
          return DFB_FAILURE;

     *ret_address = dlsym( handle, name );

     dlclose( handle );

     return (*ret_address) ? DFB_OK : DFB_UNSUPPORTED;
}

/* exported symbols */

static DirectResult
Probe( void *ctx, ... )
{
     IDirectFBSurface       *surface = ctx;
     IDirectFBSurface_data  *surface_data;
     DFBSurfaceCapabilities  caps;

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     if (!surface)
          return DFB_UNSUPPORTED;

     surface->GetCapabilities( surface, &caps );

     if (caps & DSCAPS_SYSTEMONLY) {
          D_DEBUG_AT( IDFBGL_GLX, "  -> SYSTEM ONLY!\n" );
          return DFB_UNSUPPORTED;
     }

     surface_data = surface->priv;
     if (!surface_data)
          return DFB_DEAD;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     DFBResult              ret;
     IDirectFBGL           *thiz = interface;
     IDirectFBSurface      *surface;
     IDirectFBSurface_data *surface_data;
     CoreSurface           *core_surface;

     D_DEBUG_AT( IDFBGL_GLX, "%s()\n", __FUNCTION__ );

     va_list tag;
     va_start(tag, interface);
     surface = va_arg(tag, IDirectFBSurface*);
     va_end( tag );

     surface_data = surface->priv;
     if (!surface_data) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_DEAD;
     }

     core_surface = surface_data->surface;
     if (!core_surface) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_DESTROYED;
     }

     D_DEBUG_AT( IDFBGL_GLX, "  -> increasing surface ref count...\n" );

     /* Increase target reference counter. */
     ret = surface->AddRef( surface );
     if (ret) {
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return ret;
     }

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBGL );

     /* Initialize interface data. */
     data->ref          = 1;
     data->surface      = surface;
     data->core_surface = core_surface;

     /* Initialize attributes. */
     data->attributes.double_buffer = !!(core_surface->config.caps & DSCAPS_FLIPPING);
     data->attributes.buffer_size   = DFB_BITS_PER_PIXEL( core_surface->config.format );
     data->attributes.alpha_size    = DFB_ALPHA_BITS_PER_PIXEL( core_surface->config.format );

     switch (core_surface->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
               data->attributes.red_size   = 8;
               data->attributes.green_size = 8;
               data->attributes.blue_size  = 8;
               break;

          default:
               D_UNIMPLEMENTED();
     }

     /* Assign interface pointers. */
     thiz->AddRef         = IDirectFBGL_AddRef;
     thiz->Release        = IDirectFBGL_Release;
     thiz->Lock           = IDirectFBGL_Lock;
     thiz->Unlock         = IDirectFBGL_Unlock;
     thiz->GetAttributes  = IDirectFBGL_GetAttributes;
     thiz->GetProcAddress = IDirectFBGL_GetProcAddress;

     return DFB_OK;
}

