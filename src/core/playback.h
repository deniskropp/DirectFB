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

#ifndef __FUSIONSOUND_CORE_PLAYBACK_H__
#define __FUSIONSOUND_CORE_PLAYBACK_H__

#include <fusionsound.h>

#include <core/fusion/object.h>

#include <core/types_sound.h>

typedef enum {
     CPNF_ADVANCED  = 0x00000001,
     CPNF_ENDED     = 0x00000002
} CorePlaybackNotificationFlags;

typedef struct {
     CorePlaybackNotificationFlags   flags;
     CorePlayback                   *playback;

     int                             pos;      /* Next sample to read in case of
                                                  CPNF_ADVANCED, or next sample
                                                  that would have been read in
                                                  case of CPNF_ENDED. */
} CorePlaybackNotification;

/*
 * Creates a pool of playback objects.
 */
FusionObjectPool *fs_playback_pool_create();

/*
 * Generates fs_playback_ref(), fs_playback_attach() etc.
 */
FUSION_OBJECT_METHODS( CorePlayback, fs_playback )


DFBResult fs_playback_create      ( CoreSound        *core,
                                    CoreSoundBuffer  *buffer,
                                    bool              notify,
                                    CorePlayback    **ret_playback );

DFBResult fs_playback_start       ( CorePlayback     *playback,
                                    int               position );

DFBResult fs_playback_stop        ( CorePlayback     *playback );

DFBResult fs_playback_set_stop    ( CorePlayback     *playback,
                                    int               stop );

DFBResult fs_playback_set_volume  ( CorePlayback     *playback,
                                    int               left,
                                    int               right );

DFBResult fs_playback_set_pitch   ( CorePlayback     *playback,
                                    int               pitch );

/*
 * Internally called by core_audio.c in the audio thread.
 */
DFBResult fs_playback_mixto       ( CorePlayback     *playback,
                                    int              *dest,
                                    int               max_samples );

#endif

