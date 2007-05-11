/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net>

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

#include <stdlib.h>
#include <string.h>

#include <fusionsound.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <media/ifusionsoundmusicprovider.h>


static DFBResult
IFusionSoundMusicProvider_AddRef( IFusionSoundMusicProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_Release( IFusionSoundMusicProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetCapabilities( IFusionSoundMusicProvider   *thiz,
                                           FSMusicProviderCapabilities *caps )
{
     if (!caps)
          return DFB_INVARG;
          
     *caps = FMCAPS_BASIC;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_EnumTracks( IFusionSoundMusicProvider *thiz,
                                      FSTrackCallback            callback,
                                      void                      *callbackdata )
{
     FSTrackDescription desc;
     DFBResult          ret;
     
     if (!callback)
          return DFB_INVARG;
          
     ret = thiz->GetTrackDescription( thiz, &desc );
     if (ret)
          return ret;
          
     callback( 0, desc, callbackdata );
          
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_GetTrackID( IFusionSoundMusicProvider *thiz,
                                      FSTrackID                 *ret_track_id )
{
     if (!ret_track_id)
          return DFB_INVARG;
          
     *ret_track_id = 0;
     
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_GetTrackDescription( IFusionSoundMusicProvider *thiz,
                                               FSTrackDescription        *desc )
{
     if (!desc)
          return DFB_INVARG;
          
     memset( desc, 0, sizeof(FSTrackDescription) );
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetStreamDescription( IFusionSoundMusicProvider *thiz,
                                                FSStreamDescription       *desc )
{
     if (!desc)
          return DFB_INVARG;
          
     desc->flags = FSSDF_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetBufferDescription( IFusionSoundMusicProvider *thiz,
                                                FSBufferDescription       *desc )
{
     if (!desc)
          return DFB_INVARG;
          
     desc->flags = FSBDF_NONE;
     
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_SelectTrack( IFusionSoundMusicProvider *thiz,
                                       FSTrackID                  track_id )
{
     if (track_id != 0)
          return DFB_UNSUPPORTED;
          
     return DFB_OK;
}

static DFBResult
IFusionSoundMusicProvider_PlayToStream( IFusionSoundMusicProvider *thiz,
                                        IFusionSoundStream        *destination )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_PlayToBuffer( IFusionSoundMusicProvider *thiz,
                                        IFusionSoundBuffer        *destination,
                                        FMBufferCallback           callback,
                                        void                      *ctx )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_Stop( IFusionSoundMusicProvider *thiz )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetStatus( IFusionSoundMusicProvider *thiz,
                                     FSMusicProviderStatus     *status )
{
     if (!status)
          return DFB_INVARG;
          
     *status = FMSTATE_UNKNOWN;
          
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_SeekTo( IFusionSoundMusicProvider *thiz,
                                  double                     seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetPos( IFusionSoundMusicProvider *thiz,
                                  double                    *seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_GetLength( IFusionSoundMusicProvider *thiz,
                                     double                    *seconds )
{
     return DFB_UNIMPLEMENTED;
}

static DFBResult
IFusionSoundMusicProvider_SetPlaybackFlags( IFusionSoundMusicProvider    *thiz,
                                            FSMusicProviderPlaybackFlags  flags )
{
     return DFB_UNIMPLEMENTED;
}

static void
IFusionSoundMusicProvider_Construct( IFusionSoundMusicProvider *thiz )
{
     thiz->AddRef               = IFusionSoundMusicProvider_AddRef;
     thiz->Release              = IFusionSoundMusicProvider_Release;
     thiz->GetCapabilities      = IFusionSoundMusicProvider_GetCapabilities;
     thiz->EnumTracks           = IFusionSoundMusicProvider_EnumTracks;
     thiz->GetTrackID           = IFusionSoundMusicProvider_GetTrackID;
     thiz->GetTrackDescription  = IFusionSoundMusicProvider_GetTrackDescription;
     thiz->GetStreamDescription = IFusionSoundMusicProvider_GetStreamDescription;
     thiz->GetBufferDescription = IFusionSoundMusicProvider_GetBufferDescription;
     thiz->SelectTrack          = IFusionSoundMusicProvider_SelectTrack;
     thiz->PlayToStream         = IFusionSoundMusicProvider_PlayToStream;
     thiz->PlayToBuffer         = IFusionSoundMusicProvider_PlayToBuffer;
     thiz->Stop                 = IFusionSoundMusicProvider_Stop;
     thiz->GetStatus            = IFusionSoundMusicProvider_GetStatus;
     thiz->SeekTo               = IFusionSoundMusicProvider_SeekTo;
     thiz->GetPos               = IFusionSoundMusicProvider_GetPos;
     thiz->GetLength            = IFusionSoundMusicProvider_GetLength;
     thiz->SetPlaybackFlags     = IFusionSoundMusicProvider_SetPlaybackFlags;
}
     

DFBResult
IFusionSoundMusicProvider_Create( const char                 *filename, 
                                  IFusionSoundMusicProvider **interface )
{
     DFBResult                               ret;
     DirectInterfaceFuncs                   *funcs = NULL;
     IFusionSoundMusicProvider              *musicprovider;
     IFusionSoundMusicProvider_ProbeContext  ctx;
     DirectStream                           *stream;
     
     /* Open filename */
     ret = direct_stream_create( filename, &stream );
     if (ret)
          return ret;
     
     /* Fill out probe context */
     ctx.filename = filename;
     ctx.mimetype = direct_stream_mime( stream );
     ctx.stream   = stream;
     
     /* Clear probe context's header */
     memset( ctx.header, sizeof(ctx.header), 0 );
     
     /* Fill probe context's header */
     direct_stream_wait( stream, sizeof(ctx.header), NULL );
     direct_stream_peek( stream, sizeof(ctx.header), 0, ctx.header, NULL );     

     /* Find a suitable implemenation */
     ret = DirectGetInterface( &funcs, "IFusionSoundMusicProvider",
                               NULL, DirectProbeInterface, &ctx );
     if (ret) {
          direct_stream_destroy( stream );
          return ret;
     }

     DIRECT_ALLOCATE_INTERFACE( musicprovider, IFusionSoundMusicProvider );

     /* Initialize interface pointers. */
     IFusionSoundMusicProvider_Construct( musicprovider );

     /* Construct the interface */
     ret = funcs->Construct( musicprovider, filename, stream );
     if (ret)
          *interface = NULL;
     else
          *interface = musicprovider;
          
     direct_stream_destroy( stream );

     return ret;
}
     
      
