/*
 * $Workfile: pnl_defs.h $
 *
 * File Contents:   This file contains definitions of the Geode 
 *                  frame buffer panel data structures.
 *
 * SubModule:       Geode FlatPanel library
 *
 */

/* 
 * NSC_LIC_COPYRIGHT
 *
 * Copyright (c) 2001-2003 National Semiconductor Corporation ("NSC").
 *
 * All Rights Reserved.  Unpublished rights reserved under the copyright 
 * laws of the United States of America, other countries and international 
 * treaties.  The software is provided without fee.  Permission to use, 
 * copy, store, modify, disclose, transmit or distribute the software is 
 * granted, provided that this copyright notice must appear in any copy, 
 * modification, disclosure, transmission or distribution of the software.
 *  
 * NSC retains all ownership, copyright, trade secret and proprietary rights 
 * in the software. 
 * THIS SOFTWARE HAS BEEN PROVIDED "AS IS," WITHOUT EXPRESS OR IMPLIED 
 * WARRANTY INCLUDING, WITHOUT LIMITATION, IMPLIED WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR USE AND NON-INFRINGEMENT.
 *
 * NSC does not assume or authorize any other person to assume for it any 
 * liability in connection with the Software. NSC SHALL NOT BE LIABLE TO 
 * COMPANY, OR ANY THIRD PARTY, IN CONTRACT, TORT, WARRANTY, STRICT 
 * LIABILITY, OR OTHERWISE FOR ANY DIRECT DAMAGES, OR FOR ANY SPECIAL, 
 * INDIRECT, INCIDENTAL OR CONSEQUENTIAL DAMAGES, INCLUDING BUT NOT 
 * LIMITED TO, BUSINESS INTERRUPTION, LOST PROFITS OR GOODWILL, OR LOSS 
 * OF INFORMATION EVEN IF NSC IS ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
 *
 * END_NSC_LIC_COPYRIGHT */

#ifndef _pnl_defs_h
#define _pnl_defs_h

typedef enum
{
   MARMOT_PLATFORM = 0,
   UNICORN_PLATFORM,
   CENTAURUS_PLATFORM,
   ARIES_PLATFORM,
   CARMEL_PLATFORM,
   HYDRA_PLATFORM,
   DORADO_PLATFORM,
   DRACO_PLATFORM,
   REDCLOUD_PLATFORM,
   OTHER_PLATFORM
}
SYS_BOARD;

#define PNL_9210             0x01
#define PNL_9211_A           0x02
#define PNL_9211_C           0x04
#define PNL_UNKNOWN_CHIP     0x08

#define PNL_TFT              0x01
#define PNL_SSTN             0x02
#define PNL_DSTN             0x04
#define PNL_TWOP             0x08
#define PNL_UNKNOWN_PANEL    0x10

#define PNL_MONO_PANEL       0x01
#define PNL_COLOR_PANEL      0x02
#define PNL_UNKNOWN_COLOR    0x08

#define PNL_PANELPRESENT     0x01
#define PNL_PLATFORM         0x02
#define PNL_PANELCHIP        0x04
#define PNL_PANELSTAT        0x08
#define PNL_OVERRIDE_STAT    0x10
#define PNL_OVERRIDE_ALL     0x1F

typedef struct _Pnl_PanelStat_
{
   int Type;
   int XRes;
   int YRes;
   int Depth;
   int MonoColor;
}
Pnl_PanelStat;

typedef struct _Pnl_Params_
{
   unsigned long Flags;
   int PanelPresent;
   int Platform;
   int PanelChip;
   Pnl_PanelStat PanelStat;
}
Pnl_PanelParams, *PPnl_PanelParams;

#endif /* _pnl_defs_h */

/* END OF FILE */
