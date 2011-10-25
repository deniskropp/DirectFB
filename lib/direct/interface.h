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

#ifndef __DIRECT__INTERFACE_H__
#define __DIRECT__INTERFACE_H__

#include <direct/debug.h>
#include <direct/mem.h>

/*
 * Forward declaration macro for interfaces.
 */
#define D_DECLARE_INTERFACE( IFACE )              \
     typedef struct _ ## IFACE IFACE;

/*
 * Macro for an interface definition.
 */
#define D_DEFINE_INTERFACE( IFACE, ... )          \
     struct _ ## IFACE {                          \
          void          *priv;                    \
          int            magic;                   \
          int            refs;                    \
                                                  \
          DirectResult (*AddRef)( IFACE *thiz );  \
          DirectResult (*Release)( IFACE *thiz ); \
                                                  \
          __VA_ARGS__                             \
     };


#ifndef DIRECT_DISABLE_DEPRECATED

// @deprecated
#define DECLARE_INTERFACE D_DECLARE_INTERFACE

// @deprecated
#define DEFINE_INTERFACE  D_DEFINE_INTERFACE

#endif


/*
 * Declare base interface
 */
D_DECLARE_INTERFACE( IAny )

/*
 * Define base interface
 */
D_DEFINE_INTERFACE( IAny, )

/*
 * Function type for probing of interface implementations
 */
typedef DirectResult (*DirectInterfaceGenericProbeFunc)( void *ctx, ... );

/*
 * Function type for initialization of interface instances
 */
typedef DirectResult (*DirectInterfaceGenericConstructFunc)( void *interface_ptr, ... );

/*
 * Function table for interface implementations
 */
typedef struct {
     const char * (*GetType)(void);
     const char * (*GetImplementation)(void);
     DirectResult (*Allocate)( void **interface_ptr );
     DirectResult (*Deallocate)( void *interface_ptr );
     
     DirectInterfaceGenericProbeFunc     Probe;
     DirectInterfaceGenericConstructFunc Construct;
} DirectInterfaceFuncs;

/*
 * Callback type for user probing interface implementations
 */
typedef DirectResult (*DirectInterfaceProbeFunc)( DirectInterfaceFuncs *impl, void *ctx );

/*
 * Loads an interface of a specific 'type'.
 * Optionally an 'implementation' can be chosen.
 * A 'probe' function can be used to check available implementations.
 *
 * After success 'funcs' is set.
 */
DirectResult DIRECT_API DirectGetInterface( DirectInterfaceFuncs     **funcs,
                                            const char                *type,
                                            const char                *implementation,
                                            DirectInterfaceProbeFunc   probe,
                                            void                      *probe_ctx );

/*
 * Default probe function. Calls "funcs->Probe(ctx)".
 * Can be used as the 'probe' argument to DirectGetInterface.
 * 'probe_ctx' should then be set to the interface specific probe context.
 */
DirectResult DIRECT_API DirectProbeInterface( DirectInterfaceFuncs *funcs, void *ctx );

/*
 * Called by implementation modules during 'dlopen'ing or at startup if linked
 * into the executable.
 */
void DIRECT_API DirectRegisterInterface( DirectInterfaceFuncs *funcs );

void DIRECT_API DirectUnregisterInterface( DirectInterfaceFuncs *funcs );

void DIRECT_API direct_print_interface_leaks(void);

#if DIRECT_BUILD_DEBUGS
void DIRECT_API direct_dbg_interface_add   ( const char *func,
                                             const char *file,
                                             int         line,
                                             const char *what,
                                             const void *interface_ptr,
                                             const char *name );

void DIRECT_API direct_dbg_interface_remove( const char *func,
                                             const char *file,
                                             int         line,
                                             const char *what,
                                             const void *interface_ptr );
#endif

#if DIRECT_BUILD_DEBUG || defined(DIRECT_ENABLE_DEBUG) || defined(DIRECT_FORCE_DEBUG)

#if !DIRECT_BUILD_DEBUGS
#error Building with debug, but library headers suggest that debug is not supported.
#endif

#define DIRECT_DBG_INTERFACE_ADD        direct_dbg_interface_add
#define DIRECT_DBG_INTERFACE_REMOVE     direct_dbg_interface_remove

#else

#define DIRECT_DBG_INTERFACE_ADD(func,file,line,what,interface,name)  do {} while (0)
#define DIRECT_DBG_INTERFACE_REMOVE(func,file,line,what,interface)    do {} while (0)

#endif



#define DIRECT_ALLOCATE_INTERFACE(p,i)                                                    \
     do {                                                                                 \
          (p) = (__typeof__(p))D_CALLOC( 1, sizeof(i) );                                  \
          if (p) {                                                                        \
               D_MAGIC_SET( (IAny*)(p), DirectInterface );                                \
                                                                                          \
               DIRECT_DBG_INTERFACE_ADD( __FUNCTION__, __FILE__, __LINE__, #p, p, #i );   \
          }                                                                               \
          else                                                                            \
               D_OOM();                                                                   \
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
     D_MAGIC_ASSERT( (IAny*)(p), DirectInterface );                                  \
                                                                                     \
     DIRECT_DBG_INTERFACE_REMOVE( __FUNCTION__, __FILE__, __LINE__, #p, p );         \
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
          return DR_THIZNULL;                                                        \
                                                                                     \
     D_MAGIC_ASSERT( (IAny*)thiz, DirectInterface );                                 \
                                                                                     \
     data = (i##_data*) thiz->priv;                                                  \
                                                                                     \
     if (!data)                                                                      \
          return DR_DEAD;


#define DIRECT_INTERFACE_GET_DATA_FROM(interface,data,prefix)                        \
     do {                                                                            \
          D_MAGIC_ASSERT( (IAny*)(interface), DirectInterface );                     \
                                                                                     \
          (data) = (prefix##_data*) (interface)->priv;                               \
                                                                                     \
          if (!(data))                                                               \
               return DR_DEAD;                                                       \
     } while (0)



void __D_interface_init( void );
void __D_interface_deinit( void );

void __D_interface_dbg_init( void );
void __D_interface_dbg_deinit( void );

#endif

