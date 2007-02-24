/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <strings.h>
#if defined(HAVE_SYSIO)
# include <sys/io.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/kd.h>

#include <pthread.h>

#ifdef USE_SYSFS
# include <sysfs/libsysfs.h>
#endif

#include <asm/types.h>    /* Needs to be included before dfb_types.h */

#include <fusion/arena.h>
#include <fusion/fusion.h>
#include <fusion/reactor.h>
#include <fusion/shmalloc.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/layer_control.h>
#include <core/layers.h>
#include <core/gfxcard.h>
#include <core/palette.h>
#include <core/screen.h>
#include <core/screens.h>
#include <core/surfaces.h>
#include <core/surfacemanager.h>
#include <core/state.h>
#include <core/windows.h>

#include <gfx/convert.h>

#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/signals.h>
#include <direct/system.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include "fbdev.h"
#include "fb.h"
#include "vt.h"
#include "agp.h"

#include <core/core_system.h>

DFB_CORE_SYSTEM( fbdev )


static int fbdev_ioctl_call_handler( int   caller,
                                     int   call_arg,
                                     void *call_ptr,
                                     void *ctx );

static int fbdev_ioctl( int request, void *arg, int arg_size );

#define FBDEV_IOCTL(request,arg)   fbdev_ioctl( request, arg, sizeof(*(arg)) )

FBDev *dfb_fbdev = NULL;

/******************************************************************************/

static int       primaryLayerDataSize ();

static int       primaryRegionDataSize();

static DFBResult primaryInitLayer     ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        DFBDisplayLayerDescription *description,
                                        DFBDisplayLayerConfig      *config,
                                        DFBColorAdjustment         *adjustment );

static DFBResult primarySetColorAdjustment( CoreLayer              *layer,
                                            void                   *driver_data,
                                            void                   *layer_data,
                                            DFBColorAdjustment     *adjustment );

static DFBResult primaryTestRegion    ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        CoreLayerRegionConfig      *config,
                                        CoreLayerRegionConfigFlags *failed );

static DFBResult primaryAddRegion     ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data,
                                        CoreLayerRegionConfig      *config );

static DFBResult primarySetRegion     ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data,
                                        CoreLayerRegionConfig      *config,
                                        CoreLayerRegionConfigFlags  updated,
                                        CoreSurface                *surface,
                                        CorePalette                *palette );

static DFBResult primaryRemoveRegion  ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data );

static DFBResult primaryFlipRegion    ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data,
                                        CoreSurface                *surface,
                                        DFBSurfaceFlipFlags         flags );


static DFBResult primaryAllocateSurface  ( CoreLayer                  *layer,
                                           void                       *driver_data,
                                           void                       *layer_data,
                                           void                       *region_data,
                                           CoreLayerRegionConfig      *config,
                                           CoreSurface               **ret_surface );

static DFBResult primaryReallocateSurface( CoreLayer                  *layer,
                                           void                       *driver_data,
                                           void                       *layer_data,
                                           void                       *region_data,
                                           CoreLayerRegionConfig      *config,
                                           CoreSurface                *surface );

static DisplayLayerFuncs primaryLayerFuncs = {
     LayerDataSize:      primaryLayerDataSize,
     RegionDataSize:     primaryRegionDataSize,
     InitLayer:          primaryInitLayer,

     SetColorAdjustment: primarySetColorAdjustment,

     TestRegion:         primaryTestRegion,
     AddRegion:          primaryAddRegion,
     SetRegion:          primarySetRegion,
     RemoveRegion:       primaryRemoveRegion,
     FlipRegion:         primaryFlipRegion,

     AllocateSurface:    primaryAllocateSurface,
     ReallocateSurface:  primaryReallocateSurface,
     /* default DeallocateSurface copes with our chunkless video buffers */
};

/******************************************************************************/

static DFBResult primaryInitScreen  ( CoreScreen           *screen,
                                      GraphicsDevice       *device,
                                      void                 *driver_data,
                                      void                 *screen_data,
                                      DFBScreenDescription *description );

static DFBResult primarySetPowerMode( CoreScreen           *screen,
                                      void                 *driver_data,
                                      void                 *screen_data,
                                      DFBScreenPowerMode    mode );

static DFBResult primaryWaitVSync   ( CoreScreen           *screen,
                                      void                 *driver_data,
                                      void                 *layer_data );

static DFBResult primaryGetScreenSize( CoreScreen           *screen,
                                       void                 *driver_data,
                                       void                 *screen_data,
                                       int                  *ret_width,
                                       int                  *ret_height );

static ScreenFuncs primaryScreenFuncs = {
     .InitScreen    = primaryInitScreen,
     .SetPowerMode  = primarySetPowerMode,
     .WaitVSync     = primaryWaitVSync,
     .GetScreenSize = primaryGetScreenSize
};

/******************************************************************************/

static DFBResult dfb_fbdev_read_modes();
static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format );
static DFBResult dfb_fbdev_set_palette( CorePalette *palette );
static DFBResult dfb_fbdev_set_rgb332_palette();
static DFBResult dfb_fbdev_pan( int offset, bool onsync );
static DFBResult dfb_fbdev_blank( int level );
static DFBResult dfb_fbdev_set_mode( CoreSurface           *surface,
                                     VideoMode             *mode,
                                     CoreLayerRegionConfig *config );
static void      dfb_fbdev_var_to_mode( struct fb_var_screeninfo *var,
                                        VideoMode                *mode );

/******************************************************************************/

static inline
void waitretrace (void)
{
#if defined(HAVE_INB_OUTB_IOPL)
     if (iopl(3))
          return;

     if (!(inb (0x3cc) & 1)) {
          while ((inb (0x3ba) & 0x8))
               ;

          while (!(inb (0x3ba) & 0x8))
               ;
     }
     else {
          while ((inb (0x3da) & 0x8))
               ;

          while (!(inb (0x3da) & 0x8))
               ;
     }
#endif
}

/******************************************************************************/

static DFBResult dfb_fbdev_open()
{
     DFBResult error_result = DFB_FAILURE;

     if (dfb_config->fb_device) {
          dfb_fbdev->fd = open( dfb_config->fb_device, O_RDWR );
          if (dfb_fbdev->fd < 0) {
               D_PERROR( "DirectFB/FBDev: Error opening '%s'!\n",
                         dfb_config->fb_device);

               error_result = errno2result( errno );
               goto error;
          }
     }
     else if (getenv( "FRAMEBUFFER" ) && *getenv( "FRAMEBUFFER" ) != '\0') {
          dfb_fbdev->fd = open( getenv ("FRAMEBUFFER"), O_RDWR );
          if (dfb_fbdev->fd < 0) {
               D_PERROR( "DirectFB/FBDev: Error opening '%s'!\n",
                          getenv ("FRAMEBUFFER"));

               error_result = errno2result( errno );
               goto error;
          }
     }
     else {
          dfb_fbdev->fd = direct_try_open( "/dev/fb0", "/dev/fb/0", O_RDWR, true );
          if (dfb_fbdev->fd < 0) {
               D_ERROR( "DirectFB/FBDev: Error opening framebuffer device!\n" );
               D_ERROR( "DirectFB/FBDev: Use 'fbdev' option or set FRAMEBUFFER environment variable.\n" );
               error_result = DFB_INIT;
               goto error;
          }
     }

     /* should be closed automatically in children upon exec(...) */
     if (fcntl( dfb_fbdev->fd, F_SETFD, FD_CLOEXEC ) < 0)
     {
          D_PERROR( "Fusion/Init: Setting FD_CLOEXEC flag failed!\n" );
          goto error;
     }

     return DFB_OK;
error:
     return error_result; 
}

/******************************************************************************/

static void
dfb_fbdev_get_pci_info( FBDevShared *shared )
{
     char buf[512];
     int  vendor = -1;
     int  model  = -1;

#ifdef USE_SYSFS
     if (!sysfs_get_mnt_path( buf, 512 )) {
          struct sysfs_class_device *classdev;
          struct sysfs_device       *device;
          struct sysfs_attribute    *attr;
          char                      *fbdev;
          char                       dev[5] = { 'f', 'b', '0', 0, 0 };
          
          fbdev = dfb_config->fb_device;
          if (!fbdev)
               fbdev = getenv( "FRAMEBUFFER" );
          
          if (fbdev) {
               if (!strncmp( fbdev, "/dev/fb/", 8 ))
                    snprintf( dev, 5, "fb%s", fbdev+8 );
               else if (!strncmp( fbdev, "/dev/fb", 7 ))
                    snprintf( dev, 5, "fb%s", fbdev+7 );
          }    
          
          classdev = sysfs_open_class_device( "graphics", dev );
          if (classdev) {
               device = sysfs_get_classdev_device( classdev );
               
               if (device) {
                    attr = sysfs_get_device_attr( device, "vendor" );
                    if (attr)
                           sscanf( attr->value, "0x%04x", &vendor );
                           
                    attr = sysfs_get_device_attr( device, "device" );
                    if (attr)
                         sscanf( attr->value, "0x%04x", &model );
                         
                    if (vendor != -1 && model != -1) {
                         sscanf( device->name, "0000:%02x:%02x.%1x", 
                                 &shared->pci.bus, 
                                 &shared->pci.dev, 
                                 &shared->pci.func );
                    
                         shared->device.vendor = vendor;
                         shared->device.model  = model;
                    }
               }
               
               sysfs_close_class_device( classdev );
          }     
     }
#endif /* USE_SYSFS */

     /* try /proc interface */
     if (vendor == -1 || model == -1) {
          FILE *fp;
          int   id;
          int   bus;
          int   dev;
          int   func;

          fp = fopen( "/proc/bus/pci/devices", "r" );
          if (!fp) {
               D_DEBUG( "DirectFB/FBDev: "
                        "couldn't access /proc/bus/pci/devices!\n" );
               return;
          }

          while (fgets( buf, 512, fp )) {
               if (sscanf( buf, "%04x\t%04x%04x", &id, &vendor, &model ) == 3) {
                    bus  = (id & 0xff00) >> 8;
                    dev  = (id & 0x00ff) >> 3;
                    func = (id & 0x0007);
                    
                    if (bus  == dfb_config->pci.bus &&
                        dev  == dfb_config->pci.dev &&
                        func == dfb_config->pci.func) 
                    {
                         shared->pci.bus  = bus;
                         shared->pci.dev  = dev;
                         shared->pci.func = func;
                         
                         shared->device.vendor = vendor;
                         shared->device.model  = model;
                         
                         break;
                    }
               }
          }

          fclose( fp );
     }
}


/** public **/

static void
system_get_info( CoreSystemInfo *info )
{
     info->type = CORE_FBDEV;
     info->caps = CSCAPS_ACCELERATION;

     snprintf( info->name, DFB_CORE_SYSTEM_INFO_NAME_LENGTH, "FBDev" );
}

static DFBResult
system_initialize( CoreDFB *core, void **data )
{
     DFBResult            ret;
     CoreScreen          *screen;
     long                 page_size;
     FBDevShared         *shared = NULL;
     FusionSHMPoolShared *pool;
     FusionSHMPoolShared *pool_data;

     D_ASSERT( dfb_fbdev == NULL );

     pool      = dfb_core_shmpool( core );
     pool_data = dfb_core_shmpool_data( core );

     dfb_fbdev = D_CALLOC( 1, sizeof(FBDev) );
     if (!dfb_fbdev)
          return D_OOM();

     dfb_fbdev->fd = -1;

     shared = (FBDevShared*) SHCALLOC( pool, 1, sizeof(FBDevShared) );
     if (!shared) {
          ret = D_OOSHM();
          goto error;
     }

     shared->shmpool      = pool;
     shared->shmpool_data = pool_data;

     fusion_arena_add_shared_field( dfb_core_arena( core ), "fbdev", shared );

     dfb_fbdev->core   = core;
     dfb_fbdev->shared = shared;

     page_size = direct_pagesize();

     shared->page_mask = page_size < 0 ? 0 : (page_size - 1);

     ret = dfb_fbdev_open();
     if (ret)
          goto error;

     if (dfb_config->vt) {
          ret = dfb_vt_initialize();
          if (ret)
               goto error;
     }

     ret = DFB_INIT;

     /* Retrieve fixed informations like video ram size */
     if (ioctl( dfb_fbdev->fd, FBIOGET_FSCREENINFO, &shared->fix ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not get fixed screen information!\n" );
          goto error;
     }

     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, shared->fix.smem_len,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         dfb_fbdev->fd, 0 );
     if (dfb_fbdev->framebuffer_base == MAP_FAILED) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not mmap the framebuffer!\n");
          dfb_fbdev->framebuffer_base = NULL;
          goto error;
     }

     if (ioctl( dfb_fbdev->fd, FBIOGET_VSCREENINFO, &shared->orig_var ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not get variable screen information!\n" );
          goto error;
     }

     shared->current_var = shared->orig_var;
     shared->current_var.accel_flags = 0;

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &shared->current_var ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not disable console acceleration!\n" );
          goto error;
     }

     dfb_fbdev_var_to_mode( &shared->current_var,
                            &shared->current_mode );

     shared->orig_cmap_memory = SHMALLOC( pool_data, 256 * 2 * 4 );
     if (!shared->orig_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->orig_cmap.start  = 0;
     shared->orig_cmap.len    = 256;
     shared->orig_cmap.red    = shared->orig_cmap_memory + 256 * 2 * 0;
     shared->orig_cmap.green  = shared->orig_cmap_memory + 256 * 2 * 1;
     shared->orig_cmap.blue   = shared->orig_cmap_memory + 256 * 2 * 2;
     shared->orig_cmap.transp = shared->orig_cmap_memory + 256 * 2 * 3;

     if (ioctl( dfb_fbdev->fd, FBIOGETCMAP, &shared->orig_cmap ) < 0) {
          D_DEBUG( "DirectFB/FBDev: "
                   "Could not retrieve palette for backup!\n" );

          memset( &shared->orig_cmap, 0, sizeof(shared->orig_cmap) );

          SHFREE( pool_data, shared->orig_cmap_memory );
          shared->orig_cmap_memory = NULL;
     }

     shared->temp_cmap_memory = SHMALLOC( pool_data, 256 * 2 * 4 );
     if (!shared->temp_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->temp_cmap.start  = 0;
     shared->temp_cmap.len    = 256;
     shared->temp_cmap.red    = shared->temp_cmap_memory + 256 * 2 * 0;
     shared->temp_cmap.green  = shared->temp_cmap_memory + 256 * 2 * 1;
     shared->temp_cmap.blue   = shared->temp_cmap_memory + 256 * 2 * 2;
     shared->temp_cmap.transp = shared->temp_cmap_memory + 256 * 2 * 3;

     shared->current_cmap_memory = SHMALLOC( pool_data, 256 * 2 * 4 );
     if (!shared->current_cmap_memory) {
          ret = D_OOSHM();
          goto error;
     }

     shared->current_cmap.start  = 0;
     shared->current_cmap.len    = 256;
     shared->current_cmap.red    = shared->current_cmap_memory + 256 * 2 * 0;
     shared->current_cmap.green  = shared->current_cmap_memory + 256 * 2 * 1;
     shared->current_cmap.blue   = shared->current_cmap_memory + 256 * 2 * 2;
     shared->current_cmap.transp = shared->current_cmap_memory + 256 * 2 * 3;
     
     dfb_fbdev_get_pci_info( shared );

     if (dfb_config->agp) {
          /* Do not fail here, AGP slot could be unavailable */
          ret = dfb_agp_initialize();
          if (ret) {
               D_DEBUG( "DirectFB/FBDev: dfb_agp_initialize()\n\t->%s\n",
                         DirectFBErrorString( ret ) );
               ret = DFB_OK;
          }
     }         

     fusion_call_init( &shared->fbdev_ioctl,
                       fbdev_ioctl_call_handler, NULL, dfb_core_world(core) );

     /* Register primary screen functions */
     screen = dfb_screens_register( NULL, NULL, &primaryScreenFuncs );

     /* Register primary layer functions */
     dfb_layers_register( screen, NULL, &primaryLayerFuncs );

     *data = dfb_fbdev;

     return DFB_OK;


error:
     if (shared) {
          if (shared->orig_cmap_memory)
               SHFREE( pool_data, shared->orig_cmap_memory );

          if (shared->temp_cmap_memory)
               SHFREE( pool_data, shared->temp_cmap_memory );

          if (shared->current_cmap_memory)
               SHFREE( pool_data, shared->current_cmap_memory );

          SHFREE( pool, shared );
     }

     if (dfb_fbdev->framebuffer_base)
          munmap( dfb_fbdev->framebuffer_base, shared->fix.smem_len );

     if (dfb_fbdev->fd != -1)
          close( dfb_fbdev->fd );
     
     D_FREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return ret;
}

static DFBResult
system_join( CoreDFB *core, void **data )
{
     DFBResult   ret;
     CoreScreen *screen;
     void       *shared;

     D_ASSERT( dfb_fbdev == NULL );

     if (dfb_config->vt) {
          ret = dfb_vt_join();
          if (ret)
               return ret;
     }

     dfb_fbdev = D_CALLOC( 1, sizeof(FBDev) );

     fusion_arena_get_shared_field( dfb_core_arena( core ),
                                    "fbdev", &shared );

     dfb_fbdev->core = core;
     dfb_fbdev->shared = shared;

     /* Open framebuffer device */
     ret = dfb_fbdev_open();
     if (ret) {
          D_FREE( dfb_fbdev );
          dfb_fbdev = NULL;
          return ret;
     }

     /* Map the framebuffer */
     dfb_fbdev->framebuffer_base = mmap( NULL, dfb_fbdev->shared->fix.smem_len,
                                         PROT_READ | PROT_WRITE, MAP_SHARED,
                                         dfb_fbdev->fd, 0 );
     if (dfb_fbdev->framebuffer_base == MAP_FAILED) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not mmap the framebuffer!\n");
          close( dfb_fbdev->fd );
          D_FREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return DFB_INIT;
     }

     /* Open AGP device */
     ret = dfb_agp_join();
     if (ret) {
          D_ERROR( "DirectFB/FBDev: Could not join AGP!\n" );
          munmap( dfb_fbdev->framebuffer_base,
                  dfb_fbdev->shared->fix.smem_len );
          close( dfb_fbdev->fd );
          D_FREE( dfb_fbdev );
          dfb_fbdev = NULL;

          return ret;
     }

     /* Register primary screen functions */
     screen = dfb_screens_register( NULL, NULL, &primaryScreenFuncs );

     /* Register primary layer functions */
     dfb_layers_register( screen, NULL, &primaryLayerFuncs );

     *data = dfb_fbdev;

     return DFB_OK;
}

static DFBResult
system_shutdown( bool emergency )
{
     DFBResult            ret;
     VideoMode           *m;
     FBDevShared         *shared;
     FusionSHMPoolShared *pool;

     D_ASSERT( dfb_fbdev != NULL );

     shared = dfb_fbdev->shared;

     D_ASSERT( shared != NULL );

     pool = shared->shmpool;

     D_ASSERT( pool != NULL );

     m = shared->modes;
     while (m) {
          VideoMode *next = m->next;
          SHFREE( pool, m );
          m = next;
     }

     if (ioctl( dfb_fbdev->fd, FBIOPUT_VSCREENINFO, &shared->orig_var ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not restore variable screen information!\n" );
     }

     if (shared->orig_cmap.len) {
          if (ioctl( dfb_fbdev->fd, FBIOPUTCMAP, &shared->orig_cmap ) < 0)
               D_DEBUG( "DirectFB/FBDev: "
                        "Could not restore palette!\n" );
     }

     if (shared->orig_cmap_memory)
          SHFREE( shared->shmpool_data, shared->orig_cmap_memory );

     if (shared->temp_cmap_memory)
          SHFREE( shared->shmpool_data, shared->temp_cmap_memory );

     if (shared->current_cmap_memory)
          SHFREE( shared->shmpool_data, shared->current_cmap_memory );

     fusion_call_destroy( &shared->fbdev_ioctl );

     dfb_agp_shutdown();

     munmap( dfb_fbdev->framebuffer_base, shared->fix.smem_len );

     if (dfb_config->vt) {
          ret = dfb_vt_shutdown( emergency );
          if (ret)
               return ret;
     }

     close( dfb_fbdev->fd );

     SHFREE( pool, shared );
     D_FREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return DFB_OK;
}

static DFBResult
system_leave( bool emergency )
{
     DFBResult ret;

     D_ASSERT( dfb_fbdev != NULL );

     dfb_agp_leave();

     munmap( dfb_fbdev->framebuffer_base,
             dfb_fbdev->shared->fix.smem_len );

     if (dfb_config->vt) {
          ret = dfb_vt_leave( emergency );
          if (ret)
               return ret;
     }

     close( dfb_fbdev->fd );

     D_FREE( dfb_fbdev );
     dfb_fbdev = NULL;

     return DFB_OK;
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

/******************************************************************************/

static volatile void *
system_map_mmio( unsigned int    offset,
                 int             length )
{
     void *addr;

     if (length <= 0)
          length = dfb_fbdev->shared->fix.mmio_len;

     addr = mmap( NULL, length, PROT_READ | PROT_WRITE, MAP_SHARED,
                  dfb_fbdev->fd, dfb_fbdev->shared->fix.smem_len + offset );
     if ((int)(addr) == -1) {
          D_PERROR( "DirectFB/FBDev: Could not mmap MMIO region "
                     "(offset %d, length %d)!\n", offset, length );
          return NULL;
     }

     return(volatile void*) ((u8*) addr + (dfb_fbdev->shared->fix.mmio_start &
                                           dfb_fbdev->shared->page_mask));
}

static void
system_unmap_mmio( volatile void  *addr,
                   int             length )
{
     if (length <= 0)
          length = dfb_fbdev->shared->fix.mmio_len;

     if (munmap( (void*) ((u8*) addr - (dfb_fbdev->shared->fix.mmio_start &
                                        dfb_fbdev->shared->page_mask)), length ) < 0)
          D_PERROR( "DirectFB/FBDev: Could not unmap MMIO region "
                     "at %p (length %d)!\n", addr, length );
}

static int
system_get_accelerator()
{
#ifdef FB_ACCEL_MATROX_MGAG400
     if (!strcmp( dfb_fbdev->shared->fix.id, "MATROX DH" ))
          return FB_ACCEL_MATROX_MGAG400;
#endif
     if (dfb_fbdev->shared->fix.mmio_len > 0)
          return dfb_fbdev->shared->fix.accel;
     return -1;
}

static VideoMode *
system_get_modes()
{
     return dfb_fbdev->shared->modes;
}

static VideoMode *
system_get_current_mode()
{
     return &dfb_fbdev->shared->current_mode;
}

static DFBResult
system_thread_init()
{
     if (dfb_config->block_all_signals)
          direct_signals_block_all();

     return DFB_OK;
}

static bool
system_input_filter( CoreInputDevice *device,
                     DFBInputEvent   *event )
{
     if (dfb_config->vt && dfb_config->vt_switching) {
          switch (event->type) {
               case DIET_KEYPRESS:
                    if (DFB_KEY_TYPE(event->key_symbol) == DIKT_FUNCTION &&
                        event->modifiers == (DIMM_CONTROL | DIMM_ALT))
                         return dfb_vt_switch( event->key_symbol - DIKS_F1 + 1 );

                    break;

               case DIET_KEYRELEASE:
                    if (DFB_KEY_TYPE(event->key_symbol) == DIKT_FUNCTION &&
                        event->modifiers == (DIMM_CONTROL | DIMM_ALT))
                         return true;

                    break;

               default:
                    break;
          }
     }

     return false;
}

static unsigned long
system_video_memory_physical( unsigned int offset )
{
     return dfb_fbdev->shared->fix.smem_start + offset;
}

static void *
system_video_memory_virtual( unsigned int offset )
{
     return(void*)((u8*)(dfb_fbdev->framebuffer_base) + offset);
}

static unsigned int
system_videoram_length()
{
     return dfb_fbdev->shared->fix.smem_len;
}

static unsigned long
system_aux_memory_physical( unsigned int offset )
{
     if (dfb_fbdev->shared->agp)
          return dfb_fbdev->shared->agp->info.aper_base + offset;
     return 0;
}

static void *
system_aux_memory_virtual( unsigned int offset )
{
     if (dfb_fbdev->agp)
          return (void*)(u8*)dfb_fbdev->agp->base + offset;
     return NULL;
}

static unsigned int
system_auxram_length()
{
     if (dfb_fbdev->shared->agp)
          return dfb_fbdev->shared->agp->agp_mem;
     return 0;
}

static void
system_get_busid( int *ret_bus, int *ret_dev, int *ret_func )
{
     *ret_bus  = dfb_fbdev->shared->pci.bus;
     *ret_dev  = dfb_fbdev->shared->pci.dev;
     *ret_func = dfb_fbdev->shared->pci.func;
}

static void
system_get_deviceid( unsigned int *ret_vendor_id,
                     unsigned int *ret_device_id )
{
     *ret_vendor_id = dfb_fbdev->shared->device.vendor;
     *ret_device_id = dfb_fbdev->shared->device.model;
}

/******************************************************************************/

static DFBResult
init_modes()
{
     dfb_fbdev_read_modes();

     if (!dfb_fbdev->shared->modes) {
          /* try to use current mode*/
          dfb_fbdev->shared->modes = (VideoMode*) SHCALLOC( dfb_fbdev->shared->shmpool,
                                                            1, sizeof(VideoMode) );

          *dfb_fbdev->shared->modes = dfb_fbdev->shared->current_mode;

          if (dfb_fbdev_set_mode(NULL, dfb_fbdev->shared->modes, NULL)) {
               D_ERROR("DirectFB/FBDev: "
                        "No supported modes found in /etc/fb.modes and "
                        "current mode not supported!\n");

               D_ERROR( "DirectFB/FBDev: Current mode's pixelformat: "
                         "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
                         dfb_fbdev->shared->orig_var.red.length,
                         dfb_fbdev->shared->orig_var.red.offset,
                         dfb_fbdev->shared->orig_var.green.length,
                         dfb_fbdev->shared->orig_var.green.offset,
                         dfb_fbdev->shared->orig_var.blue.length,
                         dfb_fbdev->shared->orig_var.blue.offset,
                         dfb_fbdev->shared->orig_var.transp.length,
                         dfb_fbdev->shared->orig_var.transp.offset,
                         dfb_fbdev->shared->orig_var.bits_per_pixel );

               return DFB_INIT;
          }
     }

     return DFB_OK;
}

/******************************************************************************/

static DFBResult
primaryInitScreen( CoreScreen           *screen,
                   GraphicsDevice       *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     /* Set the screen capabilities. */
     description->caps = DSCCAPS_VSYNC | DSCCAPS_POWER_MANAGEMENT;

     /* Set the screen name. */
     snprintf( description->name,
               DFB_SCREEN_DESC_NAME_LENGTH, "FBDev Primary Screen" );

     return DFB_OK;
}

static DFBResult
primarySetPowerMode( CoreScreen         *screen,
                     void               *driver_data,
                     void               *screen_data,
                     DFBScreenPowerMode  mode )
{
     int level;

     switch (mode) {
          case DSPM_OFF:
               level = 4;
               break;
          case DSPM_SUSPEND:
               level = 3;
               break;
          case DSPM_STANDBY:
               level = 2;
               break;
          case DSPM_ON:
               level = 0;
               break;
          default:
               return DFB_INVARG;
     }

     return dfb_fbdev_blank( level );
}

static DFBResult
primaryWaitVSync( CoreScreen *screen,
                  void       *driver_data,
                  void       *screen_data )
{
     static const int zero = 0;

     if (dfb_config->pollvsync_none)
          return DFB_OK;

     if (ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, &zero ))
          waitretrace();

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_ASSERT( dfb_fbdev != NULL );
     D_ASSERT( dfb_fbdev->shared != NULL );

     *ret_width  = dfb_fbdev->shared->current_mode.xres;
     *ret_height = dfb_fbdev->shared->current_mode.yres;

     return DFB_OK;
}

/******************************************************************************/

static int
primaryLayerDataSize()
{
     return 0;
}

static int
primaryRegionDataSize()
{
     return 0;
}

static DFBResult
primaryInitLayer( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  DFBDisplayLayerDescription *description,
                  DFBDisplayLayerConfig      *config,
                  DFBColorAdjustment         *adjustment )
{
     DFBResult  ret;
     VideoMode *default_mode;
     CoreLayerRegionConfig tmp;

     /* initialize mode table */
     ret = init_modes();
     if (ret)
          return ret;

     default_mode = dfb_fbdev->shared->modes;

     /* set capabilities and type */
     description->caps = DLCAPS_SURFACE    | DLCAPS_CONTRAST |
                         DLCAPS_SATURATION | DLCAPS_BRIGHTNESS;
     description->type = DLTF_GRAPHICS;

     /* set name */
     snprintf( description->name,
               DFB_DISPLAY_LAYER_DESC_NAME_LENGTH, "FBDev Primary Layer" );

     /* fill out default color adjustment */
     adjustment->flags      = DCAF_BRIGHTNESS | DCAF_CONTRAST | DCAF_SATURATION;
     adjustment->brightness = 0x8000;
     adjustment->contrast   = 0x8000;
     adjustment->saturation = 0x8000;

     /* fill out the default configuration */
     config->flags      = DLCONF_WIDTH       | DLCONF_HEIGHT |
                          DLCONF_PIXELFORMAT | DLCONF_BUFFERMODE;
     config->buffermode = DLBM_FRONTONLY;
     config->width      = default_mode->xres;
     config->height     = default_mode->yres;
     
     tmp.format     = DSPF_RGB16;
     tmp.buffermode = DLBM_FRONTONLY;
     if (dfb_fbdev_set_mode( NULL, NULL, &tmp ))
          config->pixelformat = dfb_pixelformat_for_depth( dfb_fbdev->shared->orig_var.bits_per_pixel );
     else
          config->pixelformat = DSPF_RGB16;

     return DFB_OK;
}

static DFBResult
primarySetColorAdjustment( CoreLayer          *layer,
                           void               *driver_data,
                           void               *layer_data,
                           DFBColorAdjustment *adjustment )
{
     struct fb_cmap *cmap       = &dfb_fbdev->shared->current_cmap;
     struct fb_cmap *temp       = &dfb_fbdev->shared->temp_cmap;
     int             contrast   = adjustment->contrast >> 8;
     int             brightness = (adjustment->brightness >> 8) - 128;
     int             saturation = adjustment->saturation >> 8;
     int             r, g, b, i;

     if (dfb_fbdev->shared->fix.visual != FB_VISUAL_DIRECTCOLOR)
          return DFB_UNIMPLEMENTED;

     /* Use gamma ramp to set color attributes */
     for (i = 0; i < (int)cmap->len; i++) {
          r = cmap->red[i];
          g = cmap->green[i];
          b = cmap->blue[i];
          r >>= 8;
          g >>= 8;
          b >>= 8;

          /*
        * Brightness Adjustment: Increase/Decrease each color channels
        * by a constant amount as specified by value of brightness.
        */
          if (adjustment->flags & DCAF_BRIGHTNESS) {
               r += brightness;
               g += brightness;
               b += brightness;

               r = (r < 0) ? 0 : r;
               g = (g < 0) ? 0 : g;
               b = (b < 0) ? 0 : b;

               r = (r > 255) ? 255 : r;
               g = (g > 255) ? 255 : g;
               b = (b > 255) ? 255 : b;
          }

          /*
           * Contrast Adjustment:  We increase/decrease the "separation"
           * between colors in proportion to the value specified by the
           * contrast control. Decreasing the contrast has a side effect
           * of decreasing the brightness.
           */

          if (adjustment->flags & DCAF_CONTRAST) {
               /* Increase contrast */
               if (contrast > 128) {
                    int c = contrast - 128;

                    r = ((r + c/2)/c) * c;
                    g = ((g + c/2)/c) * c;
                    b = ((b + c/2)/c) * c;
               }
               /* Decrease contrast */
               else if (contrast < 127) {
                    float c = (float)contrast/128.0;

                    r = (int)((float)r * c);
                    g = (int)((float)g * c);
                    b = (int)((float)b * c);
               }
               r = (r < 0) ? 0 : r;
               g = (g < 0) ? 0 : g;
               b = (b < 0) ? 0 : b;

               r = (r > 255) ? 255 : r;
               g = (g > 255) ? 255 : g;
               b = (b > 255) ? 255 : b;
          }

          /*
           * Saturation Adjustment:  This is is a better implementation.
           * Saturation is implemented by "mixing" a proportion of medium
           * gray to the color value.  On the other side, "removing"
           * a proportion of medium gray oversaturates the color.
           */
          if (adjustment->flags & DCAF_SATURATION) {
               if (saturation > 128) {
                    float gray = ((float)saturation - 128.0)/128.0;
                    float color = 1.0 - gray;

                    r = (int)(((float)r - 128.0 * gray)/color);
                    g = (int)(((float)g - 128.0 * gray)/color);
                    b = (int)(((float)b - 128.0 * gray)/color);
               }
               else if (saturation < 128) {
                    float color = (float)saturation/128.0;
                    float gray = 1.0 - color;

                    r = (int)(((float) r * color) + (128.0 * gray));
                    g = (int)(((float) g * color) + (128.0 * gray));
                    b = (int)(((float) b * color) + (128.0 * gray));
               }

               r = (r < 0) ? 0 : r;
               g = (g < 0) ? 0 : g;
               b = (b < 0) ? 0 : b;

               r = (r > 255) ? 255 : r;
               g = (g > 255) ? 255 : g;
               b = (b > 255) ? 255 : b;
          }
          r |= r << 8;
          g |= g << 8;
          b |= b << 8;

          temp->red[i]   =  (unsigned short)r;
          temp->green[i] =  (unsigned short)g;
          temp->blue[i]  =  (unsigned short)b;
     }

     temp->len = cmap->len;
     temp->start = cmap->start;
     if (FBDEV_IOCTL( FBIOPUTCMAP, temp ) < 0) {
          D_PERROR( "DirectFB/FBDev: Could not set the palette!\n" );

          return errno2result(errno);
     }

     return DFB_OK;
}

static DFBResult
primaryTestRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     VideoMode                  *videomode = NULL;
     CoreLayerRegionConfigFlags  fail = 0;

     videomode = dfb_fbdev->shared->modes;
     while (videomode) {
          if (videomode->xres == config->width  &&
              videomode->yres == config->height)
               break;

          videomode = videomode->next;
     }

     if (!videomode || dfb_fbdev_set_mode( NULL, videomode, config ))
          fail |= CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE;

     if (config->options)
          fail |= CLRCF_OPTIONS;

     if (failed)
          *failed = fail;

     if (fail)
          return DFB_UNSUPPORTED;

     return DFB_OK;
}

static DFBResult
primaryAddRegion( CoreLayer             *layer,
                  void                  *driver_data,
                  void                  *layer_data,
                  void                  *region_data,
                  CoreLayerRegionConfig *config )
{
     return DFB_OK;
}

static DFBResult
primarySetRegion( CoreLayer                  *layer,
                  void                       *driver_data,
                  void                       *layer_data,
                  void                       *region_data,
                  CoreLayerRegionConfig      *config,
                  CoreLayerRegionConfigFlags  updated,
                  CoreSurface                *surface,
                  CorePalette                *palette )
{
     DFBResult  ret;
     VideoMode *videomode;
     VideoMode *highest = NULL;

     videomode = dfb_fbdev->shared->modes;
     while (videomode) {
          if (videomode->xres == config->width  &&
              videomode->yres == config->height)
          {
               if (!highest || highest->priority < videomode->priority)
                    highest = videomode;
          }

          videomode = videomode->next;
     }

     if (!highest)
          return DFB_UNSUPPORTED;

     if (updated & (CLRCF_BUFFERMODE | CLRCF_FORMAT | CLRCF_HEIGHT | CLRCF_SURFACE | CLRCF_WIDTH)) {
          ret = dfb_fbdev_set_mode( surface, highest, config );
          if (ret)
               return ret;
     }

     if ((updated & CLRCF_PALETTE) && palette)
          dfb_fbdev_set_palette( palette );

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer           *layer,
                   void                *driver_data,
                   void                *layer_data,
                   void                *region_data,
                   CoreSurface         *surface,
                   DFBSurfaceFlipFlags  flags )
{
     DFBResult ret;

     if (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) &&
         !dfb_config->pollvsync_after)
          dfb_screen_wait_vsync( dfb_screens_at(DSCID_PRIMARY) );

     ret = dfb_fbdev_pan( surface->back_buffer->video.offset /
                          surface->back_buffer->video.pitch,
                          (flags & DSFLIP_WAITFORSYNC) == DSFLIP_ONSYNC );
     if (ret)
          return ret;

     if ((flags & DSFLIP_WAIT) &&
         (dfb_config->pollvsync_after || !(flags & DSFLIP_ONSYNC)))
          dfb_screen_wait_vsync( dfb_screens_at(DSCID_PRIMARY) );

     dfb_surface_flip_buffers( surface, false );

     return DFB_OK;
}

static DFBResult
primaryAllocateSurface( CoreLayer              *layer,
                        void                   *driver_data,
                        void                   *layer_data,
                        void                   *region_data,
                        CoreLayerRegionConfig  *config,
                        CoreSurface           **ret_surface )
{
     DFBResult               ret;
     CoreSurface            *surface;
     DFBSurfaceCapabilities  caps = DSCAPS_VIDEOONLY;

     /* determine further capabilities */
     if (config->buffermode == DLBM_TRIPLE)
          caps |= DSCAPS_TRIPLE;
     else if (config->buffermode != DLBM_FRONTONLY)
          caps |= DSCAPS_DOUBLE;

     caps |= config->surface_caps & DSCAPS_PREMULTIPLIED;

     /* allocate surface object */
     surface = dfb_core_create_surface( dfb_fbdev->core );
     if (!surface)
          return DFB_FAILURE;

     /* initialize surface structure */
     ret = dfb_surface_init( dfb_fbdev->core, surface,
                             config->width, config->height,
                             config->format, caps, NULL );
     if (ret) {
          fusion_object_destroy( &surface->object );
          return ret;
     }

     /* reallocation just needs an allocated buffer structure */
     surface->idle_buffer  =
     surface->back_buffer  =
     surface->front_buffer = SHCALLOC( surface->shmpool, 1, sizeof(SurfaceBuffer) );

     if (!surface->front_buffer) {
          fusion_object_destroy( &surface->object );
          return D_OOSHM();
     }

     /* activate object */
     fusion_object_activate( &surface->object );

     /* return surface */
     *ret_surface = surface;

     return DFB_OK;
}

static DFBResult
primaryReallocateSurface( CoreLayer             *layer,
                          void                  *driver_data,
                          void                  *layer_data,
                          void                  *region_data,
                          CoreLayerRegionConfig *config,
                          CoreSurface           *surface )
{
     /* reallocation is done during SetConfiguration,
        because the pitch can only be determined AFTER setting the mode */
     if (DFB_PIXELFORMAT_IS_INDEXED(config->format) && !surface->palette) {
          DFBResult    ret;
          CorePalette *palette;

          ret = dfb_palette_create( dfb_fbdev->core,
                                    1 << DFB_COLOR_BITS_PER_PIXEL( config->format ),
                                    &palette );
          if (ret)
               return ret;

          if (config->format == DSPF_LUT8)
               dfb_palette_generate_rgb332_map( palette );

          dfb_surface_set_palette( surface, palette );

          dfb_palette_unref( palette );
     }

     if (config->surface_caps & DSCAPS_PREMULTIPLIED)
          surface->caps |= DSCAPS_PREMULTIPLIED;
     else
          surface->caps &= ~DSCAPS_PREMULTIPLIED;

     return DFB_OK;
}


/** fbdev internal **/

static void dfb_fbdev_var_to_mode( struct fb_var_screeninfo *var, 
                                   VideoMode                *mode )
{
     mode->xres          = var->xres;
     mode->yres          = var->yres;
     mode->bpp           = var->bits_per_pixel;
     mode->hsync_len     = var->hsync_len;
     mode->vsync_len     = var->vsync_len;
     mode->left_margin   = var->left_margin;
     mode->right_margin  = var->right_margin;
     mode->upper_margin  = var->upper_margin;
     mode->lower_margin  = var->lower_margin;
     mode->pixclock      = var->pixclock;
     mode->hsync_high    = (var->sync & FB_SYNC_HOR_HIGH_ACT) ? 1 : 0;
     mode->vsync_high    = (var->sync & FB_SYNC_VERT_HIGH_ACT) ? 1 : 0;
     mode->csync_high    = (var->sync & FB_SYNC_COMP_HIGH_ACT) ? 1 : 0;
     mode->sync_on_green = (var->sync & FB_SYNC_ON_GREEN) ? 1 : 0;
     mode->external_sync = (var->sync & FB_SYNC_EXT) ? 1 : 0;
     mode->broadcast     = (var->sync & FB_SYNC_BROADCAST) ? 1 : 0;
     mode->laced         = (var->vmode & FB_VMODE_INTERLACED) ? 1 : 0;
     mode->doubled       = (var->vmode & FB_VMODE_DOUBLE) ? 1 : 0;
}

static int dfb_fbdev_compatible_format( struct fb_var_screeninfo *var,
                                        int al, int rl, int gl, int bl,
                                        int ao, int ro, int go, int bo )
{
     int ah, rh, gh, bh;
     int vah, vrh, vgh, vbh;

     ah = al + ao - 1;
     rh = rl + ro - 1;
     gh = gl + go - 1;
     bh = bl + bo - 1;

     vah = var->transp.length + var->transp.offset - 1;
     vrh = var->red.length + var->red.offset - 1;
     vgh = var->green.length + var->green.offset - 1;
     vbh = var->blue.length + var->blue.offset - 1;

     if ((!al || (ah == vah && al >= (int)var->transp.length)) &&
         (!rl || (rh == vrh && rl >= (int)var->red.length)) &&
         (!gl || (gh == vgh && gl >= (int)var->green.length)) &&
         (!bl || (bh == vbh && bl >= (int)var->blue.length)))
          return 1;

     return 0;
}

static DFBSurfacePixelFormat dfb_fbdev_get_pixelformat( struct fb_var_screeninfo *var )
{
     switch (var->bits_per_pixel) {

          case 8:
/*
               This check is omitted, since we want to use RGB332 even if the
               hardware uses a palette (in that case we initialize a calculated
               one to have correct colors)

               if (fbdev_compatible_format( var, 0, 3, 3, 2, 0, 5, 2, 0 ))*/

               return DSPF_RGB332;

          case 15:
               if (dfb_fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   dfb_fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_ARGB1555;

               break;

          case 16:
               if (dfb_fbdev_compatible_format( var, 0, 5, 5, 5, 0, 10, 5, 0 ) |
                   dfb_fbdev_compatible_format( var, 1, 5, 5, 5,15, 10, 5, 0 ) )
                    return DSPF_ARGB1555;

               if (dfb_fbdev_compatible_format( var, 0, 4, 4, 4,  0, 8, 4, 0 ) |
                   dfb_fbdev_compatible_format( var, 4, 4, 4, 4, 12, 8, 4, 0 ) )
                    return DSPF_ARGB4444;

               if (dfb_fbdev_compatible_format( var, 0, 5, 6, 5, 0, 11, 5, 0 ))
                    return DSPF_RGB16;

               break;

          case 18:
               if (dfb_fbdev_compatible_format( var, 1, 6, 6, 6, 18, 12, 6, 0 ))
                    return DSPF_ARGB1666;

               if (dfb_fbdev_compatible_format( var, 6, 6, 6, 6, 18, 12, 6, 0 ))
                    return DSPF_ARGB6666;

               if (dfb_fbdev_compatible_format( var, 0, 6, 6, 6, 0, 12, 6, 0 ))
                    return DSPF_RGB18;
               break;

          case 24:
               if (dfb_fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB24;

               break;

          case 32:
               if (dfb_fbdev_compatible_format( var, 0, 8, 8, 8, 0, 16, 8, 0 ))
                    return DSPF_RGB32;

               if (dfb_fbdev_compatible_format( var, 8, 8, 8, 8, 24, 16, 8, 0 ))
                    return DSPF_ARGB;

               break;
     }

     D_ERROR( "DirectFB/FBDev: Unsupported pixelformat: "
               "rgba %d/%d, %d/%d, %d/%d, %d/%d (%dbit)\n",
               var->red.length,    var->red.offset,
               var->green.length,  var->green.offset,
               var->blue.length,   var->blue.offset,
               var->transp.length, var->transp.offset,
               var->bits_per_pixel );

     return DSPF_UNKNOWN;
}

/*
 * pans display (flips buffer) using fbdev ioctl
 */
static DFBResult dfb_fbdev_pan( int offset, bool onsync )
{
     struct fb_var_screeninfo *var;

     var = &dfb_fbdev->shared->current_var;

     if (var->yres_virtual < offset + var->yres) {
          D_ERROR( "DirectFB/FBDev: yres %d, vyres %d, offset %d\n",
                    var->yres, var->yres_virtual, offset );
          D_BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     var->xoffset = 0;
     var->yoffset = offset;
     var->activate = onsync ? FB_ACTIVATE_VBL : FB_ACTIVATE_NOW;

     if (FBDEV_IOCTL( FBIOPAN_DISPLAY, var ) < 0) {
          int erno = errno;

          D_PERROR( "DirectFB/FBDev: Panning display failed!\n" );

          return errno2result( erno );
     }

     return DFB_OK;
}

/*
 * blanks display using fbdev ioctl
 */
static DFBResult dfb_fbdev_blank( int level )
{
     if (ioctl( dfb_fbdev->fd, FBIOBLANK, level ) < 0) {
          D_PERROR( "DirectFB/FBDev: Display blanking failed!\n" );

          return errno2result( errno );
     }

     return DFB_OK;
}

/*
 * sets (if surface != NULL) or tests (if surface == NULL) video mode,
 * sets virtual y-resolution according to buffermode
 */
static DFBResult dfb_fbdev_set_mode( CoreSurface           *surface,
                                     VideoMode             *mode,
                                     CoreLayerRegionConfig *config )
{
     unsigned int              vyres;
     struct fb_var_screeninfo  var;
     FBDevShared              *shared = dfb_fbdev->shared;
     DFBSurfacePixelFormat     format;

     D_DEBUG("DirectFB/FBDev: dfb_fbdev_set_mode (surface: %p, "
              "mode: %p, buffermode: %d)\n", surface, mode,
              config ? config->buffermode : DLBM_FRONTONLY);

     if (surface) {
          /* This should never happen */
          if (surface->front_buffer->storage == CSS_AUXILIARY ||
              surface->back_buffer->storage  == CSS_AUXILIARY ||
              surface->idle_buffer->storage  == CSS_AUXILIARY)
               return DFB_UNSUPPORTED;
     }

     if (!mode)
          mode = &shared->current_mode;

     vyres = mode->yres;

     var = shared->current_var;

     var.xoffset = 0;
     var.yoffset = 0;

     if (config) {
          switch (config->buffermode) {
               case DLBM_TRIPLE:
                    vyres *= 3;
                    break;

               case DLBM_BACKVIDEO:
                    vyres *= 2;
                    break;

               case DLBM_BACKSYSTEM:
               case DLBM_FRONTONLY:
                    break;

               default:
                    return DFB_UNSUPPORTED;
          }

          var.bits_per_pixel = DFB_BYTES_PER_PIXEL(config->format) * 8;

          var.transp.length = var.transp.offset = 0;

          switch (config->format) {
               case DSPF_ARGB1555:
                    var.transp.length = 1;
                    var.red.length    = 5;
                    var.green.length  = 5;
                    var.blue.length   = 5;
                    var.transp.offset = 15;
                    var.red.offset    = 10;
                    var.green.offset  = 5;
                    var.blue.offset   = 0;
                    break;

               case DSPF_ARGB4444:
                    var.transp.length = 4;
                    var.red.length    = 4;
                    var.green.length  = 4;
                    var.blue.length   = 4;
                    var.transp.offset = 12;
                    var.red.offset    = 8;
                    var.green.offset  = 4;
                    var.blue.offset   = 0;
                    break;

               case DSPF_RGB16:
                    var.red.length    = 5;
                    var.green.length  = 6;
                    var.blue.length   = 5;
                    var.red.offset    = 11;
                    var.green.offset  = 5;
                    var.blue.offset   = 0;
                    break;

               case DSPF_ARGB:
               case DSPF_AiRGB:
                    var.transp.length = 8;
                    var.red.length    = 8;
                    var.green.length  = 8;
                    var.blue.length   = 8;
                    var.transp.offset = 24;
                    var.red.offset    = 16;
                    var.green.offset  = 8;
                    var.blue.offset   = 0;
                    break;

               case DSPF_LUT8:
               case DSPF_RGB24:
               case DSPF_RGB32:
               case DSPF_RGB332:
                    break;

               case DSPF_ARGB1666:
                    var.transp.length = 1;
                    var.red.length    = 6;
                    var.green.length  = 6;
                    var.blue.length   = 6;
                    var.transp.offset = 18;
                    var.red.offset    = 12;
                    var.green.offset  = 6;
                    var.blue.offset   = 0;
                    break;

               case DSPF_ARGB6666:
                    var.transp.length = 6;
                    var.red.length    = 6;
                    var.green.length  = 6;
                    var.blue.length   = 6;
                    var.transp.offset = 18;
                    var.red.offset    = 12;
                    var.green.offset  = 6;
                    var.blue.offset   = 0;
                    break;

               case DSPF_RGB18:
                    var.red.length    = 6;
                    var.green.length  = 6;
                    var.blue.length   = 6;
                    var.red.offset    = 12;
                    var.green.offset  = 6;
                    var.blue.offset   = 0;
                    break;

               default:
                    return DFB_UNSUPPORTED;
          }
     }
     else
          var.bits_per_pixel = mode->bpp;

     var.activate = surface ? FB_ACTIVATE_NOW : FB_ACTIVATE_TEST;

     var.xres = mode->xres;
     var.yres = mode->yres;
     var.xres_virtual = mode->xres;
     var.yres_virtual = vyres;

     var.pixclock = mode->pixclock;
     var.left_margin = mode->left_margin;
     var.right_margin = mode->right_margin;
     var.upper_margin = mode->upper_margin;
     var.lower_margin = mode->lower_margin;
     var.hsync_len = mode->hsync_len;
     var.vsync_len = mode->vsync_len;

     var.sync = 0;
     if (mode->hsync_high)
          var.sync |= FB_SYNC_HOR_HIGH_ACT;
     if (mode->vsync_high)
          var.sync |= FB_SYNC_VERT_HIGH_ACT;
     if (mode->csync_high)
          var.sync |= FB_SYNC_COMP_HIGH_ACT;
     if (mode->sync_on_green)
          var.sync |= FB_SYNC_ON_GREEN;
     if (mode->external_sync)
          var.sync |= FB_SYNC_EXT;
     if (mode->broadcast)
          var.sync |= FB_SYNC_BROADCAST;

     var.vmode = 0;
     if (mode->laced)
          var.vmode |= FB_VMODE_INTERLACED;
     if (mode->doubled)
          var.vmode |= FB_VMODE_DOUBLE;

     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     if (FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;

          if (surface)
               D_PERROR( "DirectFB/FBDev: "
                          "Could not set video mode (FBIOPUT_VSCREENINFO)!\n" );

          dfb_gfxcard_unlock();

          return errno2result( erno );
     }

     /*
      * the video mode was set successfully, check if there is enough
      * video ram (for buggy framebuffer drivers)
      */

     if (shared->fix.smem_len < (var.yres_virtual *
                                 var.xres_virtual *
                                 var.bits_per_pixel >> 3)
         || (var.yres_virtual < vyres))
     {
          if (surface) {
               D_PERROR( "DirectFB/FBDev: "
                          "Could not set video mode (not enough video ram)!\n" );

               /* restore mode */
               FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &shared->current_var );
          }

          dfb_gfxcard_unlock();

          return DFB_INVARG;
     }

     /* If surface is NULL the mode was only tested, otherwise apply changes. */
     if (surface) {
          struct fb_fix_screeninfo  fix;

          FBDEV_IOCTL( FBIOGET_VSCREENINFO, &var );

          format = dfb_fbdev_get_pixelformat( &var );
          if (format == DSPF_UNKNOWN) {
               D_WARN( "unknown format" );

               /* restore mode */
               FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &shared->current_var );

               dfb_gfxcard_unlock();

               return DFB_UNSUPPORTED;
          }

          if (!config) {
               dfb_gfxcard_unlock();

               return DFB_OK;
          }

          if (format != config->format) {
               if (DFB_BYTES_PER_PIXEL(format) == 1                      ||
                  (format == DSPF_RGB32 && config->format == DSPF_ARGB)  ||
                  (format == DSPF_RGB32 && config->format == DSPF_AiRGB) ||
                  (format == DSPF_ARGB  && config->format == DSPF_AiRGB))
                    format = config->format;
          }

          if (config->format == DSPF_RGB332)
               dfb_fbdev_set_rgb332_palette();
          else
               dfb_fbdev_set_gamma_ramp( config->format );

          shared->current_var = var;
          dfb_fbdev_var_to_mode( &var, &shared->current_mode );

          /* invalidate original pan offset */
          shared->orig_var.xoffset = 0;
          shared->orig_var.yoffset = 0;

          surface->width  = mode->xres;
          surface->height = mode->yres;
          surface->format = format;

          /* To get the new pitch */
          FBDEV_IOCTL( FBIOGET_FSCREENINFO, &fix );

          /* ++Tony: Other information (such as visual formats) will also change */
          shared->fix = fix;

          dfb_gfxcard_adjust_heap_offset( var.yres_virtual * fix.line_length );

          surface->front_buffer->surface = surface;
          surface->front_buffer->policy = CSP_VIDEOONLY;
          surface->front_buffer->format = format;
          surface->front_buffer->video.health = CSH_STORED;
          surface->front_buffer->video.pitch = fix.line_length;
          surface->front_buffer->video.offset = 0;

          switch (config->buffermode) {
               case DLBM_FRONTONLY:
                    surface->caps &= ~DSCAPS_FLIPPING;

                    if (surface->back_buffer != surface->front_buffer) {
                         if (surface->back_buffer->system.addr)
                              SHFREE( surface->shmpool_data, surface->back_buffer->system.addr );

                         SHFREE( surface->shmpool, surface->back_buffer );

                         surface->back_buffer = surface->front_buffer;
                    }

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              SHFREE( surface->shmpool_data, surface->idle_buffer->system.addr );

                         SHFREE( surface->shmpool, surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_BACKVIDEO:
                    surface->caps |= DSCAPS_DOUBLE;
                    surface->caps &= ~DSCAPS_TRIPLE;

                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = SHCALLOC( surface->shmpool, 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.addr) {
                              SHFREE( surface->shmpool_data, surface->back_buffer->system.addr );
                              surface->back_buffer->system.addr = NULL;
                         }

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->format = format;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = fix.line_length;
                    surface->back_buffer->video.offset =
                         surface->back_buffer->video.pitch * var.yres;

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              SHFREE( surface->shmpool_data, surface->idle_buffer->system.addr );

                         SHFREE( surface->shmpool, surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
               case DLBM_TRIPLE:
                    surface->caps |= DSCAPS_TRIPLE;
                    surface->caps &= ~DSCAPS_DOUBLE;

                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = SHCALLOC( surface->shmpool, 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->back_buffer->system.addr) {
                              SHFREE( surface->shmpool_data, surface->back_buffer->system.addr );
                              surface->back_buffer->system.addr = NULL;
                         }

                         surface->back_buffer->system.health = CSH_INVALID;
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_VIDEOONLY;
                    surface->back_buffer->format = format;
                    surface->back_buffer->video.health = CSH_STORED;
                    surface->back_buffer->video.pitch = fix.line_length;
                    surface->back_buffer->video.offset =
                         surface->back_buffer->video.pitch * var.yres;

                    if (surface->idle_buffer == surface->front_buffer) {
                         surface->idle_buffer = SHCALLOC( surface->shmpool, 1, sizeof(SurfaceBuffer) );
                    }
                    else {
                         if (surface->idle_buffer->system.addr) {
                              SHFREE( surface->shmpool_data, surface->idle_buffer->system.addr );
                              surface->idle_buffer->system.addr = NULL;
                         }

                         surface->idle_buffer->system.health = CSH_INVALID;
                    }
                    surface->idle_buffer->surface = surface;
                    surface->idle_buffer->policy = CSP_VIDEOONLY;
                    surface->idle_buffer->format = format;
                    surface->idle_buffer->video.health = CSH_STORED;
                    surface->idle_buffer->video.pitch = fix.line_length;
                    surface->idle_buffer->video.offset =
                         surface->idle_buffer->video.pitch * var.yres * 2;
                    break;
               case DLBM_BACKSYSTEM:
                    surface->caps |= DSCAPS_DOUBLE;
                    surface->caps &= ~DSCAPS_TRIPLE;

                    if (surface->back_buffer == surface->front_buffer) {
                         surface->back_buffer = SHCALLOC( surface->shmpool, 1, sizeof(SurfaceBuffer) );
                    }
                    surface->back_buffer->surface = surface;
                    surface->back_buffer->policy = CSP_SYSTEMONLY;
                    surface->back_buffer->format = format;
                    surface->back_buffer->video.health = CSH_INVALID;
                    surface->back_buffer->system.health = CSH_STORED;
                    surface->back_buffer->system.pitch =
                         (DFB_BYTES_PER_LINE(format, var.xres) + 3) & ~3;

                    if (surface->back_buffer->system.addr)
                         SHFREE( surface->shmpool_data, surface->back_buffer->system.addr );

                    surface->back_buffer->system.addr =
                         SHMALLOC( surface->shmpool_data, surface->back_buffer->system.pitch * var.yres );

                    if (surface->idle_buffer != surface->front_buffer) {
                         if (surface->idle_buffer->system.addr)
                              SHFREE( surface->shmpool_data, surface->idle_buffer->system.addr );

                         SHFREE( surface->shmpool, surface->idle_buffer );

                         surface->idle_buffer = surface->front_buffer;
                    }
                    break;
               default:
                    D_BUG( "unexpected buffer mode" );
                    break;
          }

          dfb_fbdev_pan( 0, false );

          dfb_gfxcard_after_set_var();

          dfb_surface_notify_listeners( surface,
                                        CSNF_SIZEFORMAT | CSNF_FLIP |
                                        CSNF_VIDEO      | CSNF_SYSTEM );
     }

     dfb_gfxcard_unlock();

     return DFB_OK;
}

/*
 * parses video modes in /etc/fb.modes and stores them in dfb_fbdev->shared->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult dfb_fbdev_read_modes()
{
     FILE *fp;
     char line[80],label[32],value[16];
     int geometry=0, timings=0;
     int dummy;
     VideoMode temp_mode;
     FBDevShared *shared = dfb_fbdev->shared;
     VideoMode   *m      = shared->modes;

     if (!(fp = fopen("/etc/fb.modes","r")))
          return errno2result( errno );

     while (fgets(line,79,fp)) {
          if (sscanf(line, "mode \"%31[^\"]\"",label) == 1) {
               memset( &temp_mode, 0, sizeof(VideoMode) );
               geometry = 0;
               timings = 0;
               while (fgets(line,79,fp) && !(strstr(line,"endmode"))) {
                    if (5 == sscanf(line," geometry %d %d %d %d %d", &temp_mode.xres, &temp_mode.yres, &dummy, &dummy, &temp_mode.bpp)) {
                         geometry = 1;
                    }
                    else if (7 == sscanf(line," timings %d %d %d %d %d %d %d", &temp_mode.pixclock, &temp_mode.left_margin,  &temp_mode.right_margin,
                                         &temp_mode.upper_margin, &temp_mode.lower_margin, &temp_mode.hsync_len,    &temp_mode.vsync_len)) {
                         timings = 1;
                    }
                    else if (1 == sscanf(line, " hsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.hsync_high = 1;
                    }
                    else if (1 == sscanf(line, " vsync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.vsync_high = 1;
                    }
                    else if (1 == sscanf(line, " csync %15s",value) && 0 == strcasecmp(value,"high")) {
                         temp_mode.csync_high = 1;
                    }
                    else if (1 == sscanf(line, " laced %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.laced = 1;
                    }
                    else if (1 == sscanf(line, " double %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.doubled = 1;
                    }
                    else if (1 == sscanf(line, " gsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.sync_on_green = 1;
                    }
                    else if (1 == sscanf(line, " extsync %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.external_sync = 1;
                    }
                    else if (1 == sscanf(line, " bcast %15s",value) && 0 == strcasecmp(value,"true")) {
                         temp_mode.broadcast = 1;
                    }
               }
               if (geometry && timings && !dfb_fbdev_set_mode(NULL, &temp_mode, NULL)) {
                    if (!m) {
                         shared->modes = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
                         m = shared->modes;
                    }
                    else {
                         m->next = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
                         m = m->next;
                    }
                    direct_memcpy (m, &temp_mode, sizeof(VideoMode));
                    D_DEBUG( "DirectFB/FBDev: %20s %4dx%4d  %s%s\n", label, temp_mode.xres, temp_mode.yres,
                              temp_mode.laced ? "interlaced " : "", temp_mode.doubled ? "doublescan" : "" );
               }
          }
     }

     fclose (fp);

     return DFB_OK;
}

/*
 * some fbdev drivers use the palette as gamma ramp in >8bpp modes, to have
 * correct colors, the gamme ramp has to be initialized.
 */

static u16 dfb_fbdev_calc_gamma(int n, int max)
{
     int ret = 65535.0 * ((float)((float)n/(max)));
     if (ret > 65535) ret = 65535;
     if (ret <     0) ret =     0;
     return ret;
}

static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format )
{
     int i;

     int red_size   = 0;
     int green_size = 0;
     int blue_size  = 0;
     int red_max    = 0;
     int green_max  = 0;
     int blue_max   = 0;

     struct fb_cmap *cmap;

     if (!dfb_fbdev) {
          D_BUG( "dfb_fbdev_set_gamma_ramp() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     switch (format) {
          case DSPF_ARGB1555:
               red_size   = 32;
               green_size = 32;
               blue_size  = 32;
               break;
          case DSPF_RGB16:
               red_size   = 32;
               green_size = 64;
               blue_size  = 32;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
               red_size   = 256;
               green_size = 256;
               blue_size  = 256;
               break;
          default:
               return DFB_OK;
     }

     /*
      * ++Tony: The gamma ramp must be set differently if in DirectColor,
      *         ie, to mimic TrueColor, index == color[index].
      */
     if (dfb_fbdev->shared->fix.visual == FB_VISUAL_DIRECTCOLOR) {
          red_max   = 65536 / (256/red_size);
          green_max = 65536 / (256/green_size);
          blue_max  = 65536 / (256/blue_size);
     }
     else {
          red_max   = red_size;
          green_max = green_size;
          blue_max  = blue_size;
     }

     cmap = &dfb_fbdev->shared->current_cmap;

     /* assume green to have most weight */
     cmap->len = green_size;

     for (i = 0; i < red_size; i++)
          cmap->red[i] = dfb_fbdev_calc_gamma( i, red_max );

     for (i = 0; i < green_size; i++)
          cmap->green[i] = dfb_fbdev_calc_gamma( i, green_max );

     for (i = 0; i < blue_size; i++)
          cmap->blue[i] = dfb_fbdev_calc_gamma( i, blue_max );

     /* ++Tony: Some drivers use the upper byte, some use the lower */
     if (dfb_fbdev->shared->fix.visual == FB_VISUAL_DIRECTCOLOR) {
          for (i = 0; i < red_size; i++)
               cmap->red[i] |= cmap->red[i] << 8;

          for (i = 0; i < green_size; i++)
               cmap->green[i] |= cmap->green[i] << 8;

          for (i = 0; i < blue_size; i++)
               cmap->blue[i] |= cmap->blue[i] << 8;
     }

     if (FBDEV_IOCTL( FBIOPUTCMAP, cmap ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                     "Could not set gamma ramp" );

          return errno2result(errno);
     }

     return DFB_OK;
}

static DFBResult
dfb_fbdev_set_palette( CorePalette *palette )
{
     int             i;
     struct fb_cmap *cmap = &dfb_fbdev->shared->current_cmap;

     D_ASSERT( palette != NULL );

     cmap->len = palette->num_entries <= 256 ? palette->num_entries : 256;

     for (i = 0; i < (int)cmap->len; i++) {
          cmap->red[i]     = palette->entries[i].r;
          cmap->green[i]   = palette->entries[i].g;
          cmap->blue[i]    = palette->entries[i].b;
          cmap->transp[i]  = 0xff - palette->entries[i].a;

          cmap->red[i]    |= cmap->red[i] << 8;
          cmap->green[i]  |= cmap->green[i] << 8;
          cmap->blue[i]   |= cmap->blue[i] << 8;
          cmap->transp[i] |= cmap->transp[i] << 8;
     }

     if (FBDEV_IOCTL( FBIOPUTCMAP, cmap ) < 0) {
          D_PERROR( "DirectFB/FBDev: Could not set the palette!\n" );

          return errno2result(errno);
     }

     return DFB_OK;
}

static DFBResult dfb_fbdev_set_rgb332_palette()
{
     int red_val;
     int green_val;
     int blue_val;
     int i = 0;
     FusionSHMPoolShared *pool = dfb_fbdev->shared->shmpool_data;

     struct fb_cmap cmap;

     if (!dfb_fbdev) {
          D_BUG( "dfb_fbdev_set_rgb332_palette() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     cmap.start  = 0;
     cmap.len    = 256;
     cmap.red    = (u16*)SHMALLOC( pool, 2 * 256 );
     cmap.green  = (u16*)SHMALLOC( pool, 2 * 256 );
     cmap.blue   = (u16*)SHMALLOC( pool, 2 * 256 );
     cmap.transp = (u16*)SHMALLOC( pool, 2 * 256 );


     for (red_val = 0; red_val  < 8 ; red_val++) {
          for (green_val = 0; green_val  < 8 ; green_val++) {
               for (blue_val = 0; blue_val  < 4 ; blue_val++) {
                    cmap.red[i]    = dfb_fbdev_calc_gamma( red_val, 7 );
                    cmap.green[i]  = dfb_fbdev_calc_gamma( green_val, 7 );
                    cmap.blue[i]   = dfb_fbdev_calc_gamma( blue_val, 3 );
                    cmap.transp[i] = (i ? 0x2000 : 0xffff);
                    i++;
               }
          }
     }

     if (FBDEV_IOCTL( FBIOPUTCMAP, &cmap ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                     "Could not set rgb332 palette" );

          SHFREE( pool, cmap.red );
          SHFREE( pool, cmap.green );
          SHFREE( pool, cmap.blue );
          SHFREE( pool, cmap.transp );

          return errno2result(errno);
     }

     SHFREE( pool, cmap.red );
     SHFREE( pool, cmap.green );
     SHFREE( pool, cmap.blue );
     SHFREE( pool, cmap.transp );

     return DFB_OK;
}

static int
fbdev_ioctl_call_handler( int   caller,
                          int   call_arg,
                          void *call_ptr,
                          void *ctx )
{
     int        ret;
     const char cursoroff_str[] = "\033[?1;0;0c";
     const char blankoff_str[] = "\033[9;0]";

     if (dfb_config->vt) {
          if (!dfb_config->kd_graphics && call_arg == FBIOPUT_VSCREENINFO)
               ioctl( dfb_fbdev->vt->fd, KDSETMODE, KD_GRAPHICS );
     }

     ret = ioctl( dfb_fbdev->fd, call_arg, call_ptr );

     if (dfb_config->vt) {
          if (call_arg == FBIOPUT_VSCREENINFO) {
               if (!dfb_config->kd_graphics) {
                    ioctl( dfb_fbdev->vt->fd, KDSETMODE, KD_TEXT );
                    write( dfb_fbdev->vt->fd, cursoroff_str, strlen(cursoroff_str) );
                    write( dfb_fbdev->vt->fd, blankoff_str, strlen(blankoff_str) );
               }
          }
     }

     return ret;
}

static int
fbdev_ioctl( int request, void *arg, int arg_size )
{
     DirectResult  ret;
     int           erno;
     void         *tmp_shm = NULL;
     FBDevShared  *shared;

     D_ASSERT( dfb_fbdev != NULL );

     shared = dfb_fbdev->shared;

     D_ASSERT( shared != NULL );

     if (dfb_core_is_master( dfb_fbdev->core ))
          return fbdev_ioctl_call_handler( 1, request, arg, NULL );

     if (arg) {
          if (!fusion_is_shared( dfb_core_world(dfb_fbdev->core), arg )) {
               tmp_shm = SHMALLOC( shared->shmpool, arg_size );
               if (!tmp_shm) {
                    errno = ENOMEM;
                    return -1;
               }

               direct_memcpy( tmp_shm, arg, arg_size );
          }
     }

     ret = fusion_call_execute( &shared->fbdev_ioctl, FCEF_NONE,
                                request, tmp_shm ? tmp_shm : arg, &erno );

     if (tmp_shm) {
          direct_memcpy( arg, tmp_shm, arg_size );
          SHFREE( shared->shmpool, tmp_shm );
     }

     errno = erno;

     return errno ? -1 : 0;
}

