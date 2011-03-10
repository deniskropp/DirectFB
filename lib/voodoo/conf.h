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

#ifndef __VOODOO__CONF_H__
#define __VOODOO__CONF_H__

#include <voodoo/play.h>


struct __V_VoodooConfig {
     VoodooPlayInfo  play_info;
     bool            forward_nodes;
     unsigned int    memory_max;
     unsigned int    surface_max;
     unsigned int    layer_mask;
     unsigned int    stacking_mask;
     unsigned int    resource_id;
     bool            server_fork;
     char           *server_single;
     char           *play_broadcast;
     unsigned int    compression_min;
     bool            link_raw;
};

extern VoodooConfig VOODOO_API *voodoo_config;


DirectResult        VOODOO_API  voodoo_config_set( const char *name, const char *value );



void __Voodoo_conf_init( void );
void __Voodoo_conf_deinit( void );

#endif

