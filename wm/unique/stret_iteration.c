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

#include <direct/debug.h>
#include <direct/util.h>

#include <misc/util.h>

#include <unique/stret_iteration.h>
#include <unique/internal.h>

D_DEBUG_DOMAIN( UniQuE_StReT, "UniQuE/StReT", "UniQuE's Stack Region Tree" );

static inline bool
accept_region( StretRegion *region, int x0, int y0, const DFBRegion *clip )
{
     if (!D_FLAGS_IS_SET( region->flags, SRF_ACTIVE ))
          return false;

     if (!clip)
          return true;

     return dfb_region_intersects( clip, DFB_REGION_VALS_TRANSLATED( &region->bounds, x0, y0 ) );
}

static inline bool
check_depth( int frame )
{
     if (frame >= STRET_ITERATION_MAX_DEPTH) {
          D_WARN( "refusing to exceed depth limit of %d", STRET_ITERATION_MAX_DEPTH );
          return false;
     }

     return true;
}

void
stret_iteration_init( StretIteration *iteration, StretRegion *region, StretRegion *abort_at )
{
     D_ASSERT( iteration != NULL );

     D_MAGIC_ASSERT( region, StretRegion );
     D_MAGIC_ASSERT_IF( abort_at, StretRegion );

     D_DEBUG_AT( UniQuE_StReT, "stret_iteration_init()\n" );

     iteration->frame = -1;
     iteration->x0    = 0;
     iteration->y0    = 0;
     iteration->abort = abort_at;

     do {
          int last_level = region->levels - 1;
          int last_child = region->children[last_level].count - 1;

          iteration->frame++;

          iteration->x0 += region->bounds.x1;
          iteration->y0 += region->bounds.y1;

          iteration->stack[iteration->frame].region = region;
          iteration->stack[iteration->frame].level  = last_level;
          iteration->stack[iteration->frame].index  = last_child - 1;

          D_DEBUG_AT( UniQuE_StReT, "  -> (%d) %p, last level %d, last index %d\n",
                      iteration->frame, region, last_level, last_child );

          if (last_child >= 0) {
               region = fusion_vector_at( &region->children[last_level], last_child );

               D_MAGIC_ASSERT( region, StretRegion );
          }
     } while (region->children[region->levels - 1].count && check_depth( iteration->frame + 1 ));

     iteration->stack[iteration->frame].index++;

     D_MAGIC_SET( iteration, StretIteration );
}

StretRegion *
stret_iteration_next( StretIteration  *iteration,
                      const DFBRegion *clip )
{
     int          index;
     int          level;
     StretRegion *region;

     D_MAGIC_ASSERT( iteration, StretIteration );

     DFB_REGION_ASSERT_IF( clip );

     if (clip)
          D_DEBUG_AT( UniQuE_StReT, "stret_iteration_next( %d, %d - %dx%d )\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( clip ) );
     else
          D_DEBUG_AT( UniQuE_StReT, "stret_iteration_next()\n" );

     while (iteration->frame >= 0) {
          StretIterationStackFrame *frame = &iteration->stack[iteration->frame];

          region = frame->region;
          level  = frame->level;
          index  = frame->index--;

          D_MAGIC_ASSERT( region, StretRegion );

          D_DEBUG_AT( UniQuE_StReT, "  -> (%d) %p, level [%d/%d], index %d\n",
                      iteration->frame, region, level, region->levels - 1, index );

          if (iteration->abort && region == iteration->abort) {
               D_MAGIC_CLEAR( iteration );
               return NULL;
          }

          if (index < 0) {
               level = --frame->level;

               if (level < 0) {
                    iteration->frame--;

                    iteration->x0 -= region->bounds.x1;
                    iteration->y0 -= region->bounds.y1;

                    if (accept_region( region, iteration->x0, iteration->y0, clip ))
                        return region;
               }
               else {
                    frame->index = region->children[level].count - 1;
               }
          }
          else {
               region = fusion_vector_at( &region->children[level], index );

               D_MAGIC_ASSERT( region, StretRegion );

               if (iteration->abort && region == iteration->abort) {
                    D_MAGIC_CLEAR( iteration );
                    return NULL;
               }

               if (accept_region( region, iteration->x0, iteration->y0, clip )) {
                    level = region->levels - 1;

                    while (!region->children[level].count) {
                         if (level)
                              level--;
                         else
                              return region;
                    }

                    if (check_depth( iteration->frame + 1 )) {
                         frame = &iteration->stack[++iteration->frame];

                         iteration->x0 += region->bounds.x1;
                         iteration->y0 += region->bounds.y1;

                         frame->region = region;
                         frame->level  = level;
                         frame->index  = region->children[level].count - 1;

                         continue;
                    }

                    return region;
               }
          }
     }

     D_ASSUME( iteration->x0 == 0 );
     D_ASSUME( iteration->y0 == 0 );

     D_MAGIC_CLEAR( iteration );

     return NULL;
}

void
stret_iteration_abort( StretIteration *iteration )
{
     D_MAGIC_CLEAR( iteration );
}

