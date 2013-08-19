/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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

#include <direct/Utils.h>

/*********************************************************************************************************************/

D_LOG_DOMAIN( Direct_Utils, "Direct/Utils", "Direct Utils" );

namespace Direct {


Module::Module( Modules &modules )
     :
     modules( modules )
{
     D_DEBUG_AT( Direct_Utils, "Module::%s( %p, modules %p )\n", __FUNCTION__, this, &modules );
}

Module::~Module()
{
     D_DEBUG_AT( Direct_Utils, "Module::%s( %p )\n", __FUNCTION__, this );

     modules.Unregister( this );
}

void
Module::Register()
{
     modules.Register( this );
}


DirectResult
Modules::Load()
{
     D_DEBUG_AT( Direct_Utils, "Modules::%s( %p )\n", __FUNCTION__, this );

     D_DEBUG_AT( Direct_Utils, "  -> loading modules...\n" );

     direct_modules_explore_directory( &modules );

     DirectModuleEntry *entry;

     direct_list_foreach( entry, modules.entries ) {
          D_DEBUG_AT( Direct_Utils, "  -> checking module %p '%s' (refs %d)\n", entry, entry->name, entry->refs );

          Module *module;

          ModuleMap::iterator it = map.find( entry );

          if (it != map.end()) {
               module = (*it).second;

               D_DEBUG_AT( Direct_Utils, "  ---> module already loaded (%p)\n", module );
               continue;
          }


          module = (Module *) direct_module_ref( entry );
          if (!module) {
               D_DEBUG_AT( Direct_Utils, "  -> direct_module_ref() returned NULL, module disabled?\n" );
               continue;
          }

          D_DEBUG_AT( Direct_Utils, "  ===> %s\n", *module->GetName() );

          module->Initialise();

          map[entry] = module;
     }

     return DR_OK;
}


void
Modules::Register( Module *module )
{
     D_DEBUG_AT( Direct_Utils, "Modules::%s( %p, module %p )\n", __FUNCTION__, this, module );

     direct_modules_register( &modules, 0, *module->GetName(), module );
}

void
Modules::Unregister( Module *module )
{
     D_DEBUG_AT( Direct_Utils, "Modules::%s( %p, module %p )\n", __FUNCTION__, this, module );

     direct_modules_unregister( &modules, *module->GetName() );
}


}

