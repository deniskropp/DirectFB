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
#include <string.h>

#include <core/thread.h>

#include <fusionsound.h>

#include <media/ifusionsoundmusicprovider.h>

#include <core/coredefs.h>

#include <fusion/lock.h>

#include <direct/mem.h>

#include "config.h"

#include "timidity/timidity.h"

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx );

static DFBResult
Construct( IFusionSoundMusicProvider *thiz,
           const char                *filename );

#include <interface_implementation.h>

DFB_INTERFACE_IMPLEMENTATION( IFusionSoundMusicProvider, Timidity )

/*
 * private data struct of IFusionSoundMusicProvider
 */
typedef struct {
     int              ref;       /* reference counter */
     char            *filename;

     void            *buffer;
     int              playing;
     CoreThread      *thread;
     pthread_mutex_t  lock;

     int              length;

     MidiSong        *song;

     IFusionSoundStream *destination;
} IFusionSoundMusicProvider_Timidity_data;

/* interface implementation */

static void
IFusionSoundMusicProvider_Timidity_Destruct( IFusionSoundMusicProvider *thiz )
{
    IFusionSoundMusicProvider_Timidity_data *data =
         (IFusionSoundMusicProvider_Timidity_data*)thiz->priv;

    thiz->Stop( thiz );

    D_FREE( data->filename );

    pthread_mutex_destroy( &data->lock );

    DFB_DEALLOCATE_INTERFACE( thiz );
}

static DFBResult
IFusionSoundMusicProvider_Timidity_AddRef( IFusionSoundMusicProvider *thiz )
{
    INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    data->ref++;

    return DFB_OK;
}


static DFBResult
IFusionSoundMusicProvider_Timidity_Release( IFusionSoundMusicProvider *thiz )
{
    INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    if (--data->ref == 0) {
        IFusionSoundMusicProvider_Timidity_Destruct( thiz );
    }

    return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetCapabilities(
                                            IFusionSoundMusicProvider   *thiz,
                                            FSMusicProviderCapabilities *caps )
{
     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!caps)
          return DFB_INVARG;

     *caps = FMCAPS_BASIC;

     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_GetStreamDescription(
                                              IFusionSoundMusicProvider *thiz,
                                              FSStreamDescription       *desc )
{
    INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

    if (!desc)
        return DFB_INVARG;

    desc->flags        = FSSDF_SAMPLERATE | FSSDF_CHANNELS |
                         FSSDF_SAMPLEFORMAT | FSSDF_BUFFERSIZE;
    desc->samplerate   = 44100;
    desc->channels     = 1;
    desc->sampleformat = FSSF_S16;
    desc->buffersize   = 11025;

    return DFB_OK;
}

static void*
TimidityThread( CoreThread *thread, void *ctx )
{
     IFusionSoundMusicProvider_Timidity_data *data =
          (IFusionSoundMusicProvider_Timidity_data*) ctx;

     while (data->playing) {
          pthread_mutex_lock( &data->lock );

          if (!data->playing || !Timidity_Active()) {
               pthread_mutex_unlock( &data->lock );
               break;
          }

          Timidity_PlaySome( data->buffer, data->length );

          pthread_mutex_unlock( &data->lock );

          data->destination->Write( data->destination,
                                    data->buffer, data->length );
     }

     return NULL;
}

static DFBResult
IFusionSoundMusicProvider_Timidity_PlayTo(
                                       IFusionSoundMusicProvider *thiz,
                                       IFusionSoundStream        *destination )
{
     FSStreamDescription desc;

     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     if (!destination)
          return DFB_INVARG;

     destination->GetDescription( destination, &desc );

     /* check if destination format is supported */
     switch (desc.sampleformat) {
          case FSSF_S16:
               break;
          default:
               return DFB_UNSUPPORTED;
     }

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = 0;
          pthread_mutex_unlock( &data->lock );
          dfb_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          dfb_thread_destroy( data->thread );
          data->thread = NULL;

          Timidity_Stop();
          Timidity_FreeSong( data->song );

          D_FREE( data->buffer );
     }

     /* release previous destination stream */
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
     }

     data->length = desc.buffersize / 2;

     if (Timidity_Init( desc.samplerate,
                        desc.channels,
                        data->length )) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }

     /* load song */
     data->song = Timidity_LoadSong( data->filename );
     if (!data->song) {
          pthread_mutex_unlock( &data->lock );
          return DFB_FAILURE;
     }
     Timidity_Start( data->song );

     /* reference destination stream */
     destination->AddRef( destination );
     data->destination = destination;

     data->playing  = 1;
     data->buffer = D_MALLOC( data->length * desc.channels * 2 );

     /* start thread */
     data->thread = dfb_thread_create( CTT_ANY, TimidityThread, data );

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}


static DFBResult
IFusionSoundMusicProvider_Timidity_Stop( IFusionSoundMusicProvider *thiz )
{
     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     pthread_mutex_lock( &data->lock );

     /* stop thread */
     if (data->thread) {
          data->playing = 0;
          pthread_mutex_unlock( &data->lock );
          dfb_thread_join( data->thread );
          pthread_mutex_lock( &data->lock );
          dfb_thread_destroy( data->thread );
          data->thread = NULL;

          Timidity_Stop();
          Timidity_FreeSong( data->song );

          D_FREE( data->buffer );
     }

     /* release destination stream */
     if (data->destination) {
          data->destination->Release( data->destination );
          data->destination = NULL;
     }

     pthread_mutex_unlock( &data->lock );

     return DFB_OK;
}


static DFBResult IFusionSoundMusicProvider_Timidity_SeekTo(
                                           IFusionSoundMusicProvider *thiz,
                                           double                     seconds )
{
     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     return DFB_UNIMPLEMENTED;
}


static DFBResult IFusionSoundMusicProvider_Timidity_GetPos(
                                           IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     return DFB_UNIMPLEMENTED;
}


static DFBResult IFusionSoundMusicProvider_Timidity_GetLength(
                                           IFusionSoundMusicProvider *thiz,
                                           double                    *seconds )
{
     INTERFACE_GET_DATA (IFusionSoundMusicProvider_Timidity)

     return DFB_UNIMPLEMENTED;
}

/* exported symbols */

static DFBResult
Probe( IFusionSoundMusicProvider_ProbeContext *ctx )
{
     char buf[5];
     FILE *file;

     file = fopen( ctx->filename, "rb" );
     if (!file)
          return DFB_UNSUPPORTED;

     memset( buf, 0, 5 );
     if (fread( buf, 1, 4, file ) != 4) {
          fclose( file );
          return DFB_UNSUPPORTED;
     }
     if (!strstr( buf, "MThd" )) {
          fclose( file );
          return DFB_UNSUPPORTED;
     }

     fclose( file );

     return DFB_OK;
}

static DFBResult
Construct( IFusionSoundMusicProvider *thiz, const char *filename )
{
     DFB_ALLOCATE_INTERFACE_DATA(thiz, IFusionSoundMusicProvider_Timidity)

     /* initialize private data */
     data->ref = 1;
     data->filename = D_STRDUP( filename );
     fusion_pthread_recursive_mutex_init( &data->lock );

     /* initialize function pointers */
     thiz->AddRef               = IFusionSoundMusicProvider_Timidity_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Timidity_Release;
     thiz->GetCapabilities      =
          IFusionSoundMusicProvider_Timidity_GetCapabilities;
     thiz->GetStreamDescription =
          IFusionSoundMusicProvider_Timidity_GetStreamDescription;
     thiz->PlayTo               = IFusionSoundMusicProvider_Timidity_PlayTo;
     thiz->Stop                 = IFusionSoundMusicProvider_Timidity_Stop;
     thiz->SeekTo               = IFusionSoundMusicProvider_Timidity_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_Timidity_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_Timidity_GetLength;

     return DFB_OK;
}
