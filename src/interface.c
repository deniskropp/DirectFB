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

#include <pthread.h>
#include <dirent.h>
#include <dlfcn.h>
#include <malloc.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#include <core/coredefs.h>

#include <misc/util.h>

#include <directfb.h>
#include <directfb_internals.h>

static pthread_mutex_t              implementations_mutex = PTHREAD_MUTEX_INITIALIZER;

static int                          n_implementations = 0;
static DFBInterfaceImplementation **implementations   = NULL;

DFBResult DFBGetInterface( DFBInterfaceImplementation **iimpl,
                           char *type,
                           char *implementation,
                           int (*probe)( DFBInterfaceImplementation *impl, void *ctx ),
                           void *probe_ctx )
{
     int                         i;
     DIR                        *dir;
     char                       *interface_dir = LIBDIR"/interfaces";
     struct dirent              *entry;
     DFBInterfaceImplementation *impl;

     pthread_mutex_lock( &implementations_mutex );

     for (i=0; i<n_implementations; i++) {
          if (type && strcmp( type, implementations[i]->type ))
               continue;

          if (implementation && strcmp( implementation,
                                        implementations[i]->implementation ))
               continue;

          if (probe && !probe( implementations[i], probe_ctx ))
               continue;
          else {
               *iimpl = implementations[i];
               implementations[i]->references++;

               pthread_mutex_unlock( &implementations_mutex );

               return DFB_OK;
          }
     }

     
     dir = opendir( interface_dir );

     if (!dir) {
          PERRORMSG( "DirectFB/interfaces: "
                     "Could not open interface directory `%s'!\n",
                     interface_dir );
          
          pthread_mutex_unlock( &implementations_mutex );
          
          return errno2dfb( errno );
     }

     impl = malloc( sizeof(DFBInterfaceImplementation) );
     memset( impl, 0, sizeof(DFBInterfaceImplementation) );

     while ( (entry = readdir(dir) ) != NULL ) {
          void *handle;
          char buf[4096];

          /* FIXME */
          if (entry->d_name[strlen(entry->d_name)-1] != 'o' ||
              entry->d_name[strlen(entry->d_name)-2] != 's')
               continue;

          sprintf( buf, "%s/%s", interface_dir, entry->d_name );

          for (i=0; i<n_implementations; i++) {
               if (!strcmp( implementations[i]->filename, buf ))
                    continue;
          }
          
          handle = dlopen( buf, RTLD_LAZY );
          if (handle) {
               char *(*get)() = NULL;

               get = dlsym( handle, "get_type"  );
               if (!get) {
                    DLERRORMSG( "DirectFB/interface: "
                                "Could not link `get_type' of `%s'!\n",
                                buf );
                    dlclose( handle );
                    continue;
               }
               impl->type = get();
               
               get = dlsym( handle, "get_implementation"  );
               if (!get) {
                    DLERRORMSG( "DirectFB/interface: "
                                "Could not link `get_implementation' of `%s'!\n",
                                buf );
                    dlclose( handle );
                    continue;
               }
               impl->implementation = get();

               impl->Probe = dlsym( handle, "Probe"  );
               if (!impl->Probe) {
                    DLERRORMSG( "DirectFB/interface: "
                                "Could not link `Probe' of `%s'!\n",
                                buf );
                    dlclose( handle );
                    continue;
               }
               
               impl->Construct = dlsym( handle, "Construct"  );
               if (!impl->Construct) {
                    DLERRORMSG( "DirectFB/interface: "
                                "Could not link `Construct' of `%s'!\n",
                                buf );
                    dlclose( handle );
                    continue;
               }
               
               HEAVYDEBUGMSG( "DirectFB/interface: Found `%s_%s'.\n",
                              impl->type, impl->implementation );
               
               if (type && strcmp( type, impl->type )) {
                    dlclose( handle );
                    continue;
               }

               if (implementation && strcmp( implementation,
                                             impl->implementation ))
               {
                    dlclose( handle );
                    continue;
               }

               if (probe && !probe( impl, probe_ctx )) {
                    dlclose( handle );
                    continue;
               }
               else {
                    *iimpl = impl;

                    impl->references = 1;
                    impl->filename = malloc( strlen(buf) + 1 );
                    strcpy( impl->filename, buf );

                    DEBUGMSG( "DirectFB/interface: "
                              "Adding %s to implementations.\n", buf);

                    implementations = realloc( implementations,
                     sizeof(DFBInterfaceImplementation) * ++n_implementations );

                    implementations[n_implementations-1] = impl;

                    pthread_mutex_unlock( &implementations_mutex );

                    return DFB_OK;
               }

               dlclose( handle );
          }
          else
               DLERRORMSG( "DirectFB/core/gfxcards: Unable to dlopen `%s'!\n",
                           buf );

     }

     closedir( dir );

     pthread_mutex_unlock( &implementations_mutex );

     free( impl );

     return DFB_NOIMPL;
}

