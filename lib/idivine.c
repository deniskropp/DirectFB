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

#include <config.h>

#include <direct/debug.h>
#include <direct/direct.h>
#include <direct/messages.h>

#include <divine.h>

#include "idivine.h"


D_DEBUG_DOMAIN( IDiVine_, "IDiVine", "DiVine Main Interface" );

/**********************************************************************************************************************/

/*
 * Destructor
 *
 * Free data structure and set the pointer to NULL,
 * to indicate the dead interface.
 */
void
IDiVine_Destruct( IDiVine *thiz )
{
     IDiVine_data *data = thiz->priv;

     D_DEBUG_AT( IDiVine_, "%s( %p )\n", __FUNCTION__, thiz );

     divine_close( data->divine );

     DIRECT_DEALLOCATE_INTERFACE( thiz );

     direct_shutdown();
}

/**********************************************************************************************************************/

static DirectResult
IDiVine_AddRef( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine)

     D_DEBUG_AT( IDiVine_, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref++;

     return DFB_OK;
}

static DirectResult
IDiVine_Release( IDiVine *thiz )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine)

     D_DEBUG_AT( IDiVine_, "%s( %p )\n", __FUNCTION__, thiz );

     if (--data->ref == 0)
          IDiVine_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IDiVine_SendEvent( IDiVine             *thiz,
                   const DFBInputEvent *event )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine)

     D_DEBUG_AT( IDiVine_, "%s( %p, %p )\n", __FUNCTION__, thiz, event );

     divine_send_event( data->divine, event );

     return DFB_OK;
}

static DFBResult
IDiVine_SendSymbol( IDiVine                 *thiz,
                    DFBInputDeviceKeySymbol  symbol )
{
     DIRECT_INTERFACE_GET_DATA(IDiVine)

     D_DEBUG_AT( IDiVine_, "%s( %p, 0x%08x )\n", __FUNCTION__, thiz, symbol );

     divine_send_symbol( data->divine, symbol );

     return DFB_OK;
}

/**********************************************************************************************************************/

/*
 * Constructor
 *
 * Fills in function pointers and intializes data structure.
 */
DFBResult
IDiVine_Construct( IDiVine *thiz )
{
     DIRECT_ALLOCATE_INTERFACE_DATA(thiz, IDiVine)

     D_DEBUG_AT( IDiVine_, "%s( %p )\n", __FUNCTION__, thiz );

     data->ref    = 1;
     data->divine = divine_open( "/tmp/divine" );

     if (!data->divine) {
          D_ERROR( "%s: divine_open() failed!\n", __FUNCTION__ );
          DIRECT_DEALLOCATE_INTERFACE( thiz );
          return DFB_FAILURE;
     }

     thiz->AddRef     = IDiVine_AddRef;
     thiz->Release    = IDiVine_Release;
     thiz->SendEvent  = IDiVine_SendEvent;
     thiz->SendSymbol = IDiVine_SendSymbol;

     return DFB_OK;
}

