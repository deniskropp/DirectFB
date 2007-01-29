/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net>.

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

#include <unistd.h>

#include <fusionsound.h>

#include <direct/interface.h>
#include <direct/mem.h>

#include <core/core_sound.h>
#include <core/sound_buffer.h>

#include <ifusionsound.h>
#include <ifusionsoundbuffer.h>
#include <ifusionsoundstream.h>

#include <media/ifusionsoundmusicprovider.h>

#include <misc/sound_conf.h>



static void
IFusionSound_Destruct( IFusionSound *thiz )
{
     IFusionSound_data *data = (IFusionSound_data*)thiz->priv;

     fs_core_destroy( data->core, false );

     DIRECT_DEALLOCATE_INTERFACE( thiz );
     
     if (ifusionsound_singleton == thiz)
          ifusionsound_singleton = NULL;
}

static DFBResult
IFusionSound_AddRef( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSound);

     data->ref++;

     return DFB_OK;
}

static DFBResult
IFusionSound_Release( IFusionSound *thiz )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSound)

     if (--data->ref == 0)
          IFusionSound_Destruct( thiz );

     return DFB_OK;
}

static DFBResult
IFusionSound_GetDeviceDescription( IFusionSound        *thiz,
                                   FSDeviceDescription *desc )
{
     DIRECT_INTERFACE_GET_DATA (IFusionSound);
     
     if (!desc)
          return DFB_INVARG;
          
     *desc = *fs_core_device_description( data->core );
     
     return DFB_OK;
}

static DFBResult
IFusionSound_CreateBuffer( IFusionSound               *thiz,
                           const FSBufferDescription  *desc,
                           IFusionSoundBuffer        **ret_interface )
{
     DFBResult                 ret;
     CoreSoundDeviceConfig    *config;
     int                       channels;
     FSSampleFormat            format;
     int                       rate;
     int                       length;
     FSBufferDescriptionFlags  flags;
     CoreSoundBuffer          *buffer;
     IFusionSoundBuffer       *interface;

     DIRECT_INTERFACE_GET_DATA (IFusionSound);

     if (!desc || !ret_interface)
          return DFB_INVARG;
          
     config   = fs_core_device_config( data->core );
     channels = config->channels;
     format   = config->format;
     rate     = config->rate;
     length   = 0;
          
     flags = desc->flags;

     if (flags & ~FSBDF_ALL)
          return DFB_INVARG;

     if (flags & FSBDF_CHANNELS) {
          switch (desc->channels) {
               case 1:
               case 2:
                    channels = desc->channels;
                    break;

               default:
                    return DFB_INVARG;
          }
     }
     
     if (flags & FSBDF_SAMPLEFORMAT) {
          switch (format) {
               case FSSF_U8:
               case FSSF_S16:
               case FSSF_S24:
               case FSSF_S32:
               case FSSF_FLOAT:
                    format = desc->sampleformat;
                    break;

               default:
                    return DFB_INVARG;
          }
     }
     
     if (flags & FSBDF_SAMPLERATE) {
          if (desc->samplerate < 1)
               return DFB_INVARG;
          rate = desc->samplerate;
     }
          
     if (flags & FSBDF_LENGTH)
          length = desc->length;
          
     if (length < 1)
          return DFB_INVARG;
          
     if (length > 0x0fffffff)
          return DFB_LIMITEXCEEDED;

     ret = fs_buffer_create( data->core,
                             length, channels, format, rate, &buffer );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IFusionSoundBuffer );

     ret = IFusionSoundBuffer_Construct( interface, data->core, buffer,
                                         length, channels, format, rate );
     fs_buffer_unref( buffer );

     if (!ret)
          *ret_interface = interface;

     return ret;
}

static DFBResult
IFusionSound_CreateStream( IFusionSound               *thiz,
                           const FSStreamDescription  *desc,
                           IFusionSoundStream        **ret_interface )
{
     DFBResult                 ret;
     CoreSoundDeviceConfig    *config;
     int                       channels;
     FSSampleFormat            format;
     int                       rate;
     int                       size;
     int                       prebuffer;
     FSStreamDescriptionFlags  flags;
     CoreSoundBuffer          *buffer;
     IFusionSoundStream       *interface;

     DIRECT_INTERFACE_GET_DATA (IFusionSound);

     if (!ret_interface)
          return DFB_INVARG;
     
     config    = fs_core_device_config( data->core );
     channels  = config->channels;
     format    = config->format;
     rate      = config->rate;
     size      = 0;
     prebuffer = 0; /* no prebuffer by default */

     if (desc) {
          flags = desc->flags;

          if (flags & ~FSSDF_ALL)
               return DFB_INVARG;

          if (flags & FSSDF_CHANNELS) {
               switch (desc->channels) {
                    case 1:
                    case 2:
                         channels = desc->channels;
                         break;

                    default:
                         return DFB_INVARG;
               }
          }               

          if (flags & FSSDF_SAMPLEFORMAT) {
               switch (desc->sampleformat) {
                    case FSSF_U8:
                    case FSSF_S16:
                    case FSSF_S24:
                    case FSSF_S32:
                    case FSSF_FLOAT:
                         format = desc->sampleformat;
                         break;

                    default:
                         return DFB_INVARG;
               }
          }    

          if (flags & FSSDF_SAMPLERATE) {
               if (desc->samplerate < 1)
                    return DFB_INVARG;
               rate = desc->samplerate;
          }
               
          if (flags & FSSDF_BUFFERSIZE) {
               if (desc->buffersize < 1)
                    return DFB_INVARG;
               size = desc->buffersize;
          }

          if (flags & FSSDF_PREBUFFER) {
               if (desc->prebuffer >= size)
                    return DFB_INVARG;
               prebuffer = desc->prebuffer;
          }
     }
     
     if (!size)
          size = rate / 5; /* defaults to 1/5 second */

     /* Limit ring buffer size to five seconds. */
     if (size > rate * 5)
          return DFB_LIMITEXCEEDED;

     ret = fs_buffer_create( data->core,
                             size, channels, format, rate, &buffer );
     if (ret)
          return ret;

     DIRECT_ALLOCATE_INTERFACE( interface, IFusionSoundStream );

     ret = IFusionSoundStream_Construct( interface, data->core, buffer, size,
                                         channels, format, rate, prebuffer );
     fs_buffer_unref( buffer );

     if (!ret)
          *ret_interface = interface;

     return ret;
}

static DFBResult
IFusionSound_CreateMusicProvider( IFusionSound               *thiz,
                                  const char                 *filename,
                                  IFusionSoundMusicProvider **interface )
{
     DIRECT_INTERFACE_GET_DATA(IFusionSound)

     /* Check arguments */
     if (!interface || !filename)
          return DFB_INVARG;

     return IFusionSoundMusicProvider_Create( filename, interface );
}

DFBResult
IFusionSound_Construct( IFusionSound *thiz )
{
     DFBResult ret;

     /* Allocate interface data. */
     DIRECT_ALLOCATE_INTERFACE_DATA( thiz, IFusionSound );

     /* Initialize interface data. */
     data->ref = 1;

     /* Create the core instance. */
     ret = fs_core_create( &data->core );
     if (ret) {
          DirectFBError( "FusionSound: fs_core_create() failed", ret );

          DIRECT_DEALLOCATE_INTERFACE( thiz );

          return ret;
     }

     /* Assign interface pointers. */
     thiz->AddRef               = IFusionSound_AddRef;
     thiz->Release              = IFusionSound_Release;
     thiz->GetDeviceDescription = IFusionSound_GetDeviceDescription;
     thiz->CreateBuffer         = IFusionSound_CreateBuffer;
     thiz->CreateStream         = IFusionSound_CreateStream;
     thiz->CreateMusicProvider  = IFusionSound_CreateMusicProvider;

     return DFB_OK;
}
