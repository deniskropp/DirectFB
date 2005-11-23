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
#include <direct/messages.h>
#include <direct/util.h>

#include <fusion/shmalloc.h>
#include <fusion/vector.h>

#include <directfb.h>

#include <misc/util.h>

#include <unique/stret.h>
#include <unique/stret_iteration.h>
#include <unique/internal.h>


#define MAX_CLASSES 16

D_DEBUG_DOMAIN( UniQuE_StReT, "UniQuE/StReT", "UniQuE's Stack Region Tree" );

/**************************************************************************************************/

static void
default_update( StretRegion     *region,
                void            *region_data,
                void            *update_data,
                unsigned long    arg,
                int              x,
                int              y,
                const DFBRegion *updates,
                int              num )
{
     D_DEBUG_AT( UniQuE_StReT, "default_update( %p, %p, %p, %lu, %d, %d, %p, %d )\n",
                 region, region_data, update_data, arg, x, y, updates, num );
}

static const StretRegionClass default_class = {
     Update:   default_update
};

/**************************************************************************************************/

static const StretRegionClass *classes[MAX_CLASSES] = { &default_class, NULL };

static pthread_mutex_t         classes_lock  = PTHREAD_MUTEX_INITIALIZER;
static int                     classes_count = 1;

/**************************************************************************************************/

DFBResult
stret_class_register( const StretRegionClass *clazz,
                      StretRegionClassID     *ret_id )
{
     int i;

     D_DEBUG_AT( UniQuE_StReT, "stret_class_register( %p )\n", clazz );

     D_ASSERT( clazz != NULL );
     D_ASSERT( ret_id != NULL );

     pthread_mutex_lock( &classes_lock );

     if (classes_count == MAX_CLASSES) {
          D_WARN( "too many classes" );
          pthread_mutex_unlock( &classes_lock );
          return DFB_LIMITEXCEEDED;
     }

     classes_count++;

     for (i=0; i<MAX_CLASSES; i++) {
          if (!classes[i]) {
               classes[i] = clazz;
               break;
          }
     }

     D_DEBUG_AT( UniQuE_StReT, "    -> New class ID is %d.\n", i );

     D_ASSERT( i < MAX_CLASSES );

     *ret_id = i;

     pthread_mutex_unlock( &classes_lock );

     return DFB_OK;
}

DFBResult
stret_class_unregister( StretRegionClassID id )
{
     D_DEBUG_AT( UniQuE_StReT, "stret_class_unregister( %d )\n", id );

     pthread_mutex_lock( &classes_lock );

     D_ASSERT( id >= 0 );
     D_ASSERT( id < MAX_CLASSES );
     D_ASSERT( classes[id] != NULL );

     classes[id] = NULL;

     classes_count--;

     pthread_mutex_unlock( &classes_lock );

     return DFB_OK;
}



DFBResult
stret_region_create( StretRegionClassID   class_id,
                     void                *data,
                     unsigned long        arg,
                     StretRegionFlags     flags,
                     int                  levels,
                     int                  x,
                     int                  y,
                     int                  width,
                     int                  height,
                     StretRegion         *parent,
                     int                  level,
                     FusionSHMPoolShared *pool,
                     StretRegion        **ret_region )
{
     int          i;
     StretRegion *region;

     D_DEBUG_AT( UniQuE_StReT, "stret_region_create( class %d, flags 0x%08x, %d,%d - %dx%d (%d), "
                 "parent %p [%d/%d] )\n", class_id, flags, x, y, width, height, levels, parent,
                 level, parent ? parent->levels-1 : 0 );

     D_ASSERT( class_id >= 0 );
     D_ASSERT( class_id < MAX_CLASSES );
     D_ASSERT( classes[class_id] != NULL );

     D_ASSERT( ! (flags & ~SRF_ALL) );

     D_ASSERT( levels > 0 );
     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     D_MAGIC_ASSERT_IF( parent, StretRegion );

     if (parent)
          D_ASSERT( level < parent->levels );

     D_ASSERT( ret_region != NULL );

     /* Allocate region data. */
     region = SHCALLOC( pool, 1, sizeof(StretRegion) + sizeof(FusionVector) * levels );
     if (!region) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     /* Initialize region data. */
     region->parent   = parent;
     region->level    = level;
     region->levels   = levels;
     region->children = (FusionVector*)(region + 1);
     region->flags    = flags;
     region->bounds   = (DFBRegion) { x, y, x + width - 1, y + height - 1 };
     region->clazz    = class_id;
     region->data     = data;
     region->arg      = arg;
     region->shmpool  = pool;

     /* Initialize levels. */
     for (i=0; i<levels; i++)
          fusion_vector_init( &region->children[i], 4, pool );


     /* Add the region to its parent. */
     if (parent) {
          FusionVector *children = &parent->children[level];

          region->index = fusion_vector_size( children );

          if (fusion_vector_add( children, region )) {
               D_WARN( "out of (shared) memory" );
               SHFREE( pool, region );
               return DFB_NOSYSTEMMEMORY;
          }
     }


     D_MAGIC_SET( region, StretRegion );

#if DIRECT_BUILD_DEBUG
{
     DFBRegion bounds;

     stret_region_get_abs( region, &bounds );

     D_DEBUG_AT( UniQuE_StReT, "    -> Index %d, absolute bounds: %d,%d - %dx%d\n",
                 region->index, DFB_RECTANGLE_VALS_FROM_REGION( &bounds ) );
}
#endif

     *ret_region = region;

     return DFB_OK;
}

DFBResult
stret_region_destroy( StretRegion *region )
{
     int          i;
     int          index;
     StretRegion *parent;
     StretRegion *child;

     D_MAGIC_ASSERT( region, StretRegion );

     D_DEBUG_AT( UniQuE_StReT,
                 "stret_region_destroy( %d, %d - %dx%d, level %d, index %d )\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ), region->level, region->index );

     parent = region->parent;
     if (parent) {
          FusionVector *children = &parent->children[region->level];

          D_MAGIC_ASSERT( parent, StretRegion );

          index = region->index;

          D_ASSERT( index >= 0 );
          D_ASSERT( index == fusion_vector_index_of( children, region ) );

          fusion_vector_remove( children, index );

          for (; index<fusion_vector_size(children); index++) {
               StretRegion *child = fusion_vector_at( children, index );

               child->index = index;
          }
     }

     for (i=0; i<region->levels; i++) {
          FusionVector *children = &region->children[i];

          D_ASSUME( ! fusion_vector_has_elements( children ) );

          fusion_vector_foreach( child, index, *children ) {
               D_MAGIC_ASSERT( child, StretRegion );
               D_ASSERT( child->parent == region );

               child->parent = NULL;
          }

          fusion_vector_destroy( children );
     }

     D_MAGIC_CLEAR( region );

     SHFREE( region->shmpool, region );

     return DFB_OK;
}


DFBResult
stret_region_enable( StretRegion      *region,
                     StretRegionFlags  flags )
{
     D_MAGIC_ASSERT( region, StretRegion );

     region->flags |= flags;

     return DFB_OK;
}

DFBResult
stret_region_disable( StretRegion      *region,
                      StretRegionFlags  flags )
{
     D_MAGIC_ASSERT( region, StretRegion );

     region->flags &= ~flags;

     return DFB_OK;
}


DFBResult
stret_region_move( StretRegion *region,
                   int          dx,
                   int          dy )
{
     D_MAGIC_ASSERT( region, StretRegion );

     dfb_region_translate( &region->bounds, dx, dy );

     return DFB_OK;
}

DFBResult
stret_region_resize( StretRegion *region,
                     int          width,
                     int          height )
{
     D_MAGIC_ASSERT( region, StretRegion );

     dfb_region_resize( &region->bounds, width, height );

     return DFB_OK;
}

DFBResult
stret_region_restack( StretRegion *region,
                      int          index )
{
     StretRegion *parent;

     D_MAGIC_ASSERT( region, StretRegion );

     D_ASSUME( region->parent != NULL );

     parent = region->parent;
     if (parent) {
          int           old;
          FusionVector *children = &parent->children[region->level];

          D_MAGIC_ASSERT( parent, StretRegion );

          old = region->index;

          D_ASSERT( old >= 0 );
          D_ASSERT( old == fusion_vector_index_of( children, region ) );

          fusion_vector_move( children, old, index );

          for (index = MIN(index,old); index<fusion_vector_size(children); index++) {
               StretRegion *child = fusion_vector_at( children, index );

               child->index = index;
          }
     }

     return DFB_OK;
}

void
stret_region_get_abs( StretRegion *region,
                      DFBRegion   *ret_bounds )
{
     DFBRegion bounds;

     D_MAGIC_ASSERT( region, StretRegion );

     D_ASSERT( ret_bounds != NULL );

     bounds = region->bounds;

     while (region->parent) {
          StretRegion *parent = region->parent;

          D_MAGIC_ASSERT( parent, StretRegion );

          dfb_region_translate( &bounds, parent->bounds.x1, parent->bounds.y1 );

          region = parent;
     }

     *ret_bounds = bounds;
}

void
stret_region_get_size( StretRegion  *region,
                       DFBDimension *ret_size )
{
     D_MAGIC_ASSERT( region, StretRegion );

     D_ASSERT( ret_size != NULL );

     ret_size->w = region->bounds.x2 - region->bounds.x1 + 1;
     ret_size->h = region->bounds.y2 - region->bounds.y1 + 1;
}

DFBResult
stret_region_get_input( StretRegion         *region,
                        int                  index,
                        int                  x,
                        int                  y,
                        UniqueInputChannel **ret_channel )
{
     const StretRegionClass *clazz;

     D_MAGIC_ASSERT( region, StretRegion );

     D_ASSERT( ret_channel != NULL );

     D_ASSERT( region->clazz >= 0 );
     D_ASSERT( region->clazz < MAX_CLASSES );

     clazz = classes[region->clazz];

     D_ASSERT( clazz != NULL );

     if (clazz->GetInput)
          return clazz->GetInput( region, region->data, region->arg, index, x, y, ret_channel );

     return DFB_UNSUPPORTED;
}


typedef struct {
     int          num;
     int          max;
     DFBRegion   *regions;
     StretRegion *region;
} ClipOutContext;

static void
clip_out( StretIteration *iteration,
          ClipOutContext *context,
          int             x1,
          int             y1,
          int             x2,
          int             y2 )
{
     StretRegion *region;
     DFBRegion    cutout;
     DFBRegion    area = { x1, y1, x2, y2 };

     D_DEBUG_AT( UniQuE_StReT, "  clip_out( %4d, %4d - %4dx%4d )\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &area ) );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     while (true) {
          region = stret_iteration_next( iteration, &area );
          if (!region || region == context->region) {
               context->num++;

               if (context->num <= context->max) {
                    DFBRegion *region = &context->regions[ context->num - 1 ];

                    D_DEBUG_AT( UniQuE_StReT, "    (%2d) %4d, %4d - %4dx%4d\n",
                                context->num - 1, x1, y1, x2 - x1 + 1, y2 - y1 + 1 );

                    region->x1 = x1;
                    region->y1 = y1;
                    region->x2 = x2;
                    region->y2 = y2;
               }
               else
                    D_DEBUG_AT( UniQuE_StReT, "  Maximum number of regions exceeded, dropping...\n" );

               if (region)
                    stret_iteration_abort( iteration );

               return;
          }

          D_MAGIC_ASSERT( region, StretRegion );

          if (D_FLAGS_ARE_SET( region->flags, SRF_OUTPUT | SRF_OPAQUE ))
               break;
     }

     cutout = DFB_REGION_INIT_TRANSLATED( &region->bounds, iteration->x0, iteration->y0 );

     dfb_region_clip( &cutout, x1, y1, x2, y2 );

     /* upper */
     if (cutout.y1 != y1) {
          StretIteration fork = *iteration;

          clip_out( &fork, context, x1, y1, x2, cutout.y1 - 1 );
     }

     /* left */
     if (cutout.x1 != x1) {
          StretIteration fork = *iteration;

          clip_out( &fork, context, x1, cutout.y1, cutout.x1 - 1, cutout.y2 );
     }

     /* right */
     if (cutout.x2 != x2) {
          StretIteration fork = *iteration;

          clip_out( &fork, context, cutout.x2 + 1, cutout.y1, x2, cutout.y2 );
     }

     /* lower */
     if (cutout.y2 != y2) {
          StretIteration fork = *iteration;

          clip_out( &fork, context, x1, cutout.y2 + 1, x2, y2 );
     }

     stret_iteration_abort( iteration );
}

DFBResult
stret_region_visible( StretRegion     *region,
                      const DFBRegion *base,
                      bool             children,
                      DFBRegion       *ret_regions,
                      int              max_num,
                      int             *ret_num )
{
     bool            visible = true;
     DFBRegion       area;
     ClipOutContext  context;
     StretIteration  iteration;
     StretRegion    *root;

     int            x0, y0;

     D_MAGIC_ASSERT( region, StretRegion );

     DFB_REGION_ASSERT_IF( base );

     D_ASSERT( ret_regions != NULL );
     D_ASSERT( max_num > 0 );
     D_ASSERT( ret_num != NULL );

     if (base) {
          D_DEBUG_AT( UniQuE_StReT,
                      "stret_region_visible( %d, %d - %dx%d  of  %d, %d - %dx%d )\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( base ),
                      DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ) );

          area = *base;
     }
     else {
          D_DEBUG_AT( UniQuE_StReT,
                      "stret_region_visible( %d, %d - %dx%d )\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ) );

          area.x1 = 0;
          area.y1 = 0;
          area.x2 = region->bounds.x2 - region->bounds.x1;
          area.y2 = region->bounds.y2 - region->bounds.y1;
     }


     if (! D_FLAGS_IS_SET( region->flags, SRF_ACTIVE )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is not active and therefore invisible!\n" );

          *ret_num = 0;

          return DFB_OK;
     }


     context.num     = 0;
     context.max     = max_num;
     context.regions = ret_regions;
     context.region  = region;

     x0 = region->bounds.x1;
     y0 = region->bounds.y1;

     root = region;

     if (region->parent) {
          int          rx2;
          int          ry2;
          StretRegion *parent = region->parent;

          do {
               D_MAGIC_ASSERT( parent, StretRegion );

               if (! D_FLAGS_IS_SET( parent->flags, SRF_ACTIVE )) {
                    D_DEBUG_AT( UniQuE_StReT, "  -> At least one parent is not active!\n" );

                    *ret_num = 0;

                    return DFB_OK;
               }

               x0 += parent->bounds.x1;
               y0 += parent->bounds.y1;

               rx2 = parent->bounds.x2;
               ry2 = parent->bounds.y2;

               root = parent;

               visible = dfb_region_intersect( &area, - x0, - y0, rx2 - x0, ry2 - y0 );

               parent = parent->parent;
          } while (visible && parent);
     }
     else if (base)
          visible = dfb_region_intersect( &area, 0, 0, region->bounds.x2, region->bounds.y2 );


     if (!visible) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is fully clipped by ancestors!\n" );

          *ret_num = 0;

          return DFB_OK;
     }


     stret_iteration_init( &iteration, root, children ? region : NULL );

     clip_out( &iteration, &context, x0 + area.x1, y0 + area.y1, x0 + area.x2, y0 + area.y2 );


     *ret_num = context.num;

     if (context.num > context.max) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Failed with %d/%d regions!\n",
                      context.num, context.max );

          return DFB_LIMITEXCEEDED;
     }

     D_DEBUG_AT( UniQuE_StReT, "  -> Succeeded with %d/%d regions.\n", context.num, context.max );

     return DFB_OK;
}


typedef struct {
     StretIteration  iteration;
     void           *update_data;
} UpdateContext;

static void
region_update( UpdateContext *context,
               int            x1,
               int            y1,
               int            x2,
               int            y2 )
{
     int          x0, y0;
     DFBRegion    area = { x1, y1, x2, y2 };
     StretRegion *region;

     D_DEBUG_AT( UniQuE_StReT, "  region_update( %4d, %4d - %4dx%4d )\n",
                 x1, y1, x2 - x1 + 1, y2 - y1 + 1 );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     while (true) {
          region = stret_iteration_next( &context->iteration, &area );
          if (!region)
               return;

          D_MAGIC_ASSERT( region, StretRegion );

          if (D_FLAGS_IS_SET( region->flags, SRF_OUTPUT ))
               break;
     }

     x0 = context->iteration.x0;
     y0 = context->iteration.y0;

     D_DEBUG_AT( UniQuE_StReT, "    -> %4d, %4d - %4dx%4d  @ %4d, %4d  (class %d, index %d)\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ),
                 x0, y0, region->clazz, region->index );

     dfb_region_clip( &area, DFB_REGION_VALS_TRANSLATED( &region->bounds, x0, y0 ) );



     if (D_FLAGS_IS_SET( region->flags, SRF_OPAQUE )) {
          /* upper */
          if (area.y1 != y1) {
               UpdateContext fork = *context;

               region_update( &fork, x1, y1, x2, area.y1 - 1 );
          }

          /* left */
          if (area.x1 != x1) {
               UpdateContext fork = *context;

               region_update( &fork, x1, area.y1, area.x1 - 1, area.y2 );
          }

          /* right */
          if (area.x2 != x2) {
               UpdateContext fork = *context;

               region_update( &fork, area.x2 + 1, area.y1, x2, area.y2 );
          }

          /* lower */
          if (area.y2 != y2) {
               UpdateContext fork = *context;

               region_update( &fork, x1, area.y2 + 1, x2, y2 );
          }

          stret_iteration_abort( &context->iteration );
     }
     else
          region_update( context, x1, y1, x2, y2 );


     x0 += region->bounds.x1;
     y0 += region->bounds.y1;

     dfb_region_translate( &area, - x0, - y0 );

     D_DEBUG_AT( UniQuE_StReT, "    => %4d, %4d - %4dx%4d  @ %4d, %4d  (class %d, index %d)\n",
                 DFB_RECTANGLE_VALS_FROM_REGION( &area ),
                 x0, y0, region->clazz, region->index );

     D_ASSERT( classes[region->clazz]->Update );

     classes[region->clazz]->Update( region, region->data, context->update_data,
                                     region->arg, x0, y0, &area, 1 );
}

DFBResult
stret_region_update( StretRegion     *region,
                     const DFBRegion *clip,
                     void            *update_data )
{
     DFBRegion     area;
     UpdateContext context;

     D_MAGIC_ASSERT( region, StretRegion );

     DFB_REGION_ASSERT_IF( clip );

     if (clip)
          D_DEBUG_AT( UniQuE_StReT,
                      "stret_region_update( %d, %d - %dx%d  of  %d, %d - %dx%d )\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( clip ),
                      DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ) );
     else
          D_DEBUG_AT( UniQuE_StReT,
                      "stret_region_update( %d, %d - %dx%d )\n",
                      DFB_RECTANGLE_VALS_FROM_REGION( &region->bounds ) );

     if (! D_FLAGS_IS_SET( region->flags, SRF_ACTIVE )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is not active and therefore invisible.\n" );
          return DFB_OK;
     }

     area.x1 = 0;
     area.y1 = 0;
     area.x2 = region->bounds.x2 - region->bounds.x1;
     area.y2 = region->bounds.y2 - region->bounds.y1;

     if (clip && !dfb_region_region_intersect( &area, clip )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region doesn't intersect with clip.\n" );
          return DFB_OK;
     }


     stret_iteration_init( &context.iteration, region, NULL );

     context.update_data = update_data;

     region_update( &context, area.x1, area.y1, area.x2, area.y2 );

     return DFB_OK;
}

StretRegion *
stret_region_at( StretRegion        *region,
                 int                 x,
                 int                 y,
                 StretRegionFlags    flags,
                 StretRegionClassID  class_id )
{
     StretIteration iteration;
     DFBRegion      area = { x, y, x, y };

     D_MAGIC_ASSERT( region, StretRegion );

     D_DEBUG_AT( UniQuE_StReT, "stret_region_at( %p, %d, %d, 0x%08x )\n", region, x, y, flags );

     if (! D_FLAGS_IS_SET( region->flags, SRF_ACTIVE )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is not active.\n" );
          return NULL;
     }


     stret_iteration_init( &iteration, region, NULL );

     while ((region = stret_iteration_next( &iteration, &area )) != NULL) {
          if (! D_FLAGS_ARE_SET( region->flags, flags ))
               continue;

          if (class_id != SRCID_UNKNOWN && region->clazz != class_id)
               continue;

          return region;
     }

     return NULL;
}

void *
stret_region_data( const StretRegion *region )
{
     D_MAGIC_ASSERT( region, StretRegion );

     return region->data;
}

