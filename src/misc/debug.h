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

#ifndef __MISC__DEBUG_H__
#define __MISC__DEBUG_H__

#include <config.h>

#include <core/fusion/fusion_types.h>

typedef struct __DFB_TraceBuffer TraceBuffer;

void dfb_trace_print_stack( TraceBuffer *buffer );
void dfb_trace_print_stacks();

#ifdef DFB_DEBUG
void dfb_assertion_fail( const char *expression,
                         const char *filename,
                         int         line,
                         const char *function );

void dfb_assumption_fail( const char *expression,
                          const char *filename,
                          int         line,
                          const char *function );

#define DFB_MAGIC(spell)        ( (((spell)[0] << 24) | \
                                   ((spell)[1] << 16) | \
                                   ((spell)[2] <<  8) | \
                                   ((spell)[3]      )) ^  \
                                  (((spell)[4] << 24) | \
                                   ((spell)[5] << 16) | \
                                   ((spell)[6] <<  8) | \
                                   ((spell)[7]      )) )

#define DFB_MAGIC_CLEAR(o)      do { (o)->magic = 0; } while (0)
#define DFB_MAGIC_SET(o,m)      do { (o)->magic = DFB_MAGIC(#m); } while (0)
#define DFB_MAGIC_ASSERT(o,m)   do { DFB_ASSERT( (o) != NULL ); DFB_ASSERT( (o)->magic == DFB_MAGIC(#m) ); } while (0)

#else

#define DFB_MAGIC_CLEAR(o)      do {} while (0)
#define DFB_MAGIC_SET(o,m)      do {} while (0)
#define DFB_MAGIC_ASSERT(o,m)   do {} while (0)
#endif

#endif

