/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef __IDIRECTFBEVENTBUFFER_DISPATCHER_H__
#define __IDIRECTFBEVENTBUFFER_DISPATCHER_H__

#include <voodoo/types.h>

#include <directfb.h>

#define IDIRECTFBEVENTBUFFER_METHOD_ID_AddRef                     1
#define IDIRECTFBEVENTBUFFER_METHOD_ID_Release                    2
#define IDIRECTFBEVENTBUFFER_METHOD_ID_Reset                      3
#define IDIRECTFBEVENTBUFFER_METHOD_ID_WaitForEvent               4
#define IDIRECTFBEVENTBUFFER_METHOD_ID_WaitForEventWithTimeout    5
#define IDIRECTFBEVENTBUFFER_METHOD_ID_GetEvent                   6
#define IDIRECTFBEVENTBUFFER_METHOD_ID_PeekEvent                  7
#define IDIRECTFBEVENTBUFFER_METHOD_ID_HasEvent                   8
#define IDIRECTFBEVENTBUFFER_METHOD_ID_PostEvent                  9
#define IDIRECTFBEVENTBUFFER_METHOD_ID_WakeUp                    10
#define IDIRECTFBEVENTBUFFER_METHOD_ID_CreateFileDescriptor      11

/*
 * private data struct of IDirectFBEventBuffer_Dispatcher
 */
typedef struct {
     int                   ref;      /* reference counter */

     IDirectFBEventBuffer *real;

     VoodooInstanceID      self;         /* The instance of this dispatcher itself. */
     VoodooInstanceID      super;        /* The instance of the super interface. */

     VoodooManager        *manager;
} IDirectFBEventBuffer_Dispatcher_data;


#endif
