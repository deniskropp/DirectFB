/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__TYPES_H__
#define __DIRECT__TYPES_H__

#include <direct/os/types.h>


#ifdef WIN32
// The following ifdef block is the standard way of creating macros which make exporting 
// from a DLL simpler. All files within this DLL are compiled with the DIRECT_EXPORTS
// symbol defined on the command line. This symbol should not be defined on any project
// that uses this DLL. This way any other project whose source files include this file see 
// DIRECT_API functions as being imported from a DLL, whereas this DLL sees symbols
// defined with this macro as being exported.
#ifdef DIRECT_EXPORTS
#define DIRECT_API __declspec(dllexport)
#else
#define DIRECT_API __declspec(dllimport)
#endif
#else
#define DIRECT_API
#endif


#include <direct/result.h>

/**********************************************************************************************************************/

/*
 * Return value of enumeration callbacks
 */
typedef enum {
     DENUM_OK       = 0x00000000,  /* Proceed with enumeration */
     DENUM_CANCEL   = 0x00000001   /* Cancel enumeration */
} DirectEnumerationResult;


typedef struct __D_DirectCleanupHandler      DirectCleanupHandler;
typedef struct __D_DirectConfig              DirectConfig;
typedef struct __D_DirectFifo                DirectFifo;
typedef struct __D_DirectFifoItem            DirectFifoItem;
typedef struct __D_DirectFile                DirectFile;
typedef struct __D_DirectHash                DirectHash;
typedef struct __D_DirectOnce                DirectOnce;
typedef struct __D_DirectLink                DirectLink;
typedef struct __D_DirectLog                 DirectLog;
typedef struct __D_DirectMap                 DirectMap;
typedef struct __D_DirectModuleDir           DirectModuleDir;
typedef struct __D_DirectModuleEntry         DirectModuleEntry;
typedef struct __D_DirectMutex               DirectMutex;
typedef struct __D_DirectProcessor           DirectProcessor;
typedef struct __D_DirectSerial              DirectSerial;
typedef struct __D_DirectSignalHandler       DirectSignalHandler;
typedef struct __D_DirectStream              DirectStream;
typedef struct __D_DirectTLS                 DirectTLS;
typedef struct __D_DirectTraceBuffer         DirectTraceBuffer;
typedef struct __D_DirectTree                DirectTree;
typedef struct __D_DirectThread              DirectThread;
typedef struct __D_DirectThreadHandle        DirectThreadHandle;
typedef struct __D_DirectThreadInitHandler   DirectThreadInitHandler;
typedef struct __D_DirectWaitQueue           DirectWaitQueue;

#endif

