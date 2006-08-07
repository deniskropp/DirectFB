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

#ifndef __FUSIONSOUND_CORE_SOUND_BUFFER_H__
#define __FUSIONSOUND_CORE_SOUND_BUFFER_H__

#include <fusionsound.h>

#include <fusion/object.h>

#include <core/fs_types.h>
#include <core/types_sound.h>

struct __FS_CoreSoundBuffer {
     FusionObject     object;

     CoreSound       *core;

     int              length;
     int              channels;
     FSSampleFormat   format;
     int              rate;
     int              bytes;

     bool             notify;
     int              break_pos;

     void            *data;

     FusionSHMPoolShared *shmpool;
};

/* mixing limitaion */
#define MAX_BUFFER_RATE  (1 << ((sizeof(long)-2)*8) - 1)


typedef enum {
     CSBNF_NONE
} CoreSoundBufferNotificationFlags;

typedef struct {
     CoreSoundBufferNotificationFlags  flags;
     CoreSoundBuffer                  *buffer;
} CoreSoundBufferNotification;

/*
 * Creates a pool of sound buffer objects.
 */
FusionObjectPool *fs_buffer_pool_create( const FusionWorld *world );

/*
 * Generates fs_buffer_ref(), fs_buffer_attach() etc.
 */
FUSION_OBJECT_METHODS( CoreSoundBuffer, fs_buffer )


DFBResult fs_buffer_create( CoreSound        *core,
                            int               length,
                            int               channels,
                            FSSampleFormat    format,
                            int               rate,
                            CoreSoundBuffer **ret_buffer );

DFBResult fs_buffer_lock  ( CoreSoundBuffer  *buffer,
                            int               pos,
                            int               length,
                            void            **ret_data,
                            int              *ret_bytes );

DFBResult fs_buffer_unlock( CoreSoundBuffer  *buffer );

DFBResult fs_buffer_mixto ( CoreSoundBuffer  *buffer,
                            __fsf            *dest,
                            int               dest_rate,
                            int               max_samples,
                            int               pos,
                            int               stop,
                            __fsf             left,
                            __fsf             right,
                            int               pitch,
                            int              *ret_pos,
                            int              *ret_num,
                            int              *ret_written );

#endif

