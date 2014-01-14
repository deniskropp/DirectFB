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



#include <config.h>

#include <direct/EvLog.h>
#include <direct/Mutex.h>
#include <direct/String.h>

extern "C" {
#include <direct/log.h>
#include <direct/system.h>
#include <direct/thread.h>
}

#include <map>
#include <string>
#include <vector>

/**********************************************************************************************************************/

namespace Direct {

class EvLog {
public:
     DirectLogDomain domain;

     class Entry {
     public:
          void         *ctx;
          std::string   event;
          std::string   info;
          std::string   function;
          std::string   file;
          unsigned int  line;
          long long     micros;
          std::string   thread_name;

          Entry( void         *ctx,
                 std::string   event,
                 std::string   info,
                 std::string   function,
                 std::string   file,
                 unsigned int  line )
               :
               ctx( ctx ),
               event( event ),
               info( info ),
               function( function ),
               file( file ),
               line( line )
          {
               micros      = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
               thread_name = direct_thread_self_name();
          }

          void dump( size_t n )
          {
               long long millis = micros / 1000LL;

               direct_log_printf( NULL,
                                  "(=)                                [%-16.16s %3lld.%03lld,%03lld] (%5d) [%2zu] %-40s | %s\n",
                                  thread_name.c_str(), millis / 1000LL, millis % 1000LL, micros % 1000LL,
                                  direct_gettid(), n, event.c_str(), info.c_str() );
          }
     };

     Direct::String                     name;
     std::map<void*,std::vector<Entry>> map;

     EvLog( const Direct::String &name )
          :
          name( name )
     {
          memset( &domain, 0, sizeof(domain) );

          domain.description = "Event Log";
          domain.name        = *name;
          domain.name_len    = name.length();
     }

     void log( void         *ctx,
               const char   *event,
               const char   *info,
               const char   *function,
               const char   *file,
               unsigned int  line )
     {
          map[ctx].push_back( Entry( ctx, event, info, function, file, line ) );
     }

     void dump( void       *ctx,
                const char *event )
     {
          if (ctx) {
               dump( map[ctx], event );
          }
          else {
               for (auto it = map.begin(); it != map.end(); it++)
                    dump( (*it).second, event );
          }
     }

     void dump( std::vector<Entry> &entries,
                const char         *event )
     {
          long long     micros = direct_clock_get_time( DIRECT_CLOCK_MONOTONIC );
          long long     millis = micros / 1000LL;
          DirectThread *thread = direct_thread_self();
          const char   *name   = thread ? thread->name : "  NO NAME";

          direct_log_printf( NULL,
                             "(=) [%-16.16s %3lld.%03lld,%03lld] (%5d) ============================== Event Log | %s | %p =====\n",
                             name, millis / 1000LL, millis % 1000LL, micros % 1000LL,
                             direct_gettid(), "", entries[0].ctx );    // FIXME: add name

          size_t n = 0;

          for (auto it2 = entries.begin(); it2 != entries.end(); it2++) {
               Entry &entry = *it2;

               if (!event || entry.event == event)
                    entry.dump( n );

               n++;
          }
     }
};

class EvLogs {
public:
     std::map<std::string,EvLog*> map;
     Direct::Mutex                lock;
};

static EvLogs logs;


extern "C" {

void direct_evlog_log( const char   *evlog,
                       void         *ctx,
                       const char   *event,
                       const char   *info,
                       const char   *function,
                       const char   *file,
                       unsigned int  line )
{
     Direct::Mutex::Lock l1( logs.lock );

     auto log = logs.map[evlog];

     if (!log)
          log = logs.map[evlog] = new EvLog( evlog );

     if (direct_log_domain_check( &log->domain ))
          log->log( ctx, event, info, function, file, line );
}

void direct_evlog_dump( const char *evlog,
                        void       *ctx,
                        const char *event )
{
     Direct::Mutex::Lock l1( logs.lock );

     if (evlog) {
          auto log = logs.map[evlog];

          if (log)
               log->dump( ctx, event );
     }
     else {
          for (auto it = logs.map.begin(); it != logs.map.end(); it++) {
               auto log = (*it).second;

               if (log)
                    log->dump( ctx, event );
          }
     }
}

}

}

