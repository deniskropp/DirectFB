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
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <sys/soundcard.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( oss )

/******************************************************************************/

typedef struct {
     int fd;
     int bytes_per_frame;
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
     OSSDeviceData *data       = device_data;
     int            fmt        = fs2oss_format( config->format );
     int            channels   = FS_CHANNELS_FOR_MODE(config->mode);
     int            rate       = config->rate;
     int            buffersize = 0;
#if defined(SNDCTL_DSP_PROFILE) && defined(APF_NORMAL)
     int            prof       = APF_NORMAL;
#endif
     
     if (fmt == -1)
          return DFB_UNSUPPORTED;
     
     /* open sound device in non-blocking mode */
     if (fs_config->device) {
          data->fd = open( fs_config->device, O_WRONLY | O_NONBLOCK );
     }
     else {
          data->fd = direct_try_open( "/dev/dsp", "/dev/sound/dsp", 
                                      O_WRONLY | O_NONBLOCK, false );
     }
     
     if (data->fd < 0) {
          D_ERROR( "FusionSound/Device/OSS: "
                   "Couldn't open output device!\n" );
          return DFB_IO;
     }
     
     /* reset to blocking mode */
     fcntl( data->fd, F_SETFL, fcntl( data->fd, F_GETFL ) & ~O_NONBLOCK );
      
     /* TODO: get device name */
     
     /* device capabilities */
     device_info->caps = DCF_WRITEBLOCKS;

     
     /* set application profile */
#if defined(SNDCTL_DSP_PROFILE) && defined(APF_NORMAL)
     if (ioctl( data->fd, SNDCTL_DSP_PROFILE, &prof ) < 0) {
          D_ERROR( "FusionSound/Device/OSS: "
                   "Unable to set application profile!\n" );
          close( data->fd );
          return DFB_FAILURE;
     }
#endif
         
     /* set output format */
     if (ioctl( data->fd, SNDCTL_DSP_SETFMT, &fmt ) < 0 || 
         oss2fs_format( fmt ) != config->format) {
          D_ERROR( "FusionSound/Device/OSS: unsupported format!\n" );
          close( data->fd );
          return DFB_UNSUPPORTED;
     }

     /* set number of channels */
     if (ioctl( data->fd, SNDCTL_DSP_CHANNELS, &channels ) < 0 || 
         channels != FS_CHANNELS_FOR_MODE(config->mode)) {
          D_ERROR( "FusionSound/Device/OSS: unsupported channels mode!\n" );
          close( data->fd );
          return DFB_UNSUPPORTED;
     }
     
     data->bytes_per_frame = channels * FS_BYTES_PER_SAMPLE(config->format);
     
     /* set sample rate */
     if (ioctl( data->fd, SNDCTL_DSP_SPEED, &rate ) < 0) {
          D_ERROR( "FusionSound/Device/OSS: "
                   "Unable to set rate to '%d'!\n", config->rate );
          close( data->fd );
          return DFB_UNSUPPORTED;
     }
     
     /* query block size */
     ioctl( data->fd, SNDCTL_DSP_GETBLKSIZE, &buffersize );
     buffersize /= data->bytes_per_frame;
     if (buffersize < 1) {
          D_ERROR( "FusionSound/Device/OSS: Unable to query block size!\n" );
          close( data->fd );
          return DFB_UNSUPPORTED;
     }

     config->rate = rate;
     config->buffersize = buffersize;

     return DFB_OK;
}

static void
device_write( void *device_data, void *samples, unsigned count )
{
     OSSDeviceData *data = device_data;
     
     if (write( data->fd, samples, count*data->bytes_per_frame ) < 0)
          D_PERROR( "FusionSound/Device/OSS: couldn't write %d frames!\n", count );
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

static void
device_close( void *device_data )
{
     OSSDeviceData *data = device_data;
     
     ioctl( data->fd, SNDCTL_DSP_RESET, 0 );
     close( data->fd );
}

