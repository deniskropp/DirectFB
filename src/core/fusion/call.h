/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.
   
   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de> and
              Sven Neumann <sven@convergence.de>.

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

#ifndef __CALL_H__
#define __CALL_H__

#ifdef __cplusplus
extern "C"
{
#endif

#include "fusion_types.h"

#ifndef FUSION_FAKE
#include <sys/ioctl.h>
#include <linux/fusion.h>
#else
typedef int (*FusionCallHandler) (int   caller,   /* fusion id of the caller */
                                  int   call_arg, /* optional call parameter */
                                  void *call_ptr, /* optional call parameter */
                                  void *ctx       /* optional handler context */
                                  );
#endif

typedef struct {
     int                call_id;
     int                fusion_id;
     FusionCallHandler  handler;
     void              *ctx;
} FusionCall;


FusionResult fusion_call_init    (FusionCall        *call,
                                  FusionCallHandler  handler,
                                  void              *ctx);

FusionResult fusion_call_execute (FusionCall        *call,
                                  int                call_arg,
                                  void              *call_ptr,
                                  int               *ret_val);

FusionResult fusion_call_return  (int                call_id,
                                  int                ret_val);

FusionResult fusion_call_destroy (FusionCall        *call);


#ifdef __cplusplus
}
#endif

#endif /* __CALL_H__ */

