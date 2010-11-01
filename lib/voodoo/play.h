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

#ifndef __VOODOO__PLAY_H__
#define __VOODOO__PLAY_H__

#include <voodoo/types.h>


#define VOODOO_PLAY_VERSION_FIXED_SIZE  32

#define VOODOO_PLAYER_NAME_LENGTH       96

#define VOODOO_PLAYER_VENDOR_LENGTH     96
#define VOODOO_PLAYER_MODEL_LENGTH      96



typedef enum {
    VPVF_NONE           = 0x00000000,

    VPVF_LITTLE_ENDIAN  = 0x00000001,   /* No big endian support for now */
    VPVF_32BIT_SERIALS  = 0x00000002,   /* Using 32bit message serials */

    VPVF_ALL            = 0x00000003
} VoodooPlayVersionFlags;

typedef struct {
    u8                          v[4];   /* flags, major, minor, micro */
} VoodooPlayVersion;


typedef enum {
    VPIF_NONE   = 0x00000000,

    VPIF_LEVEL2 = 0x00000001,

    VPIF_ALL    = 0x00000001
} VoodooPlayInfoFlags;

typedef struct {
    VoodooPlayInfoFlags         flags;

    char                        uuid[16];

    char                        name[VOODOO_PLAYER_NAME_LENGTH];        /* "My Philips TV" */

    char                        vendor[VOODOO_PLAYER_VENDOR_LENGTH];    /* "Philips Consumer Lifestyle" */
    char                        model[VOODOO_PLAYER_MODEL_LENGTH];      /* "32PFL9604H/10" */
} VoodooPlayInfo;


typedef enum {
    VPMT_INVALID,

    VPMT_DISCOVER,
    VPMT_SENDINFO,
} VoodooPlayMessageType;

typedef struct {
    /* Version information first in structure, fixed size (union!) */
    union {
        char __fixed[VOODOO_PLAY_VERSION_FIXED_SIZE];


        VoodooPlayVersion       version;                                /* (1.0, ...) */
    };

    VoodooPlayMessageType       type;

    union {
        VoodooPlayInfo          info;       /* DISCOVER, SENDINFO */
    };
} VoodooPlayMessage;


typedef DirectEnumerationResult (*VoodooPlayerCallback)( void                    *ctx,
                                                         const VoodooPlayInfo    *info,
                                                         const VoodooPlayVersion *version,
                                                         const char              *address,
                                                         unsigned int             ms_since_last_seen );


DirectResult voodoo_player_create   ( const VoodooPlayInfo  *info,
                                      VoodooPlayer         **ret_player );

DirectResult voodoo_player_destroy  ( VoodooPlayer          *player );

DirectResult voodoo_player_broadcast( VoodooPlayer          *player );

DirectResult voodoo_player_lookup   ( VoodooPlayer          *player,
                                      const char            *name,
                                      char                  *ret_addr,
                                      int                    max_addr );

DirectResult voodoo_player_enumerate( VoodooPlayer          *player,
                                      VoodooPlayerCallback   callback,
                                      void                  *ctx );

#endif
