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

#include <config.h>

#include <direct/debug.h>
#include <direct/interface.h>
#include <direct/list.h>
#include <direct/interface.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/print.h>
#include <direct/thread.h>
#include <direct/trace.h>
#include <direct/util.h>

D_LOG_DOMAIN( Direct_Interface, "Direct/Interface", "Direct Interface" );

/**********************************************************************************************************************/

typedef struct {
     DirectLink            link;

     int                   magic;

     char                 *filename;
     void                 *module_handle;

     DirectInterfaceFuncs *funcs;

     const char           *type;
     const char           *implementation;

     int                   references;
} DirectInterfaceImplementation;

/**********************************************************************************************************************/

static DirectMutex  implementations_mutex;
static DirectLink  *implementations;

void
__D_interface_init()
{
     direct_recursive_mutex_init( &implementations_mutex );
}

void
__D_interface_deinit()
{
     direct_mutex_deinit( &implementations_mutex );
}

static inline int
probe_interface( DirectInterfaceImplementation  *impl,
                 DirectInterfaceFuncs          **funcs,
                 const char                     *type,
                 const char                     *implementation,
                 DirectInterfaceProbeFunc        probe,
                 void                           *probe_ctx )
{
     if (type && strcmp( type, impl->type ))
          return 0;

     if (implementation && strcmp( implementation, impl->implementation ))
          return 0;

     D_DEBUG_AT( Direct_Interface, "  -> Probing '%s'...\n", impl->implementation );

     if (probe && !probe( impl->funcs, probe_ctx ))
          return 0;

     *funcs = impl->funcs;
     impl->references++;

     return 1;
}

/**********************************************************************************************************************/

void
DirectRegisterInterface( DirectInterfaceFuncs *funcs )
{
     DirectInterfaceImplementation *impl;

     D_DEBUG_AT( Direct_Interface, "%s( %p )\n", __FUNCTION__, funcs );

     impl = (DirectInterfaceImplementation*) D_CALLOC( 1, sizeof(DirectInterfaceImplementation) );

     D_DEBUG_AT( Direct_Interface, "  -> %p\n", impl );

     impl->funcs          = funcs;
     impl->type           = funcs->GetType();
     impl->implementation = funcs->GetImplementation();

     D_MAGIC_SET( impl, DirectInterfaceImplementation );

     D_DEBUG_AT( Direct_Interface, "  -> %s | %s\n", impl->type, impl->implementation );

     direct_mutex_lock( &implementations_mutex );
     direct_list_prepend( &implementations, &impl->link );
     direct_mutex_unlock( &implementations_mutex );
}

void
DirectUnregisterInterface( DirectInterfaceFuncs *funcs )
{
     DirectInterfaceImplementation *impl;

     D_DEBUG_AT( Direct_Interface, "%s( %p )\n", __FUNCTION__, funcs );

     direct_mutex_lock( &implementations_mutex );

     direct_list_foreach (impl, implementations) {
          D_MAGIC_ASSERT( impl, DirectInterfaceImplementation );

          if (impl->funcs == funcs) {
               direct_list_remove( &implementations, &impl->link );

               break;
          }
     }

     direct_mutex_unlock( &implementations_mutex );

     if (!impl) {
          D_BUG( "implementation not found" );
          return;
     }

     D_DEBUG_AT( Direct_Interface, "  -> %s | %s\n", impl->type, impl->implementation );

     D_DEBUG_AT( Direct_Interface, "  -> %p\n", impl );

     D_MAGIC_CLEAR( impl );

     D_FREE( impl );
}

DirectResult
DirectProbeInterface( DirectInterfaceFuncs *funcs, void *ctx )
{
     return (funcs->Probe( ctx ) == DR_OK);
}

DirectResult
DirectGetInterface( DirectInterfaceFuncs     **funcs,
                    const char                *type,
                    const char                *implementation,
                    DirectInterfaceProbeFunc   probe,
                    void                      *probe_ctx )
{
     int                         n   = 0;
     int                         idx = -1;

#if DIRECT_BUILD_DYNLOAD
     int                         len;
     DIR                        *dir;
     char                       *interface_dir;
     struct dirent              *entry = NULL;
     struct dirent               tmp;
     const char                 *path;
#endif

     DirectLink *link;

     D_DEBUG_AT( Direct_Interface, "%s( %p, '%s', '%s', %p, %p )\n", __FUNCTION__,
                 funcs, type, implementation, probe, probe_ctx );

     direct_mutex_lock( &implementations_mutex );

     /* Check whether there is a default existing implementation set for the type in config */
     if (type && !implementation && direct_config->default_interface_implementation_types) {
          while (direct_config->default_interface_implementation_types[n]) {
               idx = -1;

               while (direct_config->default_interface_implementation_types[n]) {
                    if (!strcmp(direct_config->default_interface_implementation_types[n++], type)) {
                         idx = n - 1;
                         break;
                    }
               }

               if (idx < 0 && !direct_config->default_interface_implementation_types[n])
                    break;

               /* Check whether we have to check for a default implementation for the selected type */
               if (idx >= 0) {
                    direct_list_foreach( link, implementations ) {
                         DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

                         if (probe_interface( impl, funcs, NULL, direct_config->default_interface_implementation_names[idx], probe, probe_ctx )) {
                              D_INFO( "Direct/Interface: Using '%s' cached default implementation of '%s'.\n",
                                      impl->implementation, impl->type );

                              direct_mutex_unlock( &implementations_mutex );

                              return DR_OK;
                         }
                    }
               }
               else
                    break;
          }
     }
     else {
          /* Check existing implementations without default. */
          direct_list_foreach( link, implementations ) {
               DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

               if (probe_interface( impl, funcs, type, implementation, probe, probe_ctx )) {
                    if (impl->references == 1) {
                         D_INFO( "Direct/Interface: Using '%s' implementation of '%s'.\n",
                                 impl->implementation, impl->type );
                    }

                    direct_mutex_unlock( &implementations_mutex );

                    return DR_OK;
               }
          }
     }
#if DIRECT_BUILD_DYNLOAD
     /* Try to load it dynamically. */

     /* NULL type means we can't find plugin, so stop immediately */
     if (type == NULL) {
          direct_mutex_unlock( &implementations_mutex );
          return DR_NOIMPL;
     }

     path = direct_config->module_dir;
     if (!path)
          path = MODULEDIR;

     len = strlen(path) + strlen("/interfaces/") + strlen(type) + 1;
     interface_dir = alloca( len );
     direct_snprintf( interface_dir, len, "%s%sinterfaces/%s", path, (path[strlen(path)-1]=='/') ? "" : "/", type );

     dir = opendir( interface_dir );
     if (!dir) {
          D_DEBUG_LOG( Direct_Interface, 1, "Could not open interface directory '%s'!\n", interface_dir );
          direct_mutex_unlock( &implementations_mutex );
          return errno2result( errno );
     }

     if (direct_config->default_interface_implementation_types) {
          n = 0;

          while (direct_config->default_interface_implementation_types[n]) {
               idx = -1;

               while (direct_config->default_interface_implementation_types[n]) {
                    if (!strcmp(direct_config->default_interface_implementation_types[n++], type)) {
                         idx = n - 1;
                         break;
                    }
               }

               if (idx < 0 && !direct_config->default_interface_implementation_types[n])
                    break;

               /* Iterate directory. */
               while (idx >= 0 && readdir_r( dir, &tmp, &entry ) == 0 && entry) {
                    void *handle = NULL;
                    char  buf[4096];

                    DirectInterfaceImplementation *old_impl = (DirectInterfaceImplementation*) implementations;

                    if (strlen(entry->d_name) < 4 ||
                        entry->d_name[strlen(entry->d_name)-1] != 'o' ||
                        entry->d_name[strlen(entry->d_name)-2] != 's')
                         continue;

                    direct_snprintf( buf, 4096, "%s/%s", interface_dir, entry->d_name );

                    /* Check if it got already loaded. */
                    direct_list_foreach( link, implementations ) {
                         DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

                         if (impl->filename && !strcmp( impl->filename, buf )) {
                              handle = impl->module_handle;
                              break;
                         }
                    }

                    /* Open it if needed and check. */
                    if (!handle) {
                         handle = dlopen( buf, RTLD_NOW );

                         /* Check if it registered itself. */
                         if (handle) {
                              DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) implementations;

                              if (old_impl == impl) {
                                   dlclose( handle );
                                   continue;
                              }

                              /* Keep filename and module handle. */
                              impl->filename      = D_STRDUP( buf );
                              impl->module_handle = handle;
                         }
                    }

                    if (handle) {
                         DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) implementations;

                         /* check whether the dlopen'ed interface supports the required implementation */
                         if (!strcmp( impl->implementation, direct_config->default_interface_implementation_names[idx] )) {
                              if (probe_interface( impl, funcs, type, direct_config->default_interface_implementation_names[idx], probe, probe_ctx )) {
                                   /* Keep filename and module handle. */
                                   impl->filename      = D_STRDUP( buf );
                                   impl->module_handle = handle;

                                   D_INFO( "Direct/Interface: Loaded '%s' implementation of '%s'.\n",
                                           impl->implementation, impl->type );

                                   closedir( dir );

                                   direct_mutex_unlock( &implementations_mutex );

                                   return DR_OK;
                              }
                              else
                                   continue;
                         }
                         else
                              continue;
                    }
                    else
                         D_DLERROR( "Direct/Interface: Unable to dlopen `%s'!\n", buf );
               }

               rewinddir( dir );
          }
     }

     /* Iterate directory. */
     while (readdir_r( dir, &tmp, &entry ) == 0 && entry) {
          void *handle = NULL;
          char  buf[4096];

          DirectInterfaceImplementation *old_impl = (DirectInterfaceImplementation*) implementations;

          if (strlen(entry->d_name) < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          direct_snprintf( buf, 4096, "%s/%s", interface_dir, entry->d_name );

          /* Check if it got already loaded. */
          direct_list_foreach( link, implementations ) {
               DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) link;

               if (impl->filename && !strcmp( impl->filename, buf )) {
                   handle = impl->module_handle;
                   break;
               }
          }

          /* Open it if needed and check. */
          if (!handle) {
               handle = dlopen( buf, RTLD_NOW );

               /* Check if it registered itself. */
               if (handle) {
                    DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) implementations;

                    if (old_impl == impl) {
                         dlclose( handle );
                         continue;
                    }

                    /* Keep filename and module handle. */
                    impl->filename      = D_STRDUP( buf );
                    impl->module_handle = handle;
               }
          }

          if (handle) {
               DirectInterfaceImplementation *impl = (DirectInterfaceImplementation*) implementations;

               if (probe_interface( impl, funcs, type, implementation, probe, probe_ctx )) {
                    D_INFO( "Direct/Interface: Loaded '%s' implementation of '%s'.\n",
                            impl->implementation, impl->type );

                    closedir( dir );

                    direct_mutex_unlock( &implementations_mutex );

                    return DR_OK;
               }
               else
                    continue;
          }
          else
               D_DLERROR( "Direct/Interface: Unable to dlopen `%s'!\n", buf );
     }

     closedir( dir );
#endif

     direct_mutex_unlock( &implementations_mutex );

     return DR_NOIMPL;
}

/**************************************************************************************************/

#if DIRECT_BUILD_DEBUGS  /* Build with debug support? */

typedef struct {
     const void        *interface_ptr;
     char              *name;
     char              *what;

     const char        *func;
     const char        *file;
     int                line;

     DirectTraceBuffer *trace;
} InterfaceDesc;

static int            alloc_count;
static int            alloc_capacity;
static InterfaceDesc *alloc_list;
static DirectMutex    alloc_lock;

void
__D_interface_dbg_init()
{
     direct_mutex_init( &alloc_lock );
}

void
__D_interface_dbg_deinit()
{
     direct_mutex_deinit( &alloc_lock );
}

/**************************************************************************************************/

void
direct_print_interface_leaks( void )
{
     int i;

     direct_mutex_lock( &alloc_lock );

     if (alloc_count /*&& (!direct_config || direct_config->debug)*/) {
          direct_log_printf( NULL, "Interface instances remaining (%d): \n", alloc_count );

          for (i=0; i<alloc_count; i++) {
               InterfaceDesc *desc = &alloc_list[i];

               direct_log_printf( NULL, "  - '%s' at %p (%s) allocated in %s (%s: %u)\n", desc->name,
                        desc->interface_ptr, desc->what, desc->func, desc->file, desc->line );

               if (desc->trace)
                    direct_trace_print_stack( desc->trace );
          }
     }

     direct_mutex_unlock( &alloc_lock );
}

/**************************************************************************************************/

__no_instrument_function__
static InterfaceDesc *
allocate_interface_desc( void )
{
     int cap = alloc_capacity;

     if (!cap)
          cap = 64;
     else if (cap == alloc_count)
          cap <<= 1;

     if (cap != alloc_capacity) {
          alloc_capacity = cap;
          alloc_list     = direct_realloc( alloc_list, sizeof(InterfaceDesc) * cap );

          D_ASSERT( alloc_list != NULL );
     }

     return &alloc_list[alloc_count++];
}

__no_instrument_function__
static __inline__ void
fill_interface_desc( InterfaceDesc     *desc,
                     const void        *interface_ptr,
                     const char        *name,
                     const char        *func,
                     const char        *file,
                     int                line,
                     const char        *what,
                     DirectTraceBuffer *trace )
{
     desc->interface_ptr = interface_ptr;
     desc->name          = direct_strdup( name );
     desc->what          = direct_strdup( what );
     desc->func          = func;
     desc->file          = file;
     desc->line          = line;
     desc->trace         = trace;
}

/**************************************************************************************************/

__no_instrument_function__
void
direct_dbg_interface_add( const char *func,
                          const char *file,
                          int         line,
                          const char *what,
                          const void *interface_ptr,
                          const char *name )
{
     InterfaceDesc *desc;

     direct_mutex_lock( &alloc_lock );

     desc = allocate_interface_desc();

     fill_interface_desc( desc, interface_ptr, name,
                          func, file, line, what, direct_trace_copy_buffer(NULL) );

     direct_mutex_unlock( &alloc_lock );
}

__no_instrument_function__
void
direct_dbg_interface_remove( const char *func,
                             const char *file,
                             int         line,
                             const char *what,
                             const void *interface_ptr )
{
     int i;

     direct_mutex_lock( &alloc_lock );

     for (i=0; i<alloc_count; i++) {
          InterfaceDesc *desc = &alloc_list[i];

          if (desc->interface_ptr == interface_ptr) {
               if (desc->trace)
                    direct_trace_free_buffer( desc->trace );

               direct_free( desc->what );
               direct_free( desc->name );

               if (i < --alloc_count)
                    direct_memmove( desc, desc + 1, (alloc_count - i) * sizeof(InterfaceDesc) );

               direct_mutex_unlock( &alloc_lock );

               return;
          }
     }

     direct_mutex_unlock( &alloc_lock );

     D_ERROR( "Direct/Interface: unknown instance %p (%s) from [%s:%d in %s()]\n",
              interface_ptr, what, file, line, func );
     D_BREAK( "unknown instance" );
}

#else     /* DIRECT_BUILD_DEBUG */

void
__D_interface_dbg_init()
{
}

void
__D_interface_dbg_deinit()
{
}

void
direct_print_interface_leaks( void )
{
}

#endif    /* DIRECT_BUILD_DEBUG */

