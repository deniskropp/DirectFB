/*
   (c) Copyright 2001-2008  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

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

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( alsa )

/******************************************************************************/

typedef struct {
     snd_pcm_t             *handle;
     
     CoreSoundDeviceConfig *config;
     
     void                  *buffer;
     
     snd_pcm_uframes_t      offset;
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

static DFBResult
alsa_device_set_configuration( snd_pcm_t             *handle,
                               CoreSoundDeviceConfig *config )
{
     snd_pcm_hw_params_t *params;
     unsigned int         buffertime, time;
     snd_pcm_uframes_t    buffersize;
     unsigned int         periods;
     int                  dir;

     buffertime = time = ((long long)config->buffersize * 1000000ll / config->rate);
          
     snd_pcm_hw_params_alloca( &params );

     if (snd_pcm_hw_params_any( handle, params ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't get hw params!\n" );
          return DFB_FAILURE;
     }

     if (snd_pcm_hw_params_set_access( handle, params,
                                       fs_config->dma
                                       ? SND_PCM_ACCESS_MMAP_INTERLEAVED
                                       : SND_PCM_ACCESS_RW_INTERLEAVED ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set interleaved %saccess!\n",
                   fs_config->dma ? "DMA " : "" );
          return DFB_FAILURE;
     }

     if (snd_pcm_hw_params_set_format( handle, params,
                                       fs2alsa_format( config->format ) ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set format!\n" );
          return DFB_UNSUPPORTED;
     }

     if (snd_pcm_hw_params_set_channels( handle, params,
                                         FS_CHANNELS_FOR_MODE(config->mode) ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set channels mode!\n" );
          return DFB_UNSUPPORTED;
     }

#if SND_LIB_VERSION >= 0x010009
     /* disable software resampling */
     snd_pcm_hw_params_set_rate_resample( handle, params, 0 );
#endif

     dir = 0;
     if (snd_pcm_hw_params_set_rate_near( handle, params,
                                          &config->rate, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set rate!\n" );
          return DFB_UNSUPPORTED;
     }

     dir = 0;
     if (snd_pcm_hw_params_set_buffer_time_near( handle, params,
                                                 &buffertime, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set buffertime!\n" );
          return DFB_UNSUPPORTED;
     }

     dir = 1;
     periods = 2;
     if (snd_pcm_hw_params_set_periods_near( handle, params,
                                             &periods, &dir ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set period count!\n" );
          return DFB_UNSUPPORTED;
     }

     if (snd_pcm_hw_params( handle, params ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't set hw params!\n" );
          return DFB_UNSUPPORTED;
     }

     /* Workaround for ALSA >= 1.0.9 always returning the maximum supported buffersize. */
     if (buffertime > time) {
          config->buffersize = ((long long)time * config->rate / 1000000ll);
     }
     else {
          snd_pcm_hw_params_get_buffer_size( params, &buffersize  );
          config->buffersize = buffersize;
     }

     if (snd_pcm_prepare( handle ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't prepare stream!\n" );
          return DFB_FAILURE;
     }
     
     return DFB_OK;
}

static DFBResult
alsa_device_getset_volume( float *get, float *set )
{
     DFBResult             ret = DFB_OK;
     snd_mixer_t          *mixer;
     snd_mixer_selem_id_t *sid;
     snd_mixer_elem_t     *elem;
     long                  vol, min, max;
     
     if (snd_mixer_open( &mixer, 0 ) < 0)
          return DFB_IO;
          
     if (snd_mixer_attach( mixer, fs_config->device ? : "default" ) < 0) {
          snd_mixer_close( mixer );
          return DFB_FAILURE;
     }
     
     if (snd_mixer_selem_register( mixer, NULL, NULL ) < 0) {
          snd_mixer_close( mixer );
          return DFB_FAILURE;
     }
     
     if (snd_mixer_load( mixer ) < 0) {
          snd_mixer_close( mixer );
          return DFB_FAILURE;
     }
     
     snd_mixer_selem_id_malloc( &sid );
     snd_mixer_selem_id_set_name( sid, "PCM" );
     
     elem = snd_mixer_find_selem( mixer, sid );
     if (!elem) {
          snd_mixer_close( mixer );
          return DFB_UNSUPPORTED;
     }
     
     snd_mixer_selem_get_playback_volume_range( elem, &min, &max );
     
     if (set) {
          vol = *set * (float)(max - min) + min;
          
          if (snd_mixer_selem_set_playback_volume_all( elem, vol ) < 0)
               ret = DFB_UNSUPPORTED;
     }
     else {
          /* Assume equal level for all channels */
          if (snd_mixer_selem_get_playback_volume( elem, 0, &vol ) < 0)
               ret = DFB_UNSUPPORTED;
          else
               *get = (float)(vol - min) / (float)(max - min);
     }
     
     snd_mixer_close( mixer );
     
     return ret;
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
     snd_ctl_t           *ctl;
     snd_ctl_card_info_t *info;
     DFBResult            ret;

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
     
     /* device capabilities */
     device_info->caps = DCF_VOLUME;
     
     ret = alsa_device_set_configuration( data->handle, config );
     if (ret) {
          snd_pcm_close( data->handle );
          return ret;
     }
     
     data->config = config;
     
     if (!fs_config->dma) {
          data->buffer = D_MALLOC( config->buffersize *
                                   FS_CHANNELS_FOR_MODE(config->mode) *
                                   FS_BYTES_PER_SAMPLE(config->format) );
          if (!data->buffer) {
               snd_pcm_close( data->handle );
               return D_OOM();
          }
     }
     else {
          D_INFO( "FusionSound/Device/ALSA: DMA enabled.\n" );
     }    

     return DFB_OK;
}

static int
try_recover( snd_pcm_t *handle, int err )
{
     switch (err) {
          case -EPIPE:
               err = snd_pcm_prepare( handle );
               break;
          
          case -ESTRPIPE:
               while ((err = snd_pcm_resume( handle )) == -EAGAIN)
                    sleep( 1 );
               if (err < 0)
                    err = snd_pcm_prepare( handle );		     
               break;
               
          default:
               break;
     }
     
     return err;
}

static DFBResult
device_get_buffer( void *device_data, u8 **addr, unsigned int *avail )
{
     AlsaDeviceData *data = device_data;
     
     if (data->buffer) {
          *addr = data->buffer;
          *avail = data->config->buffersize;
     }
     else {
          const snd_pcm_channel_area_t *dst;
          snd_pcm_uframes_t             frames;
          snd_pcm_sframes_t             r;
         
          while (true) {
               r = snd_pcm_avail_update( data->handle );
               if (r < 0) {
                    r = try_recover( data->handle, r );
                    if (r < 0) {
                         D_ERROR( "FusionSound/Device/ALSA: snd_pcm_avail_update() failed: %s\n",
                                   snd_strerror( r ) );
                         return DFB_FAILURE;
                     }
                     continue;
               }
               else if (r == 0) {
                    if (snd_pcm_state( data->handle ) == SND_PCM_STATE_PREPARED)
                         r = snd_pcm_start( data->handle );
                    else
                         r = snd_pcm_wait( data->handle, -1 );
                    if (r < 0)
                         r = try_recover( data->handle, r );
                    if (r < 0) {
                         D_ERROR( "FusionSound/Device/ALSA: snd_pcm_avail_update() failed: %s\n",
                                  snd_strerror( r ) );
                         return DFB_FAILURE;
                    }
                    continue;
               }
               
               frames = r;
               r = snd_pcm_mmap_begin( data->handle, &dst, &data->offset, &frames );
               if (r < 0) {
                    r = try_recover( data->handle, r );
                    if (r < 0) {
                         D_ERROR( "FusionSound/Device/ALSA: snd_pcm_mmap_begin() failed: %s\n",
                                   snd_strerror( r ) );
                         return DFB_FAILURE;
                    }
                    continue;
               }
               
               *addr = dst[0].addr + (data->offset * dst[0].step >> 3);
               *avail = frames;
               
               break;
          }
     }
     
     return DFB_OK;
}

static DFBResult
device_commit_buffer( void *device_data, unsigned int frames )
{
     AlsaDeviceData    *data = device_data;
     snd_pcm_sframes_t  r;
     
     if (data->buffer) {
          u8 *src = data->buffer;

          while (frames) {
               r = snd_pcm_writei( data->handle, src, frames );
               if (r < 0) {
                    r = try_recover( data->handle, r );
                    if (r < 0) {
                         D_ERROR( "FusionSound/Device/ALSA: snd_pcm_writei() failed: %s\n",
                                  snd_strerror( r ) );
                         return DFB_FAILURE;
                    }
               }
               frames -= r;
               src += snd_pcm_frames_to_bytes( data->handle, r );
          }
     }
     else {
          while (true) {
               r = snd_pcm_mmap_commit( data->handle, data->offset, frames );
               if (r < 0) {
                    r = try_recover( data->handle, r );
                    if (r < 0) {
                         D_ERROR( "FusionSound/Device/ALSA: snd_pcm_mmap_commit() failed: %s\n",
                                  snd_strerror( r ) );
                         return DFB_FAILURE;
                    }
                    continue;
               }
               break;
          }
     }
     
     return DFB_OK;
}

static void
device_get_output_delay( void *device_data, int *delay )
{
     AlsaDeviceData    *data   = device_data;
     snd_pcm_sframes_t  odelay = 0;

     snd_pcm_delay( data->handle, &odelay );
     *delay = odelay;
}

static DFBResult
device_get_volume( void *device_data, float *level )
{
     return alsa_device_getset_volume( level, NULL );
}

static DFBResult
device_set_volume( void *device_data, float level )
{
     return alsa_device_getset_volume( NULL, &level );
}

static DFBResult
device_suspend( void *device_data )
{
     AlsaDeviceData *data = device_data;

     snd_pcm_drop( data->handle );
     snd_pcm_close( data->handle );
     data->handle = NULL;
     
     return DFB_OK;
}

static DFBResult
device_resume( void *device_data )
{
     AlsaDeviceData *data = device_data;
     DFBResult       ret;

     if (snd_pcm_open( &data->handle, fs_config->device ? : "default",
                       SND_PCM_STREAM_PLAYBACK, 0 ) < 0) {
          D_ERROR( "FusionSound/Device/Alsa: couldn't reopen pcm device!\n" );
          return DFB_IO;
     }
     
     ret = alsa_device_set_configuration( data->handle, data->config );
     if (ret) {
          snd_pcm_close( data->handle );
          data->handle = NULL;
     }
     
     return ret;
} 

static void
device_close( void *device_data )
{
     AlsaDeviceData *data = device_data;

     if (data->buffer)
          D_FREE( data->buffer );

     if (data->handle) {
          snd_pcm_drop( data->handle );
          snd_pcm_close( data->handle );
     }
}
