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

#ifndef _S3BCI_H_
#define _S3BCI_H_

#define REVERSE_BYTE_ORDER32(dword) {\
    unsigned int temp; \
    dword =  (temp & 0xFF) << 24; \
    dword |= (temp & 0xFF00) << 8; \
    dword |= (temp & 0xFF0000) >> 8; \
    dword |= (temp & 0xFF000000) >> 24; }


#define BCI_SIZE 0x4000

#define BCI_SEND(dw)   { \
                           if (sdev->s.bci_ptr == BCI_SIZE) sdev->s.bci_ptr = 0; \
                           sdrv->s.bci_base[sdev->s.bci_ptr++] = (__u32)(dw); \
                       }

#define BCI_SENDF(dw)  { \
                           if (sdev->s.bci_ptr == BCI_SIZE) sdev->s.bci_ptr = 0; \
                           ((float*)sdrv->s.bci_base)[sdev->s.bci_ptr++] = (float)(dw); \
                       }


#define BCI_CMD_NOP                  0x40000000
//#define BCI_CMD_SETREG               0x96000000 /* 8}CMD | 8}count | 16}index */
#define BCI_CMD_RECT                 0x48000000
#define BCI_CMD_RECT_XP              0x01000000
#define BCI_CMD_RECT_YP              0x02000000
#define BCI_CMD_SCANLINE             0x50000000
#define BCI_CMD_LINE                 0x5C000000
#define BCI_CMD_LINE_LAST_PIXEL      0x58000000
#define BCI_CMD_BYTE_TEXT            0x63000000
#define BCI_CMD_NT_BYTE_TEXT         0x67000000
#define BCI_CMD_BIT_TEXT             0x6C000000
#define BCI_CMD_GET_ROP(cmd)         (((cmd) >> 16) & 0xFF)
#define BCI_CMD_SET_ROP(cmd, rop)    ((cmd) |= ((rop & 0xFF) << 16))
#define BCI_CMD_SEND_COLOR           0x00008000

#define BCI_CMD_CLIP_NONE            0x00000000
#define BCI_CMD_CLIP_CURRENT         0x00002000
#define BCI_CMD_CLIP_LR              0x00004000
#define BCI_CMD_CLIP_NEW             0x00006000

#define BCI_CMD_DEST_GBD             0x00000000
#define BCI_CMD_DEST_PBD             0x00000800
#define BCI_CMD_DEST_PBD_NEW         0x00000C00
#define BCI_CMD_DEST_SBD             0x00001000
#define BCI_CMD_DEST_SBD_NEW         0x00001400

#define BCI_CMD_SRC_TRANSPARENT      0x00000200
#define BCI_CMD_SRC_SOLID            0x00000000
#define BCI_CMD_SRC_GBD              0x00000020
#define BCI_CMD_SRC_COLOR            0x00000040
#define BCI_CMD_SRC_MONO             0x00000060
#define BCI_CMD_SRC_PBD_COLOR        0x00000080
#define BCI_CMD_SRC_PBD_MONO         0x000000A0
#define BCI_CMD_SRC_PBD_COLOR_NEW    0x000000C0
#define BCI_CMD_SRC_PBD_MONO_NEW     0x000000E0
#define BCI_CMD_SRC_SBD_COLOR        0x00000100
#define BCI_CMD_SRC_SBD_MONO         0x00000120
#define BCI_CMD_SRC_SBD_COLOR_NEW    0x00000140
#define BCI_CMD_SRC_SBD_MONO_NEW     0x00000160

#define BCI_CMD_PAT_TRANSPARENT      0x00000010
#define BCI_CMD_PAT_NONE             0x00000000
#define BCI_CMD_PAT_COLOR            0x00000002
#define BCI_CMD_PAT_MONO             0x00000003
#define BCI_CMD_PAT_PBD_COLOR        0x00000004
#define BCI_CMD_PAT_PBD_MONO         0x00000005
#define BCI_CMD_PAT_PBD_COLOR_NEW    0x00000006
#define BCI_CMD_PAT_PBD_MONO_NEW     0x00000007
#define BCI_CMD_PAT_SBD_COLOR        0x00000008
#define BCI_CMD_PAT_SBD_MONO         0x00000009
#define BCI_CMD_PAT_SBD_COLOR_NEW    0x0000000A
#define BCI_CMD_PAT_SBD_MONO_NEW     0x0000000B

#define BCI_BD_BW_DISABLE            0x10000000
#define BCI_BD_TILE_MASK             0x03000000
#define BCI_BD_TILE_NONE             0x00000000
#define BCI_BD_TILE_16               0x02000000
#define BCI_BD_TILE_32               0x04000000
#define BCI_BD_GET_BPP(bd)           (((bd) >> 16) & 0xFF)
#define BCI_BD_SET_BPP(bd, bpp)      ((bd) |= (((bpp) & 0xFF) << 16))
#define BCI_BD_GET_STRIDE(bd)        ((bd) & 0xFFFF)
#define BCI_BD_SET_STRIDE(bd, st)    ((bd) |= ((st) & 0xFFFF))

#define BCI_W_H(w, h)                (((h) << 16) | ((w) & 0xFFF))
#define BCI_X_Y(x, y)                (((y) << 16) | ((x) & 0xFFF))
#define BCI_X_W(x, y)                (((w) << 16) | ((x) & 0xFFF))
#define BCI_CLIP_LR(l, r)            (((r) << 16) | ((l) & 0xFFF))
#define BCI_CLIP_TL(t, l)            (((t) << 16) | ((l) & 0xFFF))
#define BCI_CLIP_BR(b, r)            (((b) << 16) | ((r) & 0xFFF))

#define BCI_LINE_X_Y(x, y)           (((y) << 16) | ((x) & 0xFFFF))
#define BCI_LINE_STEPS(diag, axi)    (((axi) << 16) | ((diag) & 0xFFFF))
#define BCI_LINE_MISC(maj, ym, xp, yp, err) \
(((maj) & 0xFFF) | (((ym) & 1) << 13) | \
(((xp) & 1) << 14) | (((yp) & 1) << 15) | \
((err) << 16))


/* definition of BCI register indices */
#define BCI_VERTEX0             0x00 
#define BCI_VERTEX1             0x08 
#define BCI_VERTEX2             0x10 
#define BCI_TEXPALADDR          0x18
#define BCI_COLORKEY            0x19 
#define BCI_TEXADDR             0x1A 
#define BCI_TEXDESC             0x1B
#define BCI_TEXCTRL             0x1C 
#define BCI_FOGTABLE            0x20 
#define BCI_FOGCTRL             0x30 
#define BCI_DRAWCTRL            0x31
#define BCI_ZBCTRL              0x32  
#define BCI_ZBADDR              0x33  
#define BCI_DESTCTRL            0x34  
#define BCI_SCSTART             0x35  
#define BCI_SCEND               0x36  
#define BCI_ZWATER              0x37
#define BCI_DWATER              0x38



// 8}CMD|8}count|16}skipflags
#define BCI_CMD_TRILIST   0x80000000L
#define BCI_CMD_TRISTRIP  0x82000000L
#define BCI_CMD_TRIFAN    0x84000000L
#define BCI_CMD_QUADLIST  0x86000000L
// or this one with previous commands if this vertex list 
// is continuation of previous one
#define BCI_CMD_CONTINUE  0x01000000L
// set any register that has bci index 8}CMD|8}count|16}index
#define BCI_CMD_SETREG    0x96000000L
// update shadow status 8}CMD|24}tag 
#define BCI_CMD_UPDSHADOW 0x98000000L

#define BCI_CMD_WAIT       0xC0000000L 
#define BCI_WAIT_3D_IDLE   0x00010000L
#define BCI_WAIT_2D_IDLE   0x00020000L
#define BCI_WAIT_PAGEFLIP  0x01000000L
#define BCI_WAIT_SCANLINE  0x02000000L

#define BCI_SKIP_Z        0x01
#define BCI_SKIP_W        0x02
#define BCI_SKIP_DIFFUSE  0x04
#define BCI_SKIP_SPECULAR 0x08
#define BCI_SKIP_U        0x10
#define BCI_SKIP_V        0x20

/* definition of BCI register indices */
#define BCI_VERTEX0		0x00    
#define BCI_VERTEX1    		0x08
#define BCI_VERTEX2             0x10
#define BCI_TEXPALADDR          0x18
#define BCI_COLORKEY            0x19
#define BCI_TEXADDR             0x1A
#define BCI_TEXDESC             0x1B
#define BCI_TEXCTRL             0x1C
#define BCI_FOGTABLE            0x20
#define BCI_FOGCTRL             0x30
#define BCI_DRAWCTRL            0x31
#define BCI_ZBCTRL              0x32
#define BCI_ZBADDR              0x33
#define BCI_DESTCTRL            0x34
#define BCI_SCSTART             0x35
#define BCI_SCEND               0x36
#define BCI_ZWATER              0x37 
#define BCI_DWATER              0x38

/* 2D regs */
#define BCI_GBD1                0xE0
#define BCI_GBD2                0xE1
#define BCI_PBD1                0xE2
#define BCI_PBD2                0xE3
#define BCI_SBD1                0xE4
#define BCI_SBD2                0xE5


#endif /* _S3BCI_H_ */
