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
#include <core/layer_region.h>
#include <core/surface_buffer.h>
}

#include <core/SurfaceTask.h>

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
ToString<DFBSurfaceFlipFlags>::ToString( const DFBSurfaceFlipFlags &flags )
{
     static const DirectFBSurfaceFlipFlagsNames(flags_names);

     for (int i=0, n=0; flags_names[i].flag; i++) {
          if (flags & flags_names[i].flag)
               PrintF( "%s%s", n++ ? "," : "", flags_names[i].name );
     }
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

template<>
ToString<DFBDisplayLayerBufferMode>::ToString( const DFBDisplayLayerBufferMode &mode )
{
     switch (mode) {
          case DLBM_FRONTONLY:
               PrintF( "FRONTONLY" );
               break;

          case DLBM_BACKVIDEO:
               PrintF( "BACKVIDEO" );
               break;

          case DLBM_BACKSYSTEM:
               PrintF( "BACKSYSTEM" );
               break;

          case DLBM_TRIPLE:
               PrintF( "TRIPLE" );
               break;

          default:
               PrintF( "invalid 0x%x", mode );
               break;
     }
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

template<>
ToString<CoreSurfaceAccessFlags>::ToString( const CoreSurfaceAccessFlags &flags )
{
     #define CORE_SURFACE_ACCESS_FLAG_PRINTF( __F )              \
          D_FLAG_PRINTFn( n, flags, CSAF_, __F )

     if (flags) {
          size_t n = 0;

          CORE_SURFACE_ACCESS_FLAG_PRINTF( READ );
          CORE_SURFACE_ACCESS_FLAG_PRINTF( WRITE );
          CORE_SURFACE_ACCESS_FLAG_PRINTF( SHARED );
          CORE_SURFACE_ACCESS_FLAG_PRINTF( CACHE_INVALIDATE );
          CORE_SURFACE_ACCESS_FLAG_PRINTF( CACHE_FLUSH );
     }
     else
          PrintF( "<NONE>" );
}


// CoreLayer types

template<>
ToString<CoreLayerRegionConfig>::ToString( const CoreLayerRegionConfig &config )
{
     PrintF( "size:%dx%d format:%s surface_caps:%s buffermode:%s",
             config.width, config.height,
             *ToString<DFBSurfacePixelFormat>(config.format),
             *ToString<DFBSurfaceCapabilities>(config.surface_caps),
             *ToString<DFBDisplayLayerBufferMode>(config.buffermode) );
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

template<>
ToString<CoreSurface>::ToString( const CoreSurface &surface )
{
     PrintF( "{CoreSurface %s [%d] buffers:%d type:%s resid:%lu %s}",
             *ToString<FusionObject>(surface.object),
             surface.clients.count, surface.num_buffers,
             *ToString<CoreSurfaceTypeFlags>(surface.type),
             surface.resource_id,
             *ToString<CoreSurfaceConfig>(surface.config) );
}

template<>
ToString<CoreSurfaceBufferLock>::ToString( const CoreSurfaceBufferLock &lock )
{
     PrintF( "accessor:0x%02x access:%s buffer:%p allocation:%p addr:%p phys:0x%08lx offset:%lu pitch:%u handle:%p",
             lock.accessor,
             *ToString<CoreSurfaceAccessFlags>(lock.access),
             lock.buffer,
             lock.allocation,
             lock.addr,
             lock.phys,
             lock.offset,
             lock.pitch,
             lock.handle );
}

template<>
ToString<DirectFB::Task>::ToString( const DirectFB::Task &task )
{
     task.Describe( *this );
}

template<>
ToString<DirectFB::TaskState>::ToString( const DirectFB::TaskState &state )
{
     switch (state) {
          case DirectFB::TASK_STATE_NONE:
               PrintF( "<NONE>" );
               break;

          case DirectFB::TASK_NEW:
               PrintF( "NEW" );
               break;

          case DirectFB::TASK_FLUSHED:
               PrintF( "FLUSHED" );
               break;

          case DirectFB::TASK_READY:
               PrintF( "READY" );
               break;

          case DirectFB::TASK_RUNNING:
               PrintF( "RUNNING" );
               break;

          case DirectFB::TASK_DONE:
               PrintF( "DONE" );
               break;

          case DirectFB::TASK_DEAD:
               PrintF( "DEAD" );
               break;

          case DirectFB::TASK_INVALID:
               PrintF( "INVALID" );
               break;

          case DirectFB::TASK_STATE_ALL:
               PrintF( "<ALL>" );
               break;

          default:
               PrintF( "invalid 0x%x", state );
               break;
     }
}

template<>
ToString<DirectFB::TaskFlags>::ToString( const DirectFB::TaskFlags &flags )
{
     #define TASK_FLAG_PRINTF( __F )                   \
          D_FLAG_PRINTFn( n, flags, DirectFB::TASK_FLAG_, __F )

     if (flags) {
          size_t n = 0;

          TASK_FLAG_PRINTF( NOSYNC );
          TASK_FLAG_PRINTF( EMITNOTIFIES );
          TASK_FLAG_PRINTF( CACHE_FLUSH );
          TASK_FLAG_PRINTF( CACHE_INVALIDATE );
          TASK_FLAG_PRINTF( NEED_SLAVE_PUSH );
          TASK_FLAG_PRINTF( LAST_IN_QUEUE );
     }
     else
          PrintF( "<NONE>" );
}

template<>
ToString<DirectFB::SurfaceAllocationAccess>::ToString( const DirectFB::SurfaceAllocationAccess &access )
{
     CORE_SURFACE_ALLOCATION_ASSERT( access.allocation );

     PrintF( "allocation:%p task_count:%d access:%s\n",
             access.allocation, access.allocation->task_count,
             *ToString<CoreSurfaceAccessFlags>(access.flags) );
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
ToString_DFBSurfaceFlipFlags( DFBSurfaceFlipFlags v )
{
     return ToString<DFBSurfaceFlipFlags>( v ).CopyTLS();
}

const char *
ToString_DFBDimension( const DFBDimension *v )
{
     return ToString<DFBDimension>( *v ).CopyTLS();
}

const char *
ToString_CoreSurfaceConfig( const CoreSurfaceConfig *v )
{
     return ToString<CoreSurfaceConfig>( *v ).CopyTLS();
}

const char *
ToString_CoreLayerRegionConfig( const CoreLayerRegionConfig *v )
{
     return ToString<CoreLayerRegionConfig>( *v ).CopyTLS();
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

const char *
ToString_CoreSurface( const CoreSurface *v )
{
     return ToString<CoreSurface>( *v ).CopyTLS();
}

const char *
ToString_Task( const DFB_Task *v )
{
     return ToString<DFB_Task>( *v ).CopyTLS();
}


}

