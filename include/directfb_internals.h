/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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
     DFBResult    (*Probe)( void *ctx, ... );
     DFBResult    (*Construct)( void *interface, ... );
} DFBInterfaceFuncs;

/*
 * Loads an interface of a specific 'type'.
 * Optionally an 'implementation' can be chosen.
 * A 'probe' function can be used to check available implementations.
 *
 * After success 'funcs' is set.
 */
DFBResult DFBGetInterface( DFBInterfaceFuncs **funcs,
                           char *type,
                           char *implementation,
                           int (*probe)( DFBInterfaceFuncs *impl, void *ctx ),
                           void *probe_ctx );

/*
 * Default probe function. Calls "funcs->Probe(ctx)".
 * Can be used as the 'probe' argument to DFBGetInterface.
 * 'probe_ctx' should then be set to the interface specific probe context.
 */
int DFBProbeInterface( DFBInterfaceFuncs *funcs, void *ctx );

/*
 * Called by implementation modules during 'dlopen'ing or at startup if linked
 * into the executable.
 */
void DFBRegisterInterface( DFBInterfaceFuncs *funcs );

#define DFB_ALLOCATE_INTERFACE(p,i)     \
     (p) = (i*)calloc( 1, sizeof(i) );


#define DFB_ALLOCATE_INTERFACE_DATA(p,i)     \
     i##_data *data;                                        \
                                                            \
     if (!(p)->priv)                                        \
          (p)->priv = DFBCALLOC( 1, sizeof(i##_data) );     \
                                                            \
     data = (i##_data*)((p)->priv);


#ifdef DFB_DEBUG
#define DFB_DEALLOCATE_INTERFACE(p)     \
     if ((p)->priv) {                   \
          DFBFREE( (p)->priv );         \
          (p)->priv = NULL;             \
     }
#else
#define DFB_DEALLOCATE_INTERFACE(p)     \
     if ((p)->priv) {                   \
          DFBFREE( (p)->priv );         \
          (p)->priv = NULL;             \
     }                                  \
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
