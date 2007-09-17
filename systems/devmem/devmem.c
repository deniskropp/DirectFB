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

#include <direct/mem.h>

#include <fusion/arena.h>
#include <fusion/shmalloc.h>

#include <core/core.h>
#include <core/surface_pool.h>

#include <misc/conf.h>

#include "devmem.h"
#include "surfacemanager.h"


#include <core/core_system.h>

DFB_CORE_SYSTEM( devmem )

/**********************************************************************************************************************/

static DevMemData *m_data;    /* FIXME: Fix Core System API to pass data in all functions. */

/**********************************************************************************************************************/

static DFBResult
MapMemAndReg( DevMemData    *data,
              unsigned long  mem_phys,
              unsigned int   mem_length,
              unsigned long  reg_phys,
              unsigned int   reg_length )
{
     int fd;

     fd = open( "/dev/mem", O_RDWR | O_SYNC );
     if (fd < 0) {
          D_PERROR( "System/DevMem: Opening '%s' failed!\n", DEV_MEM );
          return DFB_INIT;
     }

     data->mem = mmap( NULL, mem_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, mem_phys );
     if (data->mem == MAP_FAILED) {
          D_PERROR( "System/DevMem: Mapping %d bytes at 0x%08lx via '%s' failed!\n", mem_length, mem_phys, DEV_MEM );
          return DFB_INIT;
     }

     if (reg_phys && reg_length) {
          data->reg = mmap( NULL, reg_length, PROT_READ | PROT_WRITE, MAP_SHARED, fd, reg_phys );
          if (data->reg == MAP_FAILED) {
               D_PERROR( "System/DevMem: Mapping %d bytes at 0x%08lx via '%s' failed!\n", reg_length, reg_phys, DEV_MEM );
               munmap( data->mem, mem_length );
               close( fd );
               return DFB_INIT;
          }
     }

     close( fd );

     return DFB_OK;
}

static void
UnmapMemAndReg( DevMemData   *data,
                unsigned int  mem_length,
                unsigned int  reg_length )
{
     munmap( data->mem, mem_length );

     if (reg_length)
          munmap( (void*) data->reg, reg_length );
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
system_initialize( CoreDFB *core, void **ret_data )
{
     DFBResult            ret;
     DevMemData          *data;
     DevMemDataShared    *shared;
     FusionSHMPoolShared *pool;

     D_ASSERT( m_data == NULL );

     if (!dfb_config->video_phys || !dfb_config->video_length) {
          D_ERROR( "System/DevMem: Please supply 'video-phys = 0xXXXXXXXX' and 'video-length = XXXX' options!\n" );
          return DFB_INVARG;
     }

     if (dfb_config->mmio_phys && !dfb_config->mmio_length) {
          D_ERROR( "System/DevMem: Please supply both 'mmio-phys = 0xXXXXXXXX' and 'mmio-length = XXXX' options or none!\n" );
          return DFB_INVARG;
     }

     data = D_CALLOC( 1, sizeof(DevMemData) );
     if (!data)
          return D_OOM();

     pool = dfb_core_shmpool( core );

     shared = SHCALLOC( pool, 1, sizeof(DevMemDataShared) );
     if (!shared) {
          D_FREE( data );
          return D_OOSHM();
     }

     data->shared = shared;

     ret = MapMemAndReg( data,
                         dfb_config->video_phys, dfb_config->video_length,
                         dfb_config->mmio_phys,  dfb_config->mmio_length );
     if (ret) {
          SHFREE( pool, shared );
          D_FREE( data );
          return ret;
     }


     *ret_data = m_data = data;

     dfb_surface_pool_initialize( core, &devmemSurfacePoolFuncs, &shared->pool );

     fusion_arena_add_shared_field( dfb_core_arena( core ), "devmem", shared );

     return DFB_OK;
}

static DFBResult
system_join( CoreDFB *core, void **ret_data )
{
     DFBResult         ret;
     void             *tmp;
     DevMemData       *data;
     DevMemDataShared *shared;

     D_ASSERT( m_data == NULL );

     if (!dfb_config->video_phys || !dfb_config->video_length) {
          D_ERROR( "System/DevMem: Please supply 'video-phys = 0xXXXXXXXX' and 'video-length = XXXX' options!\n" );
          return DFB_INVARG;
     }

     if (dfb_config->mmio_phys && !dfb_config->mmio_length) {
          D_ERROR( "System/DevMem: Please supply both 'mmio-phys = 0xXXXXXXXX' and 'mmio-length = XXXX' options or none!\n" );
          return DFB_INVARG;
     }

     data = D_CALLOC( 1, sizeof(DevMemData) );
     if (!data)
          return D_OOM();

     ret = fusion_arena_get_shared_field( dfb_core_arena( core ), "devmem", &tmp );
     if (ret) {
          D_FREE( data );
          return ret;
     }

     data->shared = shared = tmp;

     ret = MapMemAndReg( data,
                         dfb_config->video_phys, dfb_config->video_length,
                         dfb_config->mmio_phys,  dfb_config->mmio_length );
     if (ret) {
          D_FREE( data );
          return ret;
     }

     *ret_data = m_data = data;

     dfb_surface_pool_join( core, shared->pool, &devmemSurfacePoolFuncs );

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DevMemDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_destroy( shared->pool );

     UnmapMemAndReg( m_data, dfb_config->video_length, dfb_config->mmio_length );

     SHFREE( shared->shmpool, shared );

     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DevMemDataShared *shared;

     D_ASSERT( m_data != NULL );

     shared = m_data->shared;
     D_ASSERT( shared != NULL );

     dfb_surface_pool_leave( shared->pool );

     UnmapMemAndReg( m_data, dfb_config->video_length, dfb_config->mmio_length );

     D_FREE( m_data );
     m_data = NULL;

     return DFB_OK;
}

static DFBResult
system_suspend()
{
     D_ASSERT( m_data != NULL );

     return DFB_OK;
}

static DFBResult
system_resume()
{
     D_ASSERT( m_data != NULL );

     return DFB_OK;
}

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     D_ASSERT( m_data != NULL );

     return m_data->reg + offset;
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
     D_ASSERT( m_data != NULL );

     return m_data->mem + offset;
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

