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

#ifndef __UNIQUE__CONTEXT_H__
#define __UNIQUE__CONTEXT_H__

#include <directfb.h>

#include <fusion/object.h>

#include <unique/types.h>


typedef enum {
     UCNF_NONE           = 0x00000000,

     UCNF_CLOSE          = 0x00000001,
     UCNF_WINDOW_ADDED   = 0x00000002,
     UCNF_WINDOW_REMOVED = 0x00000004,

     UCNF_ALL            = 0x00000007
} UniqueContextNotificationFlags;

typedef struct {
     UniqueContextNotificationFlags  flags;
     UniqueContext                  *context;
} UniqueContextNotification;


DFBResult unique_context_create   ( StackData      *data,
                                    UniqueContext **ret_context );

DFBResult unique_context_close    ( UniqueContext  *context );

DFBResult unique_context_set_color( UniqueContext  *context,
                                    const DFBColor *color );


/*
 * Creates a pool of context objects.
 */
FusionObjectPool *unique_context_pool_create();

/*
 * Generates unique_context_ref(), unique_context_attach() etc.
 */
FUSION_OBJECT_METHODS( UniqueContext, unique_context )


/* global reactions */

/*typedef enum {
} UNIQUE_CONTEXT_GLOBALS;*/

#endif

