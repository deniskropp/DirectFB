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
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <directfb.h>
#include <interface.h>

#include <core/coredefs.h>
#include <direct/list.h>

#include <direct/mem.h>
#include <direct/messages.h>

#include <misc/util.h>

typedef struct {
     DirectLink         link;

     char              *filename;
     void              *module_handle;

     DFBInterfaceFuncs *funcs;

     const char        *type;
     const char        *implementation;

     int                references;
} DFBInterfaceImplementation;

static pthread_mutex_t  implementations_mutex = PTHREAD_MUTEX_INITIALIZER;
static DirectLink      *implementations       = NULL;

void DFBRegisterInterface( DFBInterfaceFuncs *funcs )
{
     DFBInterfaceImplementation *impl;

     impl = calloc( 1, sizeof(DFBInterfaceImplementation) );

     impl->funcs          = funcs;
     impl->type           = funcs->GetType();
     impl->implementation = funcs->GetImplementation();

     direct_list_prepend( &implementations, &impl->link );
}

int DFBProbeInterface( DFBInterfaceFuncs *funcs, void *ctx )
{
     return (funcs->Probe( ctx ) == DFB_OK);
}

DFBResult DFBGetInterface( DFBInterfaceFuncs **funcs,
                           const char *type,
                           const char *implementation,
                           int (*probe)( DFBInterfaceFuncs *funcs, void *ctx ),
                           void *probe_ctx )
{
#ifdef DFB_DYNAMIC_LINKING
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
          DFBInterfaceImplementation *impl = (DFBInterfaceImplementation*) link;

          if (type && strcmp( type, impl->type ))
               continue;

          if (implementation && strcmp( implementation, impl->implementation ))
               continue;

          if (probe && !probe( impl->funcs, probe_ctx ))
               continue;
          else {
               if (!impl->references) {
                    D_INFO( "DirectFB/Interface: Using '%s' implementation of '%s'.\n",
                            impl->implementation, impl->type );
               }

               *funcs = impl->funcs;
               impl->references++;

               pthread_mutex_unlock( &implementations_mutex );

               return DFB_OK;
          }
     }

#ifdef DFB_DYNAMIC_LINKING
     /*
      * Try to load it dynamically.
      */
     len = strlen(MODULEDIR"/interfaces/") + strlen(type) + 1;
     interface_dir = alloca( len );
     snprintf( interface_dir, len, MODULEDIR"/interfaces/%s", type );

     dir = opendir( interface_dir );
     if (!dir) {
          D_PERROR( "DirectFB/interfaces: "
                     "Could not open interface directory `%s'!\n",
                     interface_dir );

          pthread_mutex_unlock( &implementations_mutex );

          return errno2dfb( errno );
     }

     /*
      * Iterate directory.
      */
     while ( (entry = readdir(dir) ) != NULL ) {
          void *handle = NULL;
          char  buf[4096];

          DFBInterfaceImplementation *old_impl =
               (DFBInterfaceImplementation*) implementations;

          if (strlen(entry->d_name) < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          snprintf( buf, 4096, "%s/%s", interface_dir, entry->d_name );

          /*
           * Check if it got already loaded.
           */
          direct_list_foreach( link, implementations ) {
               DFBInterfaceImplementation *impl =
                    (DFBInterfaceImplementation*) link;

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
          handle = dlopen( buf, RTLD_LAZY | RTLD_GLOBAL );
#else
          /* RTLD_GLOBAL is missing on OpenBSD*/
          handle = dlopen( buf, RTLD_LAZY );
#endif
          if (handle) {
               DFBInterfaceImplementation *impl =
                    (DFBInterfaceImplementation*) implementations;

               /*
                * Check if it registered itself.
                */
               if (impl == old_impl) {
                    dlclose( handle );
                    continue;
               }

               D_HEAVYDEBUG( "DirectFB/interface: Found `%s_%s'.\n",
                              impl->type, impl->implementation );

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
                    D_INFO( "DirectFB/Interface: Loaded '%s' implementation of '%s'.\n",
                            impl->implementation, impl->type );

                    *funcs = impl->funcs;
                    impl->references++;

                    closedir( dir );

                    pthread_mutex_unlock( &implementations_mutex );

                    return DFB_OK;
               }
          }
          else
               D_DLERROR( "DirectFB/core/gfxcards: Unable to dlopen `%s'!\n",
                           buf );

     }

     closedir( dir );

     pthread_mutex_unlock( &implementations_mutex );
#endif

     return DFB_NOIMPL;
}

