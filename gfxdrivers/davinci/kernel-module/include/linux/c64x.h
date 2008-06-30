/*
   TI Davinci driver - C64X+ DSP Firmware Interface

   (c) Copyright 2008  directfb.org
   (c) Copyright 2007  Telio AG

   Written by Olaf Dreesen <olaf@directfb.org> and
              Denis Oliver Kropp <dok@directfb.org>.

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

#ifndef __KERNEL__
#include <stdint.h>
#endif


#ifndef DAVINCI_C64X_MEM
#define DAVINCI_C64X_MEM 0x8e000000
#endif


typedef volatile struct {
     uint32_t  c64x_function;
     uint32_t  c64x_arg[7];
} c64xTask;


#define c64x_return	c64x_arg[0]
#define c64x_errno	c64x_arg[1]

#define c64x_flags	c64x_function

typedef enum {
     C64X_STATE_DONE     = 0,
     C64X_STATE_ERROR    = 1,
     C64X_STATE_TODO     = 2,
     C64X_STATE_RUNNING  = 3
} C64XTaskState;

typedef enum {
     C64X_FLAG_RUN       = 1,
     C64X_FLAG_TODO      = 2,
     C64X_FLAG_INTERRUPT = 0x80000000
} C64XTaskFlags;

#define C64X_TASK_STATE(task) ((task)->c64x_flags & 3)

typedef volatile struct {
     uint32_t  QH_dsp;
     uint32_t  QH_arm;
     uint32_t  QL_dsp;
     uint32_t  QL_arm;
     uint32_t  idlecounter;
} c64xTaskControl;

#define C64X_QUEUE_LENGTH     0x4000
#define C64X_QUEUE_MASK       0x3fff


typedef enum {
     C64X_BLEND_SRC_INVSRC               = 2,  /* old school fader on all channels including alpha on itself */
     C64X_BLEND_ONE_INVSRC               = 1,  /* SrcOver using premultiplied alpha channel */
     C64X_BLEND_ONE_INVSRC_PREMULT_SRC   = 0,  /* SrcOver doing premultiplication of source S[rgb] with S[a] */
     C64X_BLEND_ONE_INVSRC_PREMULT_ALPHA = 3,  /* SrcOver with Alpha using premultiplied alpha channel,
                                                  but doing premultiplication of S[rgb] with Alpha */
} C64XBlendSubFunction;

typedef enum {
     C64X_FLUSH_WRITE_BACK,
     C64X_FLUSH_WB_INVALIDATE,
     C64X_FLUSH_INVALIDATE
} C64XFlushFunction;

#define C64X_IOCTL_RESET      _IO( 'c', 0 )
#define C64X_IOCTL_WAIT_LOW   _IO( 'c', 1 )


/* function macro */
#define _C64XFUNC(val)             (((val)&0x3fff)<<2)


#define C64X_NOP	                _C64XFUNC(0)

/*
void c64x_dither_argb(u32*dst_rgb, u8*dst_alpha, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height);
*/
#define C64X_DITHER_ARGB	        _C64XFUNC(1)

/*
void c64x_fill_16(u16*dst, u32 pitch, u32 width, u32 height, u16 val);
void c64x_fill_32(u32*dst, u32 pitch, u32 width, u32 height, u32 val);
*/
#define C64X_FILL_16	                _C64XFUNC(2)
#define C64X_FILL_32	                _C64XFUNC(3)

/*
void c64x_copy_16(u16*dst, u32 dst_pitch, u16*src, u32 src_pitch, u32 width, u32 height);
void c64x_copy_32(u32*dst, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height);
*/
#define C64X_COPY_16	                _C64XFUNC(4)
#define C64X_COPY_32	                _C64XFUNC(5)

/*
void c64x_blend_32(u32*dst, u32 dst_pitch, u32*src, u32 src_pitch, u32 width, u32 height, u8 alpha);
*/
//#define C64X_BLEND_16	                  _C64XFUNC(6)
#define C64X_BLEND_32	                _C64XFUNC(7)

/*
void c64x_copy_keyed_16(u16*dst, u32 pitches, u16*src, u32 width, u32 height, u16 key, u16 mask);
void c64x_copy_keyed_32(u32*dst, u32 pitches, u32*src, u32 width, u32 height, u32 key, u32 mask);
*/
#define C64X_COPY_KEYED_16	        _C64XFUNC(8)
#define C64X_COPY_KEYED_32	        _C64XFUNC(9)

/*
void c64x_stretch_32(u32 *dst, u32 *src, u32 pitches, u32 dsize, u32 ssize, u32 clip2, u32 clip1);
*/
#define C64X_STRETCH_32_up              _C64XFUNC(10)
#define C64X_STRETCH_32_down            _C64XFUNC(11)


/*
void c64x_wb_inv_range(u32 *start, u32 len, C64XFlushFunction func);
*/
#define C64X_WB_INV_RANGE	        _C64XFUNC(14)


/*
void c64x_write_back_all(void);
*/
#define C64X_WRITE_BACK_ALL	        _C64XFUNC(15)



/*
void c64x_load_block(s32*blockwords, u32 num_words, u32 cbp);
*/
#define C64X_LOAD_BLOCK                 _C64XFUNC(48)

/*
void c64x_put_idct_uyvy_16x16(u16*dst, u32 pitch, u32 flags);
*/
#define C64X_PUT_IDCT_UYVY_16x16        _C64XFUNC(49)

/*
void c64x_put_mc_uyvy_16x16(u16*dst, u32 pitch, u32 flags);
*/
#define C64X_PUT_MC_UYVY_16x16          _C64XFUNC(50)

/*
void c64x_put_sum_uyvy_16x16(u16*dst, u32 pitch, u32 flags);
*/
#define C64X_PUT_SUM_UYVY_16x16         _C64XFUNC(51)

/*
void c64x_dva_begin_frame(u32 pitch, u16 *current, u16 *past, u16 *future, u32 flags);
*/
#define C64X_DVA_BEGIN_FRAME            _C64XFUNC(52)

/*
void c64x_dva_motion(DVAMacroBlock *macroblock);
*/
#define C64X_DVA_MOTION_BLOCK           _C64XFUNC(53)

/*
void c64x_dva_idct(u16* dst, u32 pitch, u16* src);
*/
#define C64X_DVA_IDCT                   _C64XFUNC(59)



/*
 * INTERNAL - for testing
 */
#define C64X_FETCH_BUFFER_PITCH    32
#define C64X_TEMP_BUFFER_PITCH     32
#define C64X_MC_BUFFER_PITCH       16
#define C64X_IDCT_BUFFER_PITCH     32

#define C64X_FETCH_BUFFER_Y(n)     (0xf05840 + ((n) << 10))
#define C64X_FETCH_BUFFER_U(n)     (C64X_FETCH_BUFFER_Y(n) + 18*C64X_FETCH_BUFFER_PITCH)
#define C64X_FETCH_BUFFER_V(n)     (C64X_FETCH_BUFFER_U(n) + 16)

#define C64X_FETCH_BUFFER0_Y       C64X_FETCH_BUFFER_Y(0)
#define C64X_FETCH_BUFFER0_U       C64X_FETCH_BUFFER_U(0)
#define C64X_FETCH_BUFFER0_V       C64X_FETCH_BUFFER_V(0)

#define C64X_FETCH_BUFFER1_Y       C64X_FETCH_BUFFER_Y(1)
#define C64X_FETCH_BUFFER1_U       C64X_FETCH_BUFFER_U(1)
#define C64X_FETCH_BUFFER1_V       C64X_FETCH_BUFFER_V(1)

#define C64X_TEMP_BUFFER_Y         0xf06040
#define C64X_TEMP_BUFFER_U         (C64X_TEMP_BUFFER_Y + 16*C64X_TEMP_BUFFER_PITCH)
#define C64X_TEMP_BUFFER_V         (C64X_TEMP_BUFFER_U + 8)

#define C64X_MC_BUFFER_Y           0xf06440
#define C64X_MC_BUFFER_U           (C64X_MC_BUFFER_Y + 16*C64X_MC_BUFFER_PITCH)
#define C64X_MC_BUFFER_V           (C64X_MC_BUFFER_U + 8)

#define C64X_MC_BUFFER_Y_          (C64X_MC_BUFFER_Y + C64X_MC_BUFFER_PITCH)
#define C64X_MC_BUFFER_U_          (C64X_MC_BUFFER_U + C64X_MC_BUFFER_PITCH)
#define C64X_MC_BUFFER_V_          (C64X_MC_BUFFER_V + C64X_MC_BUFFER_PITCH)

#define C64X_IDCT_BUFFER_Y         0xf06a40
#define C64X_IDCT_BUFFER_U         (C64X_IDCT_BUFFER_Y + 16*C64X_IDCT_BUFFER_PITCH)
#define C64X_IDCT_BUFFER_V         (C64X_IDCT_BUFFER_U + 8)


/*  OBSOLETE
void c64x_dezigzag(u16*dst, u16*src);
*/
#define C64X_DEZIGZAG              _C64XFUNC(16)

/*  OBSOLETE
void c64x_dealternate(u16*dst, u16*src);
*/
#define C64X_DEALTERNATE           _C64XFUNC(17)

/*
void c64x_put_uyvy_16x16(u16*dst, u32 pitch, u8*src, u32 flags);
*/
#define C64X_PUT_UYVY_16x16        _C64XFUNC(18)

/*
void c64x_fetch_uyvy(u8 *dst, u8 *src, u32 spitch, u32 height);
*/
#define C64X_FETCH_UYVY            _C64XFUNC(19)

/*
void mc_put_o_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_x_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_y_8  (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_xy_8 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
*/
#define C64X_MC_PUT_8(avgX,avgY)   _C64XFUNC(32+(avgX)+(avgY)+(avgY))

/*
void mc_put_o_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_x_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_y_16 (u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
void mc_put_xy_16(u8*dst, u32 dstride, u8*ref_src, u8*ignored, u32 rstride, u32 height);
*/
#define C64X_MC_PUT_16(avgX,avgY)  _C64XFUNC(36+(avgX)+(avgY)+(avgY))

/*
void mc_avg_o_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_x_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_y_8  (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_xy_8 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
*/
#define C64X_MC_AVG_8(avgX,avgY)   _C64XFUNC(40+(avgX)+(avgY)+(avgY))

/*
void mc_avg_o_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_x_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_y_16 (u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
void mc_avg_xy_16(u8*dst, u32 dstride, u8*ref_src, u8*ref_dst, u32 rstride, u32 height);
*/
#define C64X_MC_AVG_16(avgX,avgY)  _C64XFUNC(44+(avgX)+(avgY)+(avgY))


#endif
