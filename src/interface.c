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

#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <directfb.h>
#include <directfb_internals.h>

#include <core/coredefs.h>
#include <core/fusion/list.h>

#include <misc/mem.h>
#include <misc/util.h>

typedef struct {
     FusionLink         link;

     char              *filename;
     void              *module_handle;

     DFBInterfaceFuncs *funcs;
     
     const char        *type;
     const char        *implementation;

     int                references;
} DFBInterfaceImplementation;

static pthread_mutex_t  implementations_mutex = PTHREAD_MUTEX_INITIALIZER;
static FusionLink      *implementations       = NULL;

void DFBRegisterInterface( DFBInterfaceFuncs *funcs )
{
     DFBInterfaceImplementation *impl;

     impl = DFBCALLOC( 1, sizeof(DFBInterfaceImplementation) );

     impl->funcs          = funcs;
     impl->type           = funcs->GetType();
     impl->implementation = funcs->GetImplementation();

     fusion_list_prepend( &implementations, &impl->link );
}

int DFBProbeInterface( DFBInterfaceFuncs *funcs, void *ctx )
{
     return (funcs->Probe( ctx ) == DFB_OK);
}

DFBResult DFBGetInterface( DFBInterfaceFuncs **funcs,
                           char *type,
                           char *implementation,
                           int (*probe)( DFBInterfaceFuncs *funcs, void *ctx ),
                           void *probe_ctx )
{
#ifdef DFB_DYNAMIC_LINKING
     int                         len;
     DIR                        *dir;
     char                       *interface_dir;
     struct dirent              *entry;
#endif
     
     FusionLink *link;

     pthread_mutex_lock( &implementations_mutex );

     /*
      * Check existing implementations first.
      */
     fusion_list_foreach( link, implementations ) {
          DFBInterfaceImplementation *impl = (DFBInterfaceImplementation*) link;

          if (type && strcmp( type, impl->type ))
               continue;

          if (implementation && strcmp( implementation, impl->implementation ))
               continue;

          if (probe && !probe( impl->funcs, probe_ctx ))
               continue;
          else {
               if (!impl->references) {
                    INITMSG( "DirectFB/Interface: "
                             "Using '%s' implementation of '%s'.\n",
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
          PERRORMSG( "DirectFB/interfaces: "
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

          if (strlen(entry->d_name) < 4 ||
              entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          snprintf( buf, 4096, "%s/%s", interface_dir, entry->d_name );

          /*
           * Check if it got already loaded.
           */
          fusion_list_foreach( link, implementations ) {
               DFBInterfaceImplementation *impl =
                    (DFBInterfaceImplementation*) link;
          
               if (!strcmp( impl->filename, buf )) {
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
          handle = dlopen( buf, RTLD_LAZY | RTLD_GLOBAL );
          if (handle) {
               DFBInterfaceImplementation *impl =
                    (DFBInterfaceImplementation*) implementations;

               /*
                * If module handle is set, it's not the newly opened module.
                */
               if (!impl || impl->module_handle) {
                    dlclose( handle );
                    continue;
               }
               
               HEAVYDEBUGMSG( "DirectFB/interface: Found `%s_%s'.\n",
                              impl->type, impl->implementation );

               /*
                * Keep filename and module handle.
                */
               impl->filename      = DFBSTRDUP( buf );
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
                    INITMSG( "DirectFB/Interface: "
                             "Loaded '%s' implementation of '%s'.\n",
                             impl->implementation, impl->type );
                    
                    *funcs = impl->funcs;
                    impl->references++;

                    pthread_mutex_unlock( &implementations_mutex );

                    return DFB_OK;
               }
          }
          else
               DLERRORMSG( "DirectFB/core/gfxcards: Unable to dlopen `%s'!\n",
                           buf );

     }

     closedir( dir );

     pthread_mutex_unlock( &implementations_mutex );
#endif

     return DFB_NOIMPL;
}

