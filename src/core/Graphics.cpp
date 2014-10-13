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

#include <directfb++.h>

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <core/Graphics.h>

#include <core/CoreDFB.h>
#include <core/CoreSurface.h>


D_LOG_DOMAIN( DirectFB_Graphics, "DirectFB/Graphics", "DirectFB Graphics" );


namespace DirectFB {

namespace Graphics {


/**********************************************************************************************************************/

Config::Config( Implementation *implementation )
     :
     implementation( implementation )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
Config::GetOption( const Direct::String &name,
                   long                 &value )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, '%s' )\n",
                 __FUNCTION__, this, *name );

     return DFB_UNSUPPORTED;
}

Direct::String
Config::GetOption( const Direct::String &name )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, '%s' )\n",
                 __FUNCTION__, this, *name );

     long v = 0;

     GetOption( name, v );

     return Direct::String::F( "%ld", v );
}

DFBResult
Config::CheckOptions( const Graphics::Options &options )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, options %p )\n",
                 __FUNCTION__, this, &options );

     return DFB_OK;
}

DFBResult
Config::CreateContext( const Direct::String  &api,
                       Context               *share,
                       Options               *options,
                       Context              **ret_context )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, api '%s', share %p, options %p )\n",
                 __FUNCTION__, this, *api, share, options );

     return DFB_UNSUPPORTED;
}

DFBResult
Config::CreateSurfacePeer( CoreSurface  *surface,
                           Options      *options,
                           SurfacePeer **ret_peer )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     DFBResult    ret;
     SurfacePeer *peer = new SurfacePeer( this, options, surface );

     ret = peer->Init();
     if (ret) {
          delete peer;
          return ret;
     }

     *ret_peer = peer;

     return DFB_OK;
}

void
Config::DumpValues( std::initializer_list<Direct::String>  names,
                    Direct::String                        &out_str )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p )\n", __FUNCTION__, this );

     for (auto name = names.begin(); name != names.end(); name++) {
          long value;

          if (GetOption( *name, value ) == DFB_OK)
               out_str.PrintF( "%s%s:0x%lx", out_str.empty() ? "" : ",", **name, value );
     }
}

/**********************************************************************************************************************/

Context::Context( const Direct::String  &api,
                  Config                *config,
                  Context               *share,
                  Options               *options )
     :
     api( api ),
     config( config ),
     share( share ),
     options( options )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Context::%s( %p, api '%s', config %p, share %p, options %p )\n",
                 __FUNCTION__, this, *api, config, share, options );

     D_ASSERT( config != NULL );
     D_ASSERT( options != NULL );
}

DFBResult
Context::GetOption( const Direct::String &name,
                    long                 &value )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Context::%s( %p, '%s' )\n",
                 __FUNCTION__, this, *name );

     if (options->Get<long>( name, value ))
          return DFB_OK;

     return config->GetOption( name, value );
}

DFBResult
Context::GetProcAddress( const Direct::String  &name,
                         void                 *&addr )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Context::%s( %p, name '%s' )\n",
                 __FUNCTION__, this, *name );

     return DFB_ITEMNOTFOUND;
}

DFBResult
Context::CreateTask( Graphics::RenderTask *&task )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Context::%s( %p )\n",  __FUNCTION__, this );

     return DFB_UNSUPPORTED;
}

/**********************************************************************************************************************/

SurfacePeer::SurfacePeer( Config      *config,
                          Options     *options,
                          CoreSurface *surface )
     :
     config( config ),
     options( options ),
     surface( surface ),
     flips( 0 ),
     index( 0 ),
     buffer_num( 0 )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, config %p, surface %p )\n",
                 __FUNCTION__, this, config, surface );

     direct_serial_init( &surface_serial );

     memset( buffer_ids, 0, sizeof(*buffer_ids) );
}

SurfacePeer::~SurfacePeer()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     if (surface)
          dfb_surface_unref( surface );
}

DFBResult
SurfacePeer::Init()
{
     DFBResult ret = DFB_OK;

     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p ) <- surface %p\n",
                 __FUNCTION__, this, surface );

     if (surface) {
          ret = (DFBResult) dfb_surface_ref( surface );
          if (ret) {
               D_DERROR( ret, "Graphics/SurfacePeer: dfb_surface_ref() failed!\n" );
               surface = NULL;
               return ret;
          }

          surface_type = surface->type;

          ret = updateBuffers();
     }

     return ret;
}

DFBResult
SurfacePeer::GetOption( const Direct::String &name,
                        long                 &value )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, '%s' )\n",
                 __FUNCTION__, this, *name );

     if (options->Get<long>( name, value ))
          return DFB_OK;

     return config->GetOption( name, value );
}

DFBResult
SurfacePeer::Flip( const DFBRegion     *region,
                   DFBSurfaceFlipFlags  flags,
                   long long            timestamp )
{
     DFBResult ret;

     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, region %p, flags 0x%08x, timestamp %lld )\n",
                 __FUNCTION__, this, region, flags, timestamp );

     Dispatch< SurfacePeer::Flush >( "Flush", this );

     D_MAGIC_ASSERT( surface, CoreSurface );

     if (0) {
          DFBSurfaceEvent event;

          event.clazz        = DFEC_SURFACE;
          event.type         = DSEVT_FRAME;
          event.surface_id   = surface->object.id;
          event.flip_flags   = flags;
          event.time_stamp   = timestamp;
          event.left_id      = buffer_left();
          event.left_serial  = surface->serial.value;
          event.right_id     = buffer_right();
          event.right_serial = event.left_serial;
          event.update       = DFBRegion( surface_config.size );
          event.update_right = event.update;

          ret = (DFBResult) dfb_surface_dispatch_channel( surface, CSCH_EVENT, &event, sizeof(DFBSurfaceEvent), NULL );
          if (ret)
               return ret;
     }
     else {
          ret = ::CoreSurface_DispatchUpdate( surface, DFB_FALSE, region, region, flags, timestamp, flips );
          if (ret)
               D_DERROR( ret, "Graphics/SurfacePeer: CoreSurface::DispatchUpdate() failed!\n" );
     }

     flips++;

     return updateBuffers();
}

DFBResult
SurfacePeer::updateBuffers()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p ) <- surface %p\n", __FUNCTION__, this, surface );

     if (direct_serial_update( &surface_serial, &surface->config_serial )) {
          DFBResult ret;

          surface_config = surface->config;

          ret = ::CoreSurface_GetBuffers( surface, &buffer_ids[0], D_ARRAY_SIZE(buffer_ids), &buffer_num );
          if (ret) {
               D_DERROR( ret, "Graphics/SurfacePeer: CoreSurface::GetBuffers() failed!\n" );
               return ret;
          }

          for (size_t i=0; i<buffer_num; i++)
               buffer_objects[i] = NULL;
     }

     if (surface_config.caps & DSCAPS_STEREO) {
          D_ASSERT( buffer_num > 1 );

          index = (flips % (buffer_num/2)) * 2;
     }
     else {
          D_ASSERT( buffer_num > 0 );

          index = flips % buffer_num;
     }

     return DFB_OK;
}

DirectFB::Util::FusionObjectWrapper<CoreSurfaceBuffer> &
SurfacePeer::getBuffer( int offset )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, offset %d ) <- surface %p\n", __FUNCTION__, this, offset, surface );

     u32 index;

     if (surface_config.caps & DSCAPS_STEREO) {
          D_ASSERT( buffer_num > 1 );

          index = ((flips + offset) % (buffer_num/2)) * 2;
     }
     else {
          D_ASSERT( buffer_num > 0 );

          index = (flips + offset) % buffer_num;
     }

     if (!buffer_objects[index]) {
          DFBResult          ret;
          CoreSurfaceBuffer *buffer;

          ret = ::CoreDFB_GetSurfaceBuffer( core_dfb, buffer_ids[index], &buffer );
          if (ret)
               D_DERROR( ret, "DirectFB/SurfacePeer: CoreDFB_GetSurfaceBuffer( 0x%x ) failed!\n", buffer_ids[index] );
          else {
               buffer_objects[index] = buffer;

               dfb_surface_buffer_unref( buffer );
          }
     }

     return buffer_objects[index];
}

/**********************************************************************************************************************/

Implementation::Implementation( std::shared_ptr<Core> core )
     :
     Direct::Module( core->modules ),
     core( core ),
     name( "unnamed" )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Implementation::%s( %p )\n", __FUNCTION__, this );
}

Implementation::~Implementation()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Implementation::%s( %p )\n", __FUNCTION__, this );
}

/**********************************************************************************************************************/

Core::Core()
     :
     modules( "graphics" )
{
}

Core::~Core()
{
}

void
Core::RegisterImplementation( Implementation *implementation )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Core::%s( implementation %p )\n",
                 __FUNCTION__, implementation );

     implementations.push_back( implementation );
}

void
Core::UnregisterImplementation( Implementation *implementation )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Core::%s( implementation %p )\n",
                 __FUNCTION__, implementation );

     implementations.remove( implementation );
}


}

}

