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

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>

#ifdef PIC
#define DYNAMIC_LINKING
#endif

#ifdef DYNAMIC_LINKING
#include <dlfcn.h>
#endif

D_DEBUG_DOMAIN( Direct_Modules, "Direct/Modules", "Module loading and registration" );

/******************************************************************************/

#ifdef DYNAMIC_LINKING

static DirectModuleEntry *lookup_by_name( const DirectModuleDir *directory,
                                          const char            *name );

static DirectModuleEntry *lookup_by_file( const DirectModuleDir *directory,
                                          const char            *file );

static void *open_module  ( DirectModuleEntry *module );
static bool  load_module  ( DirectModuleEntry *module );
static void  unload_module( DirectModuleEntry *module );

#endif

/******************************************************************************/

static int
suppress_module (const char *name)
{
     int i = 0;

     if (!direct_config || !direct_config->disable_module)
          return 0;

     while (direct_config->disable_module[i]) {
          if (strcmp (direct_config->disable_module[i], name) == 0) {
               D_INFO( "Direct/Modules: suppress module '%s'\n", direct_config->disable_module[i] );
               return 1;
	  }

	  i++;
     }

     return 0;
}

void
direct_modules_register( DirectModuleDir *directory,
                         unsigned int     abi_version,
                         const char      *name,
                         const void      *funcs )
{
     DirectModuleEntry *entry;

     D_ASSERT( directory != NULL );
     D_ASSERT( name != NULL );
     D_ASSERT( funcs != NULL );

     D_DEBUG_AT( Direct_Modules, "Registering '%s' ('%s')\n", name, directory->path );

#ifdef DYNAMIC_LINKING
     if ((entry = lookup_by_name( directory, name )) != NULL) {
          entry->loaded = true;
          entry->funcs  = funcs;

          return;
     }
#endif

     if (directory->loading)
          entry = directory->loading;
     else if (! (entry = D_CALLOC( 1, sizeof(DirectModuleEntry) )))
          return;

     entry->directory = directory;
     entry->loaded    = true;
     entry->name      = D_STRDUP( name );
     entry->funcs     = funcs;

     entry->disabled  = suppress_module( name );

     if (abi_version != directory->abi_version) {
          D_ERROR( "Direct/Modules: ABI version of '%s' (%d) does not match %d!\n",
                   entry->file ? entry->file : entry->name,
                   abi_version, directory->abi_version );

          entry->disabled = true;
     }

     D_MAGIC_SET( entry, DirectModuleEntry );

     direct_list_prepend( &directory->entries, &entry->link );

     D_DEBUG_AT( Direct_Modules, "...registered.\n" );
}

int
direct_modules_explore_directory( DirectModuleDir *directory )
{
#ifdef DYNAMIC_LINKING
     int            dir_len;
     DIR           *dir;
     struct dirent *entry = NULL;
     struct dirent  tmp;
     int            count = 0;

     D_ASSERT( directory != NULL );
     D_ASSERT( directory->path != NULL );

     dir_len = strlen( directory->path );
     dir     = opendir( directory->path );

     if (!dir) {
          D_PERROR( "Direct/Modules: Could not open module directory `%s'!\n", directory->path );
          return 0;
     }

     while (readdir_r( dir, &tmp, &entry ) == 0 && entry) {
          void              *handle;
          DirectModuleEntry *module;
          int                entry_len = strlen(entry->d_name);

          if (entry_len < 4 ||
              entry->d_name[entry_len-1] != 'o' ||
              entry->d_name[entry_len-2] != 's')
               continue;

          if (lookup_by_file( directory, entry->d_name ))
               continue;


          module = D_CALLOC( 1, sizeof(DirectModuleEntry) );
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

                    D_ERROR( "Direct/Modules: Module '%s' did not register itself after loading! "
                             "Trying default module constructor...\n", entry->d_name );

                    len = strlen( entry->d_name );

                    entry->d_name[len-3] = 0;

                    func = dlsym( handle, entry->d_name + 3 );
                    if (func) {
                         func();

                         if (!module->loaded) {
                              D_ERROR( "Direct/Modules: ... even did not register after "
                                       "explicitly calling the module constructor!\n" );
                         }
                    }
                    else {
                         D_ERROR( "Direct/Modules: ... default contructor not found!\n" );
                    }

                    if (!module->loaded) {
                         module->disabled = true;

                         D_MAGIC_SET( module, DirectModuleEntry );

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

               D_MAGIC_SET( module, DirectModuleEntry );

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
direct_module_ref( DirectModuleEntry *module )
{
     D_MAGIC_ASSERT( module, DirectModuleEntry );

     if (module->disabled)
          return NULL;

#ifdef DYNAMIC_LINKING
     if (!module->loaded && !load_module( module ))
          return NULL;
#endif

     module->refs++;

     return module->funcs;
}

void
direct_module_unref( DirectModuleEntry *module )
{
     D_MAGIC_ASSERT( module, DirectModuleEntry );
     D_ASSERT( module->refs > 0 );

     if (--module->refs)
          return;

#ifdef DYNAMIC_LINKING
     if (module->dynamic)
          unload_module( module );
#endif
}

/******************************************************************************/

#ifdef DYNAMIC_LINKING

static DirectModuleEntry *
lookup_by_name( const DirectModuleDir *directory,
                const char            *name )
{
     DirectLink *l;

     D_ASSERT( directory != NULL );
     D_ASSERT( name != NULL );

     direct_list_foreach (l, directory->entries) {
          DirectModuleEntry *entry = (DirectModuleEntry*) l;

          D_MAGIC_ASSERT( entry, DirectModuleEntry );

          if (!entry->name)
               continue;

          if (!strcmp( entry->name, name ))
               return entry;
     }

     return NULL;
}

static DirectModuleEntry *
lookup_by_file( const DirectModuleDir *directory,
                const char            *file )
{
     DirectLink *l;

     D_ASSERT( directory != NULL );
     D_ASSERT( file != NULL );

     direct_list_foreach (l, directory->entries) {
          DirectModuleEntry *entry = (DirectModuleEntry*) l;

          D_MAGIC_ASSERT( entry, DirectModuleEntry );

          if (!entry->file)
               continue;

          if (!strcmp( entry->file, file ))
               return entry;
     }

     return NULL;
}

static bool
load_module( DirectModuleEntry *module )
{
     D_MAGIC_ASSERT( module, DirectModuleEntry );
     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->file != NULL );
     D_ASSERT( module->loaded == false );
     D_ASSERT( module->disabled == false );

     module->handle = open_module( module );

     return module->loaded;
}

static void
unload_module( DirectModuleEntry *module )
{
     D_MAGIC_ASSERT( module, DirectModuleEntry );
     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->handle != NULL );
     D_ASSERT( module->loaded == true );

     dlclose( module->handle );

     module->handle = NULL;
     module->loaded = false;
}

static void *
open_module( DirectModuleEntry *module )
{
     DirectModuleDir *directory = module->directory;
     int              entry_len = strlen(module->file);
     int              buf_len   = strlen(directory->path) + entry_len + 2;
     char             buf[buf_len];
     void            *handle;

     snprintf( buf, buf_len, "%s/%s", directory->path, module->file );

     D_DEBUG_AT( Direct_Modules, "Loading '%s'...\n", buf );

     handle = dlopen( buf, RTLD_NOW );
     if (!handle)
          D_DLERROR( "Direct/Modules: Unable to dlopen `%s'!\n", buf );

     return handle;
}

#endif

