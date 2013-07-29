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

extern "C" {
#include <direct/messages.h>
}

#include <direct/ToString.h>

#include <core/Graphics.h>


D_LOG_DOMAIN( DirectFB_Graphics, "DirectFB/Graphics", "DirectFB Graphics" );


namespace DirectFB {

namespace Graphics {


//D_TYPE_DEFINE_( Core::Type<Context>, Context );
//D_TYPE_DEFINE_( Core::Type<Image>, Image );


/**********************************************************************************************************************/

DFBResult
Config::GetOption( const Direct::String &name,
                   long                 &value )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Config::%s( %p, '%s' )\n", 
                 __FUNCTION__, this, *name );

     return DFB_UNSUPPORTED;
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
Config::CreateSurfacePeer( IDirectFBSurface  *surface,
                           Options           *options,
                           SurfacePeer      **ret_peer )
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

/**********************************************************************************************************************/

SurfacePeer::SurfacePeer( Config           *config,
                          Options          *options,
                          IDirectFBSurface *surface )
     :
     config( config ),
     options( options ),
     surface( surface )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, config %p, surface %p )\n",
                 __FUNCTION__, this, config, surface );
}

SurfacePeer::~SurfacePeer()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, surface %p )\n",
                 __FUNCTION__, this, surface );

     if (surface)
          surface->Release( surface );
}

DFBResult
SurfacePeer::Init()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p ) <- surface %p\n",
                 __FUNCTION__, this, surface );

     if (surface) {
          DFBResult ret;

          ret = (DFBResult) surface->AddRef( surface );
          if (ret) {
               D_DERROR( ret, "Graphics/SurfacePeer: IDirectFBSurface::AddRef() failed!\n" );
               return ret;
          }
     }

     return DFB_OK;
}

DFBResult
SurfacePeer::Flip( const DFBRegion     *region,
                   DFBSurfaceFlipFlags  flags )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::SurfacePeer::%s( %p, region %p, flags 0x%08x )\n",
                 __FUNCTION__, this, region, flags );

     DFBResult ret;

     ret = surface->Flip( surface, region, flags );
     if (ret) {
          D_DERROR( ret, "Graphics/SurfacePeer: IDirectFBSurface::Flip() failed!\n" );
          return ret;
     }

     return DFB_OK;
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

/**********************************************************************************************************************/

Implementation::Implementation()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Implementation::%s( %p )\n", __FUNCTION__, this );
}

Implementation::~Implementation()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Implementation::%s( %p )\n", __FUNCTION__, this );
}

DFBResult
Implementation::Init()
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Implementation::%s( %p ) <- init %d\n", __FUNCTION__, this, init );

     if (!init) {
          DFBResult ret;

          ret = Initialise();
          if (ret)
               return ret;

          init = true;
     }

     return DFB_OK;
}

/**********************************************************************************************************************/

void
Core::RegisterImplementation( Implementation *implementation )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Core::%s( implementation %p )\n",
                 __FUNCTION__, implementation );

//     D_DEBUG_AT( DirectFB_Graphics, "  -> name '%s'\n",
//                 *implementation->GetName() );

//     D_DEBUG_AT( DirectFB_Graphics, "  -> APIS '%s'\n",
//                 *implementation->GetAPIs().Concatenated(" ") );

     implementations.push_back( implementation );
}

void
Core::UnregisterImplementation( Implementation *implementation )
{
     D_DEBUG_AT( DirectFB_Graphics, "Graphics::Core::%s( implementation %p )\n",
                 __FUNCTION__, implementation );

     implementations.remove( implementation );
}

Implementations Core::implementations;


}

}

