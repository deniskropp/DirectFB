/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2003  convergence GmbH.

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

#ifndef __FUSIONSOUND_CORE_PLAYBACK_INTERNAL_H__
#define __FUSIONSOUND_CORE_PLAYBACK_INTERNAL_H__

#include <fusionsound.h>

#include <fusion/lock.h>
#include <fusion/object.h>

#include <core/fs_types.h>
#include <core/types_sound.h>


struct __FS_CorePlayback {
     FusionObject     object;

     FusionSkirmish   lock;

     CoreSound       *core;
     CoreSoundBuffer *buffer;
     bool             notify;

     bool             disabled;
     bool             running;
     int              position;
     int              stop;
     
     int              pitch;       /* multiplier for sample rate in FS_PITCH_ONE units */

     __fsf            center;      /* downmixing level for center channel */
     __fsf            rear;        /* downmixing level for rear channel */
     
     __fsf            levels[6];   /* multipliers for channels  */
     
     __fsf            volume;      /* local volume level */
};

#endif
