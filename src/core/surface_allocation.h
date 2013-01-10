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

#ifndef __CORE__SURFACE_ALLOCATION_H__
#define __CORE__SURFACE_ALLOCATION_H__

#include <direct/debug.h>

#include <fusion/object.h>
#include <fusion/vector.h>

#include <core/gfxcard.h>
#include <core/surface.h>

#include <directfb.h>


/*
 * Configuration and State flags of a Surface Buffer Allocation
 */
typedef enum {
     CSALF_NONE          = 0x00000000,  /* None of these. */
     CSALF_INITIALIZING  = 0x00000001,  /* Allocation is being initialized */

     CSALF_VOLATILE      = 0x00000002,  /* Allocation should be freed when no longer up to date. */
     CSALF_PREALLOCATED  = 0x00000004,  /* Preallocated memory, don't zap when "thrifty-surface-buffers" is active. */

     CSALF_MUCKOUT       = 0x00001000,  /* Indicates surface pool being in the progress of mucking out this and possibly
                                           other allocations to have enough space for a new allocation to be made. */

     CSALF_DEALLOCATED   = 0x00002000,  /* Decoupled and deallocated surface buffer allocation */

     CSALF_ALL           = 0x00003006   /* All of these. */
} CoreSurfaceAllocationFlags;

typedef enum {
     CSANF_NONE     = 0x00000000
} CoreSurfaceAllocationNotificationFlags;

typedef struct {
     CoreSurfaceAllocationNotificationFlags  flags;
} CoreSurfaceAllocationNotification;

/*
 * An Allocation of a Surface Buffer
 */
struct __DFB_CoreSurfaceAllocation {
     FusionObject                   object;

     int                            magic;

     DirectSerial                   serial;       /* Equals serial of buffer if content is up to date. */

     CoreSurfaceBuffer             *buffer;       /* Surface Buffer owning this allocation. */
     CoreSurface                   *surface;      /* Surface owning the Buffer of this allocation. */
     CoreSurfacePool               *pool;         /* Surface Pool providing the allocation. */
     void                          *data;         /* Pool's private data for this allocation. */
     int                            size;         /* Amount of data used by this allocation. */
     unsigned long                  offset;       /* Offset within address range of pool if contiguous. */

     CoreSurfaceAllocationFlags     flags;        /* Pool can return CSALF_ONEFORALL upon allocation of first buffer. */

     const CoreSurfaceAccessFlags  *access;                 /* Possible access flags (pointer to pool description). */
     CoreSurfaceAccessFlags         accessed[_CSAID_NUM];   /* Access since last synchronization. */

     CoreSurfaceConfig              config;       /* Configuration of its surface at the time of the allocation creation */
     CoreSurfaceTypeFlags           type;
     unsigned long                  resource_id;  /* layer id, window id, or user specified */

     CoreGraphicsSerial             gfx_serial;

     int                            index;
};

#define CORE_SURFACE_ALLOCATION_ASSERT(alloc)                                                  \
     do {                                                                                      \
          D_MAGIC_ASSERT( alloc, CoreSurfaceAllocation );                                      \
          D_ASSUME( (alloc)->size > 0 );                                                       \
          D_ASSERT( (alloc)->size >= 0 );                                                      \
          D_ASSERT( (alloc)->offset + (alloc)->size <= ((alloc)->pool->desc.size ?:~0UL) );    \
          D_FLAGS_ASSERT( (alloc)->access[CSAID_CPU], CSAF_ALL );                              \
          D_FLAGS_ASSERT( (alloc)->access[CSAID_GPU], CSAF_ALL );                              \
          D_FLAGS_ASSERT( (alloc)->flags, CSALF_ALL );                                         \
          D_FLAGS_ASSERT( (alloc)->accessed[CSAID_CPU], CSAF_ALL );                            \
          D_FLAGS_ASSERT( (alloc)->accessed[CSAID_GPU], CSAF_ALL );                            \
     } while (0)


DFBResult dfb_surface_allocation_create  ( CoreDFB                     *core,
                                           CoreSurfaceBuffer           *buffer,
                                           CoreSurfacePool             *pool,
                                           CoreSurfaceAllocation      **ret_allocation );

DFBResult dfb_surface_allocation_decouple( CoreSurfaceAllocation       *allocation );


DFBResult dfb_surface_allocation_update  ( CoreSurfaceAllocation       *allocation,
                                           CoreSurfaceAccessFlags       access );


static inline int
dfb_surface_allocation_locks( CoreSurfaceAllocation *allocation )
{
     int refs;

     fusion_ref_stat( &allocation->object.ref, &refs );

     D_ASSERT( refs > 0 );

     return refs - 1;
}


FUSION_OBJECT_METHODS( CoreSurfaceAllocation, dfb_surface_allocation )

FusionObjectPool *dfb_surface_allocation_pool_create( const FusionWorld *world );

#endif

