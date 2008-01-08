/*
   TI Davinci driver - C64X+ DSP Firmware Interface

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org> and
              Olaf Dreesen <dreesen@qarx.de>.

   All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   version 2 as published by the Free Software Foundation.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   General Public License for more details.

   You should have received a copy of the GNU General Public
   License along with this library; if not, write to the
   Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.
*/

#ifndef __C64X_H__
#define __C64X_H__


typedef volatile struct {
     uint32_t  c64x_function;
     uint32_t  c64x_arg[7];
} c64xTask;


#define c64x_return	c64x_arg[0]
#define c64x_errno	c64x_arg[1]

#define c64x_flags	c64x_function

#define C64X_STATE_DONE		0
#define C64X_STATE_ERROR	1
#define C64X_STATE_TODO		2
#define C64X_STATE_RUNNING	3

#define C64X_FLAG_RUN       1
#define C64X_FLAG_TODO      2

#define C64X_TASK_STATE(task) ((task)->c64x_flags & 3)

typedef volatile struct {
     uint32_t  QH_dsp;
     uint32_t  QH_arm;
     uint32_t  QL_dsp;
     uint32_t  QL_arm;
     uint32_t  idlecounter;
} c64xTaskControl;


/* function macro */
#define _C64XFUNC(val)             (((val)&0x3fff)<<2)


/*  USED INTERNALLY
void c64x_mpeg2_idct(int16_t*src,int16_t*dst);
*/
#define C64X_MPEG2_IDCT	           _C64XFUNC(0)

/*
void c64x_dither_argb(u32*dst_rgb, u8*dst_alpha, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height);
*/
#define C64X_DITHER_ARGB	       _C64XFUNC(1)

/*
void c64x_fill_16(u16*dst, u32 pitch, u32 width, u32 height, u16 val);
void c64x_fill_32(u32*dst, u32 pitch, u32 width, u32 height, u32 val);
*/
#define C64X_FILL_16	           _C64XFUNC(2)
#define C64X_FILL_32	           _C64XFUNC(3)

/*
void c64x_copy_16(u16*dst, u32 dst_pitch, u16*src, u32 src_pitch, u32 width, u32 height);
void c64x_copy_32(u32*dst, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height);
*/
#define C64X_COPY_16	           _C64XFUNC(4)
#define C64X_COPY_32	           _C64XFUNC(5)

/*
void c64x_blend_32(u32*dst, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height, u8 alpha);
*/
#define C64X_BLEND_32	           _C64XFUNC(7)

/*
void c64x_copy_keyed_16(u16*dst, u32 pitches, u16*src, u32 width, u32 height, u16 key, u16 mask);
void c64x_copy_keyed_32(u32*dst, u32 pitches, u32*src, u32 width, u32 height, u32 key, u32 mask);
*/
#define C64X_COPY_KEYED_16	       _C64XFUNC(8)
#define C64X_COPY_KEYED_32	       _C64XFUNC(9)

/*
void c64x_write_back_all(void);
*/
#define C64X_WRITE_BACK_ALL	       _C64XFUNC(15)

/*  USED INTERNALLY
void c64x_dezigzag(u16*dst, u16*src);
*/
#define C64X_DEZIGZAG              _C64XFUNC(16)

/*
void c64x_put_uyvy_16x16(u16*dst, u32 pitch, u8*src);
*/
#define C64X_PUT_UYVY_16x16        _C64XFUNC(18)

/*
void c64x_fetch_uyvy(u8 *dst, u8 *src, u32 spitch, u32 height, u32 flags);
*/
#define C64X_FETCH_UYVY            _C64XFUNC(19)

/*  USED INTERNALLY
void mc_put_o_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_x_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_y_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_xy_8 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
*/
#define C64X_MC_PUT_8  (avgX,avgY) _C64XFUNC(32+avgX+avgY+avgY)

/*  USED INTERNALLY
void mc_put_o_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_x_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_y_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_xy_16(u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
*/
#define C64X_MC_PUT_16 (avgX,avgY) _C64XFUNC(36+avgX+avgY+avgY)

/*  USED INTERNALLY
void mc_avg_o_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_x_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_y_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_xy_8 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
*/
#define C64X_MC_AVG_8  (avgX,avgY) _C64XFUNC(40+avgX+avgY+avgY)

/*  USED INTERNALLY
void mc_avg_o_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_x_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_y_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_xy_16(u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
*/
#define C64X_MC_AVG_16 (avgX,avgY) _C64XFUNC(44+avgX+avgY+avgY)

/*
void c64x_load_block(s32*blockwords, u32 num_words, u32 cbp);
*/
#define C64X_LOAD_BLOCK            _C64XFUNC(48)

/*  USED INTERNALLY
void c64x_saturate(u8*dest, u16 *source);
*/
#define C64X_SATURATE              _C64XFUNC(49)


#endif
