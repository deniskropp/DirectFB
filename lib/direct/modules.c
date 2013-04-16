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

#include <sys/types.h>
#include <string.h>
#include <stdio.h>

#include <direct/conf.h>
#include <direct/debug.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/modules.h>

#if DIRECT_BUILD_DYNLOAD
#include <alloca.h>
#include <dirent.h>
#include <dlfcn.h>
#endif

D_LOG_DOMAIN( Direct_Modules, "Direct/Modules", "Module loading and registration" );

/******************************************************************************/

#if DIRECT_BUILD_DYNLOAD

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

     D_DEBUG_AT( Direct_Modules, "Registering '%s' ('%s')...\n", name, directory->path );

#if DIRECT_BUILD_DYNLOAD
     if ((entry = lookup_by_name( directory, name )) != NULL) {
          D_MAGIC_ASSERT( entry, DirectModuleEntry );

          entry->loaded = true;
          entry->funcs  = funcs;

          return;
     }
#endif

     if (directory->loading) {
          entry = directory->loading;
          D_MAGIC_ASSERT( entry, DirectModuleEntry );

          directory->loading = NULL;
     }
     else {
          entry = D_CALLOC( 1, sizeof(DirectModuleEntry) );
          if (!entry) {
               D_OOM();
               return;
          }

          D_MAGIC_SET( entry, DirectModuleEntry );
     }

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

     direct_list_prepend( &directory->entries, &entry->link );

     D_DEBUG_AT( Direct_Modules, "...registered.\n" );
}

void
direct_modules_unregister( DirectModuleDir *directory,
                           const char      *name )
{
#if DIRECT_BUILD_DYNLOAD
     DirectModuleEntry *entry;
#endif

     D_DEBUG_AT( Direct_Modules, "Unregistering '%s' ('%s')...\n", name, directory->path );

#if DIRECT_BUILD_DYNLOAD
     entry = lookup_by_name( directory, name );
     if (!entry) {
          D_ERROR( "Direct/Modules: Unregister failed, could not find '%s' module!\n", name );
          return;
     }

     D_MAGIC_ASSERT( entry, DirectModuleEntry );

     D_FREE( entry->name );

     direct_list_remove( &directory->entries, &entry->link );

     D_MAGIC_CLEAR( entry );

     D_FREE( entry );
#endif

     D_DEBUG_AT( Direct_Modules, "...unregistered.\n" );
}

int
direct_modules_explore_directory( DirectModuleDir *directory )
{
#if DIRECT_BUILD_DYNLOAD
     DIR           *dir;
     struct dirent *entry = NULL;
     struct dirent  tmp;
     int            count = 0;
     const char    *pathfront = "";
     const char    *path;
     char          *buf;

     D_ASSERT( directory != NULL );
     D_ASSERT( directory->path != NULL );

     D_DEBUG_AT( Direct_Modules, "%s( '%s' )\n", __FUNCTION__, directory->path );

     path = directory->path;

     if (path[0] != '/') {
          pathfront = direct_config->module_dir;
          if (!pathfront)
               pathfront = MODULEDIR;
     }

     buf = alloca( strlen(pathfront) + 1 + strlen(path) + 1 ); /* pre, slash, post, 0 */
     sprintf( buf, "%s/%s", pathfront, path );

     dir = opendir( buf );
     if (!dir) {
          D_DEBUG_AT( Direct_Modules, "  -> ERROR opening directory: %s!\n", strerror(errno) );
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

          D_MAGIC_SET( module, DirectModuleEntry );

          module->directory = directory;
          module->dynamic   = true;
          module->file      = D_STRDUP( entry->d_name );
          if (!module->file) {
               D_MAGIC_CLEAR( module );
               D_FREE( module );
               continue;
          }

          directory->loading = module;

          if ((handle = open_module( module )) != NULL) {
               if (!module->loaded) {
                    dlclose( handle );

                    D_ERROR( "Direct/Modules: Module '%s' did not "
                             "register itself after loading!\n",
                             entry->d_name );

                    module->disabled = true;

                    direct_list_prepend( &directory->entries, &module->link );
               }
               else if (module->disabled) {
                    module->loaded = false;

                    /* may call direct_modules_unregister() */
                    dlclose( handle );
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
direct_module_ref( DirectModuleEntry *module )
{
     D_ASSERT( module != NULL );

     D_DEBUG_AT( Direct_Modules, "%s( %p '%s', %d refs )\n", __FUNCTION__, module, module->name, module->refs );

     D_MAGIC_ASSERT( module, DirectModuleEntry );

     if (module->disabled)
          return NULL;

#if DIRECT_BUILD_DYNLOAD
     if (!module->loaded && !load_module( module )) {
          D_DEBUG_AT( Direct_Modules, "  -> load_module failed, returning NULL\n" );

          return NULL;
     }
#endif

     module->refs++;

     D_DEBUG_AT( Direct_Modules, "  -> refs %d, funcs %p\n", module->refs, module->funcs );

     return module->funcs;
}

void
direct_module_unref( DirectModuleEntry *module )
{
     D_ASSERT( module != NULL );

     D_DEBUG_AT( Direct_Modules, "%s( %p '%s', %d refs )\n", __FUNCTION__, module, module->name, module->refs );

     D_MAGIC_ASSERT( module, DirectModuleEntry );
     D_ASSERT( module->refs > 0 );

     if (--module->refs)
          return;

#if DIRECT_BUILD_DYNLOAD
     if (module->dynamic)
          unload_module( module );
#endif
}

/******************************************************************************/

#if DIRECT_BUILD_DYNLOAD

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

     D_DEBUG_AT( Direct_Modules, "%s()\n", __FUNCTION__ );

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
     D_DEBUG_AT( Direct_Modules, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( module, DirectModuleEntry );

     D_DEBUG_AT( Direct_Modules, "%s( %p '%s', %d refs )\n", __FUNCTION__, module, module->file, module->refs );

     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->file != NULL );
     D_ASSERT( module->loaded == false );
     D_ASSERT( module->disabled == false );

     module->handle = open_module( module );
     if (module->handle)
          module->loaded = true;

     return module->loaded;
}

static void
unload_module( DirectModuleEntry *module )
{
     void *handle;

     D_DEBUG_AT( Direct_Modules, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( module, DirectModuleEntry );

     D_DEBUG_AT( Direct_Modules, "%s( %p '%s', %d refs )\n", __FUNCTION__, module, module->file, module->refs );

     D_ASSERT( module->dynamic == true );
     D_ASSERT( module->handle != NULL );
     D_ASSERT( module->loaded == true );

     handle = module->handle;

     module->handle = NULL;
     module->loaded = false;

     /* may call direct_modules_unregister() */
     dlclose( handle );
}

static void *
open_module( DirectModuleEntry *module )
{
     DirectModuleDir *directory;
     const char      *pathfront = "";
     const char      *path;
     char            *buf;
     void            *handle;

     D_DEBUG_AT( Direct_Modules, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( module, DirectModuleEntry );

     D_DEBUG_AT( Direct_Modules, "%s( %p '%s', %d refs )\n", __FUNCTION__, module, module->file, module->refs );

     D_ASSERT( module->file != NULL );
     D_ASSERT( module->directory != NULL );
     D_ASSERT( module->directory->path != NULL );

     directory = module->directory;
     path      = directory->path;

     if (path[0] != '/') {
          pathfront = direct_config->module_dir;
          if (!pathfront)
               pathfront = MODULEDIR;
     }

     buf = alloca( strlen( pathfront ) + 1 + strlen( path ) + 1 + strlen( module->file ) + 1 );
     sprintf( buf, "%s/%s/%s", pathfront, path, module->file );

     D_DEBUG_AT( Direct_Modules, "Loading '%s'...\n", buf );

     handle = dlopen( buf, RTLD_NOW );
     D_DEBUG_AT( Direct_Modules, "  -> dlopen returned %p\n", handle );
     if (!handle)
          D_DLERROR( "Direct/Modules: Unable to dlopen `%s'!\n", buf );

     D_MAGIC_ASSERT( module, DirectModuleEntry );

     return handle;
}

#endif

