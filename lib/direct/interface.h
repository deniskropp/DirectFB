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

#ifndef __DIRECT__INTERFACE_H__
#define __DIRECT__INTERFACE_H__

#include <direct/build.h>
#include <direct/types.h>

#include <direct/debug.h>
#include <direct/mem.h>


#include <directfb.h>    /* FIXME: needed for DECLARE_INTERFACE and DEFINE_INTERFACE */
/* the following hack can be removed if the above FIXME has been resolved */
#ifdef __APPLE__
#undef main  
#endif


DECLARE_INTERFACE( IAny )
DEFINE_INTERFACE( IAny )


typedef struct {
     const char * (*GetType)();
     const char * (*GetImplementation)();
     DirectResult (*Allocate)( void **interface );
     DirectResult (*Probe)( void *ctx, ... );
     DirectResult (*Construct)( void *interface, ... );
} DirectInterfaceFuncs;

typedef DirectResult (*DirectInterfaceProbeFunc)( DirectInterfaceFuncs *impl, void *ctx );

/*
 * Loads an interface of a specific 'type'.
 * Optionally an 'implementation' can be chosen.
 * A 'probe' function can be used to check available implementations.
 *
 * After success 'funcs' is set.
 */
DirectResult DirectGetInterface( DirectInterfaceFuncs     **funcs,
                                 const char                *type,
                                 const char                *implementation,
                                 DirectInterfaceProbeFunc   probe,
                                 void                      *probe_ctx );

/*
 * Default probe function. Calls "funcs->Probe(ctx)".
 * Can be used as the 'probe' argument to DirectGetInterface.
 * 'probe_ctx' should then be set to the interface specific probe context.
 */
DirectResult DirectProbeInterface( DirectInterfaceFuncs *funcs, void *ctx );

/*
 * Called by implementation modules during 'dlopen'ing or at startup if linked
 * into the executable.
 */
void DirectRegisterInterface( DirectInterfaceFuncs *funcs );


void direct_print_interface_leaks();


#if DIRECT_BUILD_DEBUG
void direct_dbg_interface_add   ( const char *func,
                                  const char *file,
                                  int         line,
                                  const char *what,
                                  const void *interface,
                                  const char *name );

void direct_dbg_interface_remove( const char *func,
                                  const char *file,
                                  int         line,
                                  const char *what,
                                  const void *interface );
#else
#define direct_dbg_interface_add(func,file,line,what,interface,name)  do {} while (0)
#define direct_dbg_interface_remove(func,file,line,what,interface)    do {} while (0)
#endif


#define DIRECT_ALLOCATE_INTERFACE(p,i)                                               \
     do {                                                                            \
          (p) = D_CALLOC( 1, sizeof(i) );                                            \
                                                                                     \
          D_MAGIC_SET( (IAny*)(p), DirectInterface );                                \
                                                                                     \
          direct_dbg_interface_add( __FUNCTION__, __FILE__, __LINE__, #p, p, #i );   \
     } while (0)


#define DIRECT_ALLOCATE_INTERFACE_DATA(p,i)                                          \
     i##_data *data;                                                                 \
                                                                                     \
     D_MAGIC_ASSERT( (IAny*)(p), DirectInterface );                                  \
                                                                                     \
     if (!(p)->priv)                                                                 \
          (p)->priv = D_CALLOC( 1, sizeof(i##_data) );                               \
                                                                                     \
     data = (i##_data*)((p)->priv);


#define DIRECT_DEALLOCATE_INTERFACE(p)                                               \
     direct_dbg_interface_remove( __FUNCTION__, __FILE__, __LINE__, #p, p );         \
                                                                                     \
     if ((p)->priv) {                                                                \
          D_FREE( (p)->priv );                                                       \
          (p)->priv = NULL;                                                          \
     }                                                                               \
                                                                                     \
     D_MAGIC_CLEAR( (IAny*)(p) );                                                    \
                                                                                     \
     D_FREE( (p) );


#define DIRECT_INTERFACE_GET_DATA(i)                                                 \
     i##_data *data;                                                                 \
                                                                                     \
     if (!thiz)                                                                      \
          return DFB_THIZNULL;                                                       \
                                                                                     \
     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );                                 \
                                                                                     \
     data = (i##_data*) thiz->priv;                                                  \
                                                                                     \
     if (!data)                                                                      \
          return DFB_DEAD;


#define DIRECT_INTERFACE_GET_DATA_FROM(interface,data,prefix)                        \
     do {                                                                            \
          D_MAGIC_ASSERT( (IAny*)(interface), DirectInterface );                     \
                                                                                     \
          (data) = (prefix##_data*) (interface)->priv;                               \
                                                                                     \
          if (!(data))                                                               \
               return DFB_DEAD;                                                      \
     } while (0)


#endif

