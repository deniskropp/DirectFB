/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net.

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

#include "sound_util.h"


FSChannelMode
fs_mode_for_channels( int channels )
{
     switch (channels) {
          case 1:
               return FSCM_MONO;
          case 2:
               return FSCM_STEREO;
          case 3:
               return FSCM_STEREO30;
          case 4:
               return FSCM_SURROUND40_2F2R;
          case 5:
               return FSCM_SURROUND50;
          case 6:
               return FSCM_SURROUND51;
          default:
               break;
     }
     
     return FSCM_UNKNOWN;
}


