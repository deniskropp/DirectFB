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

#include "cyber5k.h"
#include "cyber5k_alpha.h"
#include "regs.h"

void cyber_cleanup_alpha()
{
	int i;

	cyber_grphw(0xfa, 0);
	for (i=0; i<16; i++) {
		if (i == 0x0A) {/*Don't clean up SyncLock video path if there is one*/
			cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
			cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) & 0x08);
		}
		else {
			cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
			cyber_out8(cyber_mmio, SEQDATA, 0x00);
		}
	}

	cyber_grphw(0xfa, 8);
	for (i=0; i<16; i++) {
		if(i==0x0F) {/*Just in case there is a SyncLock video*/
			cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
			cyber_out8(cyber_mmio, SEQDATA, 0x00);
			cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) | 0xC0);
		}
		else {
			cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
			cyber_out8(cyber_mmio, SEQDATA, 0x00);
		}
	}

	cyber_grphw(0xfa, 0x10);
	for (i=0; i<16; i++) {
		cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
		cyber_out8(cyber_mmio, SEQDATA, 0x00);
	}

	cyber_grphw(0xfa, 0x18);
	for (i=0; i<16; i++) {
		cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
		cyber_out8(cyber_mmio, SEQDATA, 0x00);
	}

	cyber_grphw(0xfa, 0x20);
	for (i=0; i<16; i++) {
		cyber_out8(cyber_mmio, SEQINDEX, 0x40 + i);
		cyber_out8(cyber_mmio, SEQDATA, 0x00);
	}

	cyber_out8(cyber_mmio, SEQINDEX, 0xA6);
	/*for video capture*/
	cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) & 0xF0);

#if 0
	/*for 8-bit Index mode*/
	if(bEnabled8Bit) /*if we are in 8-bit alpha-blending mode, remember to disable it*/
		EnablePaletteMode(0);
#endif
	
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 0x80);
	cyber_out8(cyber_mmio, GRAINDEX, 0xe0);
	cyber_out8(cyber_mmio, GRADATA, cyber_in8(cyber_mmio, 0x03cf) | 0x04);
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 0x00);
}

void cyber_enable_alpha(int enable)
{
	cyber_grphw(0xfa, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4b);
	if (enable)
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) | 0x80));
     else
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & 0x7F));
}

void cyber_enable_fullscreen_alpha(int enable)
{
	cyber_grphw(0xfa, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4b);
	if (enable)
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) | 0x40));
     else
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & 0xBF));
}

void cyber_select_blend_src1(int src)
{
	cyber_grphw(0xfa, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x49);
	cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & ~0x03) | src);
}

void cyber_select_blend_src2(int src)
{
	cyber_grphw(0xfa, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4d);
	cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & ~0x30) | (src << 4));
	if(src == SRC2_OVERLAY1) { /*if source is Overlay one only, disable Overlay 2*/
		cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
		cyber_out8(cyber_mmio, GRADATA, 0x08);
		cyber_out8(cyber_mmio, SEQINDEX, 0x4f);
		cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) | 0x08);
		cyber_out8(cyber_mmio, GRADATA, 0x00);
	}
}

void cyber_select_alpha_src(int src)
{
	cyber_grphw(0xfa, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x49);
	cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & ~0x60) | (src << 5));
	/*if alpha source comes form Overlay2, we need to disable Overlay2 color key function*/
	if(src == ALPHA_OVERLAY2) {
		/*Disable Overlay 2 in Source B path*/
		cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
		cyber_out8(cyber_mmio, GRADATA, 0x08);
		cyber_out8(cyber_mmio, SEQINDEX, 0x4f);
		cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) | 0x08);
		/*Disable V2 generally      */
		cyber_out8(cyber_mmio, GRADATA, 0x20);
		cyber_out8(cyber_mmio, SEQINDEX, 0x47);
		cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) | 0x02);
		cyber_out8(cyber_mmio, GRADATA, 0x00);
	}
}

void cyber_set_alpha_reg(unsigned char r, unsigned char g, unsigned char b)
{
	cyber_grphw(0xfa, 0);
	
	cyber_seqw(0x46, r);
	cyber_seqw(0x47, g);
	cyber_seqw(0x48, b);
}


void cyber_set_magic_match_reg( unsigned char bR, unsigned char bG, unsigned char bB )
{
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 8);
	/*Disable range feature first*/
	cyber_out8(cyber_mmio, SEQINDEX, 0x46);
	cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) & 0x7F);

	cyber_out8(cyber_mmio, SEQINDEX, 0x40);
	cyber_out8(cyber_mmio, SEQDATA, bR);
	cyber_out8(cyber_mmio, SEQINDEX, 0x41);
	cyber_out8(cyber_mmio, SEQDATA, bG);
	cyber_out8(cyber_mmio, SEQINDEX, 0x42);
	cyber_out8(cyber_mmio, SEQDATA, bB);
}

void cyber_set_alpha_RAM_reg( unsigned char bIndex, unsigned char bR, unsigned char bG, unsigned char bB)
{
	unsigned char bData;
	
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x49);
	bData = cyber_in8(cyber_mmio, SEQDATA);
	cyber_out8(cyber_mmio, SEQDATA, 0x18);   /*select CPU to write*/

	cyber_out8(cyber_mmio, SEQINDEX, 0x4e);   /*enable index of alpha RAM R*/
	cyber_out8(cyber_mmio, SEQDATA, 0x20+bIndex);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4f);   /*RAM data port*/
	cyber_out8(cyber_mmio, SEQDATA, bR);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4e);   /*enable index of alpha RAM G*/
	cyber_out8(cyber_mmio, SEQDATA, 0x40+bIndex);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4f);   /*RAM data port*/
	cyber_out8(cyber_mmio, SEQDATA, bG);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4e);   /*enable index of alpha RAM B*/
	cyber_out8(cyber_mmio, SEQDATA, 0x80+bIndex);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4f);   /*RAM data port*/
	cyber_out8(cyber_mmio, SEQDATA, bB);

	cyber_out8(cyber_mmio, SEQINDEX, 0x49);
	cyber_out8(cyber_mmio, SEQDATA, bData);

	cyber_out8(cyber_mmio, SEQINDEX, 0x4e);   /*Set index of alpha RAM */
	cyber_out8(cyber_mmio, SEQDATA, bIndex);
}

void cyber_select_RAM_addr( unsigned char bRAMAddrSel )
{
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 0);

	cyber_out8(cyber_mmio, SEQINDEX, 0x49);
	cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & ~0x18) | (bRAMAddrSel << 3));
}

void cyber_enable_magic_alpha_blend( unsigned char enable )
{
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 8);

	cyber_out8(cyber_mmio, SEQINDEX, 0x46);
	if (enable)
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) | 0x01));
	else
		cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & 0xFE));
		
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 0x20);
	cyber_out8(cyber_mmio, SEQINDEX, 0x47);
	cyber_out8(cyber_mmio, SEQDATA, cyber_in8(cyber_mmio, SEQDATA) & 0x7F);
	cyber_out8(cyber_mmio, GRADATA, 0x00);
}

void cyber_select_magic_alpha_src( unsigned char bAlphaSrc )
{
	cyber_out8(cyber_mmio, GRAINDEX, 0xfa);
	cyber_out8(cyber_mmio, GRADATA, 8);

	cyber_out8(cyber_mmio, SEQINDEX, 0x46);
	cyber_out8(cyber_mmio, SEQDATA, (cyber_in8(cyber_mmio, SEQDATA) & ~0x0C) | (bAlphaSrc << 2));
}
