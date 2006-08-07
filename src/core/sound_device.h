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

#ifndef __FUSIONSOUND_CORE_SOUND_DEVICE_H__
#define __FUSIONSOUND_CORE_SOUND_DEVICE_H__

#include <direct/modules.h>

#include <fusionsound.h>

#include <core/types_sound.h>


DECLARE_MODULE_DIRECTORY( fs_sound_drivers );

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define FS_SOUND_DRIVER_ABI_VERSION           1

#define FS_SOUND_DRIVER_INFO_NAME_LENGTH     40
#define FS_SOUND_DRIVER_INFO_VENDOR_LENGTH   60
#define FS_SOUND_DRIVER_INFO_URL_LENGTH     100
#define FS_SOUND_DRIVER_INFO_LICENSE_LENGTH  40

#define FS_SOUND_DEVICE_INFO_NAME_LENGTH    256


typedef struct {
     int          major;        /* major version */
     int          minor;        /* minor version */
} SoundDriverVersion;           /* major.minor, e.g. 0.1 */

typedef struct {
     SoundDriverVersion version;

     char               name[FS_SOUND_DRIVER_INFO_NAME_LENGTH];
                                /* Name of driver, e.g. 'OSS' */

     char               vendor[FS_SOUND_DRIVER_INFO_VENDOR_LENGTH];
                                /* Vendor (or author) of the driver,
                                   e.g. 'directfb.org' */

     char               url[FS_SOUND_DRIVER_INFO_URL_LENGTH];
                                /* URL for driver updates,
                                   e.g. 'http://www.directfb.org/' */

     char               license[FS_SOUND_DRIVER_INFO_LICENSE_LENGTH];
                                /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int       device_data_size;
} SoundDriverInfo;


typedef enum {
     DCF_WRITEBLOCKS = 0x00000001  /* This device write blocks instead of frames,
                                      therefore the amount of frames to write must
                                      be multiple of buffersize */
} DeviceCapabilities;

typedef struct {
     char                name[FS_SOUND_DEVICE_INFO_NAME_LENGTH];
                                   /* Device name, e.g 'Intel 8x0' */
     
     /* Flags representing device capabilities */                             
     DeviceCapabilities  caps;  
} SoundDeviceInfo;  


/* Device Configuration. */
typedef struct {
     unsigned int    channels;
     FSSampleFormat  format;
     unsigned int    rate;       /* only suggested, the driver can modify it */
     unsigned int    buffersize; /* only suggested, the driver can modify it */
} CoreSoundDeviceConfig;


/* Device funcs. */
typedef struct {
     DFBResult (*Probe)            ( void );
     
     /* Get device driver information. */
     void      (*GetDriverInfo)    ( SoundDriverInfo       *info);

     /* Open the device, get device information and apply given configuration. */
     DFBResult (*OpenDevice)       ( void                  *device_data,
                                     SoundDeviceInfo       *device_info,
                                     CoreSoundDeviceConfig *config );
     
     /* Write "count" frames. */
     void      (*Write)            ( void                  *device_data,
                                     void                  *samples,
                                     unsigned int           count );
     
     /* Get output delay in frames. */                             
     void      (*GetOutputDelay)   ( void                  *device_data,
                                     int                   *delay );
     
     /* Close device. */
     void      (*CloseDevice)      ( void                  *device_data );
} SoundDriverFuncs;
     
     
DFBResult fs_device_initialize( CoreSound *core, CoreSoundDevice **ret_device );
void      fs_device_shutdown  ( CoreSoundDevice *device );

void      fs_device_get_capabilities( CoreSoundDevice    *device,
                                      DeviceCapabilities *caps );

void      fs_device_get_configuration( CoreSoundDevice       *device, 
                                       CoreSoundDeviceConfig *config );
                                                                                                          
void      fs_device_write( CoreSoundDevice *device, 
                           void            *samples,
                           unsigned int     count );
                           
void      fs_device_get_output_delay( CoreSoundDevice *device, int *delay );
                   
#endif                                   
