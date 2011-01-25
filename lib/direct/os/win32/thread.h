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

#ifndef __DIRECT__OS__WIN32__THREAD_H__
#define __DIRECT__OS__WIN32__THREAD_H__

#include <direct/util.h>

/**********************************************************************************************************************/

struct __D_DirectThreadHandle {
     HANDLE  thread;
     DWORD   gen;
};

/**********************************************************************************************************************/
/**********************************************************************************************************************/

/**********************************************************************************************************************/

struct __D_DirectOnce {
     int x;
};

/**********************************************************************************************************************/

#define DIRECT_ONCE_INIT      { x 0 }

/**********************************************************************************************************************/

typedef void (*DirectOnceInitHandler)( void );

/**********************************************************************************************************************/

static __inline__ DirectResult
direct_once( DirectOnce            *once,
             DirectOnceInitHandler  handler )
{
     D_UNIMPLEMENTED();

     return DR_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

struct __D_DirectTLS {
     DWORD handle;
};

/**********************************************************************************************************************/

#define DIRECT_TLS_DATA( name )    \
     static DirectTLS name = { x 0 }

/**********************************************************************************************************************/

static __inline__ void *direct_tls_get__( DirectTLS *tls );

static __inline__ DirectResult direct_tls_set__( DirectTLS *tls,
                                                 void      *value );

static __inline__ DirectResult direct_tls_register( DirectTLS  *tls,
                                                    void      (*destructor)( void* ) );

static __inline__ DirectResult direct_tls_unregister( DirectTLS *tls );

/**********************************************************************************************************************/

#define direct_tls_get( name )                    direct_tls_get__( &name )
#define direct_tls_set( name, v )                 direct_tls_set__( &name, v )

/**********************************************************************************************************************/

static __inline__ void *
direct_tls_get__( DirectTLS *tls )
{
     return TlsGetValue( tls->handle );
}

static __inline__ DirectResult
direct_tls_set__( DirectTLS *tls,
                  void      *value )
{
     return TlsSetValue( tls->handle, value ) ? DR_OK : DR_FAILURE;
}

static __inline__ DirectResult
direct_tls_register( DirectTLS *tls, void (*destructor)( void* ) )	// FIXME: destructor not implemented
{
     tls->handle = TlsAlloc();

     return (tls->handle != TLS_OUT_OF_INDEXES) ? DR_OK : DR_FAILURE;
}

static __inline__ DirectResult
direct_tls_unregister( DirectTLS *tls )
{
     return TlsFree( tls->handle ) ? DR_OK : DR_FAILURE;
}


#endif

