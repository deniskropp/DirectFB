/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
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

#ifndef __IFUSIONSOUND_DISPATCHER_H___
#define __IFUSIONSOUND_DISPATCHER_H___

#include <fusionsound.h>

enum {
     IFUSIONSOUND_METHOD_ID_AddRef = 1,
     IFUSIONSOUND_METHOD_ID_Release,
     IFUSIONSOUND_METHOD_ID_GetDeviceDescription,
     IFUSIONSOUND_METHOD_ID_CreateBuffer,
     IFUSIONSOUND_METHOD_ID_CreateStream,
     IFUSIONSOUND_METHOD_ID_CreateMusicProvider,
     IFUSIONSOUND_METHOD_ID_GetMasterVolume,
     IFUSIONSOUND_METHOD_ID_SetMasterVolume,
     IFUSIONSOUND_METHOD_ID_GetLocalVolume,
     IFUSIONSOUND_METHOD_ID_SetLocalVolume,
     IFUSIONSOUND_METHOD_ID_Suspend,
     IFUSIONSOUND_METHOD_ID_Resume
};

#endif
