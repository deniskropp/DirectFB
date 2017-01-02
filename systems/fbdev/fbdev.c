/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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



//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>    /* Needs to be included before dfb_types.h */

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

#define SYS_CLASS_GRAPHICS_DEV "/sys/class/graphics/%s/device"
#define SYS_CLASS_GRAPHICS_DEV_VENDOR "/sys/class/graphics/%s/device/vendor"
#define SYS_CLASS_GRAPHICS_DEV_MODEL "/sys/class/graphics/%s/device/device"
#define SYSFS_PATH_MAX 128

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
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>
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


D_DEBUG_DOMAIN( FBDev_Mode, "FBDev/Mode", "FBDev System Module Mode Switching" );
D_DEBUG_DOMAIN( FBDev_Primary, "FBDev/Primary", "FBDev Primary Layer" );

/******************************************************************************/

extern const SurfacePoolFuncs fbdevSurfacePoolFuncs;

static FusionCallHandlerResult
fbdev_ioctl_call_handler( int           caller,
                          int           call_arg,
                          void         *call_ptr,
                          void         *ctx,
                          unsigned int  serial,
                          int          *ret_val );

static int fbdev_ioctl( int request, void *arg, int arg_size );

#define FBDEV_IOCTL(request,arg)   fbdev_ioctl( request, arg, sizeof(*(arg)) )

FBDev *dfb_fbdev = NULL;

/******************************************************************************/

static int       primaryLayerDataSize ( void );

static int       primaryRegionDataSize( void );

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
                                        CorePalette                *palette,
                                        CoreSurfaceBufferLock      *left_lock,
                                        CoreSurfaceBufferLock      *right_lock );

static DFBResult primaryRemoveRegion  ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data );

static DFBResult primaryFlipRegion    ( CoreLayer                  *layer,
                                        void                       *driver_data,
                                        void                       *layer_data,
                                        void                       *region_data,
                                        CoreSurface                *surface,
                                        DFBSurfaceFlipFlags         flags,
                                        const DFBRegion            *left_update,
                                        CoreSurfaceBufferLock      *left_lock,
                                        const DFBRegion            *right_update,
                                        CoreSurfaceBufferLock      *right_lock );


static DisplayLayerFuncs primaryLayerFuncs = {
     .LayerDataSize      = primaryLayerDataSize,
     .RegionDataSize     = primaryRegionDataSize,
     .InitLayer          = primaryInitLayer,

     .SetColorAdjustment = primarySetColorAdjustment,

     .TestRegion         = primaryTestRegion,
     .AddRegion          = primaryAddRegion,
     .SetRegion          = primarySetRegion,
     .RemoveRegion       = primaryRemoveRegion,
     .FlipRegion         = primaryFlipRegion,
};

/******************************************************************************/

static DFBResult primaryInitScreen  ( CoreScreen           *screen,
                                      CoreGraphicsDevice   *device,
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

static DFBResult primaryGetVSyncCount( CoreScreen           *screen,
                                       void                 *driver_data,
                                       void                 *screen_data,
                                       unsigned long        *ret_count );

static ScreenFuncs primaryScreenFuncs = {
     .InitScreen    = primaryInitScreen,
     .SetPowerMode  = primarySetPowerMode,
     .WaitVSync     = primaryWaitVSync,
     .GetScreenSize = primaryGetScreenSize,
     .GetVSyncCount = primaryGetVSyncCount,
};

/******************************************************************************/

static DFBResult dfb_fbdev_read_modes( void );
static DFBResult dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format );
static DFBResult dfb_fbdev_set_palette( CorePalette *palette );
static DFBResult dfb_fbdev_set_rgb332_palette( void );
static DFBResult dfb_fbdev_pan( int xoffset, int yoffset, bool onsync );
static DFBResult dfb_fbdev_blank( int level );
static void      dfb_fbdev_var_to_mode( const struct fb_var_screeninfo *var,
                                        VideoMode                      *mode );
static const VideoMode *dfb_fbdev_find_mode( int                          width,
                                             int                          height );
static DFBResult dfb_fbdev_test_mode       ( const VideoMode             *mode,
                                             const CoreLayerRegionConfig *config );
static DFBResult dfb_fbdev_test_mode_simple( const VideoMode             *mode );

static DFBResult dfb_fbdev_set_mode        ( const VideoMode             *mode,
                                             CoreSurface                 *surface,
                                             unsigned int                 xoffset,
                                             unsigned int                 yoffset );

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

static DFBResult dfb_fbdev_open( void )
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
     char  buf[512];
     int   vendor = -1;
     int   model  = -1;
     FILE *fp;
     int   bus;
     int   dev;
     int   func;
     char *fbdev;
     char  devname[5] = { 'f', 'b', '0', 0, 0 };
     char  path[SYSFS_PATH_MAX];
     int   len;

     /* try sysfs interface */
     fbdev = dfb_config->fb_device;
     if (!fbdev)
          fbdev = getenv( "FRAMEBUFFER" );

     if (fbdev) {
          if (!strncmp( fbdev, "/dev/fb/", 8 ))
               snprintf( devname, 5, "fb%s", fbdev+8 );
          else if (!strncmp( fbdev, "/dev/fb", 7 ))
               snprintf( devname, 5, "fb%s", fbdev+7 );
     }

     snprintf(path, SYSFS_PATH_MAX, SYS_CLASS_GRAPHICS_DEV, devname);

     len = readlink(path,buf,512);
     if(len != -1) {
          char * base;
          buf[len] = '\0';
          base = basename(buf);

          if (sscanf( base, "0000:%02x:%02x.%1x", &bus, &dev, &func ) == 3) {
               shared->pci.bus  = bus;
               shared->pci.dev  = dev;
               shared->pci.func = func;
           }

          snprintf(path, SYSFS_PATH_MAX, SYS_CLASS_GRAPHICS_DEV_VENDOR, devname);

          fp = fopen(path,"r");
          if(fp) {
               if(fgets(buf,512,fp)) {
                    if(sscanf(buf,"0x%04x", &vendor) == 1)
                         shared->device.vendor = vendor;
               }
               fclose(fp);
          } else {
               D_DEBUG( "DirectFB/FBDev: "
                        "couldn't access %s!\n", path );
          }

          snprintf(path, SYSFS_PATH_MAX, SYS_CLASS_GRAPHICS_DEV_MODEL, devname);

          fp = fopen(path,"r");
          if(fp) {
               if(fgets(buf,512,fp)) {
                    if(sscanf(buf,"0x%04x", &model) == 1)
                         shared->device.model = model;
               }
               fclose(fp);
          } else {
               D_DEBUG( "DirectFB/FBDev: "
                        "couldn't access %s!\n", path );
          }
     } else {
          D_DEBUG( "DirectFB/FBDev: "
                   "couldn't access %s!\n", path );
     }

     /* try /proc interface */
     if (vendor == -1 || model == -1) {
          int   id;

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

     core_arena_add_shared_field( core, "fbdev", shared );

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

     /* Retrieve fixed information like video ram size */
     if (ioctl( dfb_fbdev->fd, FBIOGET_FSCREENINFO, &shared->fix ) < 0) {
          D_PERROR( "DirectFB/FBDev: "
                    "Could not get fixed screen information!\n" );
          goto error;
     }

     D_INFO( "DirectFB/FBDev: Found '%s' (ID %d) with frame buffer at 0x%08lx, %dk (MMIO 0x%08lx, %dk)\n",
             shared->fix.id, shared->fix.accel,
             shared->fix.smem_start, shared->fix.smem_len >> 10,
             shared->fix.mmio_start, shared->fix.mmio_len >> 10 );

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

     dfb_surface_pool_initialize( core, &fbdevSurfacePoolFuncs, &dfb_fbdev->shared->pool );

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
     if (!dfb_fbdev)
          return D_OOM();

     core_arena_get_shared_field( core, "fbdev", &shared );

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

     dfb_surface_pool_join( core, dfb_fbdev->shared->pool, &fbdevSurfacePoolFuncs );

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

     dfb_surface_pool_destroy( dfb_fbdev->shared->pool );

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

     dfb_surface_pool_leave( dfb_fbdev->shared->pool );

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
system_suspend( void )
{
     return DFB_OK;
}

static DFBResult
system_resume( void )
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
     if (addr == MAP_FAILED) {
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
system_get_accelerator( void )
{
#ifdef FB_ACCEL_MATROX_MGAG400
     if (!strcmp( dfb_fbdev->shared->fix.id, "MATROX DH" ))
          return FB_ACCEL_MATROX_MGAG400;
#endif
#ifdef FB_ACCEL_EP9X
     if (!strcmp( dfb_fbdev->shared->fix.id, "ep9xfb" ))
	  return FB_ACCEL_EP9X;
#endif

     if (dfb_config->accelerator)
          return dfb_config->accelerator;

     return dfb_fbdev->shared->fix.accel;
}

static VideoMode *
system_get_modes( void )
{
     return dfb_fbdev->shared->modes;
}

static VideoMode *
system_get_current_mode( void )
{
     return &dfb_fbdev->shared->current_mode;
}

static DFBResult
system_thread_init( void )
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
system_videoram_length( void )
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
system_auxram_length( void )
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

static int
system_surface_data_size( void )
{
     /* Return zero because shared surface data is unneeded. */
     return 0;
}

static void
system_surface_data_init( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
}

static void
system_surface_data_destroy( CoreSurface *surface, void *data )
{
     /* Ignore since unneeded. */
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
init_modes( void )
{
     dfb_fbdev_read_modes();

     if (!dfb_fbdev->shared->modes) {
          /* try to use current mode*/
          dfb_fbdev->shared->modes = (VideoMode*) SHCALLOC( dfb_fbdev->shared->shmpool,
                                                            1, sizeof(VideoMode) );
          if (!dfb_fbdev->shared->modes)
               return D_OOSHM();

          *dfb_fbdev->shared->modes = dfb_fbdev->shared->current_mode;

          if (dfb_fbdev_test_mode_simple(dfb_fbdev->shared->modes)) {
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
                   CoreGraphicsDevice   *device,
                   void                 *driver_data,
                   void                 *screen_data,
                   DFBScreenDescription *description )
{
     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

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

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

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

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     if (dfb_config->pollvsync_none)
          return DFB_OK;

     if (ioctl( dfb_fbdev->fd, FBIO_WAITFORVSYNC, &zero ))
          waitretrace();

     return DFB_OK;
}

static DFBResult
primaryGetVSyncCount( CoreScreen    *screen,
                      void          *driver_data,
                      void          *screen_data,
                      unsigned long *ret_count )
{
     struct fb_vblank vblank;

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_count != NULL );

     if (!ret_count)
          return DFB_INVARG;

     if (ioctl( dfb_fbdev->fd, FBIOGET_VBLANK, &vblank ))
          return errno2result( errno );

     if (!D_FLAGS_IS_SET( vblank.flags, FB_VBLANK_HAVE_COUNT ))
          return DFB_UNSUPPORTED;

     *ret_count = vblank.count;

     return DFB_OK;
}

static DFBResult
primaryGetScreenSize( CoreScreen *screen,
                      void       *driver_data,
                      void       *screen_data,
                      int        *ret_width,
                      int        *ret_height )
{
     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     D_ASSERT( dfb_fbdev != NULL );
     D_ASSERT( dfb_fbdev->shared != NULL );

     *ret_width  = dfb_fbdev->shared->current_mode.xres;
     *ret_height = dfb_fbdev->shared->current_mode.yres;

     return DFB_OK;
}

/******************************************************************************/

static int
primaryLayerDataSize( void )
{
     return 0;
}

static int
primaryRegionDataSize( void )
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

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

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
     config->width      = dfb_config->mode.width  ? dfb_config->mode.width  : default_mode->xres;
     config->height     = dfb_config->mode.height ? dfb_config->mode.height : default_mode->yres;

     if (dfb_config->mode.format)
          config->pixelformat = dfb_config->mode.format;
     else
          config->pixelformat = dfb_pixelformat_for_depth( default_mode->bpp );

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

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

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

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
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
                    r = (r * contrast) >> 7;
                    g = (g * contrast) >> 7;
                    b = (b * contrast) >> 7;
               }

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
          }

          /*
           * Saturation Adjustment:  This is is a better implementation.
           * Saturation is implemented by "mixing" a proportion of medium
           * gray to the color value.  On the other side, "removing"
           * a proportion of medium gray oversaturates the color.
           */
          if (adjustment->flags & DCAF_SATURATION) {
               if (saturation > 128) {
                    int gray = saturation - 128;
                    int color = 128 - gray;

                    r = ((r - gray) << 7) / color;
                    g = ((g - gray) << 7) / color;
                    b = ((b - gray) << 7) / color;
               }
               else if (saturation < 128) {
                    int color = saturation;
                    int gray = 128 - color;

                    r = ((r * color) >> 7) + gray;
                    g = ((g * color) >> 7) + gray;
                    b = ((b * color) >> 7) + gray;
               }

               r = CLAMP( r, 0, 255 );
               g = CLAMP( g, 0, 255 );
               b = CLAMP( b, 0, 255 );
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

const VideoMode *
dfb_fbdev_find_mode( int width, int height )
{
     FBDevShared     *shared    = dfb_fbdev->shared;
     const VideoMode *videomode = shared->modes;
     const VideoMode *highest   = NULL;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     while (videomode) {
          if (videomode->xres == width && videomode->yres == height) {
               if (!highest || highest->priority < videomode->priority)
                    highest = videomode;
          }

          videomode = videomode->next;
     }

     if (!highest)
          D_ONCE( "no mode found for %dx%d", width, height );

     return highest;
}

static DFBResult
primaryTestRegion( CoreLayer                  *layer,
                   void                       *driver_data,
                   void                       *layer_data,
                   CoreLayerRegionConfig      *config,
                   CoreLayerRegionConfigFlags *failed )
{
     FBDevShared                *shared = dfb_fbdev->shared;
     CoreLayerRegionConfigFlags  fail   = CLRCF_NONE;
     VideoMode                   dummy;
     const VideoMode            *mode;

     D_DEBUG_AT( FBDev_Primary, "%s( %dx%d, %s )\n", __FUNCTION__,
                 config->source.w, config->source.h, dfb_pixelformat_name(config->format) );

     mode = dfb_fbdev_find_mode( config->source.w, config->source.h );
     if (!mode) {
          dummy = shared->current_mode;

          dummy.xres = config->source.w;
          dummy.yres = config->source.h;
          dummy.bpp  = DFB_BITS_PER_PIXEL(config->format);

          mode = &dummy;
     }

     if (dfb_fbdev_test_mode( mode, config ))
          fail |= CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE;

     if (config->options)
          fail |= CLRCF_OPTIONS;

     if ((config->source.x && !shared->fix.xpanstep) ||
         (config->source.y && !shared->fix.ypanstep && !shared->fix.ywrapstep))
          fail |= CLRCF_SOURCE;

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
     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

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
                  CorePalette                *palette,
                  CoreSurfaceBufferLock      *left_lock,
                  CoreSurfaceBufferLock      *right_lock )
{
     DFBResult    ret;
     FBDevShared *shared = dfb_fbdev->shared;

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     if (updated & (CLRCF_SOURCE | CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE)) {
          if (updated & (CLRCF_WIDTH | CLRCF_HEIGHT | CLRCF_FORMAT | CLRCF_BUFFERMODE) ||
              config->source.w != shared->current_var.xres ||
              config->source.h != shared->current_var.yres) {
               const VideoMode *mode;
               VideoMode        dummy;

               D_INFO( "FBDev/Mode: Setting %dx%d %s\n", config->source.w, config->source.h,
                       dfb_pixelformat_name( surface->config.format ) );

               mode = dfb_fbdev_find_mode( config->source.w, config->source.h );
               if (!mode) {
                    dummy = shared->current_mode;

                    dummy.xres = config->source.w;
                    dummy.yres = config->source.h;
                    dummy.bpp  = DFB_BITS_PER_PIXEL(config->format);

                    mode = &dummy;
               }

               ret = dfb_fbdev_set_mode( mode, surface, config->source.x,
                                         left_lock->offset / left_lock->pitch + config->source.y );
               if (ret)
                    return ret;
          }
          else {
               ret = dfb_fbdev_pan( config->source.x, left_lock->offset / left_lock->pitch + config->source.y, true );
               if (ret)
                    return ret;
          }
     }

     if ((updated & CLRCF_PALETTE) && palette)
          dfb_fbdev_set_palette( palette );

     /* remember configuration */
     shared->config = *config;

     return DFB_OK;
}

static DFBResult
primaryRemoveRegion( CoreLayer             *layer,
                     void                  *driver_data,
                     void                  *layer_data,
                     void                  *region_data )
{
     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     return DFB_OK;
}

static DFBResult
primaryFlipRegion( CoreLayer             *layer,
                   void                  *driver_data,
                   void                  *layer_data,
                   void                  *region_data,
                   CoreSurface           *surface,
                   DFBSurfaceFlipFlags    flags,
                   const DFBRegion       *left_update,
                   CoreSurfaceBufferLock *left_lock,
                   const DFBRegion       *right_update,
                   CoreSurfaceBufferLock *right_lock )
{
     DFBResult ret;
     CoreLayerRegionConfig *config = &dfb_fbdev->shared->config;

     D_DEBUG_AT( FBDev_Primary, "%s()\n", __FUNCTION__ );

     if (((flags & DSFLIP_WAITFORSYNC) == DSFLIP_WAITFORSYNC) &&
         !dfb_config->pollvsync_after)
          dfb_screen_wait_vsync( dfb_screens_at(DSCID_PRIMARY) );

     ret = dfb_fbdev_pan( config->source.x,
                          left_lock->offset / left_lock->pitch + config->source.y,
                          (flags & DSFLIP_WAITFORSYNC) == DSFLIP_ONSYNC );
     if (ret)
          return ret;

     if ((flags & DSFLIP_WAIT) &&
         (dfb_config->pollvsync_after || !(flags & DSFLIP_ONSYNC)))
          dfb_screen_wait_vsync( dfb_screens_at(DSCID_PRIMARY) );

     dfb_surface_flip( surface, false );

     return DFB_OK;
}

/** fbdev internal **/

static void
dfb_fbdev_var_to_mode( const struct fb_var_screeninfo *var,
                       VideoMode                      *mode )
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

/*
 * pans display (flips buffer) using fbdev ioctl
 */
static DFBResult
dfb_fbdev_pan( int xoffset, int yoffset, bool onsync )
{
//     DFBResult                 ret;
     int                       result;
     struct fb_var_screeninfo *var;
     FBDevShared              *shared = dfb_fbdev->shared;

     if (!shared->fix.xpanstep && !shared->fix.ypanstep && !shared->fix.ywrapstep)
          return DFB_OK;

     var = &shared->current_var;

     if (var->xres_virtual < xoffset + var->xres) {
          D_ERROR( "DirectFB/FBDev: xres %d, vxres %d, xoffset %d\n",
                    var->xres, var->xres_virtual, xoffset );
          D_BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     if (var->yres_virtual < yoffset + var->yres) {
          D_ERROR( "DirectFB/FBDev: yres %d, vyres %d, offset %d\n",
                    var->yres, var->yres_virtual, yoffset );
          D_BUG( "panning buffer out of range" );
          return DFB_BUG;
     }

     if (shared->fix.xpanstep)
          var->xoffset = xoffset - (xoffset % shared->fix.xpanstep);
     else
          var->xoffset = 0;

     if (shared->fix.ywrapstep) {
          var->yoffset = yoffset - (yoffset % shared->fix.ywrapstep);
          var->vmode |= FB_VMODE_YWRAP;
     }
     else if (shared->fix.ypanstep) {
          var->yoffset = yoffset - (yoffset % shared->fix.ypanstep);
          var->vmode &= ~FB_VMODE_YWRAP;
     }
     else {
          var->yoffset = 0;
     }

     var->activate = onsync ? FB_ACTIVATE_VBL : FB_ACTIVATE_NOW;

#if 0
     ret = fusion_call_execute( &shared->fbdev_ioctl, FCEF_NONE, FBIOPAN_DISPLAY, var, &result );
     if (ret)
          return DFB_FUSION;

     if (result) {
          errno = result;
#else
     if (ioctl( dfb_fbdev->fd, FBIOPAN_DISPLAY, var ) < 0) {
          result = errno;
#endif
          D_PERROR( "DirectFB/FBDev: Panning display failed (x=%u y=%u ywrap=%d vbl=%d)!\n",
                    var->xoffset, var->yoffset,
                    (var->vmode & FB_VMODE_YWRAP) ? 1 : 0,
                    (var->activate & FB_ACTIVATE_VBL) ? 1 : 0);

          return errno2result(result);
     }

     return DFB_OK;
}

/*
 * blanks display using fbdev ioctl
 */
static DFBResult
dfb_fbdev_blank( int level )
{
     if (ioctl( dfb_fbdev->fd, FBIOBLANK, level ) < 0) {
          D_PERROR( "DirectFB/FBDev: Display blanking failed!\n" );

          return errno2result( errno );
     }

     return DFB_OK;
}

static DFBResult
dfb_fbdev_mode_to_var( const VideoMode           *mode,
                       DFBSurfacePixelFormat      pixelformat,
                       unsigned int               vxres,
                       unsigned int               vyres,
                       unsigned int               xoffset,
                       unsigned int               yoffset,
                       DFBDisplayLayerBufferMode  buffermode,
                       struct fb_var_screeninfo  *ret_var )
{
     struct fb_var_screeninfo  var;
     FBDevShared              *shared = dfb_fbdev->shared;

     D_DEBUG_AT( FBDev_Mode, "%s( mode: %p )\n", __FUNCTION__, mode );

     D_ASSERT( mode != NULL );
     D_ASSERT( ret_var != NULL );

     D_DEBUG_AT( FBDev_Mode, "  -> resolution   %dx%d\n", mode->xres, mode->yres );
     D_DEBUG_AT( FBDev_Mode, "  -> virtual      %dx%d\n", vxres, vyres );
     D_DEBUG_AT( FBDev_Mode, "  -> pixelformat  %s\n", dfb_pixelformat_name(pixelformat) );
     D_DEBUG_AT( FBDev_Mode, "  -> buffermode   %s\n",
                 buffermode == DLBM_FRONTONLY  ? "FRONTONLY"  :
                 buffermode == DLBM_BACKVIDEO  ? "BACKVIDEO"  :
                 buffermode == DLBM_BACKSYSTEM ? "BACKSYSTEM" :
                 buffermode == DLBM_TRIPLE     ? "TRIPLE"     : "invalid!" );

     /* Start from current information */
     var              = shared->current_var;
     var.activate     = FB_ACTIVATE_NOW;

     /* Set timings */
     var.pixclock     = mode->pixclock;
     var.left_margin  = mode->left_margin;
     var.right_margin = mode->right_margin;
     var.upper_margin = mode->upper_margin;
     var.lower_margin = mode->lower_margin;
     var.hsync_len    = mode->hsync_len;
     var.vsync_len    = mode->vsync_len;

     /* Set resolution */
     var.xres         = mode->xres;
     var.yres         = mode->yres;
     var.xres_virtual = vxres;
     var.yres_virtual = vyres;

     if (shared->fix.xpanstep)
          var.xoffset = xoffset - (xoffset % shared->fix.xpanstep);
     else
          var.xoffset = 0;

     if (shared->fix.ywrapstep)
          var.yoffset = yoffset - (yoffset % shared->fix.ywrapstep);
     else if (shared->fix.ypanstep)
          var.yoffset = yoffset - (yoffset % shared->fix.ypanstep);
     else
          var.yoffset = 0;

     /* Set buffer mode */
     switch (buffermode) {
          case DLBM_TRIPLE:
               if (shared->fix.ypanstep == 0 && shared->fix.ywrapstep == 0)
                    return DFB_UNSUPPORTED;

               var.yres_virtual *= 3;
               break;

          case DLBM_BACKVIDEO:
               if (shared->fix.ypanstep == 0 && shared->fix.ywrapstep == 0)
                    return DFB_UNSUPPORTED;

               var.yres_virtual *= 2;
               break;

          case DLBM_BACKSYSTEM:
          case DLBM_FRONTONLY:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* Set pixel format */
     var.bits_per_pixel = DFB_BITS_PER_PIXEL(pixelformat);
     var.transp.length  = var.transp.offset = 0;

     switch (pixelformat) {
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

          case DSPF_RGBA5551:
               var.transp.length = 1;
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 11;
               var.green.offset  = 6;
               var.blue.offset   = 1;
               var.transp.offset = 0;
               break;

          case DSPF_RGB555:
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 10;
               var.green.offset  = 5;
               var.blue.offset   = 0;
               break;

          case DSPF_BGR555:
               var.red.length    = 5;
               var.green.length  = 5;
               var.blue.length   = 5;
               var.red.offset    = 0;
               var.green.offset  = 5;
               var.blue.offset   = 10;
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

          case DSPF_RGBA4444:
               var.transp.length = 4;
               var.red.length    = 4;
               var.green.length  = 4;
               var.blue.length   = 4;
               var.transp.offset = 0;
               var.red.offset    = 12;
               var.green.offset  = 8;
               var.blue.offset   = 4;
               break;

         case DSPF_RGB444:
               var.red.length    = 4;
               var.green.length  = 4;
               var.blue.length   = 4;
               var.red.offset    = 8;
               var.green.offset  = 4;
               var.blue.offset   = 0;
               break;

         case DSPF_RGB32:
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.red.offset    = 16;
               var.green.offset  = 8;
               var.blue.offset   = 0;
               break;

          case DSPF_ARGB8565:
               var.transp.length = 8;
               var.transp.offset = 16;
               /* fall through */

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

          case DSPF_ABGR:
               var.transp.length = 8;
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.transp.offset = 24;
               var.red.offset    = 0;
               var.green.offset  = 8;
               var.blue.offset   = 16;
               break;

          case DSPF_LUT8:
          case DSPF_RGB24:
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

          case DSPF_RGBAF88871:
               var.transp.length = 7;
               var.red.length    = 8;
               var.green.length  = 8;
               var.blue.length   = 8;
               var.transp.offset = 1;
               var.red.offset    = 24;
               var.green.offset  = 16;
               var.blue.offset   = 8;
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* Set sync options */
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

     /* Set interlace/linedouble */
     var.vmode = 0;
     if (mode->laced)
          var.vmode |= FB_VMODE_INTERLACED;
     if (mode->doubled)
          var.vmode |= FB_VMODE_DOUBLE;

     *ret_var = var;

     return DFB_OK;
}

static DFBResult
dfb_fbdev_test_mode( const VideoMode             *mode,
                     const CoreLayerRegionConfig *config )
{
     DFBResult                  ret;
     struct fb_var_screeninfo   var;
     unsigned int               need_mem;
     FBDevShared               *shared = dfb_fbdev->shared;
     const DFBRectangle        *source = &config->source;

     D_DEBUG_AT( FBDev_Mode, "%s( mode: %p, config: %p )\n", __FUNCTION__, mode, config );

     D_ASSERT( mode != NULL );
     D_ASSERT( config != NULL );

     /* Is panning supported? */
     if (source->w != mode->xres && shared->fix.xpanstep == 0)
          return DFB_UNSUPPORTED;
     if (source->h != mode->yres && shared->fix.ypanstep == 0 && shared->fix.ywrapstep == 0)
          return DFB_UNSUPPORTED;

     ret = dfb_fbdev_mode_to_var( mode, config->format, config->width, config->height,
                                  0, 0, config->buffermode, &var );
     if (ret)
          return ret;

     need_mem = DFB_BYTES_PER_LINE( config->format, var.xres_virtual ) *
                DFB_PLANE_MULTIPLY( config->format, var.yres_virtual );
     if (shared->fix.smem_len < need_mem) {
          D_DEBUG_AT( FBDev_Mode, "  => not enough framebuffer memory (%u < %u)!\n",
                      shared->fix.smem_len, need_mem );

          return DFB_LIMITEXCEEDED;
     }

     /* Enable test mode */
     var.activate = FB_ACTIVATE_TEST;


     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     if (FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &var ) < 0) {
          int erno = errno;
          dfb_gfxcard_unlock();
          D_DEBUG_AT( FBDev_Mode, "  => FAILED!\n" );
          return errno2result( erno );
     }

     dfb_gfxcard_unlock();

     D_DEBUG_AT( FBDev_Mode, "  => SUCCESS\n" );

     return DFB_OK;
}

static DFBResult
dfb_fbdev_test_mode_simple( const VideoMode *mode )
{
     DFBResult                ret;
     struct fb_var_screeninfo var;

     D_DEBUG_AT( FBDev_Mode, "%s( mode: %p )\n", __FUNCTION__, mode );

     D_ASSERT( mode != NULL );

     ret = dfb_fbdev_mode_to_var( mode, dfb_pixelformat_for_depth(mode->bpp), mode->xres, mode->yres,
                                  0, 0, DLBM_FRONTONLY, &var );
     if (ret)
          return ret;

     /* Enable test mode */
     var.activate = FB_ACTIVATE_TEST;

     if (FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &var ) < 0) {
          D_DEBUG_AT( FBDev_Mode, "  => FAILED!\n" );
          return errno2result( errno );
     }

     D_DEBUG_AT( FBDev_Mode, "  => SUCCESS\n" );

     return DFB_OK;
}

static inline int
num_video_buffers( CoreSurface *surface )
{
      int i;

      for (i = 0; i < surface->num_buffers; i++) {
           if (surface->buffers[i]->policy == CSP_SYSTEMONLY)
                break;
      }

      return i;
}

static DFBResult
dfb_fbdev_set_mode( const VideoMode         *mode,
                    CoreSurface             *surface,
                    unsigned int             xoffset,
                    unsigned int             yoffset )
{
     DFBResult                  ret;
     int                        bufs;
     struct fb_var_screeninfo   var;
     struct fb_var_screeninfo   var2;
     FBDevShared               *shared     = dfb_fbdev->shared;
     DFBDisplayLayerBufferMode  buffermode = DLBM_FRONTONLY;
     const CoreSurfaceConfig   *config     = &surface->config;

     D_DEBUG_AT( FBDev_Mode, "%s( mode: %p, config: %p )\n", __FUNCTION__, mode, config );

     D_ASSERT( mode != NULL );
     D_ASSERT( config != NULL );

     bufs = num_video_buffers( surface );
     switch (bufs) {
          case 3:
               buffermode = DLBM_TRIPLE;
               break;
          case 2:
               buffermode = DLBM_BACKVIDEO;
               break;
          case 1:
               buffermode = DLBM_FRONTONLY;
               break;
          default:
               D_BUG( "dfb_fbdev_set_mode() called with %d video buffers!", bufs );
               return DFB_BUG;
     }


     ret = dfb_fbdev_mode_to_var( mode, config->format, config->size.w, config->size.h,
                                  xoffset, yoffset, buffermode, &var );
     if (ret) {
          D_ERROR( "FBDev/Mode: Failed to switch to %dx%d %s (buffermode %d)\n",
                   config->size.w, config->size.h, dfb_pixelformat_name(config->format), buffermode );
          return ret;
     }


     dfb_gfxcard_lock( GDLF_WAIT | GDLF_SYNC | GDLF_RESET | GDLF_INVALIDATE );

     if (FBDEV_IOCTL( FBIOPUT_VSCREENINFO, &var )) {
          D_DEBUG_AT( FBDev_Mode, "  => FBIOPUT_VSCREENINFO failed!\n" );

          ret = errno2result( errno );
          goto error;
     }

     if (FBDEV_IOCTL( FBIOGET_VSCREENINFO, &var2 )) {
          D_DEBUG_AT( FBDev_Mode, "  => FBIOGET_VSCREENINFO failed!\n" );

          ret = errno2result( errno );
          goto error;
     }

     if (var.xres != var2.xres || var.xres_virtual != var2.xres_virtual ||
         var.yres != var2.yres || var.yres_virtual != var2.yres_virtual)
     {
          D_DEBUG_AT( FBDev_Mode, "  => read back mismatch! (%dx%d [%dx%d] should be %dx%d [%dx%d])\n",
                      var2.xres, var2.yres, var2.xres_virtual, var2.yres_virtual,
                      var.xres, var.yres, var.xres_virtual, var.yres_virtual );

          ret = DFB_IO;
          goto error;
     }


     D_DEBUG_AT( FBDev_Mode, "  => SUCCESS\n" );


     shared->current_var = var;
     dfb_fbdev_var_to_mode( &var, &shared->current_mode );

     /* To get the new pitch */
     FBDEV_IOCTL( FBIOGET_FSCREENINFO, &shared->fix );

     D_INFO( "FBDev/Mode: Switched to %dx%d (virtual %dx%d) at %d bit (%s), pitch %d\n",
             var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel,
             dfb_pixelformat_name(config->format), shared->fix.line_length );

     if (config->format == DSPF_RGB332)
          dfb_fbdev_set_rgb332_palette();
     else
          dfb_fbdev_set_gamma_ramp( config->format );

     /* invalidate original pan offset */
     shared->orig_var.xoffset = 0;
     shared->orig_var.yoffset = 0;

     dfb_surfacemanager_adjust_heap_offset( dfb_fbdev->shared->manager,
                                            var.yres_virtual * shared->fix.line_length );

     dfb_gfxcard_after_set_var();

     dfb_gfxcard_unlock();

     return DFB_OK;


error:
     dfb_gfxcard_unlock();

     D_ERROR( "FBDev/Mode: Failed to switched to %dx%d (virtual %dx%d) at %d bit (%s)!\n",
              var.xres, var.yres, var.xres_virtual, var.yres_virtual, var.bits_per_pixel,
              dfb_pixelformat_name(config->format) );

     return ret;
}

/*
 * parses video modes in /etc/fb.modes and stores them in dfb_fbdev->shared->modes
 * (to be replaced by DirectFB's own config system
 */
static DFBResult
dfb_fbdev_read_modes( void )
{
     FILE        *fp;
     char         line[80],label[32],value[16];
     int          geometry=0, timings=0;
     int          dummy;
     VideoMode    temp_mode;
     FBDevShared *shared = dfb_fbdev->shared;
     VideoMode   *prev   = shared->modes;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

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

               if (geometry && timings && !dfb_fbdev_test_mode_simple(&temp_mode)) {
                    VideoMode *mode = SHCALLOC( shared->shmpool, 1, sizeof(VideoMode) );
                    if (!mode) {
                         D_OOSHM();
                         continue;
                    }

                    if (!prev)
                         shared->modes = mode;
                    else
                         prev->next = mode;

                    direct_memcpy (mode, &temp_mode, sizeof(VideoMode));

                    prev = mode;

                    D_DEBUG_AT( FBDev_Mode, " +-> %16s %4dx%4d  %s%s\n", label, temp_mode.xres, temp_mode.yres,
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

static u16
dfb_fbdev_calc_gamma(int n, int max)
{
     int ret = 65535 * n / max;
     return CLAMP( ret, 0, 65535 );
}

static DFBResult
dfb_fbdev_set_gamma_ramp( DFBSurfacePixelFormat format )
{
     int i;

     int red_size   = 0;
     int green_size = 0;
     int blue_size  = 0;
     int red_max    = 0;
     int green_max  = 0;
     int blue_max   = 0;

     struct fb_cmap *cmap;

     D_DEBUG_AT( FBDev_Mode, "%s()\n", __FUNCTION__ );

     if (!dfb_fbdev) {
          D_BUG( "dfb_fbdev_set_gamma_ramp() called while dfb_fbdev == NULL!" );

          return DFB_BUG;
     }

     switch (format) {
          case DSPF_ARGB1555:
          case DSPF_RGBA5551:
          case DSPF_RGB555:
          case DSPF_BGR555:
               red_size   = 32;
               green_size = 32;
               blue_size  = 32;
               break;
          case DSPF_ARGB4444:
          case DSPF_RGBA4444:
          case DSPF_RGB444:
               red_size   = 16;
               green_size = 16;
               blue_size  = 16;
               break;
          case DSPF_RGB16:
          case DSPF_ARGB8565:
               red_size   = 32;
               green_size = 64;
               blue_size  = 32;
               break;
          case DSPF_RGB24:
          case DSPF_RGB32:
          case DSPF_ARGB:
          case DSPF_ABGR:
          case DSPF_RGBAF88871:
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

static DFBResult
dfb_fbdev_set_rgb332_palette( void )
{
     DFBResult ret = DFB_OK;
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
     if (!cmap.red) {
          return D_OOSHM();
     }
     cmap.green  = (u16*)SHMALLOC( pool, 2 * 256 );
     if (!cmap.green) {
          ret = D_OOSHM();
          goto free_red;
     }
     cmap.blue   = (u16*)SHMALLOC( pool, 2 * 256 );
     if (!cmap.blue) {
          ret = D_OOSHM();
          goto free_green;
     }
     cmap.transp = (u16*)SHMALLOC( pool, 2 * 256 );
     if (!cmap.transp) {
          ret = D_OOSHM();
          goto free_blue;
     }

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
          ret = errno2result(errno);
          goto free_transp;
     }

 free_transp:
     SHFREE( pool, cmap.transp );
 free_blue:
     SHFREE( pool, cmap.blue );
 free_green:
     SHFREE( pool, cmap.green );
 free_red:
     SHFREE( pool, cmap.red );

     return ret;
}

static FusionCallHandlerResult
fbdev_ioctl_call_handler( int           caller,
                          int           call_arg,
                          void         *call_ptr,
                          void         *ctx,
                          unsigned int  serial,
                          int          *ret_val )
{
     int        ret;
     int        res;
     const char cursoroff_str[] = "\033[?1;0;0c";
     const char blankoff_str[] = "\033[9;0]";

     if (dfb_config->vt && !dfb_config->kd_graphics && call_arg == FBIOPUT_VSCREENINFO)
          ioctl( dfb_fbdev->vt->fd, KDSETMODE, KD_GRAPHICS );

     ret = ioctl( dfb_fbdev->fd, call_arg, call_ptr );
     if (ret)
          ret = errno;

     if (dfb_config->vt && !dfb_config->kd_graphics && call_arg == FBIOPUT_VSCREENINFO) {
          ioctl( dfb_fbdev->vt->fd, KDSETMODE, KD_TEXT );
          res = write( dfb_fbdev->vt->fd, cursoroff_str, strlen(cursoroff_str) );
          res = write( dfb_fbdev->vt->fd, blankoff_str, strlen(blankoff_str) );
          (void)res;
     }

     *ret_val = ret;

     return FCHR_RETURN;
}

static int
fbdev_ioctl( int request, void *arg, int arg_size )
{
     int          ret;
     int          erno;
     void        *tmp_shm = NULL;
     FBDevShared *shared;

     D_ASSERT( dfb_fbdev != NULL );

     shared = dfb_fbdev->shared;

     D_ASSERT( shared != NULL );

     if (dfb_core_is_master( dfb_fbdev->core )) {
          fbdev_ioctl_call_handler( 1, request, arg, NULL, 0, &ret );
          errno = ret;
          return errno ? -1 : 0;
     }

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

