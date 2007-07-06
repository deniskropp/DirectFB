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

#ifndef __SAWMAN__SAWMAN_CONFIG_H__
#define __SAWMAN__SAWMAN_CONFIG_H__

#include <directfb.h>
#include <sawman.h>

typedef struct {
     int                   thickness;
     DFBDimension          resolution;
     DFBSurfacePixelFormat format;
     DFBColor              focused[4];
     DFBColor              unfocused[4];
     int                   focused_index[4];
     int                   unfocused_index[4];
} SaWManBorderInit;

typedef struct {
     SaWManBorderInit     *border;
     SaWManBorderInit      borders[3];
} SaWManConfig;


extern SaWManConfig *sawman_config;


/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as SaWMan options are stripped out
 * of the array.
 */
DirectResult sawman_config_init( int *argc, char **argv[] );

/*
 * Set individual option. Used by sawman_config_init(), and SaWManSetOption()
 */
DirectResult sawman_config_set( const char *name, const char *value );

const char *sawman_config_usage( void );


#endif /* __SAWMAN__SAWMAN_CONFIG_H__ */

