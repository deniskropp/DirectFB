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

#ifndef __GFXCARD_H__
#define __GFXCARD_H__

#include <linux/fb.h>
#include <pthread.h>

#include <core/fusion/lock.h>

#include <directfb.h>
#include <core/coretypes.h>

typedef enum {
     CCF_CLIPPING = 0x00000001
} CardCapabilitiesFlags;


/*
 * return value for hardware accelerated card functions
 */
typedef enum {
     CR_OK = 0,
     CR_FALLBACK
} CardResult;

typedef struct {
     CardCapabilitiesFlags   flags;

     DFBAccelerationMask     accel;
     DFBSurfaceBlittingFlags blitting;
     DFBSurfaceDrawingFlags  drawing;
} CardCapabilities;

typedef struct {
     unsigned int            surface_byteoffset_alignment;
     unsigned int            surface_pixelpitch_alignment;
} CardLimitations;

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_GRAPHICS_DRIVER_ABI_VERSION          13

#define DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH     60
#define DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH   80
#define DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH     120
#define DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH  40

#define DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH     60
#define DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH   80


typedef struct {
     int          major;        /* major version */
     int          minor;        /* minor version */
} GraphicsDriverVersion;        /* major.minor, e.g. 0.1 */

typedef struct {
     GraphicsDriverVersion version;

     char               name[DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH+1];
                                /* Name of driver, e.g. 'Matrox Driver' */

     char               vendor[DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH+1];
                                /* Vendor (or author) of the driver,
                                   e.g. 'convergence' or 'Denis Oliver Kropp' */

     char               url[DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH+1];
                                /* URL for driver updates,
                                   e.g. 'http://www.directfb.org/' */

     char               license[DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH+1];
                                /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int       driver_data_size;
     unsigned int       device_data_size;
} GraphicsDriverInfo;

typedef struct {
     char               name[DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH+1];
                                /* Device name, e.g. 'G400' */

     char               vendor[DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH+1];
                                /* Vendor of the device,
                                   e.g. 'Matrox' or 'ATI' */

     /* hardware acceleration capabilities */
     CardCapabilities   caps;

     /* hardware limitations */
     CardLimitations    limits;
} GraphicsDeviceInfo;

typedef struct _GraphicsDeviceFuncs {
     /*
      * function that is called after variable screeninfo is changed
      * (used for buggy fbdev drivers, that reinitialize something when
      * calling FBIO_PUT_VSCREENINFO)
      */
     void (*AfterSetVar)( void *driver_data, void *device_data );

     /*
      * makes sure that graphics hardware has finished all operations
      */
     void (*EngineSync)( void *driver_data, void *device_data );

     /*
      * after the video memory has been written to by the CPU (e.g. modification
      * of a texture) make sure the accelerator won't use cached texture data
      */
     void (*FlushTextureCache)( void *driver_data, void *device_data );

     /*
      * Check if the function 'accel' can be accelerated with the 'state'.
      * If that's true, the function sets the 'accel' bit in 'state->accel'.
      * Otherwise the function just returns, no need to clear the bit.
      */
     void (*CheckState)( void *driver_data, void *device_data,
                         CardState *state, DFBAccelerationMask accel );

     /*
      * Program card for execution of the function 'accel' with the 'state'.
      * 'state->modified' contains information about changed entries.
      * This function has to set at least 'accel' in 'state->set'.
      * The driver should remember 'state->modified' and clear it.
      * The driver may modify 'funcs' depending on 'state' settings.
      */
     void (*SetState)  ( void *driver_data, void *device_data,
                         struct _GraphicsDeviceFuncs *funcs,
                         CardState *state, DFBAccelerationMask accel );

     /*
      * drawing functions
      */
     void (*FillRectangle) ( void *driver_data, void *device_data,
                             DFBRectangle *rect );

     void (*DrawRectangle) ( void *driver_data, void *device_data,
                             DFBRectangle *rect );

     void (*DrawLine)      ( void *driver_data, void *device_data,
                             DFBRegion *line );

     void (*FillTriangle)  ( void *driver_data, void *device_data,
                             DFBTriangle *tri );

     /*
      * blitting functions
      */
     void (*Blit)          ( void *driver_data, void *device_data,
                             DFBRectangle *rect, int dx, int dy );

     void (*StretchBlit)   ( void *driver_data, void *device_data,
                             DFBRectangle *srect, DFBRectangle *drect );

} GraphicsDeviceFuncs;

typedef struct {
     int       (*GetAbiVersion)  ();
     int       (*Probe)          (GraphicsDevice      *device);
     void      (*GetDriverInfo)  (GraphicsDevice      *device,
                                  GraphicsDriverInfo  *driver_info);

     DFBResult (*InitDriver)     (GraphicsDevice      *device,
                                  GraphicsDeviceFuncs *funcs,
                                  void                *driver_data);

     DFBResult (*InitDevice)     (GraphicsDevice      *device,
                                  GraphicsDeviceInfo  *device_info,
                                  void                *driver_data,
                                  void                *device_data);

     void      (*CloseDevice)    (GraphicsDevice      *device,
                                  void                *driver_data,
                                  void                *device_data);
     void      (*CloseDriver)    (GraphicsDevice      *device,
                                  void                *driver_data);
} GraphicsDriverFuncs;

/*
 * initializes card struct, maps framebuffer, chooses accelerated driver
 */
DFBResult dfb_gfxcard_initialize();
DFBResult dfb_gfxcard_join();

DFBResult dfb_gfxcard_shutdown( bool emergency );
DFBResult dfb_gfxcard_leave( bool emergency );

#ifdef FUSION_FAKE
DFBResult dfb_gfxcard_suspend();
DFBResult dfb_gfxcard_resume();
#endif

void dfb_graphics_register_module( GraphicsDriverFuncs *funcs );

int dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel );
int dfb_gfxcard_state_acquire( CardState *state, DFBAccelerationMask accel );
void dfb_gfxcard_state_release( CardState *state );

/*
 * drawing functions, lock source and destination surfaces,
 * handle clipping and drawing method (hardware/software)
 */
void dfb_gfxcard_fillrectangle( DFBRectangle *rect, CardState *state );

void dfb_gfxcard_drawrectangle( DFBRectangle *rect, CardState *state );

void dfb_gfxcard_drawlines( DFBRegion *lines, int num_lines, CardState *state );

void dfb_gfxcard_filltriangle( DFBTriangle *tri, CardState *state );

void dfb_gfxcard_blit( DFBRectangle *rect, int dx, int dy, CardState *state );

void dfb_gfxcard_tileblit( DFBRectangle *rect, int dx, int dy, int w, int h,
                           CardState *state );

void dfb_gfxcard_stretchblit( DFBRectangle *srect, DFBRectangle *drect,
                              CardState *state );

void dfb_gfxcard_drawstring( const __u8 *text, int bytes, int x, int y,
                             CoreFont *font, CardState *state );
void dfb_gfxcard_drawglyph( unichar index, int x, int y,
                            CoreFont *font, CardState *state );

/*
 * Graphics drivers call this function to get access to MMIO regions.
 *
 * device: Graphics device to map
 * offset: Offset from MMIO base (default offset is 0)
 * length: Length of mapped region (-1 uses default length)
 *
 * Returns the virtual address or NULL if mapping failed.
 */
volatile void *dfb_gfxcard_map_mmio( GraphicsDevice *device,
                                     unsigned int    offset,
                                     int             length );

/*
 * Graphics drivers call this function to unmap MMIO regions.
 *
 * addr:   Virtual address returned by gfxcard_map_mmio
 * length: Length of mapped region (-1 uses default length)
 */
void dfb_gfxcard_unmap_mmio( GraphicsDevice *device,
                             volatile void  *addr,
                             int             length );

int dfb_gfxcard_get_accelerator( GraphicsDevice *device );

void dfb_gfxcard_sync();
void dfb_gfxcard_flush_texture_cache();
void dfb_gfxcard_after_set_var();

DFBResult dfb_gfxcard_adjust_heap_offset( unsigned int offset );

SurfaceManager   *dfb_gfxcard_surface_manager();
FusionObjectPool *dfb_gfxcard_surface_pool();
FusionObjectPool *dfb_gfxcard_palette_pool();
CardCapabilities  dfb_gfxcard_capabilities();

int            dfb_gfxcard_reserve_memory( GraphicsDevice *device,
                                           unsigned int    size );

unsigned long  dfb_gfxcard_memory_physical( unsigned int offset );
void          *dfb_gfxcard_memory_virtual( unsigned int offset );
unsigned int   dfb_gfxcard_memory_length();
#endif

