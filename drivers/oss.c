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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/soundcard.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( oss )

/******************************************************************************/

typedef struct {
     int                    fd;
     
     CoreSoundDeviceConfig *config;
     int                    bytes_per_frame;
     void                  *buffer;
} OSSDeviceData;

/******************************************************************************/

#ifdef WORDS_BIGENDIAN
# ifndef AFMT_S24_BE
#  define AFMT_S24_BE 0x00001000
# endif
# ifndef AFMT_S32_BE
#  define AFMT_S32_BE 0x00010000
# endif
# define AFMT_S16  AFMT_S16_BE
# define AFMT_S24  AFMT_S24_BE
# define AFMT_S32  AFMT_S32_BE
#else
# ifndef AFMT_S24_LE
#  define AFMT_S24_LE 0x00000800
# endif
# ifndef AFMT_S32_LE
#  define AFMT_S32_LE 0x00008000
# endif
# define AFMT_S16  AFMT_S16_LE
# define AFMT_S24  AFMT_S24_LE
# define AFMT_S32  AFMT_S32_LE
#endif

static inline int
fs2oss_format( FSSampleFormat format )
{
     switch (format) {
          case FSSF_U8:
               return AFMT_U8;
          case FSSF_S16:
               return AFMT_S16;
          case FSSF_S24:
               return AFMT_S24;
          case FSSF_S32:
               return AFMT_S32;
          default:
               break;
     }
     
     return -1;
}

static inline FSSampleFormat
oss2fs_format( int format )
{
     switch (format) {
          case AFMT_U8:
               return FSSF_U8;
          case AFMT_S16:
               return FSSF_S16;
          case AFMT_S24:
               return FSSF_S24;
          case AFMT_S32:
               return FSSF_S32;
          default:
               break;
     }
     
     return -1;
}

static DFBResult
oss_device_set_configuration( int fd, CoreSoundDeviceConfig *config )
{
     int fmt;
     int channels   = FS_CHANNELS_FOR_MODE(config->mode);
     int rate       = config->rate;
     int buffersize = 0;
#if defined(SNDCTL_DSP_PROFILE) && defined(APF_NORMAL)
     int prof       = APF_NORMAL;
#endif

     fmt = fs2oss_format( config->format );
     if (fmt == -1)
          return DFB_UNSUPPORTED;

     /* set application profile */
#if defined(SNDCTL_DSP_PROFILE) && defined(APF_NORMAL)
     if (ioctl( fd, SNDCTL_DSP_PROFILE, &prof ) < 0)
          D_WARN( "Unable to set application profile!" );
#endif
         
     /* set output format */
     if (ioctl( fd, SNDCTL_DSP_SETFMT, &fmt ) < 0 || 
         oss2fs_format( fmt ) != config->format) {
          D_ERROR( "FusionSound/Device/OSS: unsupported format!\n" );
          return DFB_UNSUPPORTED;
     }

     /* set number of channels */
     if (ioctl( fd, SNDCTL_DSP_CHANNELS, &channels ) < 0 || 
         channels != FS_CHANNELS_FOR_MODE(config->mode)) {
          D_ERROR( "FusionSound/Device/OSS: unsupported channels mode!\n" );
          return DFB_UNSUPPORTED;
     }
     
     /* set sample rate */
     if (ioctl( fd, SNDCTL_DSP_SPEED, &rate ) < 0) {
          D_ERROR( "FusionSound/Device/OSS: unable to set rate to '%d'!\n", config->rate );
          return DFB_UNSUPPORTED;
     }
     
     /* query block size */
     ioctl( fd, SNDCTL_DSP_GETBLKSIZE, &buffersize );
     buffersize /= channels * FS_BYTES_PER_SAMPLE(config->format);
     if (buffersize < 1) {
          D_ERROR( "FusionSound/Device/OSS: unable to query block size!\n" );
          return DFB_UNSUPPORTED;
     }

     config->rate = rate;
     config->buffersize = buffersize;
     
     return DFB_OK;
}

/******************************************************************************/

static DFBResult
device_probe( void )
{
     int fd, fmt;
     
     if (fs_config->device) {
          fd = open( fs_config->device, O_WRONLY | O_NONBLOCK );
     }
     else {
          fd = direct_try_open( "/dev/dsp", "/dev/sound/dsp", 
                                O_WRONLY | O_NONBLOCK, false );
     }
     
     if (fd < 0)
          return DFB_IO;
     
     /* issue a generic ioctl to test the device */     
     if (ioctl( fd, SNDCTL_DSP_GETFMTS, &fmt ) < 0) {
          close( fd );
          return DFB_UNSUPPORTED;
     }
          
     close( fd );
     
     return DFB_OK;
}

static void
device_get_driver_info( SoundDriverInfo *info )
{
     snprintf( info->name, 
               FS_SOUND_DRIVER_INFO_NAME_LENGTH, 
               "OSS" );
     
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
     
     info->device_data_size = sizeof(OSSDeviceData);
}

static DFBResult
device_open( void                  *device_data, 
             SoundDeviceInfo       *device_info,
             CoreSoundDeviceConfig *config )
{
     OSSDeviceData *data = device_data;
     int            mixer_fd;
     audio_buf_info info;
     DFBResult      ret;
     
     /* open sound device in non-blocking mode */
     if (fs_config->device) {
          data->fd = open( fs_config->device, O_WRONLY | O_NONBLOCK );
     }
     else {
          data->fd = direct_try_open( "/dev/dsp", "/dev/sound/dsp", 
                                      O_WRONLY | O_NONBLOCK, false );
     }
     
     if (data->fd < 0) {
          D_ERROR( "FusionSound/Device/OSS: Couldn't open output device!\n" );
          return DFB_IO;
     }
     
     /* reset to blocking mode */
     fcntl( data->fd, F_SETFL, fcntl( data->fd, F_GETFL ) & ~O_NONBLOCK );
      
     /* TODO: get device name */
     
     /* device capabilities */
     device_info->caps = DCF_NONE;
     
     ret = oss_device_set_configuration( data->fd, config );
     if (ret) {
          close( data->fd );
          return ret;
     }
     
     data->config = config;
     data->bytes_per_frame = FS_CHANNELS_FOR_MODE(config->mode) * FS_BYTES_PER_SAMPLE(config->format);
     
     data->buffer = D_MALLOC( config->buffersize * data->bytes_per_frame );
     if (!data->buffer) {
          close( data->fd );
          return D_OOM();
     }

     /* query output space */
     if (ioctl( data->fd, SNDCTL_DSP_GETOSPACE, &info ) < 0)
          D_WARN( "ioctl SNDCTL_DSP_GETOSPACE failed" );
     else
          D_INFO( "FusionSound/OSS: Max output delay is %d.%d ms.\n",
                  (info.bytes / data->bytes_per_frame) * 1000 / config->rate,
                  ((info.bytes / data->bytes_per_frame) * 10000 / config->rate) % 10 );

     /* check whether hardware volume is supported */
     mixer_fd = direct_try_open( "/dev/mixer", "/dev/sound/mixer", O_RDONLY, true );
     if (mixer_fd > 0) {
          int mask = 0;
          
          ioctl( mixer_fd, SOUND_MIXER_READ_DEVMASK, &mask );
          if (mask & SOUND_MASK_PCM) {
               device_info->caps |= DCF_VOLUME;
          } 
          close( mixer_fd );
     }

     return DFB_OK;
}

static DFBResult
device_get_buffer( void *device_data, u8 **addr, unsigned int *avail )
{
     OSSDeviceData *data = device_data;
     
     *addr = data->buffer;
     *avail = data->config->buffersize;
     
     return DFB_OK;
}

static DFBResult
device_commit_buffer( void *device_data, unsigned int frames )
{
     OSSDeviceData *data = device_data;
     
     if (write( data->fd, data->buffer, frames*data->bytes_per_frame ) < 0) {
          DFBResult ret = errno2result( errno );
          D_DERROR( ret, "FusionSound/Device/OSS: couldn't write %d frames!\n", frames );
          return ret;
     }
     
     return DFB_OK;         
}

static void
device_get_output_delay( void *device_data, int *delay )
{
     OSSDeviceData  *data = device_data;
     audio_buf_info  info;
     
     if (ioctl( data->fd, SNDCTL_DSP_GETOSPACE, &info ) < 0) {
          D_ONCE( "ioctl SNDCTL_DSP_GETOSPACE failed" );
          *delay = 0;
          return;
     }
     
     *delay = (info.fragsize * info.fragstotal - info.bytes) / data->bytes_per_frame;
}

static DFBResult
device_get_volume( void *device_data, float *level )
{
     int fd;
     int vol;

     fd = direct_try_open( "/dev/mixer", "/dev/sound/mixer", O_RDONLY, false );
     if (fd < 0)
          return DFB_IO;

     if (ioctl( fd, SOUND_MIXER_READ_PCM, &vol ) < 0) {
          D_PERROR( "FusionSound/Device/OSS: couldn't get volume level!\n" );
          close( fd );
          return DFB_FAILURE;
     }
     
     close( fd );

     *level = (float)((vol & 0xff) + ((vol >> 8) & 0xff)) / 200.0f;

     return DFB_OK;
}

static DFBResult
device_set_volume( void *device_data, float level )
{
     int fd;
     int vol;

     fd = direct_try_open( "/dev/mixer", "/dev/sound/mixer", O_RDONLY, false );
     if (fd < 0)
          return DFB_IO;

     vol  = level * 100.0f;
     vol |= vol << 8;
     if (ioctl( fd, SOUND_MIXER_WRITE_PCM, &vol ) < 0) {
          D_PERROR( "FusionSound/Device/OSS: couldn't set volume level!\n" );
          close( fd );
          return DFB_FAILURE;
     }
     
     close( fd );

     return DFB_OK;
}

static DFBResult
device_suspend( void *device_data )
{
     OSSDeviceData *data = device_data;
     
     ioctl( data->fd, SNDCTL_DSP_RESET, 0 );
     close( data->fd );
     data->fd = -1;
     
     return DFB_OK;
}

static DFBResult
device_resume( void *device_data )
{
     OSSDeviceData *data = device_data;
     DFBResult      ret;
     
     data->fd = (fs_config->device)
                ? open( fs_config->device, O_WRONLY )
                : direct_try_open( "/dev/dsp", "/dev/sound/dsp", O_WRONLY, false );
     if (data->fd < 0) {
          D_ERROR( "FusionSound/Device/OSS: Couldn't reopen output device!\n" );
          return DFB_IO;
     }
     
     ret = oss_device_set_configuration( data->fd, data->config );
     if (ret) {
          close( data->fd );
          data->fd = -1;
     }
     
     return ret;     
}  

static void
device_close( void *device_data )
{
     OSSDeviceData *data = device_data;
     
     if (data->fd > 0) {
          ioctl( data->fd, SNDCTL_DSP_RESET, 0 );
          close( data->fd );
     }
}

