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

#include <directfb.h>

#include <core/coredefs.h>

#include "cyber5k.h"
#include "cyber5k_overlay.h"
#include "regs.h"
#include "mmio.h"

int overlay_byte_per_pixel = 2;
int overlay_init = 0;

unsigned char savedReg74, savedReg75; /*FIFO control registers for 2D Graphics*/
unsigned char savedRegD9[2], savedRegDA[2], savedRegDD[2]; /*FIFO control registers for Overlay*/
/*Following is our FIFO policy number, should be programmed to
0x3CE/0x74, 0x3CE/0x75, 0x3CE(0x3C4)/0xD9, 0x3CE(0x3C4)/0xDA,
0x3CE(0x3c4)/0xDD respectively in order to get a best memory bandwidth.
Current value is a group of experence value based on 70MHZ EDO/SG RAM.*/
unsigned char bFIFOPolicyNum[5] = {0x10, 0x10, 0x1C, 0x1C, 0x06};


void cyber_videoreg_mask( unsigned char index, unsigned char value, unsigned char mask )
{
     unsigned char tmp;

     cyber_out8( mmio_base, GRAINDEX, index );
     tmp = cyber_in8( mmio_base, GRADATA );
     tmp &= mask;
     tmp |= value;
     cyber_out8( mmio_base, GRADATA, tmp );
}

void cyber_seqreg_mask( unsigned char index, unsigned char value, unsigned char mask )
{
     unsigned char tmp;

     cyber_out8( mmio_base, SEQINDEX, index );
     tmp = cyber_in8( mmio_base, SEQDATA );

     tmp &= mask;
     tmp |= value;
     cyber_out8( mmio_base, SEQDATA, tmp );
}

void cyber_overlayreg_mask( unsigned char index, unsigned char value, unsigned char mask ) {
     unsigned char tmp;

     cyber_out8( mmio_base, GRAINDEX, index );
     tmp = cyber_in8( mmio_base, GRADATA );

     tmp &= mask;
     tmp |= value;
     cyber_out8(mmio_base, GRADATA, tmp);
}

void cyber_cleanup_overlay()
{
     /*restore FIFO control regs*/
     cyber_seqreg_mask(0xA7, 0x0, ~0x5);


     if (!overlay_init)
          return;
     overlay_init = 0;


     cyber_grphw(0x74, savedReg74);
     cyber_grphw(0x75, savedReg75);

     cyber_grphw(0xD9, savedRegD9[0]);
     cyber_grphw(0xDA, savedRegDA[0]);
     cyber_grphw(0xDD, savedRegDD[0]);

     cyber_seqw(0xD9, savedRegD9[1]);
     cyber_seqw(0xDA, savedRegDA[1]);
     cyber_seqw(0xDD, savedRegDD[1]);
}

void cyber_init_overlay(void)
{
     /*Clear Overlay path first*/
     cyber_grphw(DISP_CTL_I, 0x00);

     /* Video Display Vertical Starting Line (may not need initiate here)*/
     cyber_grphw(DEST_RECT_TOP_L, 0x00);
     cyber_grphw(DEST_RECT_TOP_H, 0x00);

     /* Overlay Vertical DDA Increment Value*/
     cyber_grphw(DDA_Y_INC_L, 0x00);
     cyber_grphw(DDA_Y_INC_H, 0x10);

     /* Video Memory Starting Address*/
     cyber_grphw(MEMORY_START_L, 0x00);
     cyber_grphw(MEMORY_START_M, 0x0f);
     cyber_grphw(MEMORY_START_H, 0x03);  /* Temporary fixed to 0x30f00 = 0xc3c00 >> 2*/
     /* 0x3c00 = 0x300*0x14 = 768*20*/

     /* Video Display Horizontal Starting Pixel -- may not need init here*/
     cyber_grphw(DEST_RECT_LEFT_L, 0x20);
     cyber_grphw(DEST_RECT_LEFT_H, 0x00);

     /* Video Display Horizontal Ending Pixel -- may not need init here*/
     cyber_grphw(DEST_RECT_RIGHT_L, 0x60);
     cyber_grphw(DEST_RECT_RIGHT_H, 0x01);

     /* Video Display Vertical Ending Line -- may not need init here*/
     cyber_grphw(DEST_RECT_BOTTOM_L, 0xe0);
     cyber_grphw(DEST_RECT_BOTTOM_H, 0x00);

     /* Video Color Compare Register*/
     cyber_grphw(COLOR_CMP_RED,  0x00);
     cyber_grphw(COLOR_CMP_GREEN,0x00);
     cyber_grphw(COLOR_CMP_BLUE, 0x00);

     /* Video Horizontal DDA Increment Value*/
     cyber_grphw(DDA_X_INC_L, 0x00);
     cyber_grphw(DDA_X_INC_H, 0x10);

     /* Video Format Control*/
     cyber_grphw(VIDEO_FORMAT, 0x00);

     /* Video Misc Control*/
     cyber_grphw(MISC_CTL_I, 0x00);

     cyber_grphw(MISC_CTL_I, 0x01); /* Video Misc Control*/

     /*default to colorkey*/
     cyber_grphw(DISP_CTL_I, 0x04 );

#if NTSCTVOUT /*if your TV output mode is NTSC*/
     cyber_seqreg_mask(0xA6, 0x20, ~0x30);
#else  /*if your TV output mode is PAL*/
     cyber_seqreg_mask(0xA6, 0x30, ~0x30);
#endif


     if (overlay_init)
          return;
     overlay_init = 1;



/* the following code is commented out, since saved values are not clean if */
/* DirectFB crashed while underlay was enabled, hardcoded bootup            */
/* values instead (see below)                                               */

/*
     cyber_out8(mmio_base, GRAINDEX, 0x74);
     savedReg74 = cyber_in8(mmio_base, GRADATA);
     cyber_out8(mmio_base, GRAINDEX, 0x75);
     savedReg75 = cyber_in8(mmio_base, GRADATA);

     cyber_out8(mmio_base, GRAINDEX, 0xD9);
     savedRegD9[0] = cyber_in8(mmio_base, GRADATA);
     cyber_out8(mmio_base, GRAINDEX, 0xDA);
     savedRegDA[0] = cyber_in8(mmio_base, GRADATA);
     cyber_out8(mmio_base, GRAINDEX, 0xDD);
     savedRegDD[0] = cyber_in8(mmio_base, GRADATA);

     cyber_out8(mmio_base, SEQINDEX, 0xD9);
     savedRegD9[1] = cyber_in8(mmio_base, SEQDATA);
     cyber_out8(mmio_base, SEQINDEX, 0xDA);
     savedRegDA[1] = cyber_in8(mmio_base, SEQDATA);
     cyber_out8(mmio_base, SEQINDEX, 0xDD);
     savedRegDD[1] = cyber_in8(mmio_base, SEQDATA);
     */


     savedReg74    = 0x1b;
     savedReg74    = 0x1e;

     savedRegD9[0] = 0x0f;
     savedRegDA[0] = 0x1b;
     savedRegDD[0] = 0x00;

     savedRegD9[1] = 0x0f;
     savedRegDA[1] = 0x1b;
     savedRegDD[1] = 0x00;
}

void cyber_change_overlay_fifo()
{
     cyber_grphw(0x74, bFIFOPolicyNum[0]);
     cyber_grphw(0x75, bFIFOPolicyNum[1]);
     cyber_grphw(0xD9, bFIFOPolicyNum[2]);
     cyber_grphw(0xDA, bFIFOPolicyNum[3]);

     cyber_videoreg_mask(0xA6, 0x08, ~0x08);
     cyber_videoreg_mask(0xF1, 0x40, (unsigned char)(~0xC0));
     cyber_overlayreg_mask(FIFO_CTL_I, bFIFOPolicyNum[4] & 0x05, ~0x05);
     cyber_overlayreg_mask(FIFO_CTL_I, 0x2, ~0x02);
}

void cyber_set_overlay_format(int format) {
     switch (format) {
          case OVERLAY_YUV422:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x00, 0xF8 );
               overlay_byte_per_pixel = 2;
               break;
          case OVERLAY_RGB555:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x01, 0xF8 );
               overlay_byte_per_pixel = 2;
               break;
          case OVERLAY_RGB565:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x02, 0xF8 );
               overlay_byte_per_pixel = 2;
               break;
          case OVERLAY_RGB888:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x03, 0xF8 );
               overlay_byte_per_pixel = 3;
               break;
          case OVERLAY_RGB8888:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x04, 0xF8 );
               overlay_byte_per_pixel = 4;
               break;
          case OVERLAY_RGB8:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x05, 0xF8 );
               overlay_byte_per_pixel = 1;
               break;
          case OVERLAY_RGB4444:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x06, 0xF8 );
               overlay_byte_per_pixel = 2;
               break;
          case OVERLAY_RGB8T:
               cyber_overlayreg_mask( VIDEO_FORMAT, 0x07, 0xF8 );
               overlay_byte_per_pixel = 1;
               break;
     }
}

void cyber_set_overlay_mode(int mode)
{
     switch (mode) {
          case OVERLAY_COLORKEY:
               cyber_overlayreg_mask( DISP_CTL_I, 0x00, 0xFD );
               break;
          case OVERLAY_WINDOWKEY:
          default:
               cyber_overlayreg_mask( DISP_CTL_I, 0x02, 0xFD );
               break;
     }
}

void cyber_set_overlay_srcaddr(int addr, int x, int y, int width, int pitch)
{
     unsigned char bHigh;
     int wByteFetch;

     addr += y * pitch  +  x * overlay_byte_per_pixel;
     addr >>= 2;

     /*playback start addr*/
     cyber_grphw( MEMORY_START_L, (unsigned char)( addr & 0x0000FF) );
     cyber_grphw( MEMORY_START_M, (unsigned char)((addr & 0x00FF00) >> 8) );
     cyber_grphw( MEMORY_START_H, (unsigned char)((addr & 0xFF0000) >> 16) );

     /* pitch is a multiple of 64 bits*/
     pitch >>= 3; /* 64 bit address field*/
     wByteFetch = (width * overlay_byte_per_pixel + 7) >> 3;

     bHigh = (unsigned char)(pitch >> 8) & 0x0F;
     bHigh = bHigh | (((unsigned char)(wByteFetch >> 8)) << 4 );

     cyber_grphw( MEMORY_PITCH_L, (unsigned char)(pitch) );
     cyber_grphw( MEMORY_PITCH_H, bHigh );

     cyber_grphw( MEMORY_OFFSET_PHASE, (unsigned char)(wByteFetch) );

     if (width > 720)  /*Turn off interpolation*/
          cyber_overlayreg_mask( DISP_CTL_I, 0x20, 0xDF );
     else { /*Turn off interpolation*/
          if (width > 360) { /* Y Only*/
               cyber_seqreg_mask(0xA6, 0x40, ~0x40);
          }
          else {
               cyber_seqreg_mask(0xA6, 0x00, ~0x40);
          }

          cyber_overlayreg_mask( DISP_CTL_I, 0x00, 0xDF );
     }
}

void cyber_set_overlay_window(int left, int top, int right, int bottom)
{
     cyber_grphw( DEST_RECT_LEFT_L,  (unsigned char)(left      ) );
     cyber_grphw( DEST_RECT_LEFT_H,  (unsigned char)(left  >> 8) );
     cyber_grphw( DEST_RECT_RIGHT_L, (unsigned char)(right     ) );
     cyber_grphw( DEST_RECT_RIGHT_H, (unsigned char)(right >> 8) );

     cyber_grphw( DEST_RECT_TOP_L,    (unsigned char)(top        ) );
     cyber_grphw( DEST_RECT_TOP_H,    (unsigned char)(top    >> 8) );
     cyber_grphw( DEST_RECT_BOTTOM_L, (unsigned char)(bottom     ) );
     cyber_grphw( DEST_RECT_BOTTOM_H, (unsigned char)(bottom >> 8) );
}

void cyber_set_overlay_scale( unsigned char bEnableBob, int wSrcXExt, int wDstXExt, int wSrcYExt, int wDstYExt )
{
     int dwScale;

     cyber_grphw( DDA_X_INIT_L, 0x0 );     /* set to 0x800;*/
     cyber_grphw( DDA_X_INIT_H, 0x8 );
     if ( wSrcXExt == wDstXExt )
          dwScale = 0x1000;
     else
          dwScale = ( wSrcXExt * 0x1000 ) / wDstXExt;
     cyber_grphw( DDA_X_INC_L, (unsigned char)( dwScale & 0x00FF) );
     cyber_grphw( DDA_X_INC_H, (unsigned char)((dwScale & 0xFF00) >> 8) );

     cyber_grphw( DDA_Y_INIT_L, 0x0 );     /* set to 0x800;*/
     cyber_grphw( DDA_Y_INIT_H, 0x8 );

     if ( wSrcYExt == wDstYExt )
          dwScale = 0x1000;
     else
          dwScale = ( wSrcYExt * 0x1000 ) / wDstYExt;


     if (bEnableBob == 0) {/*Disable Bob mode*/
          cyber_seqreg_mask(0xA7, 0x0, ~0x5); /*Bob/Weave disable*/
     }
     else {/*Enable Bob mode*/
          wSrcYExt = wSrcYExt / 2;
	     if (wSrcYExt == wDstYExt)
	          dwScale = 0x1000;   
	     else
	          dwScale = ( wSrcYExt * 0x1000 ) / wDstYExt;
	     if (dwScale <= 0x815 && dwScale >= 0x7eb) {
	          cyber_seqreg_mask(0xA7, 0x5, ~0x5); /*Bob/Weave enable*/
	     }
	     else {
	          cyber_seqreg_mask(0xA7, 0x4, ~0x5); /*Bob/Weave enable*/
	     }
	}

     cyber_grphw( DDA_Y_INC_L, (unsigned char)( dwScale & 0x00FF) );
     cyber_grphw( DDA_Y_INC_H, (unsigned char)((dwScale & 0xFF00) >> 8) );
}

void cyber_enable_overlay(int enable)
{
     if (enable)
          cyber_overlayreg_mask( DISP_CTL_I, 0x84, (unsigned char)(~0x84) );
     else
          cyber_overlayreg_mask( DISP_CTL_I, 0x00, 0x7F );  /* Disable Vafc !!!*/
}
