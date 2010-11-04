/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#include <string.h>

#include <direct/conf.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <voodoo/conf.h>


static VoodooConfig config = {
};

VoodooConfig *voodoo_config = &config;
const char   *voodoo_config_usage =
     "libvoodoo options:\n"
     "  player-name=<name>             Set player name\n"
     "  player-vendor=<name>           Set player vendor\n"
     "  player-model=<name>            Set player model\n"
     "  player-uuid=<name>             Set player uuid\n"
     "  proxy-memory-max=<kB>          Set maximum amount of memory per connection\n"
     "  proxy-surface-max=<kB>         Set maximum amount of memory per surface\n"
     "  [no-]server-fork               Fork a new process for each connection (default: no)\n"
     "  server-single=<interface>      Enable single client mode for super interface, e.g. IDirectFB\n"
     "\n";

/**********************************************************************************************************************/

DirectResult
voodoo_config_set( const char *name, const char *value )
{
     if (strcmp (name, "player-name" ) == 0) {
          if (value) {
               direct_snputs( voodoo_config->play_info.name, value, VOODOO_PLAYER_NAME_LENGTH );
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "player-vendor" ) == 0) {
          if (value) {
               direct_snputs( voodoo_config->play_info.vendor, value, VOODOO_PLAYER_VENDOR_LENGTH );
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "player-model" ) == 0) {
          if (value) {
               direct_snputs( voodoo_config->play_info.model, value, VOODOO_PLAYER_MODEL_LENGTH );
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "player-uuid" ) == 0) {
          if (value) {
               direct_snputs( voodoo_config->play_info.uuid, value, 16 );
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "proxy-memory-max" ) == 0) {
          if (value) {
               unsigned int max;

               if (sscanf( value, "%u", &max ) != 1) {
                    D_ERROR( "Voodoo/Config '%s': Invalid value specified!\n", name );
                    return DR_INVARG;
               }

               voodoo_config->memory_max = max * 1024;
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "proxy-surface-max" ) == 0) {
          if (value) {
               unsigned int max;

               if (sscanf( value, "%u", &max ) != 1) {
                    D_ERROR( "Voodoo/Config '%s': Invalid value specified!\n", name );
                    return DR_INVARG;
               }

               voodoo_config->surface_max = max * 1024;
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "proxy-layer-mask" ) == 0) {
          if (value) {
               unsigned int mask;

               if (sscanf( value, "%u", &mask ) != 1) {
                    D_ERROR( "Voodoo/Config '%s': Invalid value specified!\n", name );
                    return DR_INVARG;
               }

               voodoo_config->layer_mask = mask;
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "proxy-stacking-mask" ) == 0) {
          if (value) {
               unsigned int mask;

               if (sscanf( value, "%u", &mask ) != 1) {
                    D_ERROR( "Voodoo/Config '%s': Invalid value specified!\n", name );
                    return DR_INVARG;
               }

               voodoo_config->stacking_mask = mask;
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "proxy-resource-id" ) == 0) {
          if (value) {
               unsigned int resource_id;

               if (sscanf( value, "%u", &resource_id ) != 1) {
                    D_ERROR( "Voodoo/Config '%s': Invalid value specified!\n", name );
                    return DR_INVARG;
               }

               voodoo_config->resource_id = resource_id;
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (strcmp (name, "server-fork" ) == 0) {
          voodoo_config->server_fork = true;
     } else
     if (strcmp (name, "no-server-fork" ) == 0) {
          voodoo_config->server_fork = false;
     } else
     if (strcmp (name, "server-single" ) == 0) {
          if (value) {
               if (voodoo_config->server_single)
                    D_FREE( voodoo_config->server_single );

               voodoo_config->server_single = D_STRDUP( value );
               if (!voodoo_config->server_single)
                    D_OOM();
          }
          else {
               D_ERROR( "Voodoo/Config '%s': No value specified!\n", name );
               return DR_INVARG;
          }
     } else
     if (direct_config_set( name, value ))
          return DR_UNSUPPORTED;

     return DR_OK;
}

