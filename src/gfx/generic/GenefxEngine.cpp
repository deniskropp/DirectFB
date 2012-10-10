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

#include <core/Renderer.h>

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

D_DEBUG_DOMAIN( DirectFB_Renderer, "DirectFB/Renderer", "DirectFB Renderer" );

/*********************************************************************************************************************/

namespace DirectFB {





D_DEBUG_DOMAIN( DirectFB_GenefxEngine, "DirectFB/Genefx/Engine", "DirectFB Genefx Engine" );
D_DEBUG_DOMAIN( DirectFB_GenefxTask,   "DirectFB/Genefx/Task",   "DirectFB Genefx Task" );




// TODO: use fixed size array instead of std::vector





class GenefxEngine;

class GenefxTask : public DirectFB::SurfaceTask
{
public:
     GenefxTask( GenefxEngine *engine )
          :
          SurfaceTask( CSAID_CPU ),
          engine( engine )
     {
     }

     virtual ~GenefxTask()
     {
     }

protected:
     virtual DFBResult Push();
     virtual DFBResult Run();

private:
     friend class GenefxEngine;

     GenefxEngine *engine;

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
          TYPE_STRETCHBLIT
     } Type;

     std::vector<u32> commands;
     DFBRegion        clip;
};


class GenefxEngine : public DirectFB::Engine {
public:
     GenefxEngine( unsigned int cores = 1 )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( cores %d )\n", __FUNCTION__, cores );

          D_ASSERT( cores > 0 );

          caps.software       = true;
          caps.cores          = cores < 8 ? cores : 8;
          caps.clipping       = (DFBAccelerationMask)(DFXL_FILLRECTANGLE | DFXL_DRAWLINE | DFXL_BLIT);
          caps.render_options = (DFBSurfaceRenderOptions)(DSRO_SMOOTH_DOWNSCALE | DSRO_SMOOTH_UPSCALE);
          caps.max_operations = 1000;

          for (unsigned int i=0; i<cores; i++) {
               char name[] = "GenefxX";

               name[6] = '0' + i;

               threads[i] = direct_thread_create( DTT_DEFAULT, myEngineLoop, this, name );
          }
     }


     virtual DFBResult bind          ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s()\n", __FUNCTION__ );

          for (unsigned int i=0; i<setup->tiles; i++) {
               setup->tasks[i] = new GenefxTask( this );
          }

          return DFB_OK;
     }

     virtual DFBResult check         ( Renderer::Setup        *setup )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s()\n", __FUNCTION__ );

          return DFB_OK;
     }

     virtual DFBResult CheckState    ( CardState              *state,
                                       DFBAccelerationMask     accel )
     {
          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s()\n", __FUNCTION__ );

          if (!gAcquireCheck( state, accel ))
               return DFB_UNSUPPORTED;

          return DFB_OK;
     }

     virtual DFBResult SetState      ( DirectFB::SurfaceTask  *task,
                                       CardState              *state,
                                       StateModificationFlags  modified,
                                       DFBAccelerationMask     accel )
     {
          GenefxTask *mytask = (GenefxTask *)task;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s()\n", __FUNCTION__ );

          // TODO: maybe validate lazily as in CoreGraphicsStateClient

          if (modified & SMF_DESTINATION) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> destination %p (%d)\n", state->dst.addr, state->dst.pitch );

               mytask->commands.push_back( GenefxTask::TYPE_SET_DESTINATION );
               mytask->commands.push_back( (long long)(long)state->dst.addr >> 32 );
               mytask->commands.push_back( (u32)(long)state->dst.addr );
               mytask->commands.push_back( state->dst.pitch );
               mytask->commands.push_back( state->destination->config.size.w ); // FIXME: maybe should use allocation->config
               mytask->commands.push_back( state->destination->config.size.h );
               mytask->commands.push_back( state->destination->config.format );
               mytask->commands.push_back( state->destination->config.caps );

               if (DFB_PIXELFORMAT_IS_INDEXED( state->destination->config.format )) {
                    mytask->commands.push_back( GenefxTask::TYPE_SET_DESTINATION_PALETTE );

                    mytask->commands.push_back( state->destination->palette->num_entries );

                    for (unsigned int i=0; i<state->destination->palette->num_entries; i++) {
                         mytask->commands.push_back( *(u32*)&state->destination->palette->entries[i] );
                         mytask->commands.push_back( *(u32*)&state->destination->palette->entries_yuv[i] );
                    }
               }
          }

          if (modified & SMF_CLIP) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> clip %d,%d-%dx%d\n", DFB_RECTANGLE_VALS_FROM_REGION(&state->clip) );

               mytask->commands.push_back( GenefxTask::TYPE_SET_CLIP );
               mytask->commands.push_back( state->clip.x1 );
               mytask->commands.push_back( state->clip.y1 );
               mytask->commands.push_back( state->clip.x2 );
               mytask->commands.push_back( state->clip.y2 );

               mytask->clip = state->clip;
          }

          if (modified & SMF_SOURCE && DFB_BLITTING_FUNCTION(accel)) {
               D_DEBUG_AT( DirectFB_GenefxEngine, "  -> source %p (%d)\n", state->src.addr, state->src.pitch );

               mytask->commands.push_back( GenefxTask::TYPE_SET_SOURCE );
               mytask->commands.push_back( (long long)(long)state->src.addr >> 32 );
               mytask->commands.push_back( (u32)(long)state->src.addr );
               mytask->commands.push_back( state->src.pitch );
               mytask->commands.push_back( state->source->config.size.w );
               mytask->commands.push_back( state->source->config.size.h );
               mytask->commands.push_back( state->source->config.format );
               mytask->commands.push_back( state->source->config.caps );

               if (DFB_PIXELFORMAT_IS_INDEXED( state->source->config.format )) {
                    mytask->commands.push_back( GenefxTask::TYPE_SET_SOURCE_PALETTE );

                    mytask->commands.push_back( state->source->palette->num_entries );

                    for (unsigned int i=0; i<state->source->palette->num_entries; i++) {
                         mytask->commands.push_back( *(u32*)&state->source->palette->entries[i] );
                         mytask->commands.push_back( *(u32*)&state->source->palette->entries_yuv[i] );
                    }
               }

               state->mod_hw = (StateModificationFlags)(state->mod_hw & ~SMF_SOURCE);
          }

          if (modified & SMF_COLOR) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_COLOR );
               mytask->commands.push_back( PIXEL_ARGB( state->color.a, state->color.r, state->color.g, state->color.b ) );
          }

          if (modified & SMF_DRAWING_FLAGS) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_DRAWINGFLAGS );
               mytask->commands.push_back( state->drawingflags );
          }

          if (modified & SMF_BLITTING_FLAGS) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_BLITTINGFLAGS );
               mytask->commands.push_back( state->blittingflags );
          }

          if (modified & SMF_SRC_BLEND) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_SRC_BLEND );
               mytask->commands.push_back( state->src_blend );
          }

          if (modified & SMF_DST_BLEND) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_DST_BLEND );
               mytask->commands.push_back( state->dst_blend );
          }

          if (modified & SMF_SRC_COLORKEY) {
               mytask->commands.push_back( GenefxTask::TYPE_SET_SRC_COLORKEY );
               mytask->commands.push_back( state->src_colorkey );
          }

          state->mod_hw = (StateModificationFlags)(state->mod_hw & SMF_SOURCE);
          state->set    = DFXL_ALL;

          return DFB_OK;
     }


     virtual DFBResult FillRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32         index;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )\n", __FUNCTION__, num_rects );

          mytask->commands.push_back( GenefxTask::TYPE_FILL_RECTS );
          mytask->commands.push_back( num_rects );
          index = mytask->commands.size() - 1;

          for (unsigned int i=0; i<num_rects; i++) {
               DFBRectangle rect = rects[i];

               if (dfb_clip_rectangle( &mytask->clip, &rect )) {
                    mytask->commands.push_back( rect.x );
                    mytask->commands.push_back( rect.y );
                    mytask->commands.push_back( rect.w );
                    mytask->commands.push_back( rect.h );

                    count++;
               }
          }

          mytask->commands[index] = count;

          return DFB_OK;
     }


     virtual DFBResult DrawRectangles( DirectFB::SurfaceTask  *task,
                                       const DFBRectangle     *rects,
                                       unsigned int            num_rects )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32         index;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )\n", __FUNCTION__, num_rects );

          mytask->commands.push_back( GenefxTask::TYPE_FILL_RECTS );
          mytask->commands.push_back( num_rects );
          index = mytask->commands.size() - 1;

          for (unsigned int i=0; i<num_rects; i++) {
               DFBRectangle rect = rects[i];
               DFBRectangle rects[4];
               int          i = 0, num = 0;

               dfb_build_clipped_rectangle_outlines( &rect, &mytask->clip, rects, &num );

               for (; i<num; i++) {
                    mytask->commands.push_back( rects[i].x );
                    mytask->commands.push_back( rects[i].y );
                    mytask->commands.push_back( rects[i].w );
                    mytask->commands.push_back( rects[i].h );

                    count++;
               }
          }

          mytask->commands[index] = count;

          return DFB_OK;
     }


     virtual DFBResult DrawLines( DirectFB::SurfaceTask *task,
                                  const DFBRegion       *lines,
                                  unsigned int           num_lines )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32         index;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )\n", __FUNCTION__, num_lines );

          mytask->commands.push_back( GenefxTask::TYPE_DRAW_LINES );
          mytask->commands.push_back( num_lines );
          index = mytask->commands.size() - 1;

          for (unsigned int i=0; i<num_lines; i++) {
               DFBRegion line = lines[i];

               if (dfb_clip_line( &mytask->clip, &line )) {
                    mytask->commands.push_back( line.x1 );
                    mytask->commands.push_back( line.y1 );
                    mytask->commands.push_back( line.x2 );
                    mytask->commands.push_back( line.y2 );

                    count++;
               }
          }

          mytask->commands[index] = count;

          return DFB_OK;
     }


     virtual DFBResult Blit( DirectFB::SurfaceTask  *task,
                             const DFBRectangle     *rects,
                             const DFBPoint         *points,
                             u32                     num )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32         index;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )\n", __FUNCTION__, num );

          mytask->commands.push_back( GenefxTask::TYPE_BLIT );
          mytask->commands.push_back( num );
          index = mytask->commands.size() - 1;

          for (unsigned int i=0; i<num; i++) {
               if (dfb_clip_blit_precheck( &mytask->clip, rects[i].w, rects[i].h, points[i].x, points[i].y )) {
                    DFBRectangle rect = rects[i];
                    int          dx   = points[i].x;
                    int          dy   = points[i].y;

                    dfb_clip_blit( &mytask->clip, &rect, &dx, &dy );

                    mytask->commands.push_back( rect.x );
                    mytask->commands.push_back( rect.y );
                    mytask->commands.push_back( rect.w );
                    mytask->commands.push_back( rect.h );
                    mytask->commands.push_back( dx );
                    mytask->commands.push_back( dy );

                    count++;
               }
          }

          mytask->commands[index] = count;

          return DFB_OK;
     }


     virtual DFBResult StretchBlit( DirectFB::SurfaceTask  *task,
                                    const DFBRectangle     *srects,
                                    const DFBRectangle     *drects,
                                    u32                     num )
     {
          GenefxTask *mytask = (GenefxTask *)task;
          u32         count  = 0;
          u32         index;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s( %d )\n", __FUNCTION__, num );

          mytask->commands.push_back( GenefxTask::TYPE_STRETCHBLIT );
          mytask->commands.push_back( num );
          index = mytask->commands.size() - 1;

          for (unsigned int i=0; i<num; i++) {
               if (dfb_clip_blit_precheck( &mytask->clip, drects[i].w, drects[i].h, drects[i].x, drects[i].y )) {
                    mytask->commands.push_back( srects[i].x );
                    mytask->commands.push_back( srects[i].y );
                    mytask->commands.push_back( srects[i].w );
                    mytask->commands.push_back( srects[i].h );

                    mytask->commands.push_back( drects[i].x );
                    mytask->commands.push_back( drects[i].y );
                    mytask->commands.push_back( drects[i].w );
                    mytask->commands.push_back( drects[i].h );

                    count++;
               }
          }

          mytask->commands[index] = count;

          return DFB_OK;
     }


private:
     friend class GenefxTask;

     DirectFB::FIFO<GenefxTask*>  fifo;
     DirectThread                *threads[8];

     static void *
     myEngineLoop( DirectThread *thread,
                   void         *arg )
     {
          GenefxEngine *engine = (GenefxEngine *)arg;
          GenefxTask   *task;

          D_DEBUG_AT( DirectFB_GenefxEngine, "GenefxEngine::%s()\n", __FUNCTION__ );

          while (true) {
               task = engine->fifo.pull();

               task->Run();
          }

          return NULL;
     }
};


DFBResult
GenefxTask::Push()
{
     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s()\n", __FUNCTION__ );

     engine->fifo.waitMost( engine->caps.cores * 2 );

     engine->fifo.push( this );

     return DFB_OK;
}

DFBResult
GenefxTask::Run()
{
     u32         ptr1;
     u32         ptr2;
     u32         color;
     u32         num;
     size_t      size;
     CoreSurface dest;
     CorePalette dest_palette;
     DFBColor    dest_entries[256];
     DFBColorYUV dest_entries_yuv[256];
     CoreSurface source;
     CorePalette source_palette;
     DFBColor    source_entries[256];
     DFBColorYUV source_entries_yuv[256];
     CardState   state;

     D_DEBUG_AT( DirectFB_GenefxTask, "GenefxTask::%s()\n", __FUNCTION__ );

     dfb_state_init( &state, core_dfb );

     state.destination = &dest;
     state.source      = &source;

     dest.num_buffers  = 1;

     size = commands.size();
     if (size > 0) {
          u32 *buffer = &commands[0];

          for (unsigned int i=0; i<size; i++) {
               D_DEBUG_AT( DirectFB_GenefxTask, "  -> next command at [%d]\n", i );

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
                         if (gAcquireSetup( &state, DFXL_FILLRECTANGLE )) {
                              for (u32 n=0; n<num; n++) {
                                   int x = buffer[++i];
                                   int y = buffer[++i];
                                   int w = buffer[++i];
                                   int h = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d\n", x, y, w, h );

                                   DFBRectangle rect = {
                                        x, y, w, h
                                   };

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
                         if (gAcquireSetup( &state, DFXL_DRAWLINE )) {
                              for (u32 n=0; n<num; n++) {
                                   int x1 = buffer[++i];
                                   int y1 = buffer[++i];
                                   int x2 = buffer[++i];
                                   int y2 = buffer[++i];

                                   D_DEBUG_AT( DirectFB_GenefxTask, "  -> %4d,%4d-%4dx%4d\n", x1, y1, x2, y2 );

                                   DFBRegion line = {
                                        x1, y1, x2, y2
                                   };

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
                         if (gAcquireSetup( &state, DFXL_BLIT )) {
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

                                   gBlit( &state, &rect, dx, dy );
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
                         if (gAcquireSetup( &state, DFXL_STRETCHBLIT )) {
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

                    default:
                         D_BUG( "unknown type %d", buffer[i] );
               }
          }
     }

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

