/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __CORE__SURFACE_BUFFER_H__
#define __CORE__SURFACE_BUFFER_H__

#include <direct/debug.h>

#include <fusion/object.h>
#include <fusion/vector.h>

#include <core/surface.h>
#include <core/surface_allocation.h>

#include <directfb.h>


/*
 * Configuration and State flags of a Surface Buffer
 */
typedef enum {
     CSBF_NONE      = 0x00000000,  /* None of these. */

     CSBF_STICKED   = 0x00000001,  /* Sticked to one Surface Pool, e.g. system only. */

     CSBF_ALL       = 0x00000001   /* All of these. */
} CoreSurfaceBufferFlags;


typedef enum {
     CSBNF_NONE     = 0x00000000
} CoreSurfaceBufferNotificationFlags;

typedef struct {
     CoreSurfaceBufferNotificationFlags  flags;
} CoreSurfaceBufferNotification;


/*
 * A Lock on a Surface Buffer
 */
struct __DFB_CoreSurfaceBufferLock {
     int                      magic;              /* Must be valid before calling dfb_surface_pool_lock() */

     CoreSurfaceAccessorID    accessor;           /* " */
     CoreSurfaceAccessFlags   access;             /* " */

     CoreSurfaceBuffer       *buffer;             /* Set by dfb_surface_pool_lock() */
     CoreSurfaceAllocation   *allocation;         /* " */

     void                    *addr;               /* " */
     unsigned long            phys;               /* " */
     unsigned long            offset;             /* " */
     unsigned int             pitch;              /* " */

     void                    *handle;             /* " */
};

static inline void
dfb_surface_buffer_lock_reset( CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->buffer     = NULL;
     lock->allocation = NULL;
     lock->addr       = NULL;
     lock->phys       = 0;
     lock->offset     = ~0;
     lock->pitch      = 0;
     lock->handle     = 0;
}

static inline void
dfb_surface_buffer_lock_init( CoreSurfaceBufferLock *lock, CoreSurfaceAccessorID accessor, CoreSurfaceAccessFlags access )
{
     D_MAGIC_SET_ONLY( lock, CoreSurfaceBufferLock );

     lock->accessor = accessor;
     lock->access   = access;

     dfb_surface_buffer_lock_reset( lock );
}

static inline void
dfb_surface_buffer_lock_deinit( CoreSurfaceBufferLock *lock )
{
     D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );

     lock->accessor = CSAID_NONE;
     lock->access   = CSAF_NONE;

     D_MAGIC_CLEAR( lock );
}

#define CORE_SURFACE_BUFFER_LOCK_ASSERT(lock)                                                       \
     do {                                                                                           \
          D_MAGIC_ASSERT( lock, CoreSurfaceBufferLock );                                            \
          D_FLAGS_ASSERT( (lock)->access, CSAF_ALL );                                               \
          if ((lock)->allocation) {                                                                 \
               /*D_ASSERT( (lock)->buffer == (lock)->allocation->buffer );*/                            \
               D_ASSUME( (lock)->addr != NULL || (lock)->phys != 0 || (lock)->offset != ~0 || (lock)->handle != NULL );\
               D_ASSUME( (lock)->offset == (lock)->allocation->offset || (lock)->offset == ~0 );    \
               D_ASSERT( (lock)->pitch > 0 || ((lock)->addr == NULL && (lock)->phys == 0) );        \
          }                                                                                         \
          else {                                                                                    \
               D_ASSERT( (lock)->buffer == NULL );                                                  \
               D_ASSERT( (lock)->addr == NULL );                                                    \
               D_ASSERT( (lock)->phys == 0 );                                                       \
               D_ASSERT( (lock)->offset == ~0 );                                                    \
               D_ASSERT( (lock)->pitch == 0 );                                                      \
               D_ASSERT( (lock)->handle == NULL );                                                  \
          }                                                                                         \
     } while (0)

/*
 * A Surface Buffer of a Surface
 */
struct __DFB_CoreSurfaceBuffer {
     FusionObject             object;

     int                      magic;

     DirectSerial             serial;        /* Increased when content is written. */
     CoreSurfaceAllocation   *written;       /* Allocation with the last write access. */
     CoreSurfaceAllocation   *read;          /* Allocation with the last read access. */

     CoreSurface             *surface;       /* Surface owning this Surface Buffer. */
     CoreSurfacePolicy        policy;

     CoreSurfaceBufferFlags   flags;         /* Configuration and State flags. */
     DFBSurfacePixelFormat    format;        /* Pixel format of buffer data. */

     FusionVector             allocs;        /* Allocations within Surface Pools. */

     CoreSurfaceConfig        config;        /* Configuration of its surface at the time of the buffer creation */
     CoreSurfaceTypeFlags     type;
     unsigned long            resource_id;   /* layer id, window id, or user specified */
     
     int                      index;
};

#define CORE_SURFACE_BUFFER_ASSERT(buffer)                                                     \
     do {                                                                                      \
          D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );                                         \
     } while (0)


DFBResult dfb_surface_buffer_create ( CoreDFB                 *core,
                                      CoreSurface             *surface,
                                      CoreSurfaceBufferFlags   flags,
                                      int                      index,
                                      CoreSurfaceBuffer      **ret_buffer );

DFBResult dfb_surface_buffer_decouple( CoreSurfaceBuffer       *buffer );

DFBResult dfb_surface_buffer_deallocate( CoreSurfaceBuffer    *buffer );

DFBResult dfb_surface_buffer_lock   ( CoreSurfaceBuffer       *buffer,
                                      CoreSurfaceAccessorID    accessor,
                                      CoreSurfaceAccessFlags   access,
                                      CoreSurfaceBufferLock   *ret_lock );

DFBResult dfb_surface_buffer_unlock ( CoreSurfaceBufferLock   *lock );

DFBResult dfb_surface_buffer_read   ( CoreSurfaceBuffer       *buffer,
                                      void                    *destination,
                                      int                      pitch,
                                      const DFBRectangle      *rect );

DFBResult dfb_surface_buffer_write  ( CoreSurfaceBuffer       *buffer,
                                      const void              *source,
                                      int                      pitch,
                                      const DFBRectangle      *rect );

DFBResult dfb_surface_buffer_dump   ( CoreSurfaceBuffer       *buffer,
                                      const char              *directory,
                                      const char              *prefix );

CoreSurfaceAllocation *
dfb_surface_buffer_find_allocation( CoreSurfaceBuffer       *buffer,
                                    CoreSurfaceAccessorID    accessor,
                                    CoreSurfaceAccessFlags   flags,
                                    bool                     lock );

static inline int
dfb_surface_buffer_index( CoreSurfaceBuffer *buffer )
{
     D_MAGIC_ASSERT( buffer, CoreSurfaceBuffer );

     return buffer->index;
}


FUSION_OBJECT_METHODS( CoreSurfaceBuffer, dfb_surface_buffer )

FusionObjectPool *dfb_surface_buffer_pool_create( const FusionWorld *world );

#endif

