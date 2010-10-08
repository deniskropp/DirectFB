/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__OS__LINUX__KERNEL__THREAD_H__
#define __DIRECT__OS__LINUX__KERNEL__THREAD_H__

#include <asm/atomic.h>

#include <linux/sched.h>

#include <direct/debug.h>

/**********************************************************************************************************************/

struct __D_DirectThreadHandle {
     struct task_struct  *task;
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/

struct __D_DirectOnce {
     atomic_t  stage;
};

/**********************************************************************************************************************/

#define DIRECT_ONCE_INIT      { ATOMIC_INIT(0) }

/**********************************************************************************************************************/

typedef void (*DirectOnceInitHandler)( void );

/**********************************************************************************************************************/

__attribute__((no_instrument_function))
static inline int
direct_once( DirectOnce            *once,
             DirectOnceInitHandler  handler )
{
     while (true) {
          switch (atomic_cmpxchg( &once->stage, 0, 1 )) {
               case 1:
                    schedule_timeout( 1 );
                    break;

               case 0:
                    handler();

                    atomic_set( &once->stage, 2 );

               case 2:
                    return 0;
          }
     }

     return 0;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

struct __D_DirectTLS {
     void *value;
};

/**********************************************************************************************************************/

#define DIRECT_TLS_DATA( name )    \
     DEFINE_PER_CPU_SECTION( DirectTLS, name, "DirectTLS" );     \
     DECLARE_PER_CPU_SECTION( DirectTLS, name, "DirectTLS" );

/**********************************************************************************************************************/

#define direct_tls_get( name )                    (__get_cpu_var(name).value)
#define direct_tls_set( name, v )                 do { (__get_cpu_var(name).value) = (v); } while (0)
#define direct_tls_register( name, destructor )   do { (void) (destructor); } while (0)
#define direct_tls_unregister( name )             do { } while (0)

#endif

