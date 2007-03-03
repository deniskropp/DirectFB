/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#ifndef CYBER5KFB_ALPHA_H
#define CYBER5KFB_ALPHA_H

#include "mmio.h"

#define SRC1_GRAPHICS	0
#define SRC1_OVERLAY1	1
#define SRC1_OVERLAY2	2

#define SRC2_OVERLAY1	0
#define SRC2_OVERLAY2	1
#define SRC2_GRAPHICS	2

#define ALPHA_GRAPHICS	0
#define ALPHA_OVERLAY2	1
#define ALPHA_LOOKUP	2
#define ALPHA_REGISTER	3

#define RAM_CPU			3

void cyber_cleanup_alpha(void);
void cyber_enable_alpha(int enable);
void cyber_enable_fullscreen_alpha(int enable);
void cyber_cleanup_alpha(void);
void cyber_select_blend_src1(int src);
void cyber_select_blend_src2(int src);
void cyber_select_alpha_src(int src);
void cyber_set_alpha_reg(unsigned char r, unsigned char g, unsigned char b);

void cyber_select_RAM_addr( unsigned char bRAMAddrSel );
void cyber_set_alpha_RAM_reg( unsigned char bIndex, unsigned char bR, unsigned char bG, unsigned char bB);
void cyber_select_magic_alpha_src( unsigned char bAlphaSrc );
void cyber_enable_magic_alpha_blend( unsigned char enable );
void cyber_set_magic_match_reg( unsigned char bR, unsigned char bG, unsigned char bB );

#endif /* CYBER5KFB_ALPHA_H */
