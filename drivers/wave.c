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
#include <fcntl.h>
#include <unistd.h>

#include <direct/messages.h>
#include <direct/util.h>

#include <fusionsound.h>

#include <core/sound_device.h>
#include <core/sound_driver.h>

#include <misc/sound_conf.h>


FS_SOUND_DRIVER( wave )

/******************************************************************************/

typedef struct {
     int fd;
     int bytes_per_frame;
} WaveDeviceData;

/******************************************************************************/

typedef struct {
     __u8  ChunkID[4];
     __u32 ChunkSize;
     __u8  Format[4];
     __u8  Subchunk1ID[4];
     __u32 Subchunk1Size;
     __u16 AudioFormat;
     __u16 NumChannels;
     __u32 SampleRate;
     __u32 ByteRate;
     __u16 BlockAlign;
     __u16 BitsPerSample;
     __u8  Subchunk2ID[4];
     __u32 Subchunk2Size;
} WaveHeader;

/******************************************************************************/


static DFBResult
device_probe( void )
{
     int  fd;
     char path[4096];
     
     /* load only when requested */
     if (!fs_config->driver)
          return DFB_UNSUPPORTED;
          
     if (fs_config->device) {
          snprintf( path, sizeof(path), "%s", fs_config->device );
     }
     else {
          snprintf( path, sizeof(path),
                    "./fusionsound-%d.wav", fs_config->session );
     }
     
     fd = open( path, O_WRONLY | O_CREAT | O_NOCTTY | O_NONBLOCK, 0644 );
     if (fd < 0)
          return DFB_UNSUPPORTED;
          
     close( fd );
     
     return DFB_OK;
}

static void
device_get_driver_info( SoundDriverInfo *info )
{
     snprintf( info->name, 
               FS_SOUND_DRIVER_INFO_NAME_LENGTH, 
               "Wave" );
     
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
     
     info->device_data_size = sizeof(WaveDeviceData);
}

static DFBResult
device_open( void *device_data, CoreSoundDeviceConfig *config )
{
     WaveDeviceData *data   = device_data;
     WaveHeader      header;
     char            path[4096];
     
     if (config->format == FSSF_FLOAT)
          return DFB_UNSUPPORTED;
     
     if (fs_config->device) {
          snprintf( path, sizeof(path), "%s", fs_config->device );
     }
     else {
          snprintf( path, sizeof(path),
                    "./fusionsound-%d.wav", fs_config->session );
     }
     
     data->fd = open( path, O_WRONLY | O_CREAT | O_TRUNC | O_NOCTTY, 0644 );     
     if (data->fd < 0) {
          D_ERROR( "FusionSound/Device/Wave: "
                   "couldn't open '%s' for writing!\n", path );
          return DFB_IO;
     }

#ifdef WORDS_BIGENDIAN
     memcpy( header.ChunkID, "RIFX", 4 );
#else
     memcpy( header.ChunkID, "RIFF", 4 );
#endif
     header.ChunkSize = 0;
     memcpy( header.Format, "WAVE", 4 );
     
     memcpy( header.Subchunk1ID, "fmt ", 4 );
     header.Subchunk1Size = 16;
     header.AudioFormat = 1;
     header.NumChannels = config->channels;
     header.SampleRate = config->rate;
     header.ByteRate = config->rate * config->channels *
                       FS_BITS_PER_SAMPLE(config->format) >> 3;
     header.BlockAlign = config->channels * 
                         FS_BITS_PER_SAMPLE(config->format) >> 3;
     header.BitsPerSample = FS_BITS_PER_SAMPLE(config->format);
     
     memcpy( header.Subchunk2ID, "data", 4 );
     header.Subchunk2Size = 0;
     
     if (write( data->fd, &header, sizeof(header) ) < sizeof(header)) {
          D_ERROR( "FusionSound/Device/Wave: write error!\n" );
          return DFB_IO;
     }    
     
     data->bytes_per_frame = config->channels *
                             FS_BITS_PER_SAMPLE(config->format) >> 3;
     
     return DFB_OK;
}

static void
device_write( void *device_data, void *samples, unsigned int size )
{
     WaveDeviceData *data = device_data;
     
     write( data->fd, samples, size*data->bytes_per_frame );
}

static void
device_get_output_delay( void *device_data, int *delay )
{
     *delay = 0;
}

static void
device_close( void *device_data )
{
     WaveDeviceData *data = device_data;
     off_t           pos;
     
     pos = lseek( data->fd, 0, SEEK_CUR );
     if (pos > 0) {
          __u32 ChunkSize     = pos - 8;
          __u32 Subchunk2Size = pos - sizeof(WaveHeader);
          
          lseek( data->fd, 4, SEEK_SET );
          write( data->fd, &ChunkSize, 4 );
          
          lseek( data->fd, 40, SEEK_SET );
          write( data->fd, &Subchunk2Size, 4 );
     }
     
     close( data->fd );
}
