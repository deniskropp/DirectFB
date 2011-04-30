/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

extern "C" {
#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/internal.h>
}

#include <voodoo/instance.h>
#include <voodoo/manager.h>


D_DEBUG_DOMAIN( Voodoo_Instance, "Voodoo/Instance", "Voodoo Instance" );

/**********************************************************************************************************************/

VoodooInstance::VoodooInstance()
     :
     magic(0),
     refs(1)
{
     D_DEBUG_AT( Voodoo_Instance, "VoodooInstance::%s( %p )\n", __func__, this );

     D_MAGIC_SET( this, VoodooInstance );
}

VoodooInstance::~VoodooInstance()
{
     D_DEBUG_AT( Voodoo_Instance, "VoodooInstance::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooInstance );


     D_MAGIC_CLEAR( this );
}

void
VoodooInstance::AddRef()
{
     D_DEBUG_AT( Voodoo_Instance, "VoodooInstance::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooInstance );

     D_ASSERT( refs > 0 );

     refs++;
}

void
VoodooInstance::Release()
{
     D_DEBUG_AT( Voodoo_Instance, "VoodooInstance::%s( %p )\n", __func__, this );

     D_MAGIC_ASSERT( this, VoodooInstance );

     D_ASSERT( refs > 0 );

     if (!--refs) {
          D_DEBUG_AT( Voodoo_Instance, "  -> zero refs, deleting instance...\n" );

          delete this;
     }
}

DirectResult
VoodooInstance::Dispatch( VoodooManager        *manager,
                          VoodooRequestMessage *msg )
{
     D_DEBUG_AT( Voodoo_Instance, "VoodooInstance::%s( %p, manager %p, msg %p )\n", __func__, this, manager, msg );

     D_MAGIC_ASSERT( this, VoodooInstance );

     return DR_OK;
}

