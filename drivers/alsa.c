/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( alsa )

/******************************************************************************/

typedef struct {
     snd_pcm_t *handle;
} AlsaDeviceData;

/******************************************************************************/

static inline snd_pcm_format_t
fs2alsa_format( FSSampleFormat format )
{
     switch (format) {
          case FSSF_U8:
               return SND_PCM_FORMAT_U8;
          case FSSF_S16:
               return SND_PCM_FORMAT_S16;
          case FSSF_S24:
#ifdef WORDS_BIGENDIAN
               return SND_PCM_FORMAT_S24_3BE;
#else
               return SND_PCM_FORMAT_S24_3LE;
#endif
          case FSSF_S32:
               return SND_PCM_FORMAT_S32;
          case FSSF_FLOAT:
               return SND_PCM_FORMAT_FLOAT;
          default:
               break;
     }

     return SND_PCM_FORMAT_UNKNOWN;
}

/******************************************************************************/


static DFBResult
device_probe( void )
{
     snd_pcm_t *handle;

     if (snd_pcm_open( &handle, fs_config->device ? : "default",
                       SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK ) == 0) {
          snd_pcm_close( handle );
          return DFB_OK;
     }

     return DFB_UNSUPPORTED;
}

static void
device_get_driver_info( SoundDriverInfo *info )
{
     snprintf( info->name,
               FS_SOUND_DRIVER_INFO_NAME_LENGTH,
               "ALSA" );

     snprintf( info->vendor,
               FS_SOUND_DRIVER_INFO_VENDOR_LENGTH,
               "directfb.org" );

     snprintf( info->url,
               FS_SOUND_DRIVER_INFO_URL_LENGTH,
               "http://www.directfb.org" );

     snprintf( info->license,
               FS_SOUND_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major = 0;
     info->version.minor = 1;

     info->device_data_size = sizeof(AlsaDeviceData);
}

static DFBResult
device_open( void                  *device_data,
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     AlsaDeviceData      *data = device_data;
     snd_pcm_hw_params_t *params;
     snd_ctl_t           *ctl;
     snd_ctl_card_info_t *info;
     unsigned int         buffertime, time;
     snd_pcm_uframes_t    buffersize;
     int                  periods, dir;

     buffertime = time = ((long long)config->buffersize * 1000000ll / config->rate);

     if (snd_pcm_open( &data->handle, fs_config->device ? : "default",
                       SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't open pcm device!\n" );
          return DFB_IO;
     }

     if (snd_pcm_nonblock( data->handle, 0 ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't disable non-blocking mode!\n" );
          return DFB_IO;
     }

     /* device name */
     if (snd_ctl_open( &ctl, fs_config->device ? : "default",
                       SND_CTL_READONLY | SND_CTL_NONBLOCK ) == 0) {
          snd_ctl_card_info_alloca( &info );

          if (snd_ctl_card_info( ctl, info ) == 0) {
               snprintf( device_info->name,
                         FS_SOUND_DEVICE_INFO_NAME_LENGTH,
                         snd_ctl_card_info_get_name( info ) );
          }

          snd_ctl_close( ctl );
     }

     snd_config_update_free_global();

     /* set configuration */
     snd_pcm_hw_params_alloca( &params );

     if (snd_pcm_hw_params_any( data->handle, params ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't get hw params!\n" );
          snd_pcm_close( data->handle );
          return DFB_FAILURE;
     }

     if (snd_pcm_hw_params_set_access( data->handle, params,
                                       SND_PCM_ACCESS_RW_INTERLEAVED ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set interleaved access!\n" );
          snd_pcm_close( data->handle );
          return DFB_FAILURE;
     }

     if (snd_pcm_hw_params_set_format( data->handle, params,
                                       fs2alsa_format( config->format ) ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set format!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

     if (snd_pcm_hw_params_set_channels( data->handle, params,
                                         config->channels ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set channels mode!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

#if SND_LIB_VERSION >= 0x010009
     /* disable software resampling */
     snd_pcm_hw_params_set_rate_resample( data->handle, params, 0 );
#endif

     dir = 0;
     if (snd_pcm_hw_params_set_rate_near( data->handle, params,
                                          &config->rate, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set rate!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

     dir = 0;
     if (snd_pcm_hw_params_set_buffer_time_near( data->handle, params,
                                                 &buffertime, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set buffertime!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

     dir = 1;
     periods = 2;
     if (snd_pcm_hw_params_set_periods_near( data->handle, params,
                                             &periods, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set period count!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

     if (snd_pcm_hw_params( data->handle, params ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set hw params!\n" );
          snd_pcm_close( data->handle );
          return DFB_UNSUPPORTED;
     }

     /* Workaround for ALSA >= 1.0.9 always returning the maximum supported buffersize.
        Actually FusionSound doesn't work fine with buffers larger than 250ms. */
     if (buffertime > time) {
          config->buffersize = ((long long)time * config->rate / 1000000ll);
     }
     else {
          snd_pcm_hw_params_get_buffer_size( params, &buffersize  );
          config->buffersize = buffersize;
     }

     if (snd_pcm_prepare( data->handle ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't prepare stream!\n" );
          snd_pcm_close( data->handle );
          return DFB_FAILURE;
     }

     return DFB_OK;
}

static void
device_write( void *device_data, void *samples, unsigned int size )
{
     AlsaDeviceData    *data   = device_data;
     snd_pcm_uframes_t  frames = size;
     snd_pcm_sframes_t  r;
     u8                *src;

     src = samples;
     while (frames) {
          r = snd_pcm_writei( data->handle, src, frames );
          if (r < 0) {
               r = snd_pcm_prepare( data->handle );
               if (r < 0) {
                    D_WARN( "FusionSound/Device/ALSA: snd_pcm_writei() failed: %s\n",
                            snd_strerror( r ) );
                    break;
               }
               continue;
          }
          frames -= r;
          src += snd_pcm_frames_to_bytes( data->handle, r );
     }
}

static void
device_get_output_delay( void *device_data, int *delay )
{
     AlsaDeviceData    *data   = device_data;
     snd_pcm_sframes_t  odelay = 0;

     snd_pcm_delay( data->handle, &odelay );
     *delay = odelay;
}

static void
device_close( void *device_data )
{
     AlsaDeviceData *data = device_data;

     snd_pcm_drop( data->handle );
     snd_pcm_close( data->handle );
}
