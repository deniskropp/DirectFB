/*
   (c) Copyright 2006-2007  directfb.org

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __FD__DALE_CONFIG_H__
#define __FD__DALE_CONFIG_H__

#include <fusiondale.h>

typedef struct {
     int             session;      /* select multi app world */

     bool            banner;       /* startup banner */

     bool            force_slave;
} FusionDaleConfig;

extern FusionDaleConfig *fusiondale_config;

/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as FusionDale options are stripped out
 * of the array.
 */
DirectResult fd_config_init( int *argc, char **argv[] );

/*
 * Set indiviual option. Used by config_init(), and FusionDaleSetOption()
 */
DirectResult fd_config_set( const char *name, const char *value );

const char *fd_config_usage( void );

#endif /* __FD__DALE_CONFIG_H__ */

