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

#include <fusion/Debug.h>

#include "Debug.h"

extern "C" {
#include <directfb_strings.h>
#include <directfb_util.h>

#include <core/core_strings.h>
#include <core/surface_buffer.h>
}

/*********************************************************************************************************************/

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
ToString<DFBSurfaceBlittingFlags>::ToString( const DFBSurfaceBlittingFlags &flags )
{
     static const DirectFBSurfaceBlittingFlagsNames(flags_names);

     for (int i=0, n=0; flags_names[i].flag; i++) {
          if (flags & flags_names[i].flag)
               PrintF( "%s%s", n++ ? "," : "", flags_names[i].name );
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
ToString<DFBSurfaceDrawingFlags>::ToString( const DFBSurfaceDrawingFlags &flags )
{
     static const DirectFBSurfaceDrawingFlagsNames(flags_names);

     for (int i=0, n=0; flags_names[i].flag; i++) {
          if (flags & flags_names[i].flag)
               PrintF( "%s%s", n++ ? "," : "", flags_names[i].name );
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

template<>
ToString<DFBSurfacePorterDuffRule>::ToString( const DFBSurfacePorterDuffRule &rule )
{
     static const DirectFBPorterDuffRuleNames(rules_names);

     for (int i=0; rules_names[i].rule; i++) {
          if (rule == rules_names[i].rule) {
               PrintF( "%s", rules_names[i].name );
               return;
          }
     }

     if (rule == DSPD_NONE)
          PrintF( "NONE" );
     else
          PrintF( "_INVALID_<0x%08x>", rule );
}


// DirectFB types

template<>
ToString<DFBDimension>::ToString( const DFBDimension &v )
{
     PrintF( "%dx%d", v.w, v.h );
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
ToString<CoreSurfaceAllocation>::ToString( const CoreSurfaceAllocation &allocation )
{
     PrintF( "{CoreSurfaceAllocation %s [%d] type:%s resid:%lu %s}",
             ToString<FusionObject>(allocation.object).buffer(),
             allocation.index,
             ToString<CoreSurfaceTypeFlags>(allocation.type).buffer(),
             allocation.resource_id,
             ToString<CoreSurfaceConfig>(allocation.config).buffer() );
}

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

/*********************************************************************************************************************/

extern "C" {


const char *
ToString_CoreSurfaceTypeFlags( CoreSurfaceTypeFlags v )
{
     return ToString<CoreSurfaceTypeFlags>( v ).CopyTLS();
}

const char *
ToString_DFBAccelerationMask( DFBAccelerationMask v )
{
     return ToString<DFBAccelerationMask>( v ).CopyTLS();
}

const char *
ToString_DFBSurfaceBlittingFlags( DFBSurfaceBlittingFlags v )
{
     return ToString<DFBSurfaceBlittingFlags>( v ).CopyTLS();
}

const char *
ToString_DFBSurfaceCapabilities( DFBSurfaceCapabilities v )
{
     return ToString<DFBSurfaceCapabilities>( v ).CopyTLS();
}

const char *
ToString_DFBSurfaceDrawingFlags( DFBSurfaceDrawingFlags v )
{
     return ToString<DFBSurfaceDrawingFlags>( v ).CopyTLS();
}

const char *
ToString_DFBSurfacePixelFormat( DFBSurfacePixelFormat v )
{
     return ToString<DFBSurfacePixelFormat>( v ).CopyTLS();
}

const char *
DFB_ToString_DFBDimension( const DFBDimension *v )
{
     return ToString<DFBDimension>( *v ).CopyTLS();
}

const char *
ToString_CoreSurfaceConfig( const CoreSurfaceConfig *v )
{
     return ToString<CoreSurfaceConfig>( *v ).CopyTLS();
}

const char *
ToString_CoreSurfaceAllocation( const CoreSurfaceAllocation *v )
{
     return ToString<CoreSurfaceAllocation>( *v ).CopyTLS();
}

const char *
ToString_CoreSurfaceBuffer( const CoreSurfaceBuffer *v )
{
     return ToString<CoreSurfaceBuffer>( *v ).CopyTLS();
}


}

