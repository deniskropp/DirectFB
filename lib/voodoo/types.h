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

#ifndef __VOODOO__TYPES_H__
#define __VOODOO__TYPES_H__

#include <direct/types.h>


#ifdef WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the VOODOO_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// VOODOO_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef VOODOO_EXPORTS
#define VOODOO_API __declspec(dllexport)
#else
#define VOODOO_API __declspec(dllimport)
#endif
#else
#define VOODOO_API
#endif


typedef u32 VoodooInstanceID;
typedef u32 VoodooMethodID;
typedef u32 VoodooMessageSerial;

#define VOODOO_INSTANCE_NONE  ((VoodooInstanceID) 0)


typedef struct __V_VoodooMessageHeader   VoodooMessageHeader;
typedef struct __V_VoodooSuperMessage    VoodooSuperMessage;
typedef struct __V_VoodooRequestMessage  VoodooRequestMessage;
typedef struct __V_VoodooResponseMessage VoodooResponseMessage;


typedef struct __V_VoodooClient          VoodooClient;
typedef struct __V_VoodooConfig          VoodooConfig;
typedef struct __V_VoodooLink            VoodooLink;
typedef struct __V_VoodooPlayer          VoodooPlayer;
typedef struct __V_VoodooServer          VoodooServer;

#ifdef __cplusplus
class VoodooConnection;
class VoodooManager;
class VoodooPacket;
#else
typedef void*         VoodooManager;
#endif


typedef DirectResult (*VoodooSuperConstruct)( VoodooServer         *server,
                                              VoodooManager        *manager,
                                              const char           *name,
                                              void                 *ctx,
                                              VoodooInstanceID     *ret_instance );

typedef DirectResult (*VoodooDispatch)      ( void                 *dispatcher,
                                              void                 *real,
                                              VoodooManager        *manager,
                                              VoodooRequestMessage *msg );


#define MAX_MSG_SIZE          (17 * 1024)
#define VOODOO_PACKET_MAX     (MAX_MSG_SIZE)

#endif

