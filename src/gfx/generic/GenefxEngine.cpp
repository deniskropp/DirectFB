/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <core/PacketBuffer.h>
#include <core/Renderer.h>
#include <core/Util.h>

extern "C" {
#include <directfb.h>

#include <direct/debug.h>
#include <direct/messages.h>

#include <core/core.h>
#include <core/palette.h>
#include <core/state.h>
#include <core/surface_allocation.h>
#include <core/surface_pool.h>

#include <gfx/clip.h>
#include <gfx/convert.h>
#include <gfx/generic/generic.h>
}

#include <core/Util.h>


D_DEBUG_DOMAIN( DirectFB_Renderer, "DirectFB/Renderer", "DirectFB Renderer" );

/*********************************************************************************************************************/

namespace DirectFB {





D_DEBUG_DOMAIN( DirectFB_GenefxEngine, "DirectFB/Genefx/Engine", "DirectFB Genefx Engine" );
D_DEBUG_DOMAIN( DirectFB_GenefxTask,   "DirectFB/Genefx/Task",   "DirectFB Genefx Task" );


// FIXME: find better auto detection, runtime options or dynamic adjustment for the following values
#if defined(ARCH_X86) || defined(ARCH_X86_64)
#define DFB_GENEFX_COMMAND_BUFFER_BLOCK_SIZE 0x40000   // 256k
#define DFB_GENEFX_COMMAND_BUFFER_MAX_SIZE   0x130000  // 1216k
#define DFB_GENEFX_TASK_WEIGHT_MAX           300000000
#else
#define DFB_GENEFX_COMMAND_BUFFER_BLOCK_SIZE 0x8000    // 32k
#define DFB_GENEFX_COMMAND_BUFFER_MAX_SIZE   0x17800   // 94k
#define DFB_GENEFX_TASK_WEIGHT_MAX           1000000
#endif


class GenefxEngine;

class GenefxTask : public DirectFB::SurfaceTask
{
public:
     GenefxTask( GenefxEngine    *engine,
                 const DFBRegion &clip,
                 unsigned int     tile_number )
          :
          SurfaceTask( CSAID_CPU ),
          engine( engine ),
          tile_clip( clip ),
          commands( DFB_GENEFX_COMMAND_BUFFER_BLOCK_SIZE ),
          weight( 0 ),
          weight_shift_draw( 0 ),
          weight_shift_blit( 0 ),
          tile_number( tile_number ),
          modified( SMF_NONE )
     {
          D_FLAGS_SET( flags, TASK_FLAG_NEED_SLAVE_PUSH );
     }

     virtual ~GenefxTask()
     {
     }

protected:
     virtual DFBResult Setup();
     virtual DFBResult Push();
     virtual DFBResult Run();
     virtual void      Finalise();

private:
     friend class GenefxEngine;

     GenefxEngine *engine;
     DFBRegion     tile_clip;

     typedef enum {
          TYPE_SET_DESTINATION,
          TYPE_SET_CLIP,
          TYPE_SET_SOURCE,
          TYPE_SET_COLOR,
          TYPE_SET_DRAWINGFLAGS,
          TYPE_SET_BLITTINGFLAGS,
          TYPE_SET_SRC_BLEND,
          TYPE_SET_DST_BLEND,
          TYPE_SET_SRC_COLORKEY,
          TYPE_SET_DESTINATION_PALETTE,
          TYPE_SET_SOURCE_PALETTE,
          TYPE_FILL_RECTS,
          TYPE_DRAW_LINES,
          TYPE_BLIT,
          TYPE_STRETCHBLIT,
          TYPE_TEXTURE_TRIANGLES
     } Type;

     typedef Util::PacketBuffer<> Commands;

     Commands                 commands;
     DFBRegion                clip;
     unsigned int             weight;
     unsigned int             weight_shift_draw;
     unsigned int             weight_shift_blit;
     unsigned int             tile_number;
     StateModificationFlags   modified;

     inline void addDrawingWeight( unsigned int w ) {
          weight += 10 + (w << weight_shift_draw);
     }

     inline void addBlittingWeight( unsigned int w ) {
          weight += 10 + (w << weight_shift_blit);
     }
};


class GenefxEngine : public DirectFB::Engine {
private:
     friend class GenefxTask;

     TaskThreadsQ        threads;

public:
     GenefxEngine( unsigned int cores = 1 )
          :
          threads( "Genefx", cores < 8 ? cores : 8 )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( cores %d )\n", __FUNCTION__, cores );

          D_ASSERT( cores > 0 );

          caps.software       = true;
          caps.cores          = cores < 8 ? cores : 8;
          caps.clipping       = (DFBAccelerationMask)(DFXL_FILLRECTANGLE |
                                                      DFXL_DRAWRECTANGLE |
                                                      DFXL_DRAWLINE |
                                                      DFXL_BLIT |
                                                      DFXL_STRETCHBLIT |
                                                      DFXL_TEXTRIANGLES);
          caps.render_options = (DFBSurfaceRenderOptions)(DSRO_SMOOTH_DOWNSCALE | DSRO_SMOOTH_UPSCALE);
          caps.max_operations = 300000;
     }


     /*
      * Engine API
      */

     virtual DFBResult bind          ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %p )\n", __FUNCTION__, this );

          threads.fifo.waitMost( caps.cores * 3 );

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->tasks[i] = new GenefxTask( this, setup->clips[i], i );
          }

          setup->tiles_render = 1;

          return DFB_OK;
     }

     virtual DFBResult check         ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %p )\n", __FUNCTION__, this );

//          for (unsigned int i=0; i<setup->tiles; i++) {
               GenefxTask *mytask = (GenefxTask *) setup->tasks[0];

               if (mytask->weight >= DFB_GENEFX_TASK_WEIGHT_MAX ||
                   mytask->commands.GetLength() >= DFB_GENEFX_COMMAND_BUFFER_MAX_SIZE)
                    return DFB_LIMITEXCEEDED;
//          }

          return DFB_OK;
     }

     virtual DFBResult CheckState    ( CardState              *state,
                                       DFBAccelerationMask     accel )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %p )\n", __FUNCTION__, this );

          switch (accel) {
               case DFXL_FILLRECTANGLE:
               case DFXL_DRAWLINE:
               case DFXL_BLIT:
               case DFXL_STRETCHBLIT:
               case DFXL_TEXTRIANGLES:
                    break;

               default:
                    return DFB_UNSUPPORTED;
          }

          if (!gAcquireCheck( state, accel ))
               return DFB_UNSUPPORTED;

          return DFB_OK;
     }

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel )
     {
          GenefxTask             *mytask   = (GenefxTask *)task;
          StateModificationFlags  required = (StateModificationFlags)(SMF_TO | SMF_DESTINATION | SMF_CLIP | SMF_RENDER_OPTIONS);
          StateModificationFlags  emitting;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %p )\n", __FUNCTION__, this );


          if (state->render_options & DSRO_MATRIX)
               required = (StateModificationFlags)(required | SMF_MATRIX);

          if (DFB_DRAWING_FUNCTION( accel )) {
               required = (StateModificationFlags)(required | SMF_DRAWING_FLAGS | SMF_COLOR);

               if (state->drawingflags & DSDRAW_BLEND)
                    required = (StateModificationFlags)(required | SMF_SRC_BLEND | SMF_DST_BLEND);

               if (state->drawingflags & DSDRAW_DST_COLORKEY)
                    required = (StateModificationFlags)(required | SMF_DST_COLORKEY);
          }
          else {
               required = (StateModificationFlags)(required | SMF_BLITTING_FLAGS | SMF_FROM | SMF_SOURCE);

               if (accel == DFXL_BLIT2)
                    required = (StateModificationFlags)(required | SMF_FROM | SMF_SOURCE2);

               if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                           DSBLIT_COLORIZE |
                                           DSBLIT_SRC_PREMULTCOLOR))
                    required = (StateModificationFlags)(required | SMF_COLOR);

               if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL |
                                           DSBLIT_BLEND_COLORALPHA))
                    required = (StateModificationFlags)(required | SMF_SRC_BLEND | SMF_DST_BLEND);

               if (state->blittingflags & DSBLIT_SRC_COLORKEY)
                    required = (StateModificationFlags)(required | SMF_SRC_COLORKEY);

               if (state->blittingflags & DSBLIT_DST_COLORKEY)
                    required = (StateModificationFlags)(required | SMF_DST_COLORKEY);

               if (state->blittingflags & (DSBLIT_SRC_MASK_ALPHA | DSBLIT_SRC_MASK_COLOR))
                    required = (StateModificationFlags)(required | SMF_FROM | SMF_SOURCE_MASK | SMF_SOURCE_MASK_VALS);

               if (state->blittingflags & DSBLIT_INDEX_TRANSLATION)
                    required = (StateModificationFlags)(required | SMF_INDEX_TRANSLATION);

               if (state->blittingflags & DSBLIT_COLORKEY_PROTECT)
                    required = (StateModificationFlags)(required | SMF_COLORKEY);

               if (state->blittingflags & DSBLIT_SRC_CONVOLUTION)
                    required = (StateModificationFlags)(required | SMF_SRC_CONVOLUTION);
          }


          D_FLAGS_SET( mytask->modified, modified );

          emitting = (StateModificationFlags)(required & mytask->modified);

          D_FLAGS_CLEAR( mytask->modified, emitting );


          u32 max = 8 + 5 + 8 + 2 + 2 + 2 + 2 + 2 + 2;

          if ((emitting & SMF_DESTINATION) && DFB_PIXELFORMAT_IS_INDEXED( state->destination->config.format ))
               max += 2 + 2 * state->destination->palette->num_entries;

          if ((emitting & SMF_SOURCE) && DFB_BLITTING_FUNCTION(accel) && DFB_PIXELFORMAT_IS_INDEXED( state->source->config.format ))
               max += 2 + 2 * state->source->palette->num_entries;


          // TODO: validate lazily as in CoreGraphicsStateClient

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * max );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          if (emitting & SMF_DESTINATION) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> destination %p (%d)\n", state->dst.addr, state->dst.pitch );

               *buf++ = GenefxTask::TYPE_SET_DESTINATION;
               *buf++ = (long long)(long)state->dst.addr >> 32;
               *buf++ = (u32)(long)state->dst.addr;
               *buf++ = state->dst.pitch;
               *buf++ = state->destination->config.size.w; // FIXME: maybe should use allocation->config
               *buf++ = state->destination->config.size.h;
               *buf++ = state->destination->config.format;
               *buf++ = state->destination->config.caps;

               if (DFB_PIXELFORMAT_IS_INDEXED( state->destination->config.format )) {
                    *buf++ = GenefxTask::TYPE_SET_DESTINATION_PALETTE;

                    *buf++ = state->destination->palette->num_entries;

                    for (unsigned int i=0; i<state->destination->palette->num_entries; i++) {
                         *buf++ = *(u32*)&state->destination->palette->entries[i];
                         *buf++ = *(u32*)&state->destination->palette->entries_yuv[i];
                    }
               }
          }

          if (emitting & SMF_CLIP) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> clip %d,%d-%dx%d\n", DFB_RECTANGLE_VALS_FROM_REGION(&state->clip) );

               *buf++ = GenefxTask::TYPE_SET_CLIP;
               *buf++ = state->clip.x1;
               *buf++ = state->clip.y1;
               *buf++ = state->clip.x2;
               *buf++ = state->clip.y2;

               mytask->clip = state->clip;
          }

          if (emitting & SMF_SOURCE && DFB_BLITTING_FUNCTION(accel)) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> source %p (%d)\n", state->src.addr, state->src.pitch );

               *buf++ = GenefxTask::TYPE_SET_SOURCE;
               *buf++ = (long long)(long)state->src.addr >> 32;
               *buf++ = (u32)(long)state->src.addr;
               *buf++ = state->src.pitch;
               *buf++ = state->source->config.size.w;
               *buf++ = state->source->config.size.h;
               *buf++ = state->source->config.format;
               *buf++ = state->source->config.caps;

               if (DFB_PIXELFORMAT_IS_INDEXED( state->source->config.format )) {
                    *buf++ = GenefxTask::TYPE_SET_SOURCE_PALETTE;

                    *buf++ = state->source->palette->num_entries;

                    for (unsigned int i=0; i<state->source->palette->num_entries; i++) {
                         *buf++ = *(u32*)&state->source->palette->entries[i];
                         *buf++ = *(u32*)&state->source->palette->entries_yuv[i];
                    }
               }

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_SOURCE);

               if (state->source->config.format != state->destination->config.format && state->source->config.format != DSPF_A8)
                    mytask->weight_shift_blit |= 4;
               else
                    mytask->weight_shift_blit &= ~4;
          }

          if (emitting & SMF_COLOR) {
               *buf++ = GenefxTask::TYPE_SET_COLOR;
               *buf++ = PIXEL_ARGB( state->color.a, state->color.r, state->color.g, state->color.b );
          }

          if (emitting & SMF_DRAWING_FLAGS) {
               *buf++ = GenefxTask::TYPE_SET_DRAWINGFLAGS;
               *buf++ = state->drawingflags;

               if (state->drawingflags) {
                    if (state->drawingflags & (DSDRAW_BLEND | DSDRAW_SRC_PREMULTIPLY | DSDRAW_DST_PREMULTIPLY | DSDRAW_DEMULTIPLY))
                         mytask->weight_shift_draw = 6;
                    else
                         mytask->weight_shift_draw = 3;
               }
               else
                    mytask->weight_shift_draw = 1;
          }

          if (emitting & SMF_BLITTING_FLAGS) {
               *buf++ = GenefxTask::TYPE_SET_BLITTINGFLAGS;
               *buf++ = state->blittingflags;

               if (state->blittingflags) {
                    if (state->blittingflags & (DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_BLEND_COLORALPHA | DSBLIT_COLORIZE | DSBLIT_SRC_PREMULTIPLY |
                                                DSBLIT_DST_PREMULTIPLY | DSBLIT_DEMULTIPLY | DSBLIT_DEINTERLACE | DSBLIT_SRC_PREMULTCOLOR |
                                                DSBLIT_SRC_COLORKEY_EXTENDED | DSBLIT_DST_COLORKEY_EXTENDED | DSBLIT_SRC_MASK_ALPHA |
                                                DSBLIT_SRC_MASK_COLOR | DSBLIT_SRC_COLORMATRIX | DSBLIT_SRC_CONVOLUTION))
                         mytask->weight_shift_blit = 8 | (mytask->weight_shift_blit & 4);
                    else
                         mytask->weight_shift_blit = 2 | (mytask->weight_shift_blit & 4);
               }
               else
                    mytask->weight_shift_blit = 1 | (mytask->weight_shift_blit & 4);
          }

          if (emitting & SMF_SRC_BLEND) {
               *buf++ = GenefxTask::TYPE_SET_SRC_BLEND;
               *buf++ = state->src_blend;
          }

          if (emitting & SMF_DST_BLEND) {
               *buf++ = GenefxTask::TYPE_SET_DST_BLEND;
               *buf++ = state->dst_blend;
          }

          if (emitting & SMF_SRC_COLORKEY) {
               *buf++ = GenefxTask::TYPE_SET_SRC_COLORKEY;
               *buf++ = state->src_colorkey;
          }

          state->mod_hw = SMF_NONE;
          state->set    = (DFBAccelerationMask)(state->set | accel);

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32        *count_ptr;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num_rects,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (2 + num_rects * 4) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_FILL_RECTS;

          count_ptr = buf++;

          for (unsigned int i=0; i<num_rects; i++) {
               DFBRectangle rect = rects[i];

               if (dfb_clip_rectangle( &mytask->clip, &rect )) {
                    *buf++ = rect.x;
                    *buf++ = rect.y;
                    *buf++ = rect.w;
                    *buf++ = rect.h;

                    count++;

                    mytask->addDrawingWeight( rect.w * rect.h );
               }
          }

          *count_ptr = count;

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult DrawRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32        *count_ptr;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num_rects,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (2 + num_rects * 4) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_FILL_RECTS;

          count_ptr = buf++;

          for (unsigned int i=0; i<num_rects; i++) {
               DFBRectangle rect = rects[i];
               DFBRectangle rects[4];
               int          n = 0, num = 0;

               dfb_build_clipped_rectangle_outlines( &rect, &mytask->clip, rects, &num );

               for (; n<num; n++) {
                    *buf++ = rects[n].x;
                    *buf++ = rects[n].y;
                    *buf++ = rects[n].w;
                    *buf++ = rects[n].h;

                    count++;

                    mytask->addDrawingWeight( rects[n].w * 2 + rects[n].h * 2 );
               }
          }

          *count_ptr = count;

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult DrawLines( DirectFB::SurfaceTask *task,
                                  const DFBRegion       *lines,
                                  unsigned int           num_lines )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32        *count_ptr;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num_lines,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (2 + num_lines * 4) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_DRAW_LINES;

          count_ptr = buf++;

          for (unsigned int i=0; i<num_lines; i++) {
               DFBRegion line = lines[i];

               if (dfb_clip_line( &mytask->clip, &line )) {
                    *buf++ = line.x1;
                    *buf++ = line.y1;
                    *buf++ = line.x2;
                    *buf++ = line.y2;

                    count++;

                    mytask->addDrawingWeight( (line.x2 - line.x1) + (line.y2 - line.y1) );
               }
          }

          *count_ptr = count;

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             u32                     num )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32        *count_ptr;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (2 + num * 6) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_BLIT;

          count_ptr = buf++;

          for (unsigned int i=0; i<num; i++) {
               D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d\n",
                           rects[i].x, rects[i].y, rects[i].w, rects[i].h, points[i].x, points[i].y );

               if (dfb_clip_blit_precheck( &mytask->clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                    DFBRectangle rect = rects[i];
                    int          dx   = points[i].x;
                    int          dy   = points[i].y;

                    dfb_clip_blit( &mytask->clip, &rect, &dx, &dy );  // FIXME: support rotation!
                    //dfb_clip_blit_flipped_rotated( &mytask->clip, &rect, &drect, blittingflags );

                    *buf++ = rect.x;
                    *buf++ = rect.y;
                    *buf++ = rect.w;
                    *buf++ = rect.h;
                    *buf++ = dx;
                    *buf++ = dy;

                    count++;

                    mytask->addBlittingWeight( rect.w * rect.h );
               }
               else {
                    *buf++ = rects[i].x;
                    *buf++ = rects[i].y;
                    *buf++ = rects[i].w;
                    *buf++ = rects[i].h;
                    *buf++ = points[i].x;
                    *buf++ = points[i].y;

                    count++;

                    mytask->addBlittingWeight( rects[i].w * rects[i].h );
               }
          }

          *count_ptr = count;

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult StretchBlit( DirectFB::SurfaceTask  *task,
                                    const DFBRectangle     *srects,
                                    const DFBRectangle     *drects,
                                    u32                     num )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32        *count_ptr;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (2 + num * 8) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_STRETCHBLIT;

          count_ptr = buf++;

          for (unsigned int i=0; i<num; i++) {
               if (dfb_clip_blit_precheck( &mytask->clip, drects[i].w, drects[i].h, drects[i].x, drects[i].y )) {
                    *buf++ = srects[i].x;
                    *buf++ = srects[i].y;
                    *buf++ = srects[i].w;
                    *buf++ = srects[i].h;

                    *buf++ = drects[i].x;
                    *buf++ = drects[i].y;
                    *buf++ = drects[i].w;
                    *buf++ = drects[i].h;

                    count++;

                    mytask->addBlittingWeight( drects[i].w * drects[i].h * 2 );
               }
          }

          *count_ptr = count;

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }


     virtual DFBResult TextureTriangles( SurfaceTask            *task,
                                         const DFBVertex1616    *vertices,
                                         unsigned int            num,
                                         DFBTriangleFormation    formation )
     {
          GenefxTask *mytask = (GenefxTask *)task;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )  <- clip %d,%d-%dx%d\n", __FUNCTION__, num,
                      DFB_RECTANGLE_VALS_FROM_REGION(&mytask->clip) );

          u32 *buf = (u32*) mytask->commands.GetBuffer( 4 * (3 + num * 4) );

          if (!buf)
               return DFB_NOSYSTEMMEMORY;


          *buf++ = GenefxTask::TYPE_TEXTURE_TRIANGLES;
          *buf++ = num;
          *buf++ = formation;

          for (unsigned int i=0; i<num; i++) {
               *buf++ = vertices[i].x >> 16;
               *buf++ = vertices[i].y >> 16;
               *buf++ = vertices[i].s;
               *buf++ = vertices[i].t;
          }

          mytask->addBlittingWeight( num * 10000 );    // FIXME: calculate weight better, maybe each diff to previous point

          mytask->commands.PutBuffer( buf );

          return DFB_OK;
     }

};


DFBResult
GenefxTask::Setup()
{
     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_FLUSHED, return DFB_BUG );

     DFB_TASK_LOG( "Genefx::Setup()" );

     const std::vector<SurfaceAllocationAccess> &accesses = master ? ((SurfaceTask*) master)->accesses : this->accesses;

     D_ASSERT( accesses.size() > 0 );
     D_ASSERT( accesses[0].flags & CSAF_WRITE );
     D_MAGIC_ASSERT( accesses[0].allocation, CoreSurfaceAllocation );

     D_ASSERT( qid == 0 );
     qid = ((u64) accesses[0].allocation->object.id << 32) | tile_number;

     return SurfaceTask::Setup();
}

DFBResult
GenefxTask::Push()
{
     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_RUNNING, return DFB_BUG );

     DFB_TASK_LOG( "Genefx::Push()" );

     if (master) {
          D_MAGIC_ASSERT( master, Task );
          D_ASSERT( ((GenefxTask*) master)->qid != 0 );

          D_ASSERT( qid == 0 );
          qid = ((u64) ((SurfaceTask*) master)->accesses[0].allocation->object.id << 32) | tile_number;
     }

     engine->threads.Push( this );

     return DFB_OK;
}

void
GenefxTask::Finalise()
{
     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s( %p )\n", __FUNCTION__, this );

     D_MAGIC_ASSERT( this, Task );

     DFB_TASK_CHECK_STATE( this, TASK_DONE, return );

     DFB_TASK_LOG( "Genefx::Finalise()" );

     engine->threads.Finalise( this );

     SurfaceTask::Finalise();
}

DFBResult
GenefxTask::Run()
{
     u32                  ptr1;
     u32                  ptr2;
     u32                  color;
     u32                  num;
     CoreSurface          dest;
     CorePalette          dest_palette;
     DFBColor             dest_entries[256];
     DFBColorYUV          dest_entries_yuv[256];
     CoreSurface          source;
     CorePalette          source_palette;
     DFBColor             source_entries[256];
     DFBColorYUV          source_entries_yuv[256];
     DFBTriangleFormation formation;
     CardState            state;
     bool                 single_tile;
     bool                 disable_rendering = false;

     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s()\n", __FUNCTION__ );

     dfb_state_init( &state, core_dfb );

     state.destination = &dest;
     state.source      = &source;

     dest.num_buffers  = 1;

     single_tile = !master && !slaves;

     D_ASSUME( this->commands.GetLength() > 0 || master != NULL );

     const Commands &commands = this->commands.GetLength() ? this->commands : ((GenefxTask*) master)->commands;

     /* Call SurfaceTask::CacheInvalidate() for cache invalidation, flush takes place at the end */
     CacheInvalidate();

     for (Commands::buffer_vector::const_iterator it = commands.buffers.begin(); it != commands.buffers.end(); ++it) {
          const Util::HeapBuffer *packet_buffer = *it;
          const u32              *buffer        = (const u32*) packet_buffer->ptr;
          size_t                  size          = packet_buffer->length / 4;

          D_DEBUG_AT( DirectFB_GenefxTask, " =-> buffer length %zu\n", size );

          for (unsigned int i=0; i<size; i++) {
               D_DEBUG_AT( DirectFB_GenefxTask, "  -> [%d]\n", i );

               switch (buffer[i]) {
                    case GenefxTask::TYPE_SET_DESTINATION:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_DESTINATION\n" );

                         ptr1 = buffer[++i];
                         ptr2 = buffer[++i];

                         state.dst.addr = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, state.dst.addr );

                         state.dst.pitch = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> pitch %d\n", state.dst.pitch );

                         dest.config.size.w = buffer[++i];
                         dest.config.size.h = buffer[++i];
                         dest.config.format = (DFBSurfacePixelFormat) buffer[++i];
                         dest.config.caps   = (DFBSurfaceCapabilities) buffer[++i];

                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> size %dx%d\n", dest.config.size.w, dest.config.size.h );
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> format %s\n", dfb_pixelformat_name( dest.config.format ) );
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> caps 0x%08x\n", dest.config.caps );
                         break;

                    case GenefxTask::TYPE_SET_CLIP:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_CLIP\n" );

                         state.clip.x1 = buffer[++i];
                         state.clip.y1 = buffer[++i];
                         state.clip.x2 = buffer[++i];
                         state.clip.y2 = buffer[++i];

                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> " DFB_RECT_FORMAT "\n", DFB_RECTANGLE_VALS_FROM_REGION(&state.clip) );

                         if (!single_tile) {
                              if (dfb_region_region_intersect( &state.clip, &tile_clip )) {
                                   disable_rendering = false;

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> " DFB_RECT_FORMAT " (tile " DFB_RECT_FORMAT ")\n",
                                               DFB_RECTANGLE_VALS_FROM_REGION(&state.clip), DFB_RECTANGLE_VALS_FROM_REGION(&tile_clip) );
                              }
                              else {
                                   disable_rendering = true;

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> NO OVERLAP WITH TILE (" DFB_RECT_FORMAT ")\n",
                                               DFB_RECTANGLE_VALS_FROM_REGION(&tile_clip) );
                              }
                         }
                         break;

                    case GenefxTask::TYPE_SET_SOURCE:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_SOURCE\n" );

                         ptr1 = buffer[++i];
                         ptr2 = buffer[++i];

                         state.src.addr = (void*)(long)(((long long)ptr1 << 32) | ptr2);
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x 0x%08x = %p\n", ptr1, ptr2, state.src.addr );

                         state.src.pitch = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> pitch %d\n", state.src.pitch );

                         source.config.size.w = buffer[++i];
                         source.config.size.h = buffer[++i];
                         source.config.format = (DFBSurfacePixelFormat) buffer[++i];
                         source.config.caps   = (DFBSurfaceCapabilities) buffer[++i];

                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> size %dx%d\n", source.config.size.w, source.config.size.h );
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> format %s\n", dfb_pixelformat_name( source.config.format ) );
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> caps 0x%08x\n", source.config.caps );
                         break;

                    case GenefxTask::TYPE_SET_COLOR:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_COLOR\n" );

                         color = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", color );

                         state.color.a = color >> 24;
                         state.color.r = color >> 16;
                         state.color.g = color >>  8;
                         state.color.b = color;
                         break;

                    case GenefxTask::TYPE_SET_DRAWINGFLAGS:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_DRAWINGFLAGS\n" );

                         state.drawingflags = (DFBSurfaceDrawingFlags) buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", state.drawingflags );
                         break;

                    case GenefxTask::TYPE_SET_BLITTINGFLAGS:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_BLITTINGFLAGS\n" );

                         state.blittingflags = (DFBSurfaceBlittingFlags) buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", state.blittingflags );
                         break;

                    case GenefxTask::TYPE_SET_SRC_BLEND:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_SRC_BLEND\n" );

                         state.src_blend = (DFBSurfaceBlendFunction) buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", state.src_blend );
                         break;

                    case GenefxTask::TYPE_SET_DST_BLEND:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_DST_BLEND\n" );

                         state.dst_blend = (DFBSurfaceBlendFunction) buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", state.dst_blend );
                         break;

                    case GenefxTask::TYPE_SET_SRC_COLORKEY:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_SRC_COLORKEY\n" );

                         state.src_colorkey = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> 0x%08x\n", state.src_colorkey );
                         break;

                    case GenefxTask::TYPE_SET_DESTINATION_PALETTE:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_DESTINATION_PALETTE\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         D_ASSERT( num <= 256 );

                         for (u32 n=0; n<num; n++) {
                              dest_entries[n]     = *(DFBColor*)&buffer[++i];
                              dest_entries_yuv[n] = *(DFBColorYUV*)&buffer[++i];
                         }

                         dest_palette.num_entries = num;
                         dest_palette.entries     = dest_entries;
                         dest_palette.entries_yuv = dest_entries_yuv;

                         dest.palette = &dest_palette;
                         break;

                    case GenefxTask::TYPE_SET_SOURCE_PALETTE:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> SET_SOURCE_PALETTE\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         D_ASSERT( num <= 256 );

                         for (u32 n=0; n<num; n++) {
                              source_entries[n]     = *(DFBColor*)&buffer[++i];
                              source_entries_yuv[n] = *(DFBColorYUV*)&buffer[++i];
                         }

                         source_palette.num_entries = num;
                         source_palette.entries     = source_entries;
                         source_palette.entries_yuv = source_entries_yuv;

                         source.palette = &source_palette;
                         break;

                    case GenefxTask::TYPE_FILL_RECTS:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> FILL_RECTS\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         // TODO: run gAcquireSetup in Engine, requires lots of Genefx changes :(
                         if (!disable_rendering && gAcquireSetup( &state, DFXL_FILLRECTANGLE )) {
                              for (u32 n=0; n<num; n++) {
                                   int x = buffer[++i];
                                   int y = buffer[++i];
                                   int w = buffer[++i];
                                   int h = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d\n", x, y, w, h );

                                   DFBRectangle rect = {
                                        x, y, w, h
                                   };

                                   if (single_tile || dfb_clip_rectangle( &state.clip, &rect ))
                                        gFillRectangle( &state, &rect );
                              }
                         }
                         else
                              i += num * 4;
                         break;

                    case GenefxTask::TYPE_DRAW_LINES:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> DRAW_LINES\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         // TODO: run gAcquireSetup in Engine, requires lots of Genefx changes :(
                         if (!disable_rendering && gAcquireSetup( &state, DFXL_DRAWLINE )) {
                              for (u32 n=0; n<num; n++) {
                                   int x1 = buffer[++i];
                                   int y1 = buffer[++i];
                                   int x2 = buffer[++i];
                                   int y2 = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d\n", x1, y1, x2, y2 );

                                   DFBRegion line = {
                                        x1, y1, x2, y2
                                   };

                                   if (single_tile || dfb_clip_line( &state.clip, &line ))
                                        gDrawLine( &state, &line );
                              }
                         }
                         else
                              i += num * 4;
                         break;

                    case GenefxTask::TYPE_BLIT:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> BLIT\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         // TODO: run gAcquireSetup in Engine, requires lots of Genefx changes :(
                         if (!disable_rendering && gAcquireSetup( &state, DFXL_BLIT )) {
                              for (u32 n=0; n<num; n++) {
                                   int x  = buffer[++i];
                                   int y  = buffer[++i];
                                   int w  = buffer[++i];
                                   int h  = buffer[++i];
                                   int dx = buffer[++i];
                                   int dy = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d\n", x, y, w, h, dx, dy );

                                   DFBRectangle rect = {
                                        x, y, w, h
                                   };

                                   if (single_tile)
                                        gBlit( &state, &rect, dx, dy );
                                   else if (dfb_clip_blit_precheck( &state.clip, rect.w, rect.h, dx, dy )) {
                                        dfb_clip_blit( &state.clip, &rect, &dx, &dy );  // FIXME: support rotation!
                                        //dfb_clip_blit_flipped_rotated( &mytask->clip, &rect, &drect, blittingflags );

                                        gBlit( &state, &rect, dx, dy );
                                   }
                              }
                         }
                         else
                              i += num * 6;
                         break;

                    case GenefxTask::TYPE_STRETCHBLIT:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> STRETCHBLIT\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num %d\n", num );

                         // TODO: run gAcquireSetup in Engine, requires lots of Genefx changes :(
                         if (!disable_rendering && gAcquireSetup( &state, DFXL_STRETCHBLIT )) {
                              for (u32 n=0; n<num; n++) {
                                   DFBRectangle srect;
                                   DFBRectangle drect;

                                   srect.x = buffer[++i];
                                   srect.y = buffer[++i];
                                   srect.w = buffer[++i];
                                   srect.h = buffer[++i];

                                   drect.x = buffer[++i];
                                   drect.y = buffer[++i];
                                   drect.w = buffer[++i];
                                   drect.h = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d -> %4d,%4d-%4dx%4d\n",
                                               srect.x, srect.y, srect.w, srect.h,
                                               drect.x, drect.y, drect.w, drect.h );

                                   gStretchBlit( &state, &srect, &drect );
                              }
                         }
                         else
                              i += num * 8;
                         break;

                    case GenefxTask::TYPE_TEXTURE_TRIANGLES:
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> TEXTURE_TRIANGLES\n" );

                         num = buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> num       %d\n", num );

                         formation = (DFBTriangleFormation) buffer[++i];
                         D_DEBUG_AT( DirectFB_GenefxTask, "  -> formation %d\n", formation );

                         // TODO: run gAcquireSetup in Engine, requires lots of Genefx changes :(
                         if (!disable_rendering && gAcquireSetup( &state, DFXL_TEXTRIANGLES )) {
                              Util::TempArray<GenefxVertexAffine> v( num );

                              for (u32 n=0; n<num; n++) {
                                   v.array[n].x = buffer[++i];
                                   v.array[n].y = buffer[++i];
                                   v.array[n].s = buffer[++i];
                                   v.array[n].t = buffer[++i];
                              }

                              Genefx_TextureTrianglesAffine( &state, v.array, num, formation, &state.clip );
                         }
                         else
                              i += num * 4;

                         break;

                    default:
                         D_BUG( "unknown type %d", buffer[i] );
               }
          }
     }

     /* Call SurfaceTask::CacheFlush() for cache flushes */
     CacheFlush();

     /* Return task to manager */
     Done();

     state.destination = NULL;
     state.source      = NULL;

     dfb_state_destroy( &state );

     return DFB_OK;
}



extern "C" {
     void
     register_genefx()
     {
          Renderer::RegisterEngine( new GenefxEngine( dfb_config->software_cores ? : 1 ) );
     }
}

}

