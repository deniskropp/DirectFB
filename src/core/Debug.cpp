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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include "Debug.h"

extern "C" {
#include <directfb_strings.h>
#include <directfb_util.h>

#include <core/core_strings.h>
#include <core/surface_buffer.h>
}

#include <direct/String.h>

/*********************************************************************************************************************/

namespace DirectFB {

namespace Debug {


// Fusion types

template<>
ToString<FusionObject>::ToString( const FusionObject &object )
{
     PrintF( "%p:%u/0x%x-%lu/%lu", &object, object.id, object.ref.multi.id, object.identity, object.owner );
}


// Core enums

template<>
ToString<CoreSurfaceTypeFlags>::ToString( const CoreSurfaceTypeFlags &flags )
{
     static const DirectFBCoreSurfaceTypeFlagsNames(flags_names);

     for (int i=0, n=0; flags_names[i].flag; i++) {
          if (flags & flags_names[i].flag)
               PrintF( "%s%s", n++ ? "," : "", flags_names[i].name );
     }
}


// DirectFB enums

template<>
ToString<DFBAccelerationMask>::ToString( const DFBAccelerationMask &accel )
{
     static const DirectFBAccelerationMaskNames(accelerationmask_names);

     for (int i=0, n=0; accelerationmask_names[i].mask; i++) {
          if (accel & accelerationmask_names[i].mask)
               PrintF( "%s%s", n++ ? "," : "", accelerationmask_names[i].name );
     }
}

template<>
ToString<DFBSurfaceCapabilities>::ToString( const DFBSurfaceCapabilities &caps )
{
     static const DirectFBSurfaceCapabilitiesNames(caps_names);

     for (int i=0, n=0; caps_names[i].capability; i++) {
          if (caps & caps_names[i].capability)
               PrintF( "%s%s", n++ ? "," : "", caps_names[i].name );
     }
}

template<>
ToString<DFBSurfacePixelFormat>::ToString( const DFBSurfacePixelFormat &format )
{
     for (int i=0; dfb_pixelformat_names[i].format; i++) {
          if (format == dfb_pixelformat_names[i].format) {
               PrintF( "%s", dfb_pixelformat_names[i].name );
               return;
          }
     }

     PrintF( "_INVALID_<0x%08x>", format );
}


// CoreSurface types

template<>
ToString<CoreSurfaceConfig>::ToString( const CoreSurfaceConfig &config )
{
     int buffers = 0;

     if (config.caps & DSCAPS_TRIPLE)
          buffers = 3;
     else if (config.caps & DSCAPS_DOUBLE)
          buffers = 2;
     else
          buffers = 1;

     PrintF( "size:%dx%d format:%s caps:%s bufs:%d",
             config.size.w, config.size.h,
             ToString<DFBSurfacePixelFormat>(config.format).buffer(),
             ToString<DFBSurfaceCapabilities>(config.caps).buffer(),
             buffers );
}


// CoreSurface objects

template<>
ToString<CoreSurfaceBuffer>::ToString( const CoreSurfaceBuffer &buffer )
{
     PrintF( "{CoreSurfaceBuffer %s [%d] allocs:%d type:%s resid:%lu %s}",
             ToString<FusionObject>(buffer.object).buffer(),
             buffer.index, buffer.allocs.count,
             ToString<CoreSurfaceTypeFlags>(buffer.type).buffer(),
             buffer.resource_id,
             ToString<CoreSurfaceConfig>(buffer.config).buffer() );
}


}

}

