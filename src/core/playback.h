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

#include <fusion/object.h>

#include <core/fs_types.h>
#include <core/types_sound.h>

typedef enum {
     CPS_NONE     = 0x00000000,
     CPS_PLAYING  = 0x00000001,
     CPS_LOOPING  = 0x00000002
} CorePlaybackStatus;

typedef enum {
     CPNF_START   = 0x00000001,
     CPNF_STOP    = 0x00000002,
     CPNF_ADVANCE = 0x00000004
} CorePlaybackNotificationFlags;

typedef struct {
     CorePlaybackNotificationFlags   flags;
     CorePlayback                   *playback;

     int                             pos;    /* Always the current playback position. */
     int                             stop;   /* Position at which the playback will stop or has
                                                stopped. A negative value indicates looping. */
     int                             num;    /* Number of samples played (CPNF_ADVANCE) or zero. */
} CorePlaybackNotification;


/*
 * Internal definitions.
 */
#define FS_PITCH_BITS  14
#define FS_PITCH_ONE   (1 << FS_PITCH_BITS)

/*
 * Creates a pool of playback objects.
 */
FusionObjectPool *fs_playback_pool_create( const FusionWorld *world );

/*
 * Generates fs_playback_ref(), fs_playback_attach() etc.
 */
FUSION_OBJECT_METHODS( CorePlayback, fs_playback )


DFBResult fs_playback_create      ( CoreSound           *core,
                                    CoreSoundBuffer     *buffer,
                                    bool                 notify,
                                    CorePlayback       **ret_playback );

DFBResult fs_playback_enable      ( CorePlayback        *playback );

DFBResult fs_playback_start       ( CorePlayback        *playback,
                                    bool                 enable );

DFBResult fs_playback_stop        ( CorePlayback        *playback,
                                    bool                 disable );

DFBResult fs_playback_set_stop    ( CorePlayback        *playback,
                                    int                  stop );

DFBResult fs_playback_set_position( CorePlayback        *playback,
                                    int                  position );

/* Must call fs_playback_set_volume() after this. */                                  
DFBResult fs_playback_set_downmix ( CorePlayback        *playback,
                                    float                center,
                                    float                rear );

DFBResult fs_playback_set_volume  ( CorePlayback        *playback,
                                    float                levels[6] );

DFBResult fs_playback_set_pitch   ( CorePlayback        *playback,
                                    int                  pitch );

DFBResult fs_playback_get_status  ( CorePlayback        *playback,
                                    CorePlaybackStatus  *ret_status,
                                    int                 *ret_position );

/*
 * Internally called by core_sound.c in the audio thread.
 */
DFBResult fs_playback_mixto       ( CorePlayback        *playback,
                                    __fsf               *dest,
                                    int                  dest_rate,
                                    int                  max_frames,
                                    int                 *ret_samples );


#endif
