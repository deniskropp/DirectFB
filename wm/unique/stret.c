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


#define MAX_CLASSES 16

D_DEBUG_DOMAIN( UniQuE_StReT, "UniQuE/StReT", "UniQuE's Stack Region Tree" );

struct __UniQuE_StretRegion {
     int                 magic;

     int                 index;

     StretRegion        *parent;
     FusionVector        children;

     StretRegionFlags    flags;

     DFBRegion           bounds;

     StretRegionClassID  clazz;

     void               *data;
     unsigned long       arg;
};

static const StretRegionClass *classes[MAX_CLASSES];
static pthread_mutex_t         classes_lock = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;
static int                     classes_count;


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
                     int                  x,
                     int                  y,
                     int                  width,
                     int                  height,
                     StretRegion         *parent,
                     StretRegion        **ret_region )
{
     StretRegion *region;

     D_DEBUG_AT( UniQuE_StReT, "stret_region_create( class %d, flags 0x%08x, "
                 "%d,%d - %dx%d, parent %p )\n", class_id, flags, x, y, width, height, parent );

     D_ASSERT( class_id >= 0 );
     D_ASSERT( class_id < MAX_CLASSES );
     D_ASSERT( classes[class_id] != NULL );

     D_ASSERT( ! (flags & ~SRF_ALL) );

     D_ASSERT( width > 0 );
     D_ASSERT( height > 0 );

     if (parent)
          D_MAGIC_ASSERT( parent, StretRegion );

     D_ASSERT( ret_region != NULL );

     region = SHCALLOC( 1, sizeof(StretRegion) );
     if (!region) {
          D_WARN( "out of (shared) memory" );
          return DFB_NOSYSTEMMEMORY;
     }

     region->parent = parent;
     region->flags  = flags;
     region->bounds = (DFBRegion) { x, y, x + width - 1, y + height - 1 };
     region->clazz  = class_id;
     region->data   = data;
     region->arg    = arg;

     fusion_vector_init( &region->children, parent ? 10 : 32 );

     if (parent) {
          if (fusion_vector_add( &parent->children, region )) {
               D_WARN( "out of (shared) memory" );
               SHFREE( region );
               return DFB_NOSYSTEMMEMORY;
          }

          region->index = fusion_vector_size( &parent->children ) - 1;
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
     int          index;
     StretRegion *parent;
     StretRegion *child;

     D_MAGIC_ASSERT( region, StretRegion );

     D_ASSUME( ! fusion_vector_has_elements( &region->children ) );

     parent = region->parent;
     if (parent) {
          int index;

          D_MAGIC_ASSERT( parent, StretRegion );

          index = region->index;

          D_ASSERT( index >= 0 );
          D_ASSERT( index == fusion_vector_index_of( &parent->children, region ) );

          fusion_vector_remove( &parent->children, index );

          for (; index<fusion_vector_size(&parent->children); index++) {
               StretRegion *child = fusion_vector_at( &parent->children, index );

               child->index = index;
          }
     }

     fusion_vector_foreach( child, index, region->children ) {
          D_MAGIC_ASSERT( child, StretRegion );
          D_ASSERT( child->parent == region );

          child->parent = NULL;
     }

     fusion_vector_destroy( &region->children );

     D_MAGIC_CLEAR( region );

     SHFREE( region );

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
          int old;

          D_MAGIC_ASSERT( parent, StretRegion );

          old = region->index;

          D_ASSERT( old >= 0 );
          D_ASSERT( old == fusion_vector_index_of( &parent->children, region ) );

          fusion_vector_move( &parent->children, old, index );

          for (index = MIN(index,old); index<fusion_vector_size(&parent->children); index++) {
               StretRegion *child = fusion_vector_at( &parent->children, index );

               child->index = index;
          }
     }

     return DFB_OK;
}


typedef struct {
     int        num;
     int        max;
     DFBRegion *regions;
} ClipOutContext;

static void
clip_out( StretRegion    *current,
          int             start,
          ClipOutContext *context,
          int             x0,
          int             y0,
          int             x1,
          int             y1,
          int             x2,
          int             y2 )
{
     int          i;
     int          num;
     StretRegion *parent;

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

restart:

     D_MAGIC_ASSERT( current, StretRegion );

     num = fusion_vector_size( &current->children );

     D_ASSERT( start >= 0 );
     D_ASSERT( start <= num );

     for (i=start; i<num; i++) {
          StretRegion *child = fusion_vector_at( &current->children, i );

          D_MAGIC_ASSERT( child, StretRegion );

          if (!FLAG_IS_SET( child->flags, SRF_ACTIVE ))
               continue;

          if (!dfb_region_intersects( &child->bounds, x1, y1, x2, y2 ))
               continue;

          if (FLAGS_ARE_SET( child->flags, SRF_OPAQUE | SRF_OUTPUT )) {
               DFBRegion cutout = DFB_REGION_INIT_INTERSECTED( &child->bounds, x1, y1, x2, y2 );

               if (child->children.count > 0) {
                    int cx  = child->bounds.x1;
                    int cy  = child->bounds.y1;
                    int cx0 = x0 + cx;
                    int cy0 = y0 + cy;

                    /* upper */
                    if (cutout.y1 != y1)
                         clip_out( child, 0, context,
                                   cx0, cy0, x1 - cx, y1 - cy, x2 - cx, cutout.y1-1 - cy );

                    /* left */
                    if (cutout.x1 != x1)
                         clip_out( child, 0, context,
                                   cx0, cy0, x1 - cx, cutout.y1 - cy, cutout.x1-1 - cx, cutout.y2 - cy );

                    /* right */
                    if (cutout.x2 != x2)
                         clip_out( child, 0, context,
                                   cx0, cy0, cutout.x2+1 - cx, cutout.y1 - cy, x2 - cx, cutout.y2 - cy );

                    /* lower */
                    if (cutout.y2 != y2)
                         clip_out( child, 0, context,
                                   cx0, cy0, x1 - cx, cutout.y2+1 - cy, x2 - cx, y2 - cy );
               }
               else {
                    /* upper */
                    if (cutout.y1 != y1)
                         clip_out( current, i + 1, context, x0, y0, x1, y1, x2, cutout.y1-1 );

                    /* left */
                    if (cutout.x1 != x1)
                         clip_out( current, i + 1, context, x0, y0, x1, cutout.y1, cutout.x1-1, cutout.y2 );

                    /* right */
                    if (cutout.x2 != x2)
                         clip_out( current, i + 1, context, x0, y0, cutout.x2+1, cutout.y1, x2, cutout.y2 );

                    /* lower */
                    if (cutout.y2 != y2)
                         clip_out( current, i + 1, context, x0, y0, x1, cutout.y2+1, x2, y2 );
               }


               return;
          }

          if (fusion_vector_size( &child->children ) > 0) {
               int cx  = child->bounds.x1;
               int cy  = child->bounds.y1;

               current = child;
               start   = 0;

               x0 += cx;
               y0 += cy;
               x1 -= cx;
               y1 -= cy;
               x2 -= cx;
               y2 -= cy;

               goto restart;
          }
     }

     parent = current->parent;
     while (parent) {
          int cx  = current->bounds.x1;
          int cy  = current->bounds.y1;
          int pos = current->index;

          D_MAGIC_ASSERT( parent, StretRegion );

          D_ASSERT( pos == fusion_vector_index_of( &parent->children, current ) );

          x0 -= cx;
          y0 -= cy;
          x1 += cx;
          y1 += cy;
          x2 += cx;
          y2 += cy;

          if (pos < fusion_vector_size( &parent->children ) - 1) {
               current = parent;
               start   = pos + 1;

               goto restart;
          }

          current = parent;
          parent  = parent->parent;
     }

     D_ASSUME( x0 == 0 );
     D_ASSUME( y0 == 0 );

     context->num++;

     if (context->num <= context->max) {
          DFBRegion *region = &context->regions[ context->num - 1 ];

          D_DEBUG_AT( UniQuE_StReT, "    (%2d) %4d, %4d - %4dx%4d\n",
                      context->num - 1, x0 + x1, y0 + y1, x2 - x1 + 1, y2 - y1 + 1 );

          region->x1 = x1 + x0;
          region->y1 = y1 + y0;
          region->x2 = x2 + x0;
          region->y2 = y2 + y0;
     }
     else
          D_DEBUG_AT( UniQuE_StReT, "  Maximum number of regions exceeded, dropping...\n" );
}

DFBResult
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

     return DFB_OK;
}

DFBResult
stret_region_visible( StretRegion     *region,
                      const DFBRegion *base,
                      DFBRegion       *ret_regions,
                      int              max_num,
                      int             *ret_num )
{
     bool           visible = true;
     DFBRegion      area;
     ClipOutContext context;
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


     if (! FLAG_IS_SET( region->flags, SRF_ACTIVE )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is not active and therefore invisible!\n" );

          *ret_num = 0;

          return DFB_OK;
     }


     context.num     = 0;
     context.max     = max_num;
     context.regions = ret_regions;

     x0 = region->bounds.x1;
     y0 = region->bounds.y1;

     if (region->parent) {
          int          rx2;
          int          ry2;
          StretRegion *parent = region->parent;

          do {
               D_MAGIC_ASSERT( parent, StretRegion );

               if (! FLAG_IS_SET( parent->flags, SRF_ACTIVE )) {
                    D_DEBUG_AT( UniQuE_StReT, "  -> At least one parent is not active!\n" );

                    *ret_num = 0;

                    return DFB_OK;
               }

               x0 += parent->bounds.x1;
               y0 += parent->bounds.y1;

               rx2 = parent->bounds.x2;
               ry2 = parent->bounds.y2;

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


     clip_out( region, 0, &context, x0, y0, area.x1, area.y1, area.x2, area.y2 );


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
     StretRegion *region;
     int          index;
} StackFrame;

typedef struct {
     StackFrame   stack[8];
     int          current;

     int          x0;
     int          y0;
} UpdateContext;

#if 0
static inline StretRegion *
get_next_intersection( UpdateContext *context, const DFBRegion *clip )
{
     int          index;
     StretRegion *region;

     do {
          region = context->stack[context->current].region;
          index  = context->stack[context->current].index--;

          if (index < 0) {
               if (context->current > 0) {
                    context->current--;
                    context->x0 -= region->x1;
                    context->y0 -= region->y1;
               }
          }
          else {

          }
     } while (  );
}
#endif

static void
region_update( StretRegion    *current,
               int             start,
               void           *update_data,
               int             x0,
               int             y0,
               int             x1,
               int             y1,
               int             x2,
               int             y2 )
{
     int          i;
     int          num;
     bool         self;
     DFBRegion    area = { x1, y1, x2, y2 };
     StretRegion *parent;

     D_DEBUG_AT( UniQuE_StReT, "  region_update( %4d, %4d - %4dx%4d  @ %4d, %4d  start %d) <- class %d\n",
                 x1, y1, x2 - x1 + 1, y2 - y1 + 1, x0, y0, start, current->clazz );

     D_ASSERT( x1 <= x2 );
     D_ASSERT( y1 <= y2 );

     D_MAGIC_ASSERT( current, StretRegion );

     num = fusion_vector_size( &current->children );

     D_ASSERT( start >= -1 );
     D_ASSERT( start < num );


     self = (FLAG_IS_SET( current->flags, SRF_ACTIVE ) &&
             dfb_region_intersect( &area, 0, 0,
                                   current->bounds.x2 - current->bounds.x1,
                                   current->bounds.y2 - current->bounds.y1 ));


     if (self) {
          for (i=start; i>=0; i--) {
               StretRegion *child = fusion_vector_at( &current->children, i );

               D_MAGIC_ASSERT( child, StretRegion );

               if (!FLAG_IS_SET( child->flags, SRF_ACTIVE ))
                    continue;

               if (dfb_region_intersects( &child->bounds, x1, y1, x2, y2 )) {
                    int       dx    = 0;
                    int       dy    = 0;
                    bool      first = start == i;
                    DFBRegion inter = DFB_REGION_INIT_INTERSECTED( &child->bounds, x1, y1, x2, y2 );

                    if (first)
                         start = i - 1;
                    else
                         start = i;

                    if (start >= 0) {
                         StretRegion *child = fusion_vector_at( &current->children, start );

                         D_MAGIC_ASSERT( child, StretRegion );

                         while (child->children.count) {
                              dx -= child->bounds.x1;
                              dy -= child->bounds.y1;

                              start   = fusion_vector_size( &child->children ) - 1;
                              current = child;

                              child = fusion_vector_at( &current->children, start );

                              D_MAGIC_ASSERT( child, StretRegion );
                         }
                    }

                    if (FLAG_IS_SET( child->flags, SRF_OPAQUE )) {
                         /* upper */
                         if (inter.y1 != y1)
                              region_update( current, start, update_data, x0 - dx, y0 - dy,
                                             x1 + dx, y1 + dy, x2 + dx, inter.y1-1 + dy );

                         /* left */
                         if (inter.x1 != x1)
                              region_update( current, start, update_data, x0 - dx, y0 - dy,
                                             x1 + dx, inter.y1 + dy, inter.x1-1 + dx, inter.y2 + dy );

                         /* right */
                         if (inter.x2 != x2)
                              region_update( current, start, update_data, x0 - dx, y0 - dy,
                                             inter.x2+1 + dx, inter.y1 + dy, x2 + dx, inter.y2 + dy );

                         /* lower */
                         if (inter.y2 != y2)
                              region_update( current, start, update_data, x0 - dx, y0 - dy,
                                             x1 + dx, inter.y2+1 + dy, x2 + dx, y2 + dy );
                    }
                    else
                         region_update( current, start, update_data, x0 - dx, y0 - dy,
                                        x1 + dx, y1 + dy, x2 + dx, y2 + dy );

                    if (first && FLAG_IS_SET( child->flags, SRF_OUTPUT )) {
                         x0 += child->bounds.x1;
                         y0 += child->bounds.y1;

                         dfb_region_translate( &inter, - child->bounds.x1, - child->bounds.y1 );

                         D_DEBUG_AT( UniQuE_StReT, "    -> %4d, %4d - %4dx%4d  @ %4d, %4d  (class %d, index %d)\n",
                                     DFB_RECTANGLE_VALS_FROM_REGION( &inter ), x0, y0, child->clazz, child->index );

                         classes[child->clazz]->Update( child, child->data, update_data,
                                                        child->arg, x0, y0, &inter, 1 );
                    }

                    return;
               }
          }
     }

     parent = current->parent;
     if (parent) {
          int dx  = current->bounds.x1;
          int dy  = current->bounds.y1;
          int pos = current->index;

          D_MAGIC_ASSERT( parent, StretRegion );

          D_ASSERT( pos == fusion_vector_index_of( &parent->children, current ) );

          start = pos - 1;

          if (start >= 0) {
               StretRegion *child = fusion_vector_at( &parent->children, start );

               D_MAGIC_ASSERT( child, StretRegion );

               while (child->children.count) {
                    dx -= child->bounds.x1;
                    dy -= child->bounds.y1;

                    start  = fusion_vector_size( &child->children ) - 1;
                    parent = child;

                    child = fusion_vector_at( &parent->children, start );

                    D_MAGIC_ASSERT( child, StretRegion );
               }
          }

          region_update( parent, start, update_data,
                         x0 - dx, y0 - dy, x1 + dx, y1 + dy, x2 + dx, y2 + dy );
     }
     else {
          D_ASSUME( x0 == 0 );
          D_ASSUME( y0 == 0 );
     }

     if (self && FLAG_IS_SET( current->flags, SRF_OUTPUT )) {
          D_DEBUG_AT( UniQuE_StReT, "    => %4d, %4d - %4dx%4d  @ %4d, %4d  (class %d, index %d)\n",
                      DFB_RECTANGLE_VALS_FROM_REGION(&area), x0, y0, current->clazz, current->index );

          classes[current->clazz]->Update( current, current->data, update_data,
                                           current->arg, x0, y0, &area, 1 );
     }
}

DFBResult
stret_region_update( StretRegion     *region,
                     const DFBRegion *clip,
                     void            *update_data )
{
     DFBRegion area;

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

     if (! FLAG_IS_SET( region->flags, SRF_ACTIVE )) {
          D_DEBUG_AT( UniQuE_StReT, "  -> Region is not active and therefore invisible.\n" );
          return DFB_OK;
     }

     area.x1 = 0;
     area.y1 = 0;
     area.x2 = region->bounds.x2 - region->bounds.x1;
     area.y2 = region->bounds.y2 - region->bounds.y1;

     if (clip && !dfb_region_region_intersect( &area, clip ))
          return DFB_OK;

     if (region->children.count) {
          int          start = fusion_vector_size( &region->children ) - 1;
          StretRegion *child = fusion_vector_at( &region->children, start );
          int          x0 = 0, y0 = 0;

          D_MAGIC_ASSERT( child, StretRegion );

          while (child->children.count) {
               int cx = child->bounds.x1;
               int cy = child->bounds.y1;

               x0 += cx;
               y0 += cy;

               dfb_region_translate( &area, -cx, -cy );

               start  = fusion_vector_size( &child->children ) - 1;
               region = child;

               child = fusion_vector_at( &region->children, start );

               D_MAGIC_ASSERT( child, StretRegion );
          }

          region_update( region, start, update_data, x0, y0, area.x1, area.y1, area.x2, area.y2 );
     }
     else
          region_update( region, -1, update_data, 0, 0, area.x1, area.y1, area.x2, area.y2 );

     return DFB_OK;
}

