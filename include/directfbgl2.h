/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECTFBGL2_H__
#define __DIRECTFBGL2_H__

#include <directfb.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * The DirectFBGL2 interface version.
 */
#define DIRECTFBGL2_INTERFACE_VERSION  1


/*
 * Flags for a context description
 */
typedef enum {
     DGL2CDF_NONE        = 0x00000000,

     DGL2CDF_CAPS        = 0x00000001,
     DGL2CDF_DEPTH_BITS  = 0x00000002,

     DGL2CDF_ALL         = 0x00000003,
} DFBGL2ContextDescriptionFlags;

/*
 * Capabilities of a context
 */
typedef enum {
     DGL2CC_NONE         = 0x00000000,

     DGL2CC_STEREO       = 0x00000001,

     DGL2CC_ALL          = 0x00000001,
} DFBGL2ContextCapabilities;

/* 
 * Description of an OpenGL context
 */
typedef struct {
     DFBGL2ContextDescriptionFlags flags;

     DFBGL2ContextCapabilities     caps;
     int                           depth_bits;
} DFBGL2ContextDescription;


/****************
 * IDirectFBGL2 *
 ****************/

/*
 * This module allows creation and control of rendering contexts
 */
D_DEFINE_INTERFACE( IDirectFBGL2,

   /** Contexts **/

     /*
      * Creates a new rendering context. 
      * 
      * Contexts are sharing objects with contexts created from the same interface.
      */
     DFBResult (*CreateContext) (
          IDirectFBGL2                    *thiz,
          const DFBGL2ContextDescription  *desc,
          IDirectFBGL2Context            **ret_context
     );


   /** Interface **/

     /*
      * Get the address of an OpenGL function. 
      * 
      * The function can be called when one of the contexts from this interface is bound.
      */
     DFBResult (*GetProcAddress) (
          IDirectFBGL2                    *thiz,
          const char                      *name,
          void                           **ret_address
     );


   /** Textures **/

     /*
      * Bind the surface buffer to the texture object. 
      * 
      * The object must be created in one of the contexts from this interface. 
      */
     DFBResult (*TextureSurface) (
          IDirectFBGL2                    *thiz,
          int                              target,
          int                              level,
          IDirectFBSurface                *surface
     );
)


/***********************
 * IDirectFBGL2Context *
 ***********************/

/*
 * This is a rendering context
 */
D_DEFINE_INTERFACE( IDirectFBGL2Context,

   /** Binding **/

     /*
      * Binds the rendering context to surface buffers.
      */
     DFBResult (*Bind) (
          IDirectFBGL2Context             *thiz,
          IDirectFBSurface                *draw,
          IDirectFBSurface                *read
     );

     /*
      * Unbinds the rendering context if bound.
      */
     DFBResult (*Unbind) (
          IDirectFBGL2Context             *thiz
     );
)


#ifdef __cplusplus
}
#endif

#endif

