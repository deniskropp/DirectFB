/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

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

#include <fcntl.h>
#include <sys/mman.h>

#include <directfb.h>

#include <misc/conf.h>

#include <core/core_system.h>

DFB_CORE_SYSTEM( devmem )

#define DEV_MEM     "/dev/mem"

/**********************************************************************************************************************/

static void          *m_mem;
static volatile void *m_reg;

/**********************************************************************************************************************/

static DFBResult
MapMemAndReg( unsigned long mem_phys,
              unsigned int  mem_length,
              unsigned long reg_phys,
              unsigned int  reg_length )
{
     int fd;

     fd = open( "/dev/mem", O_RDWR | O_SYNC );
     if (fd < 0) {
          D_PERROR( "System/DevMem: Opening '%s' failed!\n", DEV_MEM );
          return DFB_INIT;
     }

     m_mem = mmap( NULL, mem_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mem_phys );
     if (m_mem == MAP_FAILED) {
          D_PERROR( "System/DevMem: Mapping %d bytes at 0x%08lx via '%s' failed!\n", mem_length, mem_phys, DEV_MEM );
          return DFB_INIT;
     }

     if (reg_phys && reg_length) {
          m_reg = mmap( NULL, reg_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, reg_phys );
          if (m_reg == MAP_FAILED) {
               D_PERROR( "System/DevMem: Mapping %d bytes at 0x%08lx via '%s' failed!\n", reg_length, reg_phys, DEV_MEM );
               munmap( m_mem, mem_length );
               close( fd );
               return DFB_INIT;
          }
     }

     close( fd );

     return DFB_OK;
}

static void
UnmapMemAndReg( unsigned int mem_length,
                unsigned int reg_length )
{
     munmap( m_mem, mem_length );

     if (reg_length)
          munmap( (void*) m_reg, reg_length );
}

/**********************************************************************************************************************/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_DEVMEM;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "DevMem" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     if (!dfb_config->video_phys || !dfb_config->video_length) {
          D_ERROR( "System/DevMem: Please supply 'video-phys = 0xXXXXXXXX' and 'video-length = XXXX' options!\n" );
          return DFB_INVARG;
     }

     if (dfb_config->mmio_phys && !dfb_config->mmio_length) {
          D_ERROR( "System/DevMem: Please supply both 'mmio-phys = 0xXXXXXXXX' and 'mmio-length = XXXX' options or none!\n" );
          return DFB_INVARG;
     }

     return MapMemAndReg( dfb_config->video_phys, dfb_config->video_length,
                          dfb_config->mmio_phys,  dfb_config->mmio_length );
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     return system_initialize( core, data );
}

static DFBResult
system_shutdown( bool emergency )
{
     UnmapMemAndReg( dfb_config->video_length, dfb_config->mmio_length );

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     return system_shutdown( emergency );
}

static DFBResult
system_suspend()
{
     return DFB_OK;
}

static DFBResult
system_resume()
{
     return DFB_OK;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
    return m_reg + offset;
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
}

static int
system_get_accelerator()
{
     return dfb_config->accelerator;
}

static VideoMode *
system_get_modes()
{
     return NULL;
}

static VideoMode *
system_get_current_mode()
{
     return NULL;
}

static DFBResult
system_thread_init()
{
     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return dfb_config->video_phys + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return m_mem + offset;
}

static unsigned int
system_videoram_length()
{
     return dfb_config->video_length;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     return NULL;
}

static unsigned int
system_auxram_length()
{
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
     return;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     return;
}

