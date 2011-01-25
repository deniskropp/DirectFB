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

#ifndef __DIRECT__INTERFACE_IMPLEMENTATION_H__
#define __DIRECT__INTERFACE_IMPLEMENTATION_H__

#include <direct/interface.h>


static const char   *GetType( void );
static const char   *GetImplementation( void );
static DirectResult  Allocate( void **ret_interface );
static DirectResult  Deallocate( void *interface_ptr );
//static DirectResult  Probe( void *ctx, ... );
//static DirectResult  Construct( void *interface, ... );


static DirectInterfaceFuncs interface_funcs = {
     /* GetType */            GetType,
     /* GetImplementation */  GetImplementation,
     /* Allocate */           Allocate,
     /* Deallocate */         Deallocate,
     /* Probe */              (void*) Probe,    //FIXME
     /* Construct */          (void*) Construct //FIXME
};

#define DIRECT_INTERFACE_IMPLEMENTATION(type, impl)              \
                                                                 \
__constructor__ void type##_##impl##_ctor( void );               \
__destructor__  void type##_##impl##_dtor( void );               \
                                                                 \
static const char *                                              \
GetType( void )                                                  \
{                                                                \
     return #type;                                               \
}                                                                \
                                                                 \
static const char *                                              \
GetImplementation( void )                                        \
{                                                                \
     return #impl;                                               \
}                                                                \
                                                                 \
static DirectResult                                              \
Allocate( void **ret_interface )                                 \
{                                                                \
     DIRECT_ALLOCATE_INTERFACE( *ret_interface, type );          \
     return DR_OK;                                               \
}                                                                \
                                                                 \
static DirectResult                                              \
Deallocate( void *interface_ptr )                                \
{                                                                \
     DIRECT_DEALLOCATE_INTERFACE( (IAny*) (interface_ptr) );     \
     return DR_OK;                                               \
}                                                                \
                                                                 \
void                                                             \
type##_##impl##_ctor( void )                                     \
{                                                                \
     DirectRegisterInterface( &interface_funcs );                \
}                                                                \
                                                                 \
void                                                             \
type##_##impl##_dtor( void )                                     \
{                                                                \
     DirectUnregisterInterface( &interface_funcs );              \
}

#endif

