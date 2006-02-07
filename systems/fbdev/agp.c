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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <errno.h>

#include <linux/pci.h>

#include <directfb.h>

#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/fusion.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <misc/conf.h>

#include "fbdev.h"
#include "agp.h"

#define PAGE_SIZE direct_pagesize()

/*****************************************************************************/

extern FBDev *dfb_fbdev;

static AGPDevice *dfb_agp = NULL;

/*****************************************************************************/

static DFBResult
dfb_agp_info( agp_info *info )
{
     D_ASSERT( info != NULL );
     
     if (ioctl( dfb_agp->fd, AGPIOC_INFO, info )) {
          D_PERROR( "DirectFB/FBDev/agp: Could not get AGP info!\n" );
          return errno2result( errno );
     }
     
     return DFB_OK;
}

static DFBResult
dfb_agp_setup( __u32 mode )
{
     agp_setup setup;

     setup.agp_mode = mode;

     if (ioctl( dfb_agp->fd, AGPIOC_SETUP, &setup )) {
          D_PERROR( "DirectFB/FBDev/agp: AGP setup failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_agp_acquire( void )
{
     if (ioctl( dfb_agp->fd, AGPIOC_ACQUIRE, 0 )) {
          D_PERROR( "DirectFB/FBDev/agp: Acquire failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_agp_release( void )
{
     if (ioctl( dfb_agp->fd, AGPIOC_RELEASE, 0 )) {
          D_PERROR( "DirectFB/FBDev/agp: Release failed!\n" );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_agp_allocate( unsigned long size, int *key )
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

     if (ioctl( dfb_agp->fd, AGPIOC_ALLOCATE, &alloc )) {
          D_PERROR( "DirectFB/FBDev/agp: "
                    "Could not allocate %d pages!\n", pages );
          return errno2result( errno );
     }

     *key = alloc.key;

     return DFB_OK;
}

static DFBResult 
dfb_agp_deallocate( int key )
{
     if (ioctl( dfb_agp->fd, AGPIOC_DEALLOCATE, key )) {
          D_PERROR( "DirectFB/FBDev/agp: "
                    "Deallocate failed (key = %d)!\n", key );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_agp_bind( unsigned int offset, int key )
{
     agp_bind bind;

     if (offset % PAGE_SIZE) {
          D_BUG( "offset is not page-aligned!" );
          return DFB_BUG;
     }

     bind.pg_start = offset / PAGE_SIZE;
     bind.key      = key;

     if (ioctl( dfb_agp->fd, AGPIOC_BIND, &bind )) {
          D_PERROR( "DirectFB/FBDev/agp: "
                    "Bind failed (key = %d, offset = 0x%x)!\n",
                    key, offset );
          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_agp_unbind( int key )
{
     agp_unbind unbind;
     
     unbind.priority = 0;
     unbind.key      = key;

     if (ioctl( dfb_agp->fd, AGPIOC_UNBIND, &unbind )) {
          D_PERROR( "DirectFB/FBDev/agp: "
                    "Unbind failed (key = %d)!\n",
                    key );
          return errno2result( errno );
     }

     return DFB_OK;
}

/*****************************************************************************/

static inline __u16
pci_read_word( int fd, int pos )
{
    __u8 b[2];
    
    if (pread( fd, b, 2, pos ) < 2)
         return 0;
    
    return b[0] | (b[1] << 8);
}

static inline __u8
pci_read_byte( int fd, int pos )
{
     __u8 b;
    
     if (pread( fd, &b, 1, pos ) < 1)
          return 0;

     return b;
}

static bool
dfb_agp_capable( int bus, int dev, int func )
{
     bool found = false;
     char path[22];
     int  fd;

     /* XXX: the following detection method requires suid root */

     snprintf( path, sizeof(path), 
               "/proc/bus/pci/%02x/%02x.%01x", bus, dev, func );

     fd = open( path, O_RDONLY | O_SYNC );
     if (fd < 0) {
          D_PERROR( "DirectFB/FBDev/agp: "
                    "Couldn't open '%s'!\n", path );
          return false;
     }
    
     /* stolen from linux/drivers/pci/pci.c */
     if (pci_read_word( fd, PCI_STATUS ) & PCI_STATUS_CAP_LIST) {
          int pos, id;
          int ttl = 48;
          
          pos = pci_read_byte( fd, PCI_CAPABILITY_LIST );
          while (ttl-- && pos >= 0x40) {
               pos &= ~3;
               
               id = pci_read_byte( fd, pos+PCI_CAP_LIST_ID );
               if (id == 0xff)
                    break;
               if (id == PCI_CAP_ID_AGP) {
                    found = true;
                    break;
               }
               
               pos = pci_read_byte( fd, pos+PCI_CAP_LIST_NEXT );
          }
     }

     close( fd );

     return found;
}     

/*****************************************************************************/

DFBResult
dfb_agp_initialize( void )
{
     AGPShared     *shared;
     unsigned int   agp_avail;
     DFBResult      ret = DFB_FAILURE;
     
     if (dfb_agp) {
          D_BUG( "dfb_agp_initialize() already called!" );
          return DFB_BUG;
     }

     /* Precheck for AGP capable device. */
     if (!dfb_agp_capable( dfb_fbdev->shared->pci.bus,
                           dfb_fbdev->shared->pci.dev,
                           dfb_fbdev->shared->pci.func ))
          return DFB_UNSUPPORTED;
     
     dfb_agp = D_CALLOC( 1, sizeof(AGPDevice) );
     if (!dfb_agp)
          return D_OOM();

     shared = SHCALLOC( dfb_fbdev->shared->shmpool, 1, sizeof(AGPShared) );
     if (!shared) {
          D_ERROR( "DirectFB/FBDev/agp: Could not allocate shared memory!\n" );
          ret = DFB_NOSHAREDMEMORY;
          goto error0;
     }

     dfb_agp->fd = direct_try_open( "/dev/agpgart", 
                                    "/dev/misc/agpgart", O_RDWR, true );
     if (dfb_agp->fd < 0) { 
          ret = errno2result( errno );
          D_ERROR( "DirectFB/FBDev/agp: Error opening AGP device!\n" );
          goto error1;
     }

     ret = dfb_agp_acquire();
     if (ret)
          goto error2;

     ret = dfb_agp_info( &shared->info );
     if (ret)
          goto error2;

     D_DEBUG( "DirectFB/FBDev/agp: "
              "Bridge supports: AGP%s%s%s%s%s%s\n",
              shared->info.agp_mode & 0x001 ? " 1X" : "",
              shared->info.agp_mode & 0x002 ? " 2X" : "",
              shared->info.agp_mode & 0x004 ? " 4X" : "",
              shared->info.agp_mode & 0x008 ? " 8X" : "",
              shared->info.agp_mode & 0x200 ? ", SBA" : "",
              shared->info.agp_mode & 0x010 ? ", FW" : "" );

     shared->info.agp_mode &= ~0xf;
     shared->info.agp_mode |= dfb_config->agp;
     shared->info.agp_mode |= dfb_config->agp - 1;
     
     ret = dfb_agp_setup( shared->info.agp_mode );
     if (ret)
          goto error2;
     dfb_agp_info( &shared->info );

     D_DEBUG( "DirectFB/FBDev/agp: "
              "AGP aperture at 0x%x (%d MB)\n",
              (unsigned int)shared->info.aper_base, shared->info.aper_size );

     agp_avail = (shared->info.pg_total - shared->info.pg_used) * PAGE_SIZE;
     if (agp_avail == 0) {
          D_ERROR( "DirectFB/FBDev/agp: No AGP memory available!\n" );
          ret = DFB_INIT;
          goto error2;
     }

     shared->agp_mem = shared->info.aper_size << 20;
     if (shared->agp_mem > agp_avail)
          shared->agp_mem = agp_avail;
     
     ret = dfb_agp_allocate( shared->agp_mem, &shared->agp_key );
     if (ret)
          goto error3;

     ret = dfb_agp_bind( shared->agp_key, 0 );
     if (ret)
          goto error4;

     dfb_agp->base = mmap( NULL, shared->info.aper_size << 20,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           dfb_agp->fd, 0 );
     if (dfb_agp->base == MAP_FAILED) {
          D_PERROR( "DirectFB/FBDev/agp: Could not mmap the AGP aperture!\n" );
          ret = DFB_INIT;
          goto error5;
     }

     dfb_agp_release();

     dfb_fbdev->agp = dfb_agp;
     dfb_fbdev->shared->agp = shared;
              
     return DFB_OK;
     
error5:
     dfb_agp_unbind( shared->agp_key );
error4:
     dfb_agp_deallocate( shared->agp_key );
error3:
     dfb_agp_release();
error2:
     close( dfb_agp->fd );
error1:
     SHFREE( dfb_fbdev->shared->shmpool, shared );
error0:
     D_FREE( dfb_agp );
     dfb_agp = NULL;
     
     return ret;
}

DFBResult
dfb_agp_join( void )
{
     AGPShared *shared;
     DFBResult  ret    = DFB_FAILURE;
     
     if (dfb_agp) {
          D_BUG( "dfb_agp_join() already called!" );
          return DFB_BUG;
     }

     shared = dfb_fbdev->shared->agp;
     if (!shared)
          return DFB_OK;

     dfb_agp = D_CALLOC( 1, sizeof(AGPDevice) );
     if (!dfb_agp)
          return D_OOM();

     dfb_agp->fd = direct_try_open( "/dev/agpgart", 
                                    "/dev/misc/agpgart", O_RDWR, true );
     if (dfb_agp->fd < 0) { 
          ret = errno2result( errno );
          D_ERROR( "DirectFB/FBDev/agp: Error opening AGP device!\n" );
          goto error0;
     }

     ret = dfb_agp_acquire();
     if (ret)
          goto error1;

     dfb_agp->base = mmap( NULL, shared->info.aper_size << 20,
                           PROT_READ | PROT_WRITE, MAP_SHARED,
                           dfb_agp->fd, 0 );
     if (dfb_agp->base == MAP_FAILED) {
          D_PERROR( "DirectFB/FBDev/agp: Could not mmap the AGP aperture!\n" );
          ret = DFB_INIT;
          goto error2;
     }

     D_DEBUG( "DirectFB/FBDev/agp: AGP aperture mapped at %p\n", dfb_agp->base );

     dfb_agp_release();

     dfb_fbdev->agp = dfb_agp;

     return DFB_OK;
     
error2:
     dfb_agp_release();
error1:
     close( dfb_agp->fd );
error0:
     D_FREE( dfb_agp );
     dfb_agp = NULL;
     
     return ret;
}     

DFBResult
dfb_agp_shutdown( void )
{
     AGPShared *shared;

     if (!dfb_agp)
          return DFB_INVARG;
    
     shared = dfb_fbdev->shared->agp;
     
     dfb_agp_acquire();

     munmap( dfb_agp->base, shared->info.aper_size << 20 );     

     dfb_agp_unbind( shared->agp_key );
     dfb_agp_deallocate( shared->agp_key );

     dfb_agp_release();
     close( dfb_agp->fd );

     SHFREE( dfb_fbdev->shared->shmpool, shared );
     D_FREE( dfb_agp );
     
     dfb_fbdev->shared->agp = NULL;
     dfb_fbdev->agp = dfb_agp = NULL;

     return DFB_OK;
}

DFBResult
dfb_agp_leave( void )
{
     AGPShared *shared;
     
     if (!dfb_agp)
          return DFB_INVARG;

     shared = dfb_fbdev->shared->agp;

     dfb_agp_acquire();

     munmap( dfb_agp->base, shared->info.aper_size << 20 );
     
     dfb_agp_release();
     
     close( dfb_agp->fd );
     D_FREE( dfb_agp );
     
     dfb_fbdev->agp = dfb_agp = NULL;

     return DFB_OK;
}

