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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <math.h>


#include <fusionsound.h>
#include <directfb_internals.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/util.h>
#include <misc/mem.h>
#include <gfx/convert.h>
#include <gfx/util.h>

#include <core/sound_buffer.h>

#include "ifusionsoundbuffer.h"

/*
 * private data struct of IFusionSoundBuffer
 */
typedef struct {
     int                    ref;             /* reference counter */

     CoreSoundBuffer       *buffer;

     bool                   locked;
     __u16                  pan;
} IFusionSoundBuffer_data;


static void
IFusionSoundBuffer_Destruct( IFusionSoundBuffer *thiz )
{
     IFusionSoundBuffer_data *data = (IFusionSoundBuffer_data*)thiz->priv;

     if (data->buffer) {
          CoreSoundBuffer *buffer = data->buffer;

          data->buffer = NULL;

          if (data->locked)
               fs_buffer_unlock( buffer );
          
          fs_buffer_stop_all( buffer );
          
          fs_buffer_unref( buffer );
     }

     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundBuffer_AddRef( IFusionSoundBuffer *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundBuffer)

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Release( IFusionSoundBuffer *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (--data->ref == 0)
          IFusionSoundBuffer_Destruct( thiz );

     return DFB_OK;
}


static DFBResult
IFusionSoundBuffer_Lock( IFusionSoundBuffer  *thiz,
                         void               **ret_data )
{
     DFBResult  ret;
     void      *lock_data;
     int        lock_bytes;

     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->buffer)
          return DFB_DESTROYED;

     if (!ret_data)
          return DFB_INVARG;

     if (data->locked)
          return DFB_LOCKED;

     ret = fs_buffer_lock( data->buffer, 0, 0, &lock_data, &lock_bytes );
     if (ret)
          return ret;

     data->locked = true;

     *ret_data = lock_data;

     /* FIXME: what about returning lock_bytes? */

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Unlock( IFusionSoundBuffer *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->buffer)
          return DFB_DESTROYED;

     if (!data->locked)
          return DFB_OK;

     fs_buffer_unlock( data->buffer );

     data->locked = false;

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_SetPan( IFusionSoundBuffer *thiz,
                           float               value )
{
     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->buffer)
          return DFB_DESTROYED;

     if (value < -1.0f || value > 1.0f)
          return DFB_INVARG;

     data->pan = (__u16)( 0x8000 + value * 0x7fff );

     return DFB_OK;
}

static DFBResult
IFusionSoundBuffer_Play( IFusionSoundBuffer *thiz,
                         FSBufferPlayFlags   flags )
{
     __u16 pan = 0x8000;

     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->buffer)
          return DFB_DESTROYED;

     if (flags & ~FSPLAY_ALL)
          return DFB_INVARG;

     if (flags & FSPLAY_PAN)
          pan = data->pan;

     return fs_buffer_playback( data->buffer, 0, pan, flags & FSPLAY_LOOPING );
}

static DFBResult
IFusionSoundBuffer_StopAll( IFusionSoundBuffer *thiz )
{
     INTERFACE_GET_DATA(IFusionSoundBuffer)

     if (!data->buffer)
          return DFB_DESTROYED;

     return fs_buffer_stop_all( data->buffer );
}

/******/

DFBResult
IFusionSoundBuffer_Construct( IFusionSoundBuffer *thiz,
                              CoreSoundBuffer    *buffer )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundBuffer)

     data->ref    = 1;
     data->buffer = buffer;

     thiz->AddRef  = IFusionSoundBuffer_AddRef;
     thiz->Release = IFusionSoundBuffer_Release;

     thiz->Lock    = IFusionSoundBuffer_Lock;
     thiz->Unlock  = IFusionSoundBuffer_Unlock;
     thiz->SetPan  = IFusionSoundBuffer_SetPan;
     thiz->Play    = IFusionSoundBuffer_Play;
     thiz->StopAll = IFusionSoundBuffer_StopAll;
     
     return DFB_OK;
}

