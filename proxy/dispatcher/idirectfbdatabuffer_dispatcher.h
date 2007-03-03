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

#ifndef __IDIRECTFBDATABUFFER_DISPATCHER_H__
#define __IDIRECTFBDATABUFFER_DISPATCHER_H__

#define IDIRECTFBDATABUFFER_METHOD_ID_AddRef                     1
#define IDIRECTFBDATABUFFER_METHOD_ID_Release                    2
#define IDIRECTFBDATABUFFER_METHOD_ID_Flush                      3
#define IDIRECTFBDATABUFFER_METHOD_ID_Finish                     4
#define IDIRECTFBDATABUFFER_METHOD_ID_SeekTo                     5
#define IDIRECTFBDATABUFFER_METHOD_ID_GetPosition                6
#define IDIRECTFBDATABUFFER_METHOD_ID_GetLength                  7
#define IDIRECTFBDATABUFFER_METHOD_ID_WaitForData                8
#define IDIRECTFBDATABUFFER_METHOD_ID_WaitForDataWithTimeout     9
#define IDIRECTFBDATABUFFER_METHOD_ID_GetData                   10
#define IDIRECTFBDATABUFFER_METHOD_ID_PeekData                  11
#define IDIRECTFBDATABUFFER_METHOD_ID_HasData                   12
#define IDIRECTFBDATABUFFER_METHOD_ID_PutData                   13
#define IDIRECTFBDATABUFFER_METHOD_ID_CreateImageProvider       14

#endif
