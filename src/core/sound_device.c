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

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>

#include <core/sound_device.h>

#include <misc/sound_conf.h>


DEFINE_MODULE_DIRECTORY( fs_sound_drivers, "snddrivers", FS_SOUND_DRIVER_ABI_VERSION );


struct __FS_CoreSoundDevice {
     DirectModuleEntry      *module;
     const SoundDriverFuncs *funcs;

     void                   *device_data;
     SoundDeviceInfo         device_info;
     
     CoreSoundDeviceConfig   config;
};


DFBResult 
fs_device_initialize( CoreSound *core, CoreSoundDevice **ret_device )
{
     DFBResult        ret;
     CoreSoundDevice *device;
     SoundDriverInfo  info;
     DirectLink      *link;
     
     D_ASSERT( core != NULL );
     D_ASSERT( ret_device != NULL );
     
     device = D_CALLOC( 1, sizeof(CoreSoundDevice) );
     if (!device)
          return D_OOM();
          
     /* Build a list of available drivers. */
     direct_modules_explore_directory( &fs_sound_drivers );
     
     /* Load driver */
     direct_list_foreach( link, fs_sound_drivers.entries ) {
          DirectModuleEntry *module = (DirectModuleEntry*) link;
          
          const SoundDriverFuncs *funcs = direct_module_ref( module );
          
          if (!funcs)
               continue;
               
          if (fs_config->driver && strcmp( module->name, fs_config->driver ))
               continue;
               
          if (funcs->Probe() == DFB_OK) {
               device->module = module;
               device->funcs  = funcs;
               
               funcs->GetDriverInfo( &info );
               break;
          }
          
          direct_module_unref( module );
     }
     
     if (!device->module) {
          if (fs_config->driver) {
               D_ERROR( "FusionSound/Device: driver '%s' not found!\n", 
                         fs_config->driver );
          } else {
               D_ERROR( "FusionSound/Device: no driver found!\n" );
          }
          
          D_FREE( device );
          return DFB_FAILURE;
     }
     
     if (info.device_data_size) {
          device->device_data = D_CALLOC( 1, info.device_data_size );
          if (!device->device_data) {
               direct_module_unref( device->module );
               D_FREE( device );
               return D_OOM();
          }
     }
     
     device->config.channels   = fs_config->channels;
     device->config.format     = fs_config->sampleformat;
     device->config.rate       = fs_config->samplerate;
     device->config.buffersize = fs_config->samplerate * fs_config->buffersize / 1000;
     /* No more than 65535 frames. */
     if (device->config.buffersize > 65535)
          device->config.buffersize = 65535;
     
     /* Open sound device. */
     ret = device->funcs->OpenDevice( device->device_data, 
                                     &device->device_info, &device->config );
     if (ret) {
          D_ERROR( "FusionSound/Device: could not open device!\n" );
          direct_module_unref( device->module );
          D_FREE( device );
          return ret;
     }
     
     D_INFO( "FusionSound/Device: %s %d.%d (%s)\n",
             info.name, info.version.major, info.version.minor, info.vendor );
             
     D_INFO( "FusionSound/Device: %d Hz, %d channel(s), %d bits, %.1f ms.\n",
             device->config.rate,
             device->config.channels,
             FS_BITS_PER_SAMPLE(device->config.format),
             (float)device->config.buffersize/device->config.rate*1000.0f  );
     
     *ret_device = device;
     
     return DFB_OK;
}

void
fs_device_get_description( CoreSoundDevice     *device,
                           FSDeviceDescription *desc )
{
     SoundDriverInfo info;
     
     D_ASSERT( device != NULL );
     D_ASSERT( device->funcs != NULL );
     D_ASSERT( desc != NULL );
     
     device->funcs->GetDriverInfo( &info );
     
     strcpy( desc->name, device->device_info.name );
     memcpy( &desc->driver, &info, sizeof(FSSoundDriverInfo) );
} 

void
fs_device_get_capabilities( CoreSoundDevice    *device,
                            DeviceCapabilities *caps )
{
     D_ASSERT( device != NULL );
     D_ASSERT( caps != NULL );
     
     *caps = device->device_info.caps;
}

void
fs_device_get_configuration( CoreSoundDevice       *device, 
                             CoreSoundDeviceConfig *config )
{
     D_ASSERT( device != NULL );
     D_ASSERT( config != NULL );
     
     *config = device->config;
}
   
void 
fs_device_write( CoreSoundDevice *device,
                 void            *samples,
                 unsigned int     count )
{
     D_ASSERT( device != NULL );
     D_ASSERT( device->funcs != NULL );
     D_ASSERT( samples != NULL );
     
     device->funcs->Write( device->device_data, samples, count );
}

void
fs_device_get_output_delay( CoreSoundDevice *device,
                            int             *delay )
{
     D_ASSERT( device != NULL );
     D_ASSERT( device->funcs != NULL );
     D_ASSERT( delay != NULL );
     
     device->funcs->GetOutputDelay( device->device_data, delay );
}

void
fs_device_shutdown( CoreSoundDevice *device )
{
     D_ASSERT( device != NULL );
     D_ASSERT( device->funcs != NULL );
     
     device->funcs->CloseDevice( device->device_data );
     
     direct_module_unref( device->module );
     
     if (device->device_data)
          D_FREE( device->device_data );
     
     D_FREE( device );
}

