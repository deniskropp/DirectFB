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

#include <sys/types.h>
#include <dirent.h>
#include <string.h>
#include <stdio.h>

#include <core/coredefs.h>
#include <core/modules.h>

#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>

#include <misc/conf.h>

#ifdef DFB_DYNAMIC_LINKING
#include <dlfcn.h>
#endif

/******************************************************************************/

#ifdef DFB_DYNAMIC_LINKING

static ModuleEntry *lookup_by_name( const ModuleDirectory *directory,
                                    const char            *name );

static ModuleEntry *lookup_by_file( const ModuleDirectory *directory,
                                    const char            *file );

static void *open_module  ( ModuleEntry *module );
static bool  load_module  ( ModuleEntry *module );
static void  unload_module( ModuleEntry *module );

#endif

/******************************************************************************/

static
int suppress_module (const char *name)
{
     int i = 0;

     if (!dfb_config || !dfb_config->disable_module)
          return 0;

     while (dfb_config->disable_module[i]) {
          if (strcmp (dfb_config->disable_module[i], name) == 0) {
               D_INFO( "DirectFB/Modules: suppress module '%s'\n", dfb_config->disable_module[i] );
               return 1;
	  }

	  i++;
     }

     return 0;
}

void
dfb_modules_register( ModuleDirectory *directory,
                      unsigned int     abi_version,
                      const char      *name,
                      const void      *funcs )
{
     ModuleEntry *entry;

     D_ASSERT( directory != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( funcs != NULL );

     D_DEBUG( "DirectFB/core/modules: Registering '%s' ('%s')\n",
               name, directory->path );

#ifdef DFB_DYNAMIC_LINKING
     if ((entry = lookup_by_name( directory, name )) != NULL) {
          entry->loaded = true;
          entry->funcs  = funcs;

          return;
     }
#endif

     if (directory->loading)
          entry = directory->loading;
     else if (! (entry = D_CALLOC( 1, sizeof(ModuleEntry) )))
          return;

     entry->directory = directory;
     entry->loaded    = true;
     entry->name      = D_STRDUP( name );
     entry->funcs     = funcs;

     entry->disabled  = suppress_module( name );

     if (abi_version != directory->abi_version) {
          D_ERROR( "DirectFB/core/modules: "
                    "ABI version of '%s' (%d) does not match %d!\n",
                    entry->file, abi_version, directory->abi_version );

          entry->disabled = true;
     }

     direct_list_prepend( &directory->entries, &entry->link );

     D_DEBUG( "DirectFB/core/modules: ...registered.\n" );
}

int
dfb_modules_explore_directory( ModuleDirectory *directory )
{
#ifdef DFB_DYNAMIC_LINKING
     int            dir_len;
     DIR           *dir;
     struct dirent *entry;
     int            count = 0;

     D_ASSERT( directory != NULL );
     D_ASSERT( directory->path != NULL );

     dir_len = strlen( directory->path );
     dir     = opendir( directory->path );

     if (!dir) {
          D_PERROR( "DirectFB/core/modules: Could not open "
                     "module directory `%s'!\n", directory->path );
          return 0;
     }

     while ((entry = readdir( dir )) != NULL) {
          void        *handle;
          ModuleEntry *module;
          int          entry_len = strlen(entry->d_name);

          if (entry_len < 4 ||
              entry->d_name[entry_len-1] != 'o' ||
              entry->d_name[entry_len-2] != 's')
               continue;

          if (lookup_by_file( directory, entry->d_name ))
               continue;


          module = D_CALLOC( 1, sizeof(ModuleEntry) );
          if (!module)
               continue;

          module->directory = directory;
          module->dynamic   = true;
          module->file      = D_STRDUP( entry->d_name );


          directory->loading = module;

          if ((handle = open_module( module )) != NULL) {
               if (!module->loaded) {
                    int    len;
                    void (*func)();

                    D_ERROR( "DirectFB/core/modules: Module '%s' did not "
                              "register itself after loading! Trying default "
                              "module constructor...\n", entry->d_name );

                    len = strlen( entry->d_name );

                    entry->d_name[len-3] = 0;

                    func = dlsym( handle, entry->d_name + 3 );
                    if (func) {
                         func();

                         if (!module->loaded) {
                              D_ERROR( "DirectFB/core/modules: ... even "
                                        "did not register after explicitly "
                                        "calling the module constructor!\n" );
                         }
                    }
                    else {
                         D_ERROR( "DirectFB/core/modules: ... "
                                   "default contructor not found!\n" );
                    }

                    if (!module->loaded) {
                         module->disabled = true;

                         direct_list_prepend( &directory->entries,
                                              &module->link );
                    }
               }

               if (module->disabled) {
                    dlclose( handle );

                    module->loaded = false;
               }
               else {
                    module->handle = handle;

                    count++;
               }
          }
          else {
               module->disabled = true;

               direct_list_prepend( &directory->entries, &module->link );
          }

          directory->loading = NULL;
     }

     closedir( dir );

     return count;
#else
     return 0;
#endif
}

const void *
dfb_module_ref( ModuleEntry *module )
{
     D_ASSERT( module != NULL );

     if (module->disabled)
          return NULL;

#ifdef DFB_DYNAMIC_LINKING
     if (!module->loaded && !load_module( module ))
          return NULL;
#endif

     module->refs++;

     return module->funcs;
}

void
dfb_module_unref( ModuleEntry *module )
{
     D_ASSERT( module != NULL );
     D_ASSERT( module->refs > 0 );

     if (--module->refs)
          return;

#ifdef DFB_DYNAMIC_LINKING
     if (module->dynamic)
          unload_module( module );
#endif
}

/******************************************************************************/

#ifdef DFB_DYNAMIC_LINKING

static ModuleEntry *
lookup_by_name( const ModuleDirectory *directory,
                const char            *name )
{
     DirectLink *l;

     D_ASSERT( directory != NULL );
     D_ASSERT( name != NULL );

     direct_list_foreach (l, directory->entries) {
          ModuleEntry *entry = (ModuleEntry*) l;

          if (!entry->name)
               continue;

          if (!strcmp( entry->name, name ))
               return entry;
     }

     return NULL;
}

static ModuleEntry *
lookup_by_file( const ModuleDirectory *directory,
                const char            *file )
{
     DirectLink *l;

     D_ASSERT( directory != NULL );
     D_ASSERT( file != NULL );

     direct_list_foreach (l, directory->entries) {
          ModuleEntry *entry = (ModuleEntry*) l;

          if (!entry->file)
               continue;

          if (!strcmp( entry->file, file ))
               return entry;
     }

     return NULL;
}

static bool
load_module( ModuleEntry *module )
{
     D_ASSERT( module != NULL );
     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->file != NULL );
     D_ASSERT( module->loaded == false );
     D_ASSERT( module->disabled == false );

     module->handle = open_module( module );

     return module->loaded;
}

static void
unload_module( ModuleEntry *module )
{
     D_ASSERT( module != NULL );
     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->handle != NULL );
     D_ASSERT( module->loaded == true );

     dlclose( module->handle );

     module->handle = NULL;
     module->loaded = false;
}

static void *
open_module( ModuleEntry *module )
{
     ModuleDirectory *directory = module->directory;
     int              entry_len = strlen(module->file);
     int              buf_len   = strlen(directory->path) + entry_len + 2;
     char             buf[buf_len];
     void            *handle;

     snprintf( buf, buf_len, "%s/%s", directory->path, module->file );

     D_DEBUG( "DirectFB/core/modules: Loading '%s'...\n", buf );

     handle = dlopen( buf, RTLD_LAZY );
     if (!handle)
          D_DLERROR( "DirectFB/core/modules: Unable to dlopen `%s'!\n", buf );

     return handle;
}

#endif

