/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/messages.h>

#include <voodoo/interface.h>


DirectResult
voodoo_construct_requestor( VoodooManager     *manager,
                            const char        *name,
                            VoodooInstanceID   instance,
                            void             **ret_requestor )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *requestor;

     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( instance != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_requestor != NULL );

     ret = DirectGetInterface( &funcs, name, "Requestor", NULL, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Interface: Could not load 'Requestor' implementation of '%s'!\n", name );
          return ret;
     }

     ret = funcs->Allocate( &requestor );
     if (ret)
          return ret;

     ret = funcs->Construct( requestor, manager, instance );
     if (ret)
          return ret;

     *ret_requestor = requestor;

     return DFB_OK;
}

DirectResult
voodoo_construct_dispatcher( VoodooManager     *manager,
                             const char        *name,
                             void              *interface,
                             VoodooInstanceID   super,
                             VoodooInstanceID  *ret_instance,
                             void             **ret_dispatcher )
{
     DirectResult          ret;
     DirectInterfaceFuncs *funcs;
     void                 *dispatcher;
     VoodooInstanceID      instance;

     D_ASSERT( manager != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( interface != NULL );
     D_ASSERT( super != VOODOO_INSTANCE_NONE );
     D_ASSERT( ret_instance != NULL );

     ret = DirectGetInterface( &funcs, name, "Dispatcher", NULL, NULL );
     if (ret) {
          D_ERROR( "Voodoo/Interface: Could not load 'Dispatcher' implementation of '%s'!\n", name );
          return ret;
     }

     ret = funcs->Allocate( &dispatcher );
     if (ret)
          return ret;

     ret = funcs->Construct( dispatcher, interface, manager, super, &instance );
     if (ret)
          return ret;

     *ret_instance = instance;

     if (ret_dispatcher)
          *ret_dispatcher = dispatcher;

     return DFB_OK;
}

