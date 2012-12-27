/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __GFXCARD_H__
#define __GFXCARD_H__

#include <pthread.h>

#include <direct/modules.h>

#include <fusion/call.h>
#include <fusion/lock.h>
#include <fusion/object.h>

#include <core/coretypes.h>

#include <directfb.h>


typedef enum {
     CCF_CLIPPING   = 0x00000001,
     CCF_NOTRIEMU   = 0x00000002,
     CCF_READSYSMEM = 0x00000004,
     CCF_WRITESYSMEM= 0x00000008,
     CCF_AUXMEMORY  = 0x00000010,
     CCF_RENDEROPTS = 0x00000020
} CardCapabilitiesFlags;

struct __DFB_CoreGraphicsSerial {
     unsigned int serial;
     unsigned int generation;
};

typedef struct {
     CardCapabilitiesFlags   flags;

     DFBAccelerationMask     accel;
     DFBSurfaceBlittingFlags blitting;
     DFBSurfaceDrawingFlags  drawing;
     DFBAccelerationMask     clip;
} CardCapabilities;

typedef struct {
     unsigned int            surface_byteoffset_alignment;
     unsigned int            surface_pixelpitch_alignment;
     unsigned int            surface_bytepitch_alignment;

     unsigned int            surface_max_power_of_two_pixelpitch;
     unsigned int            surface_max_power_of_two_bytepitch;
     unsigned int            surface_max_power_of_two_height;

     DFBDimension            dst_min;
     DFBDimension            dst_max;
     DFBDimension            src_min;
     DFBDimension            src_max;
} CardLimitations;

DECLARE_MODULE_DIRECTORY( dfb_graphics_drivers );

/*
 * Increase this number when changes result in binary incompatibility!
 */
#define DFB_GRAPHICS_DRIVER_ABI_VERSION          35

#define DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH     40
#define DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH   60
#define DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH     100
#define DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH  40

#define DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH     48
#define DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH   64


typedef struct {
     int          major;        /* major version */
     int          minor;        /* minor version */
} GraphicsDriverVersion;        /* major.minor, e.g. 0.1 */

typedef struct {
     GraphicsDriverVersion version;

     char               name[DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH];
                                /* Name of driver, e.g. 'Matrox Driver' */

     char               vendor[DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH];
                                /* Vendor (or author) of the driver,
                                   e.g. 'directfb.org' or 'Denis Oliver Kropp' */

     char               url[DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH];
                                /* URL for driver updates,
                                   e.g. 'http://www.directfb.org/' */

     char               license[DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH];
                                /* License, e.g. 'LGPL' or 'proprietary' */

     unsigned int       driver_data_size;
     unsigned int       device_data_size;
} GraphicsDriverInfo;

typedef struct {
     char               name[DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH];
                                /* Device name, e.g. 'G400' */

     char               vendor[DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH];
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
      * Called after driver->InitDevice() and during dfb_gfxcard_unlock( true ).
      * The driver should do the one time initialization of the engine,
      * e.g. writing some registers that are supposed to have a fixed value.
      *
      * This happens after mode switching or after returning from
      * OpenGL state (e.g. DRI driver).
      */
     void (*EngineReset)( void *driver_data, void *device_data );

     /*
      * Makes sure that graphics hardware has finished all operations.
      *
      * This method is called before the CPU accesses a surface' buffer
      * that had been written to by the hardware after this method has been
      * called the last time.
      *
      * It's also called before entering the OpenGL state (e.g. DRI driver).
      */
     DFBResult (*EngineSync)( void *driver_data, void *device_data );
     
     /*
      * Called during dfb_gfxcard_lock() to notify the driver that
      * the current rendering state is no longer valid.
      */
     void (*InvalidateState)( void *driver_data, void *device_data );

     /*
      * after the video memory has been written to by the CPU (e.g. modification
      * of a texture) make sure the accelerator won't use cached texture data
      */
     void (*FlushTextureCache)( void *driver_data, void *device_data );

     /*
      * After the video memory has been written to by the accelerator
      * make sure the CPU won't read back cached data.
      */
     void (*FlushReadCache)( void *driver_data, void *device_data );

     /*
      * Called before a software access to a video surface buffer.
      */
     void (*SurfaceEnter)( void *driver_data, void *device_data,
                           CoreSurfaceBuffer *buffer, DFBSurfaceLockFlags flags );

     /*
      * Called after a software access to a video surface buffer.
      */
     void (*SurfaceLeave)( void *driver_data, void *device_data, CoreSurfaceBuffer *buffer );

     /*
      * Return the serial of the last (queued) operation.
      *
      * The serial is used to wait for finishing a specific graphics
      * operation instead of the whole engine being idle.
      */
     void (*GetSerial)( void *driver_data, void *device_data, CoreGraphicsSerial *serial );

     /*
      * Makes sure that graphics hardware has finished the specified operation.
      */
     DFBResult (*WaitSerial)( void *driver_data, void *device_data, const CoreGraphicsSerial *serial );

     /*
      * emit any buffered commands, i.e. trigger processing
      */
     void (*EmitCommands)  ( void *driver_data, void *device_data );

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
     bool (*FillRectangle) ( void *driver_data, void *device_data,
                             DFBRectangle *rect );

     bool (*DrawRectangle) ( void *driver_data, void *device_data,
                             DFBRectangle *rect );

     bool (*DrawLine)      ( void *driver_data, void *device_data,
                             DFBRegion *line );

     bool (*FillTriangle)  ( void *driver_data, void *device_data,
                             DFBTriangle *tri );

     bool (*FillTrapezoid) ( void *driver_data, void *device_data,
                             DFBTrapezoid *trap );

     bool (*FillQuadrangles)( void *driver_data, void *device_data,
                              DFBPoint *points, int num );

     bool (*DrawMonoGlyph)   ( void *driver_data, void *device_data,
                               const void *glyph, int glyph_width, int glyph_height, int glyph_rowbyte, int glyph_offset,
                               int dx, int dy, int fg_color, int bg_color, int hzoom, int vzoom );

     /*
      * blitting functions
      */
     bool (*Blit)            ( void *driver_data, void *device_data,
                               DFBRectangle *rect, int dx, int dy );

     bool (*Blit2)           ( void *driver_data, void *device_data,
                               DFBRectangle *rect, int dx, int dy, int sx2, int sy2 );

     bool (*StretchBlit)     ( void *driver_data, void *device_data,
                               DFBRectangle *srect, DFBRectangle *drect );

     bool (*TextureTriangles)( void *driver_data, void *device_data,
                               DFBVertex *vertices, int num,
                               DFBTriangleFormation formation );

     /*
      * Signal beginning of a sequence of operations using this state.
      * Any number of states can be 'drawing'.
      */
     void (*StartDrawing)( void *driver_data, void *device_data, CardState *state );

     /*
      * Signal end of sequence, i.e. destination surface is consistent again.
      */
     void (*StopDrawing)( void *driver_data, void *device_data, CardState *state );


     /*
      * BatchBlit
      *
      * When driver returns false (late fallback), it may set *ret_num
      * to the number of successful blits in case of partial execution.
      */
     bool (*BatchBlit)( void *driver_data, void *device_data,
                        const DFBRectangle *rects, const DFBPoint *points,
                        unsigned int num, unsigned int *ret_num );

     /*
      * BatchFill
      *
      * When driver returns false (late fallback), it may set *ret_num
      * to the number of successful fills in case of partial execution.
      */
     bool (*BatchFill)( void *driver_data, void *device_data,
                        const DFBRectangle *rects,
                        unsigned int num, unsigned int *ret_num );

     /* callbacks when a state is created or destroyed. This allows a graphics
        driver to hold additional state. */
     void (*StateInit)   ( void *driver_data, void *device_data, CardState *state );
     void (*StateDestroy)( void *driver_data, void *device_data, CardState *state );

     /*
      * Calculate the amount of memory and pitch for the specified
      * surface buffer.
      */
     DFBResult (*CalcBufferSize)( void *driver_data, void *device_data,
                                  CoreSurfaceBuffer  *buffer,
                                  int *ret_pitch, int *ret_length );
} GraphicsDeviceFuncs;

typedef struct {
     int       (*Probe)          (CoreGraphicsDevice  *device);
     void      (*GetDriverInfo)  (CoreGraphicsDevice  *device,
                                  GraphicsDriverInfo  *driver_info);

     DFBResult (*InitDriver)     (CoreGraphicsDevice  *device,
                                  GraphicsDeviceFuncs *funcs,
                                  void                *driver_data,
                                  void                *device_data,
                                  CoreDFB             *core);

     DFBResult (*InitDevice)     (CoreGraphicsDevice  *device,
                                  GraphicsDeviceInfo  *device_info,
                                  void                *driver_data,
                                  void                *device_data);

     void      (*CloseDevice)    (CoreGraphicsDevice  *device,
                                  void                *driver_data,
                                  void                *device_data);
     void      (*CloseDriver)    (CoreGraphicsDevice  *device,
                                  void                *driver_data);
} GraphicsDriverFuncs;

typedef enum {
     GDLF_NONE       = 0x00000000,

     GDLF_WAIT       = 0x00000001,
     GDLF_SYNC       = 0x00000002,
     GDLF_INVALIDATE = 0x00000004,
     GDLF_RESET      = 0x00000008
} GraphicsDeviceLockFlags;

DFBResult dfb_gfxcard_lock( GraphicsDeviceLockFlags flags );
void dfb_gfxcard_unlock( void );

DFBResult dfb_gfxcard_flush( void );

bool dfb_gfxcard_state_check( CardState *state, DFBAccelerationMask accel );

void dfb_gfxcard_state_init( CardState *state );
void dfb_gfxcard_state_destroy( CardState *state );

/*
 * Signal beginning of a sequence of operations using this state.
 * Any number of states can be 'drawing'.
 */
void dfb_gfxcard_start_drawing( CoreGraphicsDevice *device,
                                CardState          *state );

/*
 * Signal end of sequence, i.e. destination surface is consistent again.
 */
void dfb_gfxcard_stop_drawing ( CoreGraphicsDevice *device,
                                CardState          *state );

/*
 * drawing functions, lock source and destination surfaces,
 * handle clipping and drawing method (hardware/software)
 */
void dfb_gfxcard_fillrectangles         ( const DFBRectangle   *rects,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_drawrectangle          ( DFBRectangle         *rect,
                                          CardState            *state );

void dfb_gfxcard_drawlines              ( DFBRegion            *lines,
                                          int                   num_lines,
                                          CardState            *state );

void dfb_gfxcard_fillspans              ( int                   y,
                                          DFBSpan              *spans,
                                          int                   num_spans,
                                          CardState            *state );

void dfb_gfxcard_filltriangles          ( const DFBTriangle    *tris,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_fillquadrangles        ( DFBPoint             *points,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_filltrapezoids         ( const DFBTrapezoid   *traps,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_draw_mono_glyphs       ( const void                   *glyph[],
                                          const DFBMonoGlyphAttributes *attributes,
                                          const DFBPoint               *points,
                                          unsigned int                  num,
                                          CardState                    *state );

void dfb_gfxcard_blit                   ( DFBRectangle         *rect,
                                          int                   dx,
                                          int                   dy,
                                          CardState            *state );

void dfb_gfxcard_batchblit              ( DFBRectangle         *rects,
                                          DFBPoint             *points,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_batchblit2             ( DFBRectangle         *rects,
                                          DFBPoint             *points,
                                          DFBPoint             *points2,
                                          int                   num,
                                          CardState            *state );

void dfb_gfxcard_tileblit               ( DFBRectangle         *rect,
                                          int                   dx1,
                                          int                   dy1,
                                          int                   dx2,
                                          int                   dy2,
                                          CardState            *state );

void dfb_gfxcard_stretchblit            ( DFBRectangle         *srect,
                                          DFBRectangle         *drect,
                                          CardState            *state );

void dfb_gfxcard_batchstretchblit       ( DFBRectangle         *srects,
                                          DFBRectangle         *drects,
                                          unsigned int          num,
                                          CardState            *state );

void dfb_gfxcard_texture_triangles      ( DFBVertex            *vertices,
                                          int                   num,
                                          DFBTriangleFormation  formation,
                                          CardState            *state );






void dfb_gfxcard_drawstring             ( const u8             *text,
                                          int                   bytes,
                                          DFBTextEncodingID     encoding,
                                          int                   x,
                                          int                   y,
                                          CoreFont             *font,
                                          unsigned int          layers, 
                                          CoreGraphicsStateClient *client );

void dfb_gfxcard_drawglyph              ( CoreGlyphData       **glyph,
                                          int                   x,
                                          int                   y,
                                          CoreFont             *font,
                                          unsigned int          layers, 
                                          CoreGraphicsStateClient *client );





bool dfb_gfxcard_drawstring_check_state ( CoreFont             *font,
                                          CardState            *state );


DFBResult dfb_gfxcard_sync( void );

void dfb_gfxcard_invalidate_state( void );
DFBResult dfb_gfxcard_wait_serial( const CoreGraphicsSerial *serial );
void dfb_gfxcard_flush_texture_cache( void );
void dfb_gfxcard_flush_read_cache( void );
void dfb_gfxcard_after_set_var( void );
void dfb_gfxcard_surface_enter( CoreSurfaceBuffer *buffer, DFBSurfaceLockFlags flags );
void dfb_gfxcard_surface_leave( CoreSurfaceBuffer *buffer );

DFBResult dfb_gfxcard_adjust_heap_offset( int offset );

void            dfb_gfxcard_get_capabilities  ( CardCapabilities   *ret_caps );
void            dfb_gfxcard_get_device_info   ( GraphicsDeviceInfo *ret_info );
void            dfb_gfxcard_get_driver_info   ( GraphicsDriverInfo *ret_info );

int             dfb_gfxcard_reserve_memory    ( CoreGraphicsDevice  *device,
                                                unsigned int         size );
int             dfb_gfxcard_reserve_auxmemory ( CoreGraphicsDevice  *device,
                                                unsigned int         size );

unsigned int    dfb_gfxcard_memory_length     ( void );
unsigned int    dfb_gfxcard_auxmemory_length  ( void );

void           *dfb_gfxcard_get_device_data   ( void );
void           *dfb_gfxcard_get_driver_data   ( void );

CoreGraphicsDevice *dfb_gfxcard_get_primary   ( void );

/*
 * Graphics drivers call this function to get access to MMIO regions.
 *
 * device: Graphics device to map
 * offset: Offset from MMIO base (default offset is 0)
 * length: Length of mapped region (-1 uses default length)
 *
 * Returns the virtual address or NULL if mapping failed.
 */
volatile void *dfb_gfxcard_map_mmio( CoreGraphicsDevice *device,
                                     unsigned int        offset,
                                     int                 length );

/*
 * Graphics drivers call this function to unmap MMIO regions.
 *
 * addr:   Virtual address returned by gfxcard_map_mmio
 * length: Length of mapped region (-1 uses default length)
 */
void dfb_gfxcard_unmap_mmio( CoreGraphicsDevice *device,
                             volatile void      *addr,
                             int                 length );

int dfb_gfxcard_get_accelerator( CoreGraphicsDevice *device );

void dfb_gfxcard_get_limits( CoreGraphicsDevice *device,
                             CardLimitations    *ret_limits );

void dfb_gfxcard_calc_buffer_size( CoreGraphicsDevice *device,
                                   CoreSurfaceBuffer  *buffer,
                                   int                *ret_pitch,
                                   int                *ret_length );

unsigned long  dfb_gfxcard_memory_physical   ( CoreGraphicsDevice *device,
                                               unsigned int        offset );
void          *dfb_gfxcard_memory_virtual    ( CoreGraphicsDevice *device,
                                               unsigned int        offset );

unsigned long  dfb_gfxcard_auxmemory_physical( CoreGraphicsDevice *device,
                                               unsigned int        offset );
void          *dfb_gfxcard_auxmemory_virtual ( CoreGraphicsDevice *device,
                                               unsigned int        offset );


/* Hook for registering additional screen(s) and layer(s) in app or lib initializing DirectFB. */
extern void (*__DFB_CoreRegisterHook)( CoreDFB *core, CoreGraphicsDevice *device, void *ctx );
extern void  *__DFB_CoreRegisterHookCtx;





typedef struct {
     int                      magic;

     /* amount of usable memory */
     unsigned int             videoram_length;
     unsigned int             auxram_length;
     unsigned int             auxram_offset;

     char                    *module_name;

     GraphicsDriverInfo       driver_info;
     GraphicsDeviceInfo       device_info;
     void                    *device_data;

     FusionSkirmish           lock;
     GraphicsDeviceLockFlags  lock_flags;

     /*
      * Points to the current state of the graphics card.
      */
     CardState               *state;
     FusionID                 holder; /* Fusion ID of state owner. */

     FusionObjectID           last_allocation_id;
     DFBAccelerationMask      last_op;
     bool                     pending_ops;
} DFBGraphicsCoreShared;

struct __DFB_DFBGraphicsCore {
     int                        magic;

     CoreDFB                   *core;

     DFBGraphicsCoreShared     *shared;

     DirectModuleEntry         *module;
     const GraphicsDriverFuncs *driver_funcs;

     void                      *driver_data;
     void                      *device_data; /* copy of shared->device_data */

     CardCapabilities           caps;        /* local caps */
     CardLimitations            limits;      /* local limits */

     GraphicsDeviceFuncs        funcs;
};


#endif

