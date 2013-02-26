//#ifdef GP2D_DEBUG_BLT
     #define DIRECT_ENABLE_DEBUG
//#endif


#include <config.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>
#include <errno.h>

#include <asm/types.h>

#include <directfb.h>

extern "C" {
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surface.h>
#include <core/surface_buffer.h>
#include <core/surface_pool.h>

#include <gfx/convert.h>

#include "gp2d_driver.h"
#include "gp2d_blt.h"
}

#include "gp2d_engine.h"


D_DEBUG_DOMAIN( GP2D_Engine, "GP2D/Engine", "Renesas GP2D Drawing Engine Engine" );
D_DEBUG_DOMAIN( GP2D_Task,   "GP2D/Task",   "Renesas GP2D Drawing Engine Task" );



extern "C" {
     static Renesas::GP2DEngine *gp2d_engine;

     void
     register_gp2d( GP2DDriverData *drv )
     {
          if (!gp2d_engine && dfb_config->task_manager) {
               gp2d_engine = new Renesas::GP2DEngine( drv );

               DirectFB::Renderer::RegisterEngine( gp2d_engine );
          }
     }
}


/*
 * State validation flags.
 *
 * There's no prefix because of the macros below.
 */
enum {
     DEST         = 0x00000001,
     CLIP         = 0x00000002,
     DEST_CLIP    = 0x00000003,
     
     COLOR16      = 0x00000100,

     ALPHA        = 0x00001000,

     SOURCE       = 0x00010000,
     STRANS       = 0x00020000,

     ALL          = 0x00031103,
};

/*
 * State handling macros.
 */

#define GP2D_VALIDATE(flags)          do { mytask->v_flags |=  (flags); } while (0)
#define GP2D_INVALIDATE(flags)        do { mytask->v_flags &= ~(flags); } while (0)

#define GP2D_CHECK_VALIDATE(flag)     do {                                                    \
                                             if ((mytask->v_flags & flag) != flag)            \
                                                  validate_##flag( mytask, state );           \
                                        } while (0)


namespace Renesas {



GP2DTask::GP2DTask( GP2DEngine *engine )
     :
     SurfaceTask( CSAID_GPU ),
     engine( engine ),
     packets( GP2DGFX_MAX_PREPARE * 4 )
{
     D_DEBUG_AT( GP2D_Task, "GP2DTask::%s( %p )\n", __FUNCTION__, (void*)this );
}

u32 *
GP2DTask::start( unsigned int num )
{
     D_DEBUG_AT( GP2D_Task, "GP2DTask::%s( %p, %u )\n", __FUNCTION__, (void*)this, num );

     D_DEBUG_AT( GP2D_Task, "  -> task length %d\n", packets.GetLength() );

     return (u32*) packets.GetBuffer( num * 4 );
}

void
GP2DTask::submit( unsigned int num )
{
     D_DEBUG_AT( GP2D_Task, "GP2DTask::%s( %p, %u )\n", __FUNCTION__, (void*)this, num );

     packets.PutBuffer( num * 4 );

     D_DEBUG_AT( GP2D_Task, "  -> task length %d\n", packets.GetLength() );
}

DFBResult
GP2DTask::Push()
{
     D_DEBUG_AT( GP2D_Task, "GP2DTask::%s( %p )\n", __FUNCTION__, (void*)this );

     if (packets.GetLength() == 0) {
          Done();

          return DFB_OK;
     }

     D_DEBUG_AT( GP2D_Task, "  -> task length %d\n", packets.GetLength() );

     for (std::vector<GP2DBuffer*>::const_iterator it=packets.buffers.begin(); it!=packets.buffers.end(); it++) {
          ::GP2DBuffer *buffer = (*it)->buffer;// gp2d_get_buffer( engine->drv );

          D_ASSERT( buffer != NULL );

          if ((*it) == packets.buffers[packets.buffers.size()-1])
               buffer->task = this;

          D_ASSERT( (*it)->length <= buffer->size - 4 );
          *(u32*)((u8*)buffer->mapped + (*it)->length) = 0;

          buffer->used = (*it)->length + 4;

          (*it)->Execute();
     }

     return DFB_OK;
}


GP2DEngine::GP2DEngine( GP2DDriverData *drv )
     :
     drv( drv )
{
     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s()\n", __FUNCTION__ );

     caps.cores            = 1;
     caps.clipping         = DFXL_ALL;
     caps.render_options   = (DFBSurfaceRenderOptions)(DSRO_SMOOTH_DOWNSCALE | DSRO_SMOOTH_UPSCALE);
     caps.max_scale_down_x = 63;
     caps.max_scale_down_y = 63;
     caps.max_operations   = 7000;
}



/**********************************************************************************************************************/

void
GP2DEngine::validate_DEST_CLIP( GP2DTask  *mytask,
                                CardState *state )
{
     __u32 *prep = mytask->start( 18 );

     D_DEBUG_AT( GP2D_Engine, "%s( %p [%d] - %4d,%4d-%4dx%4d )\n", __FUNCTION__,
                 state->dst.handle, state->dst.pitch, DFB_RECTANGLE_VALS_FROM_REGION( &state->clip ) );


     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x0d4;
     prep[2] = GP2D_XY( state->clip.x1, state->clip.y1 ) ;

     prep[3] = M2DG_OPCODE_WPR;
     prep[4] = 0x0d8;
     prep[5] = GP2D_XY( state->clip.x2, state->clip.y2) ;
     
     if (mytask->v_flags & DEST) {
          mytask->submit( 6 );
     }
     else {
          CoreSurface       *surface = state->destination;
          CoreSurfaceBuffer *buffer  = state->dst.buffer;
     
          mytask->dst_phys  = (unsigned long) state->dst.handle;
          mytask->dst_pitch = state->dst.pitch;
          mytask->dst_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
          mytask->dst_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

          mytask->rclr &= ~0x00140000;
     
          switch (buffer->format) {
               case DSPF_RGB16:
                    mytask->rclr |= 0x00040000;
                    break;
     
               case DSPF_ARGB1555:
                    mytask->rclr |= 0x00140000;
                    break;
     
               default:
                    D_BUG("Unexpected pixelformat\n");
                    return;
          }
     
          /* Set destination start address. */
          prep[ 6] = M2DG_OPCODE_WPR;
          prep[ 7] = 0x50;
          prep[ 8] = mytask->dst_phys;
     
          /* Set destination stride. */
          prep[ 9] = M2DG_OPCODE_WPR;
          prep[10] = 0x5c;
          prep[11] = mytask->dst_pitch / mytask->dst_bpp;
     
          /* Set destination pixelformat in rendering control. */
          prep[12] = M2DG_OPCODE_WPR;
          prep[13] = 0xc0;
          prep[14] = mytask->rclr;
     
          /* Set system clipping rectangle. */
          prep[15] = M2DG_OPCODE_WPR;
          prep[16] = 0xd0;
          prep[17] = GP2D_XY( surface->config.size.w - 1, surface->config.size.h - 1 );
     
          mytask->submit( 18 );
     }

     /* Set the flags. */
     GP2D_VALIDATE( DEST_CLIP );
}

void
GP2DEngine::validate_COLOR16( GP2DTask  *mytask,
                              CardState *state )
{
     mytask->color16 = dfb_pixel_from_color( state->destination->config.format, &state->color );

     /* Set the flags. */
     GP2D_VALIDATE( COLOR16 );
}

void
GP2DEngine::validate_ALPHA( GP2DTask  *mytask,
                            CardState *state )
{
     __u32 *prep = mytask->start( 3 );

     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x088;
     prep[2] = state->color.a;

     mytask->submit( 3 );

     /* Set the flags. */
     GP2D_VALIDATE( ALPHA );
}

void
GP2DEngine::validate_SOURCE( GP2DTask  *mytask,
                             CardState *state )
{
     __u32 *prep = mytask->start( 6 );

     CoreSurfaceBuffer *buffer = state->src.buffer;

     mytask->src_phys  = (unsigned long) state->src.handle;
     mytask->src_pitch = state->src.pitch;
     mytask->src_bpp   = DFB_BYTES_PER_PIXEL( buffer->format );
     mytask->src_index = DFB_PIXELFORMAT_INDEX( buffer->format ) % DFB_NUM_PIXELFORMATS;

     /* Set source start address. */
     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x4c;
     prep[2] = mytask->src_phys;

     /* Set source stride. */
     prep[3] = M2DG_OPCODE_WPR;
     prep[4] = 0x58;
     prep[5] = mytask->src_pitch / mytask->src_bpp;

     mytask->submit( 6 );

     /* Set the flags. */
     GP2D_VALIDATE( SOURCE );
}

void
GP2DEngine::validate_STRANS( GP2DTask  *mytask,
                             CardState *state )
{
     __u32 *prep = mytask->start( 3 );

     prep[0] = M2DG_OPCODE_WPR;
     prep[1] = 0x080;
     prep[2] = state->src_colorkey;

     mytask->submit( 3 );

     /* Set the flags. */
     GP2D_VALIDATE( STRANS );
}

/**********************************************************************************************************************/

DFBResult
GP2DEngine::bind( DirectFB::Renderer::Setup *setup )
{
     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s()\n", __FUNCTION__ );

     for (unsigned int i=0; i<setup->tiles; i++) {
          setup->tasks[i] = new GP2DTask( this );
     }

     return DFB_OK;
}

DFBResult
GP2DEngine::check( DirectFB::Renderer::Setup *setup )
{
     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s()\n", __FUNCTION__ );

     for (unsigned int i=0; i<setup->tiles; i++) {
          GP2DTask *mytask = (GP2DTask *) setup->tasks[i];

          if (mytask->packets.GetLength() >= GP2DGFX_MAX_PREPARE*4*4) {
//               fprintf(stderr,"limit %u/%u\n",mytask->buffer->used,mytask->buffer->size);
               return DFB_LIMITEXCEEDED;
          }
     }

     return DFB_OK;
}

DFBResult
GP2DEngine::CheckState( CardState              *state,
                        DFBAccelerationMask     accel )
{
     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s()\n", __FUNCTION__ );

     /* Return if the desired function is not supported at all. */
     if (accel & ~(GP2D_SUPPORTED_DRAWINGFUNCTIONS | GP2D_SUPPORTED_BLITTINGFUNCTIONS))
          return DFB_UNSUPPORTED;

     /* Return if the destination format is not supported. */
     switch (state->destination->config.format) {
          case DSPF_RGB16:
//          case DSPF_ARGB1555:
               break;

          default:
               return DFB_UNSUPPORTED;
     }

     /* Check if drawing or blitting is requested. */
     if (DFB_DRAWING_FUNCTION( accel )) {
          /* Return if unsupported drawing flags are set. */
          if (state->drawingflags & ~GP2D_SUPPORTED_DRAWINGFLAGS)
               return DFB_UNSUPPORTED;

          /* Return if blending with unsupported blend functions is requested. */
          if (state->drawingflags & DSDRAW_BLEND) {
               switch (accel) {
                    case DFXL_FILLRECTANGLE:
                    case DFXL_FILLTRIANGLE:
                         break;
                    default:
                         return DFB_UNSUPPORTED;
               }

               /* Return if blending with unsupported blend functions is requested. */
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA)
                    return DFB_UNSUPPORTED;

               /* XOR only without blending. */
               if (state->drawingflags & DSDRAW_XOR)
                    return DFB_UNSUPPORTED;
          }
     } else {
          DFBSurfaceBlittingFlags flags = state->blittingflags;

          /* Return if unsupported blitting flags are set. */
          if (flags & ~GP2D_SUPPORTED_BLITTINGFLAGS)
               return DFB_UNSUPPORTED;

          /* Return if the source format is not supported. */
          if (state->source->config.format != state->destination->config.format)
               return DFB_UNSUPPORTED;

          /* Return if blending with unsupported blend functions is requested. */
          if (flags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA)) {
               if (state->src_blend != DSBF_SRCALPHA || state->dst_blend != DSBF_INVSRCALPHA)
                    return DFB_UNSUPPORTED;
          }

          /* XOR only without blending etc. */
          if (flags & DSBLIT_XOR &&
              flags & ~(DSBLIT_SRC_COLORKEY | DSBLIT_ROTATE180 | DSBLIT_XOR))
               return DFB_UNSUPPORTED;

          /* Return if colorizing for non-font surfaces is requested. */
          if ((flags & DSBLIT_COLORIZE) && !(state->source->type & CSTF_FONT))
               return DFB_UNSUPPORTED;

          /* Return if blending with both alpha channel and value is requested. */
          if (D_FLAGS_ARE_SET( flags, DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA))
               return DFB_UNSUPPORTED;
     }

     return DFB_OK;
}

DFBResult
GP2DEngine::SetState( DirectFB::SurfaceTask  *task,
                      CardState              *state,
                      StateModificationFlags  modified,
                      DFBAccelerationMask     accel )
{
     GP2DTask *mytask = (GP2DTask *)task;

     (void)modified;

     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s( %p, 0x%08x ) <- modified 0x%08x\n",
                 __FUNCTION__, state, accel, modified );

     /*
      * 1) Invalidate hardware states
      *
      * Each modification to the hw independent state invalidates one or more hardware states.
      */

     /* Simply invalidate all? */
     if (modified == SMF_ALL) {
          GP2D_INVALIDATE( ALL );
     } else if (modified) {
          /* Invalidate destination registers. */
          if (modified & SMF_DESTINATION)
               GP2D_INVALIDATE( DEST | COLOR16 );

          /* Invalidate clipping registers. */
          if (modified & SMF_CLIP)
               GP2D_INVALIDATE( CLIP );
     
          /* Invalidate color registers. */
          if (modified & SMF_COLOR)
               GP2D_INVALIDATE( ALPHA | COLOR16 );

          /* Invalidate source registers. */
          if (modified & SMF_SOURCE)
               GP2D_INVALIDATE( SOURCE );

          /* Invalidate source colorkey. */
          if (modified & SMF_SRC_COLORKEY)
               GP2D_INVALIDATE( STRANS );
     }          
      
     /*
      * 2) Validate hardware states
      *
      * Each function has its own set of states that need to be validated.
      */

     /* Always requiring valid destination and clip. */
     GP2D_CHECK_VALIDATE( DEST_CLIP );

     /* Depending on the function... */
     switch (accel) {
          case DFXL_FILLRECTANGLE:
          case DFXL_DRAWRECTANGLE:
          case DFXL_FILLTRIANGLE:
          case DFXL_DRAWLINE:
               /* ...require valid color. */
               GP2D_CHECK_VALIDATE( COLOR16 );

               /* If blending is used, validate the alpha value. */
               if (state->drawingflags & DSDRAW_BLEND)
                    GP2D_CHECK_VALIDATE( ALPHA );
               
               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = (DFBAccelerationMask) GP2D_SUPPORTED_DRAWINGFUNCTIONS;

               break;

          case DFXL_BLIT:
               /* ...require valid source. */
               GP2D_CHECK_VALIDATE( SOURCE );

               /* If blending is used, validate the alpha value. */
               if (state->blittingflags & DSBLIT_BLEND_COLORALPHA)
                    GP2D_CHECK_VALIDATE( ALPHA );

               /* If colorkeying is used, validate the colorkey. */
               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    GP2D_CHECK_VALIDATE( STRANS );
               
               /*
                * 3) Tell which functions can be called without further validation, i.e. SetState()
                *
                * When the hw independent state is changed, this collection is reset.
                */
               state->set = (DFBAccelerationMask) GP2D_SUPPORTED_BLITTINGFUNCTIONS;

               break;

          default:
               D_BUG( "unexpected drawing/blitting function" );
               break;

     }

     mytask->dflags         = state->drawingflags;
     mytask->bflags         = state->blittingflags;
     mytask->render_options = state->render_options;
     mytask->color          = state->color;
     
     /*
      * 4) Clear modification flags
      *
      * All flags have been evaluated in 1) and remembered for further validation.
      * If the hw independent state is not modified, this function won't get called
      * for subsequent rendering functions, unless they aren't defined by 3).
      */
     state->mod_hw = SMF_NONE;

     return DFB_OK;
}

DFBResult
GP2DEngine::FillRectangles( DirectFB::SurfaceTask  *task,
                            const DFBRectangle     *rects,
                            unsigned int            num_rects )
{
     GP2DTask *mytask = (GP2DTask *)task;

     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s( %d )\n", __FUNCTION__, num_rects );

     __u32 *prep = mytask->start( 6 * num_rects );

     for (unsigned int i=0; i<num_rects; i++) {
          prep[0] = M2DG_OPCODE_BITBLTC | M2DG_DRAWMODE_CLIP;

          if (mytask->dflags & DSDRAW_BLEND) 
               prep[0] |= M2DG_DRAWMODE_ALPHA;

          prep[1] = 0xcc; 
          prep[2] = mytask->color16;
          prep[3] = rects[i].w - 1;
          prep[4] = rects[i].h - 1;
          prep[5] = GP2D_XY( rects[i].x, rects[i].y );

          prep += 6;
     }

     mytask->submit( 6 * num_rects );

     return DFB_OK;
}

DFBResult
GP2DEngine::Blit( DirectFB::SurfaceTask  *task,
                  const DFBRectangle     *rects,
                  const DFBPoint         *points,
                  unsigned int            num )
{
     GP2DTask *mytask = (GP2DTask *)task;

     D_DEBUG_AT( GP2D_Engine, "GP2DEngine::%s( %d )\n", __FUNCTION__, num );

     __u32 *prep = mytask->start( 6 * num );

     for (unsigned int i=0; i<num; i++) {
          prep[0] = M2DG_OPCODE_BITBLTA | M2DG_DRAWMODE_CLIP;

          if (mytask->bflags & DSBLIT_BLEND_COLORALPHA) 
               prep[0] |= M2DG_DRAWMODE_ALPHA;

          if (mytask->bflags & DSBLIT_SRC_COLORKEY) 
               prep[0] |= M2DG_DRAWMODE_STRANS;

          if (mytask->src_phys == mytask->dst_phys) {
               if (points[i].y > rects[i].y)
                    prep[0] |= M2DG_DRAWMODE_DSTDIR_Y | M2DG_DRAWMODE_SRCDIR_Y;
               else if (points[i].y == rects[i].y) {
                    if (points[i].x > rects[i].x) 
                         prep[0] |= M2DG_DRAWMODE_DSTDIR_X | M2DG_DRAWMODE_SRCDIR_X;
               }
          }

          prep[1] = 0xcc; 
          prep[2] = GP2D_XY( rects[i].x, rects[i].y );
          prep[3] = rects[i].w - 1;
          prep[4] = rects[i].h - 1;
          prep[5] = GP2D_XY( points[i].x, points[i].y );

          prep += 6;
     }

     mytask->submit( 6 * num );

     return DFB_OK;
}


}

