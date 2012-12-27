/*
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__PERF_H__
#define __DIRECT__PERF_H__


#include <direct/types.h>




typedef struct {
     unsigned long  counter_id;    // maybe switch to direct pointer

     char           name[100];
} DirectPerfCounterInstallation;


typedef struct {
     long long      start;
     long long      stop;

     unsigned long  count;

     char           name[100];
} DirectPerfCounter;


#define D_PERF_COUNTER( _identifier, _name )           \
     DirectPerfCounterInstallation _identifier = {     \
          counter_id:         ~0UL,                    \
          name:               (_name)                  \
     };


#define D_PERF_COUNT( _identifier )                    \
     direct_perf_count( &_identifier )




void direct_perf_count( DirectPerfCounterInstallation *installation );


void direct_perf_dump_all( void );


void __D_perf_init( void );
void __D_perf_deinit( void );

#endif
