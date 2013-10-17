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



#ifndef ___DirectFB__Debug__H___
#define ___DirectFB__Debug__H___

#include <directfb.h>    // include here to prevent it being included indirectly causing nested extern "C"

#ifdef __cplusplus
extern "C" {
#endif

#include <core/surface.h>

const char *ToString_CoreSurfaceTypeFlags     ( CoreSurfaceTypeFlags                  flags );

const char *ToString_DFBAccelerationMask      ( DFBAccelerationMask                   accel );
const char *ToString_DFBSurfaceBlittingFlags  ( DFBSurfaceBlittingFlags               flags );
const char *ToString_DFBSurfaceCapabilities   ( DFBSurfaceCapabilities                caps );
const char *ToString_DFBSurfaceDrawingFlags   ( DFBSurfaceDrawingFlags                flags );
const char *ToString_DFBSurfacePixelFormat    ( DFBSurfacePixelFormat                 format );
const char *ToString_DFBSurfaceFlipFlags      ( DFBSurfaceFlipFlags                   flags );

const char *ToString_DFBDimension             ( const DFBDimension                   *dimension );

const char *ToString_CoreSurfaceConfig        ( const CoreSurfaceConfig              *config );
const char *ToString_CoreLayerRegion          ( const CoreLayerRegion                *region );
const char *ToString_CoreLayerRegionConfig    ( const CoreLayerRegionConfig          *config );

const char *ToString_CoreSurfaceAllocation    ( const CoreSurfaceAllocation          *allocation );
const char *ToString_CoreSurfaceBuffer        ( const CoreSurfaceBuffer              *buffer );
const char *ToString_CoreSurface              ( const CoreSurface                    *surface );

const char *ToString_Task                     ( const DFB_Task                       *task );


#ifdef __cplusplus
}


#include <direct/String.h>
#include <direct/ToString.h>


#endif // __cplusplus

#endif

