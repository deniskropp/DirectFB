/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#include <stdarg.h>

#define EGL_EGLEXT_PROTOTYPES
#define GL_GLEXT_PROTOTYPES

#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>

#include <directfbgl2.h>

#include <direct/debug.h>

#include <core/surface.h>

#include <display/idirectfbsurface.h>


D_DEBUG_DOMAIN( IDFBGL_Android, "IDirectFBGL2/Android", "IDirectFBGL2 Android Implementation" );

static DirectResult
Probe( void *ctx, ... );

static DirectResult
Construct( void *interface, ... );

#include <direct/interface_implementation.h>

DIRECT_INTERFACE_IMPLEMENTATION( IDirectFBGL2, Android )

/*
 * private data struct of IDirectFBGL2
 */
typedef struct {
     int                      ref;       /* reference counter */

     CoreDFB                 *core;
} IDirectFBGL2_data;


static void
IDirectFBGL2_Destruct( IDirectFBGL2 *thiz )
{
//     IDirectFBGL2_data *data = thiz->priv;

     DIRECT_DEALLOCATE_INTERFACE( thiz );
}

static DirectResult
IDirectFBGL2_AddRef( IDirectFBGL2 *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL2);

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDirectFBGL2_Release( IDirectFBGL2 *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL2)

     if (--data->ref == 0)
          IDirectFBGL2_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDirectFBGL2_CreateContext( IDirectFBGL2                    *thiz,
                            const DFBGL2ContextDescription  *desc,
                            IDirectFBGL2Context            **ret_context )
{
     DFBResult ret;

     DIRECT_INTERFACE_GET_DATA (IDirectFBGL2);

     D_DEBUG_AT( IDFBGL_Android, "%s()\n", __FUNCTION__ );

     (void) desc;

     DirectInterfaceFuncs *funcs = NULL;

     ret = DirectGetInterface( &funcs, "IDirectFBGL2Context", "Android", DirectProbeInterface, NULL );
     if (ret)
          return ret;

     IDirectFBGL2Context *context;

     ret = funcs->Allocate( (void**) &context );
     if (ret)
          return ret;

     /* Construct the interface. */
     ret = funcs->Construct( context, data->core );
     if (ret)
          return ret;

     *ret_context = context;

     return DFB_OK;
}

static DFBResult
IDirectFBGL2_GetProcAddress( IDirectFBGL2  *thiz,
                             const char    *name,
                             void         **ret_address )
{
     void *handle;
     
     DIRECT_INTERFACE_GET_DATA (IDirectFBGL2);

     D_DEBUG_AT( IDFBGL_Android, "%s()\n", __FUNCTION__ );

     if (!name)
          return DFB_INVARG;
          
     if (!ret_address)
          return DFB_INVARG;
          
     handle = (void*) eglGetProcAddress( name );
     if (!handle)
          return DFB_FAILURE;

     *ret_address = handle;
     
     return (*ret_address) ? DFB_OK : DFB_UNSUPPORTED;
}

static DFBResult
IDirectFBGL2_TextureSurface( IDirectFBGL2     *thiz,
                             int               target,
                             int               level,
                             IDirectFBSurface *surface )
{
     DFBResult              ret;
     IDirectFBSurface_data *surface_data;
     CoreSurface           *core_surface;
     CoreSurfaceBufferLock  lock;

     DIRECT_INTERFACE_GET_DATA (IDirectFBGL2);

     D_DEBUG_AT( IDFBGL_Android, "%s( %d, %d, %p )\n", __FUNCTION__, target, level, (void *)surface );

     if (level)
          return DFB_UNSUPPORTED;

     DIRECT_INTERFACE_GET_DATA_FROM (surface, surface_data, IDirectFBSurface);

     core_surface = surface_data->surface;
     if (!core_surface)
          return DFB_DESTROYED;


     /* Lock buffer */
     ret = dfb_surface_lock_buffer( core_surface, CSBR_FRONT, CSAID_ACCEL1, CSAF_READ, &lock );
     if (ret) {
          D_DERROR( ret, "IDirectFBGL2/Android: Could not lock texture buffer!\n" );
          return ret;
     }

     glEGLImageTargetTexture2DOES( target, lock.handle );

     dfb_surface_unlock_buffer( core_surface, &lock );

     return DFB_OK;
}

/* exported symbols */

static DirectResult
Probe( void *ctx, ... )
{
     D_DEBUG_AT( IDFBGL_Android, "%s()\n", __FUNCTION__ );

     (void) ctx;

     /* ... */

     return DFB_OK;
}

static DirectResult
Construct( void *interface, ... )
{
     IDirectFB    *dfb;
     IDirectFBGL2 *thiz = interface;
     CoreDFB      *core;

     D_DEBUG_AT( IDFBGL_Android, "%s()\n", __FUNCTION__ );

     va_list tag;
     va_start(tag, interface);
     dfb = va_arg(tag, IDirectFB *);
     core = va_arg(tag, CoreDFB *);
     va_end( tag );

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IDirectFBGL2 );

     /* Initialize interface data. */
     data->core = core;
     data->ref  = 1;

     /* Assign interface pointers. */
     thiz->AddRef         = IDirectFBGL2_AddRef;
     thiz->Release        = IDirectFBGL2_Release;
     thiz->CreateContext  = IDirectFBGL2_CreateContext;
     thiz->GetProcAddress = IDirectFBGL2_GetProcAddress;
     thiz->TextureSurface = IDirectFBGL2_TextureSurface;

     return DFB_OK;
}

