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

#include <sys/types.h>
#include <dirent.h>

#include <core/coredefs.h>
#include <core/modules.h>

#include <misc/mem.h>

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

     if (!dfb_config->disable_module)
          return 0;
     
     while (dfb_config->disable_module[i]) {
          if (strcmp (dfb_config->disable_module[i], name) == 0) {
               INITMSG( "DirectFB/Core: suppress module '%s'\n", 
			dfb_config->disable_module[i] );
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

     DFB_ASSERT( directory != NULL );
     DFB_ASSERT( name != NULL );
     DFB_ASSERT( funcs != NULL );

#ifdef DFB_DYNAMIC_LINKING
     if ((entry = lookup_by_name( directory, name )) != NULL) {
          entry->loaded = true;
          entry->funcs  = funcs;
          
          return;
     }
#endif

     if (directory->loading)
          entry = directory->loading;
     else if (! (entry = DFBCALLOC( 1, sizeof(ModuleEntry) )))
          return;

     entry->directory = directory;
     entry->loaded    = true;
     entry->name      = DFBSTRDUP( name );
     entry->funcs     = funcs;

     entry->disabled  = suppress_module( name );

     if (abi_version != directory->abi_version) {
          ERRORMSG( "DirectFB/core/modules: "
                    "ABI version of '%s' (%d) does not match %d!\n",
                    entry->file, abi_version, directory->abi_version );

          entry->disabled = true;
     }

     fusion_list_prepend( &directory->entries, &entry->link );
}

int
dfb_modules_explore_directory( ModuleDirectory *directory )
{
#ifdef DFB_DYNAMIC_LINKING
     int            dir_len;
     DIR           *dir;
     struct dirent *entry;
     int            count = 0;

     DFB_ASSERT( directory != NULL );
     DFB_ASSERT( directory->path != NULL );
     
     dir_len = strlen( directory->path );
     dir     = opendir( directory->path );

     if (!dir) {
          PERRORMSG( "DirectFB/core/modules: Could not open "
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
          
          
          module = DFBCALLOC( 1, sizeof(ModuleEntry) );
          if (!module)
               continue;

          module->directory = directory;
          module->dynamic   = true;
          module->file      = DFBSTRDUP( entry->d_name );
          

          directory->loading = module;

          if ((handle = open_module( module )) != NULL) {
               if (!module->loaded) {
                    dlclose( handle );

                    ERRORMSG( "DirectFB/core/modules: Module '%s' did not "
                              "register itself after loading!\n", entry->d_name );
                    
                    module->disabled = true;
                    
                    fusion_list_prepend( &directory->entries, &module->link );
               }
               else if (module->disabled) {
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

               fusion_list_prepend( &directory->entries, &module->link );
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
     DFB_ASSERT( module != NULL );

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
     DFB_ASSERT( module != NULL );
     DFB_ASSERT( module->refs > 0 );
     
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
     FusionLink *l;

     DFB_ASSERT( directory != NULL );
     DFB_ASSERT( name != NULL );

     fusion_list_foreach (l, directory->entries) {
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
     FusionLink *l;

     DFB_ASSERT( directory != NULL );
     DFB_ASSERT( file != NULL );

     fusion_list_foreach (l, directory->entries) {
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
     DFB_ASSERT( module != NULL );
     DFB_ASSERT( module->dynamic == true );
     DFB_ASSERT( module->file != NULL );
     DFB_ASSERT( module->loaded == false );
     DFB_ASSERT( module->disabled == false );

     module->handle = open_module( module );

     return module->loaded;
}

static void
unload_module( ModuleEntry *module )
{
     DFB_ASSERT( module != NULL );
     DFB_ASSERT( module->dynamic == true );
     DFB_ASSERT( module->handle != NULL );
     DFB_ASSERT( module->loaded == true );

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

     handle = dlopen( buf, RTLD_LAZY );
     if (!handle)
          DLERRORMSG( "DirectFB/core/modules: Unable to dlopen `%s'!\n", buf );

     return handle;
}

#endif

