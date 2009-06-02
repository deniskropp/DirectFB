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

#ifndef __FUSION_PROTOCOL_H__
#define __FUSION_PROTOCOL_H__

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <direct/types.h>


typedef enum {
     FMT_SEND,
     FMT_ENTER,
     FMT_LEAVE,
     FMT_CALL,
     FMT_CALLRET,
     FMT_REACTOR
} FusionMessageType;

/*
 * Enter world (slave).
 */
typedef struct {
     FusionMessageType    type;
     
     FusionID             fusion_id;
} FusionEnter;

/*
 * Leave the world (slave).
 */
typedef struct {
     FusionMessageType    type;
     
     FusionID             fusion_id;
} FusionLeave;

/*
 * Execute a call.
 */
typedef struct {
     FusionMessageType    type;

     unsigned int         serial;

     FusionID             caller;
     int                  call_id;
     int                  call_arg;
     void                *call_ptr;

     void                *handler;
     void                *ctx;
     
     FusionCallExecFlags  flags;
} FusionCallMessage, FusionCallExecute;

/*
 * Send call return.
 */
typedef struct {
     FusionMessageType    type;
     
     int                  val;
} FusionCallReturn;

/*
 * Send reactor message.
 */
typedef struct {
     FusionMessageType    type;
     
     int                  id;
     int                  channel;
     
     FusionRef           *ref;
} FusionReactorMessage;


typedef union {
     FusionMessageType    type;
     
     FusionEnter          enter;
     FusionLeave          leave;
     FusionCallMessage    call;
     FusionCallReturn     callret;
     FusionReactorMessage reactor;
} FusionMessage;


#endif

