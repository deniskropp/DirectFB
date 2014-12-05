#ifdef VSP1_DEBUG_DRIVER
#define DIRECT_ENABLE_DEBUG
#endif

#include <stdio.h>

#undef HAVE_STDLIB_H

#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>

#include <asm/types.h>

#include <directfb.h>
#include <directfb_util.h>

#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/system.h>

#include <misc/conf.h>

#include <core/core.h>
#include <core/gfxcard.h>
#include <core/layers.h>
#include <core/screens.h>
#include <core/surface_buffer.h>
#include <core/system.h>

#include <core/CoreSurface.h>
#include <core/Task.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( vsp1 )


#include "vsp1_driver.h"
#include "vsp1_blt.h"

D_DEBUG_DOMAIN( VSP1_Driver, "VSP1/Driver", "Renesas VSP1 Driver" );

/**********************************************************************************************************************/

/*

     The global dequeue thread

     - waits for buffers to be returned from an operation in kernel space, e.g. Blit or Composite

*/

static void *
vsp1_event_loop( DirectThread *thread,
                 void         *arg )
{
     VSP1DriverData *gdrv = arg;

     while (true) {
          VSP1Buffer *buffer;

          D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );

          direct_mutex_lock( &gdrv->q_lock );

          while (!gdrv->queue) {
               gdrv->idle = true;

               D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );
               direct_waitqueue_broadcast( &gdrv->q_idle );

               D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );
               direct_waitqueue_wait( &gdrv->q_submit, &gdrv->q_lock );
          }

          buffer = (VSP1Buffer*) gdrv->queue;

          D_MAGIC_ASSERT( buffer, VSP1Buffer );

          direct_list_remove( &gdrv->queue, gdrv->queue );

          D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );
          v4l2_device_interface.finish_compose( gdrv->vsp_renderer_data, false );

          D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );
          direct_mutex_unlock( &gdrv->q_lock );


          D_DEBUG_AT( VSP1_Driver, "%s() waiting... %d\n", __FUNCTION__, __LINE__ );
          vsp1_buffer_finished( gdrv, gdrv->dev, buffer );
     }

     return NULL;
}

/**********************************************************************************************************************/

static void
vsp1_destroy_fake_source( VSP1FakeSource *fake_source );

static DFBResult
vsp1_create_fake_source( VSP1DriverData  *gdrv,
                         VSP1FakeSource **ret_fake_source )
{
     DFBResult          ret;
     VSP1FakeSource    *fake_source;
     CoreSurfaceConfig  config;

     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     D_ASSERT( ret_fake_source != NULL );

     /* Allocate new fake_source */
     fake_source = D_CALLOC( 1, sizeof(VSP1FakeSource) );
     if (!fake_source) {
          ret = D_OOSHM();
          goto error;
     }

     dfb_surface_buffer_lock_init( &fake_source->lock, CSAID_GPU, CSAF_READ | CSAF_WRITE );

     /* Initialize fake_source */
     config.flags  = CSCONF_SIZE | CSCONF_FORMAT;
     config.size.w = 256;
     config.size.h = 256;
     config.format = DSPF_ARGB;

     // FIXME: check result values
     ret = dfb_surface_create( gdrv->core, &config, CSTF_NONE, 0, NULL, &fake_source->surface );
     if (ret) {
          D_DERROR( ret, "VSP1/Driver: Failed to create a surface for rectangle filling!\n" );
          goto error;
     }

     ret = CoreSurface_GetBuffers( fake_source->surface, &fake_source->buffer_id, 1, NULL );
     if (ret) {
          D_DERROR( ret, "VSP1/Driver: Failed to get buffers for rectangle filling!\n" );
          goto error;
     }

     ret = CoreSurface_GetOrAllocate( fake_source->surface, fake_source->buffer_id, "Pixmap/DRM",
                                      sizeof("Pixmap/DRM")+1, 0, DSAO_KEEP | DSAO_UPDATED, &fake_source->allocation );
     if (ret) {
          D_DERROR( ret, "VSP1/Driver: Failed to allocate a surface for rectangle filling!\n" );
          goto error;
     }

     ret = dfb_core_get_surface_buffer( gdrv->core, fake_source->buffer_id, &fake_source->buffer );
     if (ret) {
          D_DERROR( ret, "VSP1/Driver: Failed to get a buffer for rectangle filling!\n" );
          goto error;
     }

     ret = dfb_surface_pool_lock( fake_source->allocation->pool, fake_source->allocation, &fake_source->lock );
     if (ret) {
          D_DERROR( ret, "VSP1/Driver: Failed to lock a surface for rectangle filling!\n" );
          goto error;
     }

     D_MAGIC_SET( fake_source, VSP1FakeSource );

     D_DEBUG_AT( VSP1_Driver, "  -> %p\n", fake_source );

     /* Return new fake_source */
     *ret_fake_source = fake_source;

     return DFB_OK;


error:
     D_MAGIC_SET( fake_source, VSP1FakeSource );

     vsp1_destroy_fake_source( fake_source );

     return ret;
}

static void
vsp1_destroy_fake_source( VSP1FakeSource *fake_source )
{
     D_DEBUG_AT( VSP1_Driver, "%s( %p )\n", __FUNCTION__, fake_source );

     D_MAGIC_ASSERT( fake_source, VSP1FakeSource );

     if (fake_source->lock.allocation)
          dfb_surface_pool_unlock( fake_source->allocation->pool, fake_source->allocation, &fake_source->lock );

     if (fake_source->buffer)
          dfb_surface_buffer_unref( fake_source->buffer );

     if (fake_source->allocation)
          dfb_surface_allocation_unref( fake_source->allocation );

     if (fake_source->surface)
          dfb_surface_unref( fake_source->surface );

     dfb_surface_buffer_lock_deinit( &fake_source->lock );

     D_MAGIC_CLEAR( fake_source );

     D_FREE( fake_source );
}

/**********************************************************************************************************************/

static int
driver_probe( CoreGraphicsDevice *device )
{
     struct media_device            *media;
     char                           *device_filename;
     const char                     *device_name;
	const struct media_device_info *info;
     char                           *p;
     int                             num;
     int                             result = 0;

     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     if (dfb_system_type() != CORE_DRMKMS)
          return 0;

     if (direct_config_get( "vsp1-device", &device_filename, 1, &num ) == DR_OK && num > 0)
          return 1;

     /* Initialize V4L2 media controller */
     media = media_device_new( "/dev/media0" );
     if (!media)
          return 0;

     /* Enumerate entities, pads and links */
     if (media_device_enumerate( media )) {
          D_PERROR( "VSP1/Driver: Can't enumerate '%s'!\n", device_filename );
          goto out;
     }

     /* Device info */
     info = media_get_info( media );

     /* Get device name */
     if ((p = strchr( info->bus_info, ':' )))
          device_name = p + 1;
     else
          device_name = info->bus_info;

     if (!strncmp( device_name, "vsp1.", 5 ))
          result = 1;

out:
     media_device_unref( media );

     return result;
}

static void
driver_get_info( CoreGraphicsDevice *device,
                 GraphicsDriverInfo *info )
{
     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Renesas VSP1 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Denis Oliver Kropp" );

     info->version.major = 0;
     info->version.minor = 5;

     info->driver_data_size = sizeof(VSP1DriverData);
     info->device_data_size = sizeof(VSP1DeviceData);
}

static DFBResult
driver_init_driver( CoreGraphicsDevice  *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     DFBResult                       ret  = DFB_OK;
     VSP1DriverData                 *gdrv = driver_data;
	const struct media_device_info *info;
     int                             num;
     char                           *device_filename;
	char                           *p;

     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     /* Keep pointer to shared device data. */
     gdrv->dev = device_data;

     /* Keep core and device pointer. */
     gdrv->core   = core;
     gdrv->device = device;

     /* Determine filename of v4l device */
     if (direct_config_get( "vsp1-device", &device_filename, 1, &num ) || num == 0)
          device_filename = (char*) "/dev/media0";

     /* Initialize V4L2 media controller */
     gdrv->media = media_device_new( device_filename );
     if (!gdrv->media) {
          D_PERROR( "VSP1/Driver: Can't create a media controller for '%s'!\n", device_filename );
          return DFB_INIT;
     }

     /* Enumerate entities, pads and links */
     if (media_device_enumerate( gdrv->media )) {
          D_PERROR( "VSP1/Driver: Can't enumerate '%s'!\n", device_filename );
          ret = DFB_INIT;
          goto error;
     }

     /* Device info */
     info = media_get_info( gdrv->media );

     D_INFO( "VSP1/Driver: Media controller API version %u.%u.%u\n",
             (info->media_version >> 16) & 0xff,
             (info->media_version >>  8) & 0xff,
             (info->media_version)       & 0xff);
     D_INFO( "VSP1/Driver: Media device information\n"
             "                  ------------------------\n"
             "                  driver         %s\n"
             "                  model          %s\n"
             "                  serial         %s\n"
             "                  bus info       %s\n"
             "                  hw revision    0x%x\n"
             "                  driver version %u.%u.%u\n",
             info->driver, info->model,
             info->serial, info->bus_info,
             info->hw_revision,
             (info->driver_version >> 16) & 0xff,
             (info->driver_version >>  8) & 0xff,
             (info->driver_version)       & 0xff);

     /* Get device name */
     if ((p = strchr( info->bus_info, ':' )))
          gdrv->device_name = strdup( p + 1 );
     else
          gdrv->device_name = strdup( info->bus_info );

     if (strncmp( gdrv->device_name, "vsp1.", 5 )) {
          D_ERROR( "VSP1/Driver: The device is not VSP1!\n" );
          ret = DFB_UNSUPPORTED;
          goto error;
     }

     D_INFO( "VSP1/Driver: Using the device '%s'\n", gdrv->device_name );

     gdrv->vsp_renderer_data = v4l2_device_interface.init( gdrv->media );


     gdrv->idle = true;


     direct_recursive_mutex_init( &gdrv->q_lock );
     direct_waitqueue_init( &gdrv->q_idle );
     direct_waitqueue_init( &gdrv->q_submit );

     /* Initialize function table. */
     funcs->EngineSync        = vsp1EngineSync;
     funcs->EmitCommands      = vsp1EmitCommands;
     funcs->CheckState        = vsp1CheckState;
     funcs->SetState          = vsp1SetState;
     funcs->FillRectangle     = vsp1FillRectangle;
     funcs->Blit              = vsp1Blit;
     funcs->StretchBlit       = vsp1StretchBlit;

     return DFB_OK;


error:
     media_device_unref( gdrv->media );

     return ret;
}

static DFBResult
driver_init_device( CoreGraphicsDevice *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     DFBResult       ret;
     VSP1DriverData *gdrv = driver_data;

     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     /* Fill in the device info. */
     snprintf( device_info->name,   DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH,   "VSP1" );
     snprintf( device_info->vendor, DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Renesas" );

     /* Set device limitations. */
     device_info->limits.surface_byteoffset_alignment = 512;
     device_info->limits.surface_bytepitch_alignment  = 512;

     device_info->limits.dst_min.w = 4;
     device_info->limits.dst_min.h = 4;
     device_info->limits.dst_max.w = 8190;
     device_info->limits.dst_max.h = 8190;
     device_info->limits.src_min.w = 4;
     device_info->limits.src_min.h = 4;
     device_info->limits.src_max.w = 8190;
     device_info->limits.src_max.h = 8190;

     /* Set device capabilities. */
     device_info->caps.flags    = 0;//CCF_CLIPPING;// | CCF_RENDEROPTS;
     device_info->caps.accel    = VSP1_SUPPORTED_DRAWINGFUNCTIONS | \
                                  VSP1_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = VSP1_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = VSP1_SUPPORTED_BLITTINGFLAGS;

     for (int i=0; i<4; i++) {
          ret = vsp1_create_fake_source( gdrv, &gdrv->fake_sources[i] );
          if (ret)
               goto error;
     }

     gdrv->event_thread = direct_thread_create( DTT_CRITICAL, vsp1_event_loop, gdrv, "VSP1 Queue" );

     return DFB_OK;


error:
     for (int i=0; i<4; i++) {
          if (gdrv->fake_sources[i])
               vsp1_destroy_fake_source( gdrv->fake_sources[i] );
     }

     return ret;
}

static void
driver_close_device( CoreGraphicsDevice *device,
                     void               *driver_data,
                     void               *device_data )
{
     VSP1DriverData *gdrv = driver_data;
     VSP1DeviceData *gdev = device_data;

     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );

     vsp1_wait_idle( gdrv, gdev );


     direct_thread_join( gdrv->event_thread );
     direct_thread_destroy( gdrv->event_thread );


     /* Shutdown V4L2 media controller */
     media_device_unref( gdrv->media );

     free( gdrv->device_name );

     direct_mutex_deinit( &gdrv->q_lock );
     direct_waitqueue_deinit( &gdrv->q_idle );
     direct_waitqueue_deinit( &gdrv->q_submit );


     for (int i=0; i<4; i++)
          vsp1_destroy_fake_source( gdrv->fake_sources[i] );
}

static void
driver_close_driver( CoreGraphicsDevice *device,
                     void               *driver_data )
{
     D_DEBUG_AT( VSP1_Driver, "%s()\n", __FUNCTION__ );
}

