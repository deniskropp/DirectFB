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

#ifndef __IFUSIONSOUNDSTREAM_H__
#define __IFUSIONSOUNDSTREAM_H__

#include <fusionsound.h>

#include <core/types_sound.h>

/*
 * private data struct of IFusionSoundStream
 */
typedef struct {
     int                    ref;             /* reference counter */

     CoreSound             *core;
     CoreSoundBuffer       *buffer;
     CorePlayback          *playback;
     int                    size;
     int                    channels;
     FSSampleFormat         format;
     int                    rate;

     Reaction               reaction;

     pthread_mutex_t        lock;
     pthread_cond_t         wait;
     bool                   playing;
     int                    pos_write;
     int                    pos_read;
} IFusionSoundStream_data;

/*
 * initializes interface struct and private data
 */
DFBResult IFusionSoundStream_Construct( IFusionSoundStream *thiz,
                                        CoreSound          *core,
                                        CoreSoundBuffer    *buffer,
                                        int                 size,
                                        int                 channels,
                                        FSSampleFormat      format,
                                        int                 rate );


#endif
