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

#include <config.h>

#include <pthread.h>
#include <dirent.h>
#ifndef USE_KOS
#include <dlfcn.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/trace.h>
#include <direct/util.h>

#ifdef PIC
#define DYNAMIC_LINKING
#endif

typedef struct {
     DirectLink            link;

     char                 *filename;
     void                 *module_handle;

     DirectInterfaceFuncs *funcs;

     const char           *type;
     const char           *implementation;

     int                   references;
} DirectInterfaceImplementation;

static pthread_mutex_t  implementations_mutex = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;
static DirectLink      *implementations       = NULL;

/**************************************************************************************************/

void
DirectRegisterInterface( DirectInterfaceFuncs *funcs )
{
     DirectInterfaceImplementation *impl;

     impl = calloc( 1, sizeof(DirectInterfaceImplementation) );

     impl->funcs          = funcs;
     impl->type           = funcs->GetType();
     impl->implementation = funcs->GetImplementation();

     direct_list_prepend( &implementations, &impl->link );
}

DirectResult
DirectProbeInterface( DirectInterfaceFuncs *funcs, void *ctx )
{
     return (funcs->Probe( ctx ) == DFB_OK);
}

DirectResult
DirectGetInterface( DirectInterfaceFuncs     **funcs,
                    const char                *type,
                    const char                *implementation,
                    DirectInterfaceProbeFunc   probe,
                    void                      *probe_ctx )
{
#ifdef DYNAMIC_LINKING
     int                         len;
     DIR                        *dir;
     char                       *interface_dir;
     struct dirent              *entry;
#endif

     DirectLink *link;

     pthread_mutex_lock( &implementations_mutex );

     /*
      * Check existing implementations first.
      */
     direct_list_foreach( link, implementations ) {
          DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

          if (type && strcmp( type, impl->type ))
               continue;

          if (implementation && strcmp( implementation, impl->implementation ))
               continue;

          if (probe && !probe( impl->funcs, probe_ctx ))
               continue;
          else {
               if (!impl->references) {
                    D_INFO( "Direct/Interface: Using '%s' implementation of '%s'.\n",
                            impl->implementation, impl->type );
               }

               *funcs = impl->funcs;
               impl->references++;

               pthread_mutex_unlock( &implementations_mutex );

               return DFB_OK;
          }
     }

#ifdef DYNAMIC_LINKING
     /*
      * Try to load it dynamically.
      */
     len = strlen(MODULEDIR"/interfaces/") + strlen(type) + 1;
     interface_dir = alloca( len );
     snprintf( interface_dir, len, MODULEDIR"/interfaces/%s", type );

     dir = opendir( interface_dir );
     if (!dir) {
          D_PERROR( "Direct/Interface: Could not open interface directory `%s'!\n", interface_dir );

          pthread_mutex_unlock( &implementations_mutex );

          return errno2result( errno );
     }

     /*
      * Iterate directory.
      */
     while ( (entry = readdir(dir) ) != NULL ) {
          void *handle = NULL;
          char  buf[4096];

          DirectInterfaceImplementation *old_impl = (DirectInterfaceImplementation*) implementations;

          if (strlen(entry->d_name) < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          snprintf( buf, 4096, "%s/%s", interface_dir, entry->d_name );

          /*
           * Check if it got already loaded.
           */
          direct_list_foreach( link, implementations ) {
               DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

               if (impl->filename && !strcmp( impl->filename, buf )) {
                    handle = impl->module_handle;
                    break;
               }
          }

          /*
           * If already loaded take the next one.
           */
          if (handle)
               continue;

          /*
           * Open it and check.
           */
#ifdef RTLD_GLOBAL
          handle = dlopen( buf, RTLD_NOW /*| RTLD_GLOBAL*/ );
#else
          /* RTLD_GLOBAL is missing on OpenBSD*/
          handle = dlopen( buf, RTLD_NOW );
#endif
          if (handle) {
               DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) implementations;

               /*
                * Check if it registered itself.
                */
               if (impl == old_impl) {
                    dlclose( handle );
                    continue;
               }

               D_HEAVYDEBUG( "Direct/Interface: Found `%s_%s'.\n", impl->type, impl->implementation );

               /*
                * Keep filename and module handle.
                */
               impl->filename      = D_STRDUP( buf );
               impl->module_handle = handle;

               /*
                * Almost the same stuff like above, TODO: make function.
                */
               if (type && strcmp( type, impl->type ))
                    continue;

               if (implementation && strcmp( implementation,
                                             impl->implementation ))
                    continue;

               if (probe && !probe( impl->funcs, probe_ctx )) {
                    continue;
               }
               else {
                    D_INFO( "Direct/Interface: Loaded '%s' implementation of '%s'.\n",
                            impl->implementation, impl->type );

                    *funcs = impl->funcs;
                    impl->references++;

                    closedir( dir );

                    pthread_mutex_unlock( &implementations_mutex );

                    return DFB_OK;
               }
          }
          else
               D_DLERROR( "Direct/Interface: Unable to dlopen `%s'!\n", buf );
     }

     closedir( dir );

     pthread_mutex_unlock( &implementations_mutex );
#endif

     return DFB_NOIMPL;
}

/**************************************************************************************************/

#if DIRECT_BUILD_DEBUG

typedef struct {
     const void        *interface;
     char              *name;
     char              *what;

     const char        *func;
     const char        *file;
     int                line;

     DirectTraceBuffer *trace;
} InterfaceDesc;

static int              alloc_count    = 0;
static int              alloc_capacity = 0;
static InterfaceDesc   *alloc_list     = NULL;
static pthread_mutex_t  alloc_lock     = DIRECT_UTIL_RECURSIVE_PTHREAD_MUTEX_INITIALIZER;

/**************************************************************************************************/

void
direct_print_interface_leaks()
{
     unsigned int i;

     pthread_mutex_lock( &alloc_lock );

     if (alloc_count && (!direct_config || direct_config->debug)) {
          D_DEBUG( "Interface instances remaining (%d): \n", alloc_count );

          for (i=0; i<alloc_count; i++) {
               InterfaceDesc *desc = &alloc_list[i];

               D_DEBUG( "  - '%s' at %p (%s) allocated in %s (%s: %u)\n", desc->name,
                        desc->interface, desc->what, desc->func, desc->file, desc->line );

               if (desc->trace)
                    direct_trace_print_stack( desc->trace );
          }
     }

     pthread_mutex_unlock( &alloc_lock );
}

/**************************************************************************************************/

static InterfaceDesc *
allocate_interface_desc()
{
     int cap = alloc_capacity;

     if (!cap)
          cap = 64;
     else if (cap == alloc_count)
          cap <<= 1;

     if (cap != alloc_capacity) {
          alloc_capacity = cap;
          alloc_list     = realloc( alloc_list, sizeof(InterfaceDesc) * cap );

          D_ASSERT( alloc_list != NULL );
     }

     return &alloc_list[alloc_count++];
}

static inline void
fill_interface_desc( InterfaceDesc     *desc,
                     const void        *interface,
                     const char        *name,
                     const char        *func,
                     const char        *file,
                     int                line,
                     const char        *what,
                     DirectTraceBuffer *trace )
{
     desc->interface = interface;
     desc->name      = strdup( name );
     desc->what      = strdup( what );
     desc->func      = func;
     desc->file      = file;
     desc->line      = line;
     desc->trace     = trace;
}

/**************************************************************************************************/

__attribute__((no_instrument_function))
void
direct_dbg_interface_add( const char *func,
                          const char *file,
                          int         line,
                          const char *what,
                          const void *interface,
                          const char *name )
{
     InterfaceDesc *desc;

     pthread_mutex_lock( &alloc_lock );

     desc = allocate_interface_desc();

     fill_interface_desc( desc, interface, name,
                          func, file, line, what, direct_trace_copy_buffer(NULL) );

     pthread_mutex_unlock( &alloc_lock );
}

__attribute__((no_instrument_function))
void
direct_dbg_interface_remove( const char *func,
                             const char *file,
                             int         line,
                             const char *what,
                             const void *interface )
{
     unsigned int i;

     pthread_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          InterfaceDesc *desc = &alloc_list[i];

          if (desc->interface == interface) {
               if (desc->trace)
                    direct_trace_free_buffer( desc->trace );

               free( desc->what );
               free( desc->name );

               if (i < --alloc_count)
                    direct_memcpy( desc, desc + 1, (alloc_count - i) * sizeof(InterfaceDesc) );

               pthread_mutex_unlock( &alloc_lock );

               return;
          }
     }

     pthread_mutex_unlock( &alloc_lock );

     D_ERROR( "Direct/Interface: unknown instance %p (%s) from [%s:%d in %s()]\n",
              interface, what, file, line, func );
     D_BREAK( "unknown instance" );
}

#else     /* DIRECT_BUILD_DEBUG */

void
direct_print_interface_leaks()
{
}

#endif    /* DIRECT_BUILD_DEBUG */

