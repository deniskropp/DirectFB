/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

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

#include <directfb.h>

#include <direct/debug.h>
#include <direct/memcpy.h>
#include <direct/messages.h>

#include <core/state.h>
#include <core/surface.h>

#include <gfx/convert.h>

#include "vmware_2d.h"
#include "vmware_gfxdriver.h"


D_DEBUG_DOMAIN( VMWare_2D, "VMWare/2D", "VMWare 2D Acceleration" );

/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DESTINATION  = 0x00000001,
     COLOR        = 0x00000002,

     SOURCE       = 0x00000010,

     ALL          = 0x00000013
};

/*
 * State handling macros.
 */

#define VMWARE_VALIDATE(flags)        do { vdev->v_flags |=  (flags); } while (0)
#define VMWARE_INVALIDATE(flags)      do { vdev->v_flags &= ~(flags); } while (0)

#define VMWARE_CHECK_VALIDATE(flag)   do {                                           \
                                           if (! (vdev->v_flags & flag))             \
                                                vmware_validate_##flag( vdev, state );  \
                                      } while (0)


/**************************************************************************************************/

/*
 * Called by vmwareSetState() to ensure that the destination registers are properly set
 * for execution of rendering functions.
 */
static inline void
vmware_validate_DESTINATION( VMWareDeviceData *vdev,
                             CardState        *state )
{
     /* Remember destination parameters for usage in rendering functions. */
     vdev->dst_addr   = state->dst.addr;
     vdev->dst_pitch  = state->dst.pitch;
     vdev->dst_format = state->dst.buffer->format;
     vdev->dst_bpp    = DFB_BYTES_PER_PIXEL( vdev->dst_format );

     /* Set the flag. */
     VMWARE_VALIDATE( DESTINATION );
}

/*
 * Called by vmwareSetState() to ensure that the color register is properly set
 * for execution of rendering functions.
 */
static inline void
vmware_validate_COLOR( VMWareDeviceData *vdev,
                       CardState        *state )
{
     switch (vdev->dst_format) {
          case DSPF_ARGB:
               vdev->color_pixel = PIXEL_ARGB( state->color.a,
                                               state->color.r,
                                               state->color.g,
                                               state->color.b );
               break;

          case DSPF_RGB32:
               vdev->color_pixel = PIXEL_RGB32( state->color.r,
                                                state->color.g,
                                                state->color.b );
               break;

          case DSPF_RGB16:
               vdev->color_pixel = PIXEL_RGB16( state->color.r,
                                                state->color.g,
                                                state->color.b );
               break;

          default:
               D_BUG( "unexpected format %s", dfb_pixelformat_name(vdev->dst_format) );
     }

     /* Set the flag. */
     VMWARE_VALIDATE( COLOR );
}

/*
 * Called by vmwareSetState() to ensure that the source registers are properly set
 * for execution of blitting functions.
 */
static inline void
vmware_validate_SOURCE( VMWareDeviceData *vdev,
                        CardState        *state )
{
     /* Remember source parameters for usage in rendering functions. */
     vdev->src_addr   = state->src.addr;
     vdev->src_pitch  = state->src.pitch;
     vdev->src_format = state->src.buffer->format;
     vdev->src_bpp    = DFB_BYTES_PER_PIXEL( vdev->src_format );

     /* Set the flag. */
     VMWARE_VALIDATE( SOURCE );
}

/**************************************************************************************************/

/*
 * Check for acceleration of 'accel' using the given 'state'.
 */
void
vmwareCheckState( void                *drv,
                  void                *dev,
                  CardState           *state,
                  DFBAccelerationMask  accel )
{
     D_DEBUG_AT( VMWare_2D, "vmwareCheckState (state %p, accel 0x%08x) <- dest %p\n",
                 state, accel, state->destination );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(VMWARE_SUPPORTED_DRAWINGFUNCTIONS | VMWARE_SUPPORTED_BLITTINGFUNCTIONS))
          return;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_ARGB:
          case DSPF_RGB32:
          case DSPF_RGB16:
               break;

          default:
               return;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~VMWARE_SUPPORTED_DRAWINGFLAGS)
               return;
     }
     else {
          /* Return if the source format is not supported. */
          switch (state->source->config.format) {
               case DSPF_ARGB:
               case DSPF_RGB32:
               case DSPF_RGB16:
                    /* FIXME: Currently only copying blits supported. */
                    if (state->source->config.format == state->destination->config.format)
                         break;

               default:
                    return;
          }

          /* Return if unsupported blitting flags are set. */
          if (state->blittingflags & ~VMWARE_SUPPORTED_BLITTINGFLAGS)
               return;
     }

     /* Enable acceleration of the function. */
     state->accel |= accel;
}

/*
 * Make sure that the hardware is programmed for execution of 'accel' according to the 'state'.
 */
void
vmwareSetState( void                *drv,
                void                *dev,
                GraphicsDeviceFuncs *funcs,
                CardState           *state,
                DFBAccelerationMask  accel )
{
     VMWareDeviceData       *vdev     = (VMWareDeviceData*) dev;
     StateModificationFlags  modified = state->mod_hw;

     D_DEBUG_AT( VMWare_2D, "vmwareSetState (state %p, accel 0x%08x) <- dest %p, modified 0x%08x\n",
                 state, accel, state->destination, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          VMWARE_INVALIDATE( ALL );
     }
     else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               VMWARE_INVALIDATE( DESTINATION | COLOR );
          else if (modified & SMF_COLOR)
               VMWARE_INVALIDATE( COLOR );

          if (modified & SMF_SOURCE)
               VMWARE_INVALIDATE( SOURCE );
     }

     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination... */
     VMWARE_CHECK_VALIDATE( DESTINATION );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
               /* ...require valid drawing color. */
               VMWARE_CHECK_VALIDATE( COLOR );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VMWARE_SUPPORTED_DRAWINGFUNCTIONS;
               break;

          case DFXL_BLIT:
               /* ...require valid source. */
               VMWARE_CHECK_VALIDATE( SOURCE );

               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = VMWARE_SUPPORTED_BLITTINGFUNCTIONS;
               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;
     }

     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further validation.
      * If the hw independent state is not modified, this function won't get called
      * for subsequent rendering functions, unless they aren't defined by 3).
      */
     state->mod_hw = 0;
}

static void
virtual2D_postPacket( VMWareDriverData *vdrv,
                      Virtual2DPacket  *packet )
{
     while (vdrv->packet_count > 7777)
          direct_thread_sleep( 1000 );

     D_SYNC_ADD( &vdrv->packet_count, 1 );

     direct_processor_post( &vdrv->processor, packet );
}

/*
 * Render a filled rectangle using the current hardware state.
 */
bool
vmwareFillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
     VMWareDeviceData *vdev = (VMWareDeviceData*) dev;
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;
     Virtual2DPacket  *packet;

     D_DEBUG_AT( VMWare_2D, "%s( %d,%d-%dx%d )\n", __FUNCTION__, DFB_RECTANGLE_VALS( rect ) );

     packet = direct_processor_allocate( &vdrv->processor );
     if (!packet) {
          D_OOM();
          return false;
     }

     packet->op        = V2D_OP_FILL;

     packet->dst_addr  = vdev->dst_addr + rect->y * vdev->dst_pitch +
                         DFB_BYTES_PER_LINE(vdev->dst_format, rect->x);
     packet->dst_bpp   = vdev->dst_bpp;
     packet->dst_pitch = vdev->dst_pitch;

     packet->dst       = *rect;
     packet->color     = vdev->color_pixel;

     virtual2D_postPacket( vdrv, packet );

     return true;
}

/*
 * Blit a surface using the current hardware state.
 */
bool
vmwareBlit( void *drv, void *dev, DFBRectangle *srect, int dx, int dy )
{
     VMWareDeviceData *vdev = (VMWareDeviceData*) dev;
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;
     Virtual2DPacket  *packet;

     D_DEBUG_AT( VMWare_2D, "%s( %d,%d-%dx%d -> %d, %d )\n", __FUNCTION__,
                 DFB_RECTANGLE_VALS( srect ), dx, dy );

     packet = direct_processor_allocate( &vdrv->processor );
     if (!packet) {
          D_OOM();
          return false;
     }

     packet->op        = V2D_OP_BLIT;

     packet->dst_addr  = vdev->dst_addr + dy * vdev->dst_pitch +
                         DFB_BYTES_PER_LINE(vdev->dst_format, dx);
     packet->dst_bpp   = vdev->dst_bpp;
     packet->dst_pitch = vdev->dst_pitch;

     packet->src_addr  = vdev->src_addr + srect->y * vdev->src_pitch +
                         DFB_BYTES_PER_LINE(vdev->src_format, srect->x);
     packet->src_bpp   = vdev->src_bpp;
     packet->src_pitch = vdev->src_pitch;

     packet->dst.x     = dx;
     packet->dst.y     = dy;
     packet->src       = *srect;

     virtual2D_postPacket( vdrv, packet );

     return true;
}

/*********************************************************************************************************************/

/*
 * Reset the graphics engine.
 */
void
vmwareEngineReset( void *drv, void *dev )
{
     D_DEBUG_AT( VMWare_2D, "%s()\n", __FUNCTION__ );
}

/*
 * Wait for the blitter to be idle.
 *
 * This function is called before memory that has been written to by the hardware is about to be
 * accessed by the CPU (software driver) or another hardware entity like video encoder (by Flip()).
 * It can also be called by applications explicitly, e.g. at the end of a benchmark loop to include
 * execution time of queued commands in the measurement.
 */
DFBResult
vmwareEngineSync( void *drv, void *dev )
{
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;

     D_DEBUG_AT( VMWare_2D, "%s()\n", __FUNCTION__ );

     direct_mutex_lock( &vdrv->wait_lock );

     while (vdrv->done.serial != vdrv->last.serial || vdrv->done.generation != vdrv->last.generation)
          direct_waitqueue_wait( &vdrv->wait_queue, &vdrv->wait_lock );

     direct_mutex_unlock( &vdrv->wait_lock );

     return DFB_OK;
}

DFBResult
vmwareWaitSerial( void *drv, void *dev,
                  const CoreGraphicsSerial *serial )
{
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;

     D_DEBUG_AT( VMWare_2D, "%s( %d, %d )\n", __FUNCTION__, serial->generation, serial->serial );

     direct_mutex_lock( &vdrv->wait_lock );

     while ((vdrv->done.generation == serial->generation && vdrv->done.serial < serial->serial) ||
             vdrv->done.generation < serial->generation)
          direct_waitqueue_wait( &vdrv->wait_queue, &vdrv->wait_lock );

     direct_mutex_unlock( &vdrv->wait_lock );

     return DFB_OK;
}

void
vmwareGetSerial( void *drv, void *dev,
                 CoreGraphicsSerial *serial )
{
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;

     *serial = vdrv->next;

     //D_DEBUG_AT( VMWare_2D, "%s() -> %d, %d\n", __FUNCTION__, serial->generation, serial->serial );
}

/*
 * Start processing of queued commands if required.
 *
 * This function is called before returning from the graphics core to the application.
 * Usually that's after each rendering function. The only functions causing multiple commands
 * to be queued with a single emition at the end are DrawString(), TileBlit(), BatchBlit(),
 * DrawLines() and possibly FillTriangle() which is emulated using multiple FillRectangle() calls.
 */
void
vmwareEmitCommands( void *drv, void *dev )
{
     VMWareDriverData *vdrv = (VMWareDriverData*) drv;
     Virtual2DPacket  *packet;

     packet = direct_processor_allocate( &vdrv->processor );
     if (!packet) {
          D_OOM();
          return;
     }

     D_DEBUG_AT( VMWare_2D, "%s() -> %d, %d\n", __FUNCTION__, vdrv->next.generation, vdrv->next.serial );

     packet->op     = V2D_OP_SERIAL;
     packet->serial = vdrv->next;

     virtual2D_postPacket( vdrv, packet );

     vdrv->last = vdrv->next;

     if (!++(vdrv->next.serial))
          vdrv->next.generation++;
}

/*********************************************************************************************************************/

static DirectResult
virtual2DProcess( DirectProcessor *processor,
                  void            *data,
                  void            *context )
{
     Virtual2DPacket  *packet = data;
     VMWareDriverData *vdrv   = (VMWareDriverData*) context;
     void             *dst    = packet->dst_addr;
     void             *src    = packet->src_addr;

     D_SYNC_ADD( &vdrv->packet_count, -1 );

     switch (packet->op) {
          case V2D_OP_SERIAL:
               D_DEBUG_AT( VMWare_2D, "  -> SERIAL %d, %d\n", packet->serial.generation, packet->serial.serial );

               direct_mutex_lock( &vdrv->wait_lock );

               vdrv->done = packet->serial;

               direct_waitqueue_broadcast( &vdrv->wait_queue );

               direct_mutex_unlock( &vdrv->wait_lock );
               break;

          case V2D_OP_FILL:
               D_DEBUG_AT( VMWare_2D, "  -> FILL %d,%d-%dx%d\n", DFB_RECTANGLE_VALS( &packet->dst ) );

               switch (packet->dst_bpp) {
                    case 4:
                         while (packet->dst.h--) {
                              int  w     = packet->dst.w;
                              u32 *dst32 = dst;

                              while (w--)
                                   *dst32++ = packet->color;

                              dst += packet->dst_pitch;
                         }
                         break;

                    case 2:
                         while (packet->dst.h--) {
                              int  w     = packet->dst.w;
                              u16 *dst16 = dst;

                              while (w--)
                                   *dst16++ = packet->color;

                              dst += packet->dst_pitch;
                         }
                         break;

                    case 1:
                         while (packet->dst.h--) {
                              int  w    = packet->dst.w;
                              u8  *dst8 = dst;

                              while (w--)
                                   *dst8++ = packet->color;

                              dst += packet->dst_pitch;
                         }
                         break;
               }
               break;

          case V2D_OP_BLIT:
               D_DEBUG_AT( VMWare_2D, "  -> BLIT %d,%d-%dx%d -> %d, %d\n", DFB_RECTANGLE_VALS( &packet->src ),
                           packet->dst.x, packet->dst.y );

               while (packet->src.h--) {
                    direct_memcpy( dst, src, packet->src.w * packet->dst_bpp );

                    dst += packet->dst_pitch;
                    src += packet->src_pitch;
               }
               break;

          default:
               D_BUG( "unknown op" );
     }

     return DR_OK;
}

static const DirectProcessorFuncs _virtual2DFuncs = {
     .Process = virtual2DProcess
};

const DirectProcessorFuncs *virtual2DFuncs = &_virtual2DFuncs;

