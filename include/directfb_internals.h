/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#ifndef __DIRECTFB_INTERNALS_H__
#define __DIRECTFB_INTERNALS_H__

#include <directfb.h>
#include <core/coretypes.h>

typedef struct {
     const char * (*GetType)();
     const char * (*GetImplementation)();
     DFBResult    (*Allocate)( void **interface );
     DFBResult    (*Probe)( void *data, ... );
     DFBResult    (*Construct)( void *interface, ... );
} DFBInterfaceFuncs;

DFBResult DFBGetInterface( DFBInterfaceFuncs **funcs,
                           char *type,
                           char *implementation,
                           int (*probe)( DFBInterfaceFuncs *impl, void *ctx ),
                           void *probe_ctx );

void DFBRegisterInterface( DFBInterfaceFuncs *funcs );

#define DFB_ALLOCATE_INTERFACE(p,i)     \
     (p) = (i*)calloc( 1, sizeof(i) );

#ifndef DFB_DEBUG
#define DFB_DEALLOCATE_INTERFACE(p)     \
     free( (p) );
#endif

#define INTERFACE_GET_DATA(i) \
     i##_data *data;                    \
                                        \
     if (!thiz)                         \
          return DFB_THIZNULL;          \
                                        \
     data = (i##_data*) thiz->priv;     \
                                        \
     if (!data)                         \
          return DFB_DEAD;

extern IDirectFB *idirectfb_singleton;

#endif /* __DIRECTFB_INTERNALS_H__ */
