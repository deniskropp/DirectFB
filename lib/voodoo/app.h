/*
   (c) Copyright 2001-2010  The DirectFB Organization (directfb.org)
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

#ifndef __VOODOO__APP_H__
#define __VOODOO__APP_H__

#include <voodoo/types.h>



typedef enum {
     VADESC_NONE    = 0x00000000,

     VADESC_NAME    = 0x00000001,
     VADESC_TEXT    = 0x00000002,

     VADESC_ALL     = 0x00000003,
} VoodooAppDescriptionFlags;


#define VOODOO_APP_DESCRIPTION_NAME_LENGTH      128
#define VOODOO_APP_DESCRIPTION_TEXT_LENGTH     1024

typedef struct {
     u8                            uuid[16];
     VoodooAppDescriptionFlags     flags;

     char                          name[VOODOO_APP_DESCRIPTION_NAME_LENGTH];
     char                          text[VOODOO_APP_DESCRIPTION_TEXT_LENGTH];
} VoodooAppDescription;

typedef struct {
     u8                            uuid[16];

     VoodooAppDescription          app;
     u8                            player_uuid[16];
} VoodooAppInstanceDescription;


#endif

