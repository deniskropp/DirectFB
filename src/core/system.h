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

#ifndef __DFB__CORE__SYSTEM_H__
#define __DFB__CORE__SYSTEM_H__

#include <core/coretypes.h>
#include <core/fusion/fusion_types.h>

typedef enum {
     CORE_ANY,
     CORE_FBDEV,
     CORE_SDL
} CoreSystemType;

/*
 * hold information of a Videomode read from /etc/fb.modes
 * (to be replaced by DirectFB's own config system)
 */
typedef struct _VideoMode {
     int xres;
     int yres;
     int bpp;

     int pixclock;
     int left_margin;
     int right_margin;
     int upper_margin;
     int lower_margin;
     int hsync_len;
     int vsync_len;
     int hsync_high;
     int vsync_high;
     int csync_high;

     int laced;
     int doubled;

     int sync_on_green;
     int external_sync;

     struct _VideoMode *next;
} VideoMode;

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_CORE_SYSTEM_ABI_VERSION           1

#define DFB_CORE_SYSTEM_INFO_NAME_LENGTH     60
#define DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH   80
#define DFB_CORE_SYSTEM_INFO_URL_LENGTH     120
#define DFB_CORE_SYSTEM_INFO_LICENSE_LENGTH  40


typedef struct {
     int          major;        /* major version */
     int          minor;        /* minor version */
} CoreSystemVersion;        /* major.minor, e.g. 0.1 */

typedef struct {
     CoreSystemVersion  version;

     CoreSystemType     type;
     
     char               name[DFB_CORE_SYSTEM_INFO_NAME_LENGTH+1];
                                /* Name of system, e.g. 'FBDev' */

     char               vendor[DFB_CORE_SYSTEM_INFO_VENDOR_LENGTH+1];
                                /* Vendor (or author) of the driver,
                                   e.g. 'convergence' or 'Denis Oliver Kropp' */

     char               url[DFB_CORE_SYSTEM_INFO_URL_LENGTH+1];
                                /* URL for driver updates,
                                   e.g. 'http://www.directfb.org/' */

     char               license[DFB_CORE_SYSTEM_INFO_LICENSE_LENGTH+1];
                                /* License, e.g. 'LGPL' or 'proprietary' */
} CoreSystemInfo;

typedef struct {
     int            (*GetAbiVersion)();

     void           (*GetSystemInfo)( CoreSystemInfo *info );

     DFBResult      (*Initialize)();
     DFBResult      (*Join)();

     DFBResult      (*Shutdown)( bool emergency );
     DFBResult      (*Leave)( bool emergency );

     DFBResult      (*Suspend)();
     DFBResult      (*Resume)();

     /*
      * return when vertical retrace is reached, works with matrox kernel patch
      * only for now
      */
     DFBResult      (*WaitVSync)();

     VideoMode*     (*GetModes)();
     VideoMode*     (*GetCurrentMode)();

     /*
      * Graphics drivers call this function to get access to MMIO regions.
      *
      * device: Graphics device to map
      * offset: Offset from MMIO base (default offset is 0)
      * length: Length of mapped region (-1 uses default length)
      *
      * Returns the virtual address or NULL if mapping failed.
      */
     volatile void* (*MapMMIO)( unsigned int    offset,
                                int             length );
     
     /*
      * Graphics drivers call this function to unmap MMIO regions.
      *
      * addr:   Virtual address returned by gfxcard_map_mmio
      * length: Length of mapped region (-1 uses default length)
      */
     void           (*UnmapMMIO)( volatile void  *addr,
                                  int             length );
     
     int            (*GetAccelerator)();
     
     unsigned long  (*VideoMemoryPhysical)( unsigned int offset );
     void*          (*VideoMemoryVirtual)( unsigned int offset );
     
     unsigned int   (*VideoRamLength)();
} CoreSystemFuncs;

void dfb_system_register_module( CoreSystemFuncs *funcs );



DFBResult
dfb_system_initialize();

DFBResult
dfb_system_join();

DFBResult
dfb_system_shutdown( bool emergency );

DFBResult
dfb_system_leave( bool emergency );

CoreSystemType
dfb_system_type();

volatile void *
dfb_system_map_mmio( unsigned int    offset,
                     int             length );

void
dfb_system_unmap_mmio( volatile void  *addr,
                       int             length );

int
dfb_system_get_accelerator();

VideoMode *
dfb_system_modes();

VideoMode *
dfb_system_current_mode();

DFBResult
dfb_system_wait_vsync();

unsigned long
dfb_system_video_memory_physical( unsigned int offset );

void *
dfb_system_video_memory_virtual( unsigned int offset );

unsigned int
dfb_system_videoram_length();

#endif

