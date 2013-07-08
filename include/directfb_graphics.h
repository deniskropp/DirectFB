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



#ifndef __DIRECTFB_GRAPHICS_H__
#define __DIRECTFB_GRAPHICS_H__

#include <directfb.h>
#include <directfb_water.h>

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * The DirectFB Graphics interface version.
 */
#define DIRECTFB_GRAPHICS_INTERFACE_VERSION  1


/*
 * Window stack extension.
 */
DECLARE_INTERFACE( IDirectFBGraphics )


typedef u32 DFBGraphicsEngineID;


typedef struct {
     bool                     software;
     unsigned int             cores;
     DFBAccelerationMask      clipping;
     DFBSurfaceRenderOptions  render_options;
     unsigned int             max_scale_down_x;
     unsigned int             max_scale_down_y;
     unsigned int             max_operations;
     WaterTransformType       transforms;
} DFBGraphicsEngineCapabilities;


typedef enum {
     DGEDF_NONE     = 0x00000000,

     DGEDF_CAPS     = 0x00000001,

     DGEDF_ALL      = 0x00000001,
} DFBGraphicsEngineDescriptionFlags;

#define DFB_GRAPHICS_ENGINE_DESC_NAME_LENGTH 64

typedef struct {
     DFBGraphicsEngineDescriptionFlags       flags;

     char                                    name[DFB_GRAPHICS_ENGINE_DESC_NAME_LENGTH];

     DFBGraphicsEngineCapabilities           caps;
} DFBGraphicsEngineDescription;


typedef enum {
     DGESF_NONE     = 0x00000000,

     DGESF_USAGE    = 0x00000001,

     DGESF_ALL      = 0x00000001,
} DFBGraphicsEngineStatusFlags;

typedef struct {
     DFBGraphicsEngineStatusFlags            flags;
     u32                                     cores;    // cores covered by this status, e.g. all or a single one

     u32                                     usage;    // 16.16 percentage
} DFBGraphicsEngineStatus;


/*
 * Called for each existing graphics engine.
 */
typedef DFBEnumerationResult (*DFBGraphicsEngineCallback) (
     void                               *context,
     DFBGraphicsEngineID                 engine_id,
     const DFBGraphicsEngineDescription *desc
);

/********************
 * IDirectFBGraphics *
 ********************/

/*
 * <i>No summary yet...</i>
 */
DEFINE_INTERFACE(   IDirectFBGraphics,

   /** Register **/

     /*
      * Registers a new graphics engine.
      */
     DFBResult (*RegisterEngine) (
          IDirectFBGraphics         *thiz,
          void                      *engine
     );

     /*
      * Unregisters a graphics engine.
      */
     DFBResult (*UnregisterEngine) (
          IDirectFBGraphics         *thiz,
          void                      *engine
     );


   /** Enumerate **/

     /*
      * Enumerates all registered graphics engines.
      */
     DFBResult (*EnumEngines) (
          IDirectFBGraphics         *thiz,
          DFBGraphicsEngineCallback  callback,
          void                      *context
     );


   /** Status **/

     /*
      * Returns status of graphics engine.
      */
     DFBResult (*GetEngineStatus) (
          IDirectFBGraphics         *thiz,
          DFBGraphicsEngineID        engine_id,
          unsigned int               core_index,
          DFBGraphicsEngineStatus   *ret_status
     );
)


#ifdef __cplusplus
}
#endif

#endif

