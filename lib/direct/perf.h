/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


#ifndef __DIRECT__PERF_H__
#define __DIRECT__PERF_H__


#include <direct/types.h>




typedef struct {
     unsigned long  counter_id;    // maybe switch to direct pointer
     bool           reset_on_dump;

     char           name[100];
} DirectPerfCounterInstallation;


typedef struct {
     long long      start;
     long long      stop;

     unsigned long  count;

     char           name[100];
     bool           reset_on_dump;
} DirectPerfCounter;


#if D_DEBUG_ENABLED

#define D_PERF_COUNTER( _identifier, _name )           \
     DirectPerfCounterInstallation _identifier = {     \
                              0,                       \
                              true,                    \
                              (_name)                  \
     };


#define D_PERF_COUNT( _identifier )                    \
     direct_perf_count( &_identifier, 1 )

#define D_PERF_COUNT_N( _identifier, _diff )           \
     direct_perf_count( &_identifier, _diff )

#else

#define D_PERF_COUNTER( _identifier, _name )           \
     D_UNUSED int _identifier


#define D_PERF_COUNT( _identifier )                    \
     do {} while (0)

#define D_PERF_COUNT_N( _identifier, _diff )           \
     do {} while (0)

#endif


void direct_perf_count( DirectPerfCounterInstallation *installation, int index );


void direct_perf_dump_all( void );


void __D_perf_init( void );
void __D_perf_deinit( void );

#endif
