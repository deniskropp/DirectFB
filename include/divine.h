/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __DIVINE_H__
#define __DIVINE_H__

#include <directfb.h>

/*
 * The DiVine struct represents the connection to the input driver.
 */
typedef struct _DiVine DiVine;

/*
 * Opens a connection to the input driver by opening the pipe
 * specified by 'path'.
 *
 * Returns the DiVine connection object.
 */
DiVine *divine_open (const char *path);

/*
 * Sends a press and a release event for the specified symbol.
 */
void divine_send_symbol (DiVine *divine, DFBInputDeviceKeySymbol symbol);

/*
 * Sends a press and a release event for the specified identifier.
 */
void divine_send_identifier (DiVine *divine, DFBInputDeviceKeyIdentifier identifier);

/*
 * Sends a press and a release event for the specified ANSI string.
 * Use this to feed terminal input into a DirectFB application.
 */
void divine_send_vt102 (DiVine *divine, int size, const char *ansistr);

/*
 * Closes the pipe and destroys the connection object.
 */
void divine_close (DiVine *divine);

#endif
