/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <sven@convergence.de> and
              Claudio Ciccani <klan@users.sf.net.

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

#ifndef __FS_CONFIG_H__
#define __FS_CONFIG_H__

#include <fusionsound.h>

typedef struct {
     char           *device;       /* Used device, e.g. "/dev/dsp" */
     
     int             channels;     /* default number of channels */
     FSSampleFormat  sampleformat; /* default sampleformat */
     long            samplerate;   /* default samplerate */

     int             session;      /* select multi app world */
} FSConfig;

extern FSConfig *fs_config;

/*
 * Allocate Config struct, fill with defaults and parse command line options
 * for overrides. Options identified as FusionSound options are stripped out
 * of the array.
 */
DFBResult fs_config_init( int *argc, char **argv[] );

/*
 * Set indiviual option. Used by config_init(), and FusionSoundSetOption()
 */
DFBResult fs_config_set( const char *name, const char *value );

const char *fs_config_usage( void );

#endif /* __FS_CONFIG_H__ */
