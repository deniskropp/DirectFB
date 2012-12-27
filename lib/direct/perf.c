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

#include <config.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/perf.h>




static DirectHash    counter_hash;
static DirectMutex   counter_lock;
static unsigned long counter_ids;

/**********************************************************************************************************************/

void
__D_perf_init()
{
     direct_hash_init( &counter_hash, 7 );
     direct_mutex_init( &counter_lock );
}

void
__D_perf_deinit()
{
     direct_perf_dump_all();

     direct_hash_deinit( &counter_hash );
     direct_mutex_deinit( &counter_lock );
}

/**********************************************************************************************************************/

void
direct_perf_count( DirectPerfCounterInstallation *installation )
{
     DirectPerfCounter *counter;

     D_ASSERT( installation != NULL );

     direct_mutex_lock( &counter_lock );

     if (installation->counter_id == ~0) {
          counter = D_CALLOC( 1, sizeof(DirectPerfCounter) );
          if (!counter) {
               direct_mutex_unlock( &counter_lock );
               D_OOM();
               return;
          }

          installation->counter_id = ++counter_ids;
          D_ASSERT( installation->counter_id != 0 );   // FIXME: can there be more than 4 billion counters?

          counter->start = direct_clock_get_time( DIRECT_CLOCK_SESSION );

          direct_snputs( counter->name, installation->name, sizeof(counter->name) );

          direct_hash_insert( &counter_hash, installation->counter_id, counter );
     }
     else {
          counter = direct_hash_lookup( &counter_hash, installation->counter_id );
          if (!counter) {
               direct_mutex_unlock( &counter_lock );
               D_BUG( "unknown performance counter installation (%lu)", installation->counter_id );
               return;
          }
     }

     counter->count++;
     counter->stop = 0;

     direct_mutex_unlock( &counter_lock );
}

static bool
perf_iterate( DirectHash    *hash,
              unsigned long  key,
              void          *value,
              void          *ctx )
{
     DirectPerfCounter *counter = value;

     counter->stop = direct_clock_get_time( DIRECT_CLOCK_SESSION );

     direct_log_printf( NULL, "  %-60s  %12lu  (%7.3f/sec)  %9lld -%9lld\n", counter->name,
                        counter->count, counter->count * 1000000.0 / (double)(counter->stop - counter->start),
                        counter->start, counter->stop );

     return true;
}

void
direct_perf_dump_all()
{
     direct_mutex_lock( &counter_lock );

     direct_log_printf( NULL, "Performance Counters                                               Total count    rate           start        end\n" );

     direct_hash_iterate( &counter_hash, perf_iterate, NULL );

     direct_mutex_unlock( &counter_lock );
}

