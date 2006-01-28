/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2006  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <linux/agpgart.h>

#include <directfb.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include "nvidia_agp.h"


#define PAGE_SIZE direct_pagesize()


static DFBResult
nv_agp_info( int fd, agp_info *info )
{
     D_ASSERT( info != NULL );
     
     if (ioctl( fd, AGPIOC_INFO, info )) {
          D_PERROR( "DirectFB/NVidia: Could not get AGP info!\n" );
          return errno2result( errno );
     }
     
     return DFB_OK;
}

static DFBResult
nv_agp_setup( int fd, __u32 mode )
{
     agp_setup setup;

     setup.agp_mode = mode;

     if (ioctl( fd, AGPIOC_SETUP, &setup )) {
          D_PERROR( "DirectFB/NVidia: AGP setup failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
nv_agp_acquire( int fd )
{
     if (ioctl( fd, AGPIOC_ACQUIRE, 0 )) {
          D_PERROR( "DirectFB/NVidia: Acquire failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
nv_agp_release( int fd )
{
     if (ioctl( fd, AGPIOC_RELEASE, 0 )) {
          D_PERROR( "DirectFB/NVidia: Release failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
nv_agp_allocate( int fd, unsigned long size, int *key )
{
     agp_allocate alloc;
     int          pages;

     D_ASSERT( key != NULL );

     pages = size / PAGE_SIZE;
     if (pages % PAGE_SIZE)
          pages++;

     if (pages == 0) {
          D_BUG( "attempted to allocate 0 pages!");
          return DFB_BUG;
     }

     alloc.pg_count = pages;
     alloc.type     = 0;

     if (ioctl( fd, AGPIOC_ALLOCATE, &alloc )) {
          D_PERROR( "DirectFB/NVidia: "
                    "Could not allocate %d pages!\n", pages );
          return errno2result( errno );
     }

     *key = alloc.key;

     return DFB_OK;
}

static DFBResult 
nv_agp_deallocate( int fd, int key )
{
     if (ioctl( fd, AGPIOC_DEALLOCATE, key )) {
          D_PERROR( "DirectFB/NVidia: "
                    "Deallocate failed (key = %d)!\n", key );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
nv_agp_bind( int fd, unsigned int offset, int key )
{
     agp_bind bind;

     if (offset % PAGE_SIZE) {
          D_BUG( "offset is not page-aligned!" );
          return DFB_BUG;
     }

     bind.pg_start = offset / PAGE_SIZE;
     bind.key      = key;

     if (ioctl( fd, AGPIOC_BIND, &bind )) {
          D_PERROR( "DirectFB/NVidia: "
                    "Bind failed (key = %d, offset = 0x%x)!\n",
                    key, offset );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
nv_agp_unbind( int fd, int key )
{
     agp_unbind unbind;
     
     unbind.priority = 0;
     unbind.key      = key;

     if (ioctl( fd, AGPIOC_UNBIND, &unbind )) {
          D_PERROR( "DirectFB/NVidia: "
                    "Unbind failed (key = %d)!\n", key );
          return errno2result( errno );
     }

     return DFB_OK;
}

/*****************************************************************************/

DFBResult
nv_agp_initialize( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     unsigned int agp_avail;
     unsigned int agp_mem;
     agp_info     info;
     DFBResult    ret = DFB_FAILURE;

     nvdrv->agp_fd = direct_try_open( "/dev/agpgart", 
                                      "/dev/misc/agpgart", O_RDWR, false );
     if (nvdrv->agp_fd < 0) { 
          ret = errno2result( errno );
          D_DEBUG( "DirectFB/NVidia: Error opening AGP device!\n" );
          goto error0;
     }

     nv_agp_acquire( nvdrv->agp_fd );

     ret = nv_agp_info( nvdrv->agp_fd, &info );
     if (ret)
          goto error1;

     D_DEBUG( "DirectFB/NVidia: "
              "Bridge supports: AGP%s%s%s%s%s%s\n",
              info.agp_mode & 0x001 ? " 1X" : "",
              info.agp_mode & 0x002 ? " 2X" : "",
              info.agp_mode & 0x004 ? " 4X" : "",
              info.agp_mode & 0x008 ? " 8X" : "",
              info.agp_mode & 0x200 ? ", SBA" : "",
              info.agp_mode & 0x010 ? ", FW" : "" );
     
     ret = nv_agp_setup( nvdrv->agp_fd, info.agp_mode );
     if (ret)
          goto error1;
     nv_agp_info( nvdrv->agp_fd, &info );

     D_DEBUG( "DirectFB/NVidia: "
              "AGP aperture at 0x%08x (%d MB)\n",
              (unsigned int)info.aper_base, info.aper_size );

     agp_avail = (info.pg_total - info.pg_used) * PAGE_SIZE;
     if (agp_avail == 0) {
          D_ERROR( "DirectFB/NVidia: No AGP memory available!\n" );
          ret = DFB_INIT;
          goto error1;
     }

     agp_mem = info.aper_size << 20;
     if (agp_mem > agp_avail)
          agp_mem = agp_avail;
     
     ret = nv_agp_allocate( nvdrv->agp_fd, agp_mem, &nvdev->agp_key );
     if (ret)
          goto error2;

     ret = nv_agp_bind( nvdrv->agp_fd, nvdev->agp_key, 0 );
     if (ret)
          goto error3;

     nvdrv->agp_base = mmap( NULL, info.aper_size << 20,
                            PROT_READ | PROT_WRITE, MAP_SHARED,
                            nvdrv->agp_fd, 0 );
     if (nvdrv->agp_base == MAP_FAILED) {
          D_PERROR( "DirectFB/NVidia: Could not mmap the AGP aperture!\n" );
          ret = DFB_INIT;
          goto error4;
     }

     D_DEBUG( "DirectFB/NVidia: AGP aperture mapped at %p\n", nvdrv->agp_base );

     nv_agp_release( nvdrv->agp_fd );
     
     nvdev->agp_aper_base = info.aper_base;
     nvdev->agp_aper_size = info.aper_size;
              
     return DFB_OK;
     
error4:
     nv_agp_unbind( nvdrv->agp_fd, nvdev->agp_key );
error3:
     nv_agp_deallocate( nvdrv->agp_fd, nvdev->agp_key );
error2:
     nv_agp_release( nvdrv->agp_fd );
error1:
     close( nvdrv->agp_fd );
error0:
     nvdrv->agp_fd = 0;
     
     return ret;
}

DFBResult
nv_agp_join( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     nvdrv->agp_fd = direct_try_open( "/dev/agpgart", 
                                      "/dev/misc/agpgart", O_RDWR, false );
     if (nvdrv->agp_fd < 0) {
          D_PERROR( "DirectFB/NVidia: Error opening AGP device!\n" );
          nvdrv->agp_fd = 0;
          return errno2result( errno );
     }

     nv_agp_acquire( nvdrv->agp_fd );

     nvdrv->agp_base = mmap( NULL, nvdev->agp_aper_size << 20,
                            PROT_READ | PROT_WRITE, MAP_SHARED,
                            nvdrv->agp_fd, 0 );
     if (nvdrv->agp_base == MAP_FAILED) {
          D_PERROR( "DirectFB/NVidia: Could not mmap the AGP aperture!\n" );
          nv_agp_release( nvdrv->agp_fd );
          close( nvdrv->agp_fd );
          nvdrv->agp_fd = 0;
          return errno2result( errno );
     }

     D_DEBUG( "DirectFB/NVidia: AGP aperture mapped at %p\n", nvdrv->agp_base );

     nv_agp_release( nvdrv->agp_fd );

     return DFB_OK;
}     

void
nv_agp_shutdown( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     if (!nvdrv->agp_fd)
          return;
     
     nv_agp_acquire( nvdrv->agp_fd );

     munmap( (void*)nvdrv->agp_base, nvdev->agp_aper_size << 20 );     

     nv_agp_unbind( nvdrv->agp_fd, nvdev->agp_key );
     nv_agp_deallocate( nvdrv->agp_fd, nvdev->agp_key );

     nv_agp_release( nvdrv->agp_fd );
     
     close( nvdrv->agp_fd );
     nvdrv->agp_fd = 0;
}

void
nv_agp_leave( NVidiaDriverData *nvdrv, NVidiaDeviceData *nvdev )
{
     if (!nvdrv->agp_fd)
          return;
     
     nv_agp_acquire( nvdrv->agp_fd );

     munmap( (void*)nvdrv->agp_base, nvdev->agp_aper_size << 20 );
     
     nv_agp_release( nvdrv->agp_fd );
     
     close( nvdrv->agp_fd );
     nvdrv->agp_fd = 0;
}

