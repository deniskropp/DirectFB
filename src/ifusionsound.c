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

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <malloc.h>

#include <fusionsound.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/mem.h>

#include <core/core_sound.h>
#include <core/sound_buffer.h>

#include "ifusionsoundbuffer.h"
#include "ifusionsoundstream.h"


static DFBResult
Probe( void *arg );

static DFBResult
Construct( IFusionSound *thiz,
           void         *arg );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IFusionSound, default )

/*
 * private data struct of IFusionSound
 */
typedef struct {
     int              ref;       /* reference counter */

     CoreSound       *core;
} IFusionSound_data;


static void
IFusionSound_Destruct( IFusionSound *thiz )
{
     IFusionSound_data *data = (IFusionSound_data*)thiz->priv;

     fs_core_destroy( data->core );
     
     DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSound_AddRef( IFusionSound *thiz )
{
     INTERFACE_GET_DATA (IFusionSound);

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSound_Release( IFusionSound *thiz )
{
     INTERFACE_GET_DATA (IFusionSound)

     if (--data->ref == 0) {
          IFusionSound_Destruct( thiz );
     }

     return DFB_OK;
}

static DFBResult
IFusionSound_CreateBuffer( IFusionSound         *thiz,
                           FSBufferDescription  *desc,
                           IFusionSoundBuffer  **interface )
{
     DFBResult                 ret;
     int                       length   = 0;
     int                       channels = 2;
     FSSampleFormat            format   = FSSF_S16;
     int                       rate     = 44100;
     FSBufferDescriptionFlags  flags;
     CoreSoundBuffer          *buffer;

     INTERFACE_GET_DATA (IFusionSound);

     if (!desc || !interface)
          return DFB_INVARG;

     flags = desc->flags;

     if (flags & ~FSBDF_ALL)
          return DFB_INVARG;

     if (flags & FSBDF_LENGTH)
          length = desc->length;

     if (flags & FSBDF_CHANNELS)
          channels = desc->channels;

     if (flags & FSBDF_SAMPLEFORMAT)
          format = desc->sampleformat;

     if (flags & FSBDF_SAMPLERATE)
          rate = desc->samplerate;

     if (length < 1)
          return DFB_INVARG;

     switch (channels) {
          case 1:
          case 2:
               break;

          default:
               return DFB_INVARG;
     }

     switch (format) {
          case FSSF_S16:
          case FSSF_U8:
               break;

          default:
               return DFB_INVARG;
     }

     if (rate < 1)
          return DFB_INVARG;

     ret = fs_buffer_create( data->core,
                             length, channels, format, rate, false, &buffer );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IFusionSoundBuffer );

     IFusionSoundBuffer_Construct( *interface, buffer );

     return DFB_OK;
}

static DFBResult
IFusionSound_CreateStream( IFusionSound         *thiz,
                           FSStreamDescription  *desc,
                           IFusionSoundStream  **interface )
{
     DFBResult                 ret;
     int                       channels = 2;
     FSSampleFormat            format   = FSSF_S16;
     int                       rate     = 44100;
     int                       size     = rate;   /* space for one second */
     FSStreamDescriptionFlags  flags    = FSSDF_NONE;
     CoreSoundBuffer          *buffer;

     INTERFACE_GET_DATA (IFusionSound);

     if (!interface)
          return DFB_INVARG;

     if (desc) {
          flags = desc->flags;

          if (flags & ~FSSDF_ALL)
               return DFB_INVARG;

          if (flags & FSSDF_BUFFERSIZE)
               size = desc->buffersize;

          if (flags & FSSDF_CHANNELS)
               channels = desc->channels;

          if (flags & FSSDF_SAMPLEFORMAT)
               format = desc->sampleformat;

          if (flags & FSSDF_SAMPLERATE)
               rate = desc->samplerate;
     }

     if (size < 1)
          return DFB_INVARG;

     /* Limit ring buffer size to five seconds. */
     if (size > rate * 5)
          return DFB_BUFFERTOOLARGE;

     switch (channels) {
          case 1:
          case 2:
               break;

          default:
               return DFB_INVARG;
     }

     switch (format) {
          case FSSF_S16:
          case FSSF_U8:
               break;

          default:
               return DFB_INVARG;
     }

     if (rate < 1)
          return DFB_INVARG;

     ret = fs_buffer_create( data->core,
                             size, channels, format, rate, true, &buffer );
     if (ret)
          return ret;

     DFB_ALLOCATE_INTERFACE( *interface, IFusionSoundStream );

     IFusionSoundStream_Construct( *interface, buffer, size );

     return DFB_OK;
}


/* exported symbols */

static DFBResult
Probe( void *arg )
{
     return DFB_OK;
}

static DFBResult
Construct( IFusionSound *thiz,
           void         *arg )
{
     DFBResult ret;

     /* Allocate interface data. */
     DFB_ALLOCATE_INTERFACE_DATA( thiz, IFusionSound );
     
     /* Initialize interface data. */
     data->ref = 1;

     /* Create the core instance. */
     ret = fs_core_create( &data->core );
     if (ret) {
          DirectFBError( "FusionSound: fs_core_create() failed", ret );

          DFB_DEALLOCATE_INTERFACE( thiz );

          return ret;
     }
     
     /* Assign interface pointers. */
     thiz->AddRef        = IFusionSound_AddRef;
     thiz->Release       = IFusionSound_Release;
     thiz->CreateBuffer  = IFusionSound_CreateBuffer;
     thiz->CreateStream  = IFusionSound_CreateStream;

     return DFB_OK;
}

