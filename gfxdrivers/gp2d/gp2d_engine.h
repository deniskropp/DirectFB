#ifndef __GP2D_ENGINE_H__
#define __GP2D_ENGINE_H__


#ifdef __cplusplus
extern "C" {
#endif

#include <direct/fifo.h>
#include <direct/thread.h>
#include <direct/trace.h>

#include <core/surface.h>

#include <directfb.h>


void register_gp2d( GP2DDriverData *drv );


#ifdef __cplusplus
}

#include <core/PacketBuffer.h>
#include <core/Renderer.h>
#include <core/Util.h>


namespace Renesas {

class Foo {
};

class GP2DBuffer {
public:
     GP2DBuffer( size_t size )
          :
          size( size ),
          length( 0 )
     {
          buffer = gp2d_get_buffer( (GP2DDriverData*)dfb_gfxcard_get_driver_data(), size );
          D_ASSERT( buffer != NULL );

          ptr = buffer->mapped;
     }

     ~GP2DBuffer()
     {
          if (buffer)
               gp2d_put_buffer( (GP2DDriverData*)dfb_gfxcard_get_driver_data(), buffer );
     }

     DFBResult Execute()
     {
          DFBResult ret;

          D_ASSERT( buffer != NULL );

          D_ASSERT( buffer->size >= length );

          buffer->used = length;

          ret = gp2d_exec_buffer( (GP2DDriverData*)dfb_gfxcard_get_driver_data(), buffer );
          if (ret) {
               D_DERROR( ret, "GP2D/Task: gp2d_exec_buffer( length %zu ) failed!\n", length );
               return ret;
          }

          buffer = NULL;

          return DFB_OK;
     }

     size_t  size;
     size_t  length;
     void   *ptr;

     ::GP2DBuffer *buffer;
};




class GP2DEngine;

class GP2DTask : public DirectFB::SurfaceTask
{
public:
     GP2DTask( GP2DEngine *engine );

     u32  *start( unsigned int num );
     void  submit( unsigned int num );

protected:
     virtual DFBResult Push();
     virtual void Describe( Direct::String string ) {
         SurfaceTask::Describe( string );
         string.PrintF( "  length %zu, buffers %zu", packets.GetLength(), packets.buffers.size() );
     }

private:
     friend class GP2DEngine;

     GP2DEngine   *engine;
//     GP2DBuffer   *buffer;

     DirectFB::Util::PacketBuffer<GP2DBuffer> packets;


     /* state validation */
     int                      v_flags;

     /* prepared register values */
     u32                      ble_srcf;
     u32                      ble_dstf;

     /* cached values */
     unsigned long            dst_phys;
     int                      dst_pitch;
     int                      dst_bpp;
     int                      dst_index;

     unsigned long            src_phys;
     int                      src_pitch;
     int                      src_bpp;
     int                      src_index;

     unsigned long            mask_phys;
     int                      mask_pitch;
     DFBSurfacePixelFormat    mask_format;
     int                      mask_index;
     DFBPoint                 mask_offset;
     DFBSurfaceMaskFlags      mask_flags;

     DFBSurfaceDrawingFlags   dflags;
     DFBSurfaceBlittingFlags  bflags;
     DFBSurfaceRenderOptions  render_options;

     bool                     ckey_b_enabled;
     bool                     color_change_enabled;
     bool                     mask_enabled;

     unsigned int             input_mask;

     s32                      matrix[6];
     DFBColor                 color;

     /* gp2d */
     u32                      rclr;
     u32                      color16;
};


class GP2DEngine : public DirectFB::Engine {
private:
     GP2DDriverData *drv;

public:
     GP2DEngine( GP2DDriverData *drv );


     virtual DFBResult bind          ( DirectFB::Renderer::Setup *setup );

     virtual DFBResult check         ( DirectFB::Renderer::Setup *setup );

     virtual DFBResult CheckState    ( CardState              *state,
                                       DFBAccelerationMask     accel );

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel );

     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects );

     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             unsigned int            num );

/*
     virtual DFBResult DrawRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects );

     virtual DFBResult DrawLines( DirectFB::SurfaceTask  *task,
                                  const DFBRegion        *lines,
                                  unsigned int            num_lines );

     virtual DFBResult FillTriangles( DirectFB::SurfaceTask  *task,
                                      const DFBTriangle      *tris,
                                      unsigned int            num_tris );

     virtual DFBResult FillTrapezoids( DirectFB::SurfaceTask  *task,
                                       const DFBTrapezoid     *traps,
                                       unsigned int            num_traps );

     virtual DFBResult FillSpans( DirectFB::SurfaceTask  *task,
                                  int                     y,
                                  const DFBSpan          *spans,
                                  unsigned int            num_spans );


     virtual DFBResult Blit2( DirectFB::SurfaceTask  *task,
                              const DFBRectangle     *rects,
                              const DFBPoint         *points1,
                              const DFBPoint         *points2,
                              u32                     num );

     virtual DFBResult StretchBlit( DirectFB::SurfaceTask  *task,
                                    const DFBRectangle     *srects,
                                    const DFBRectangle     *drects,
                                    u32                     num );

     virtual DFBResult TileBlit( DirectFB::SurfaceTask  *task,
                                 const DFBRectangle     *rects,
                                 const DFBPoint         *points1,
                                 const DFBPoint         *points2,
                                 u32                     num );

     virtual DFBResult TextureTriangles( DirectFB::SurfaceTask  *task,
                                         const DFBVertex        *vertices,
                                         int                     num,
                                         DFBTriangleFormation    formation );
*/

private:
     friend class GP2DTask;



     void
     validate_DEST_CLIP( GP2DTask  *mytask,
                         CardState *state );
     
     void
     validate_COLOR16( GP2DTask  *mytask,
                       CardState *state );
     
     void
     validate_ALPHA( GP2DTask  *mytask,
                     CardState *state );
     
     void
     validate_SOURCE( GP2DTask  *mytask,
                      CardState *state );
     
     void
     validate_STRANS( GP2DTask  *mytask,
                      CardState *state );
};


}

#endif // __cplusplus


#endif
