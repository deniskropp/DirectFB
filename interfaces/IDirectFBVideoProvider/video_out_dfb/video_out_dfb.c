/*
 * Copyright (C) 2004 Claudio "KLaN" Ciccani <klan82@cheapnet.it>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  video_out_dfb: unofficial xine video output driver using DirectFB
 *
 *
 *  NOTE: adjusting contrast is disabled in __dummy_* when output is RGB
 *
 *
 *  TODO: speed up at 24bpp and 32bpp, add support for overlays on YUV
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#include <directfb.h>
#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>
#include <display/idirectfbsurface.h>
#include <misc/util.h>

#include <xine.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>

#include "video_out_dfb.h"
#include "video_out_dfb_tables.h"


#define THIS  "video_out_dfb"


#define TEST(exp) \
{\
	if(!(exp))\
	{\
		fprintf(stderr, THIS \
			": at line %i [" #exp "] failed !!\n", \
			__LINE__);\
		goto FAILURE;\
	}\
}

#define SAY(fmt, ...) \
{\
	if(this->verbosity)\
		fprintf(stderr, THIS ": " fmt "\n", ## __VA_ARGS__);\
}

#define DBUG(fmt, ...) \
{\
	if(this->verbosity == XINE_VERBOSITY_DEBUG)\
		fprintf(stderr, THIS ": " fmt "\n", ## __VA_ARGS__);\
}

#define ONESHOT(fmt, ...) \
{\
	if(this->verbosity)\
	{\
		static int one = 1;\
		if(one)\
		{\
			fprintf(stderr, THIS ": " fmt "\n", ## __VA_ARGS__);\
			one = 0;\
		}\
	}\
}

#define release(ptr) \
{\
	if(ptr) free(ptr);\
	ptr = NULL;\
}


#ifdef DFB_DEBUG

static inline uint64_t
rdtsc(void)
{
	uint64_t t;

	__asm__ __volatile__(
		".byte 0x0f, 0x31\n\t"
		: "=A" (t));

	return(t);
}

#define SPEED(x) \
{\
	if(test)\
	{\
		uint64_t t = rdtsc();\
		x;\
		t = rdtsc() - t;\
		fprintf(stderr, THIS ": [%lli] speed test\n", t);\
		test--;\
	} else\
	{\
		x;\
	}\
}

#else

#define SPEED(x)  x;\

#endif


/*
 * This formula seems to work properly:
 * 
 *  R = Y + (1.40200 * (V - 128))
 *  G = Y - (0.71414 * (V - 128)) - (0.34414 * (U - 128))
 *  B = Y + (1.77200 * (U - 128))
 *
 */


/* we multiply each factor by 2^14 for mmx */
static const int16_t yuv_factors[] =
{
	22970, 22970, 22970, 22970,
	11700, 11700, 11700, 11700,
	 5638,  5638,  5638,  5638,
	29032, 29032, 29032, 29032
};


static const uint16_t chroma_sub[] = {128, 128, 128, 128};

static const uint32_t lmask[]   = {0x000000ff, 0x000000ff};
static const uint32_t wmask[]   = {0x00ff00ff, 0x00ff00ff};

static const uint32_t b5_mask[] = {0xf8f8f8f8, 0xf8f8f8f8};
static const uint32_t b6_mask[] = {0xfcfcfcfc, 0xfcfcfcfc};








static void
__mmx_yuy2_be_yuy2(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	if(!this->brightness.l_val &&
		this->contrast.l_val == 0x4000)
	{

		xine_fast_memcpy(data, frame->vo_frame.base[0],
					pitch * frame->height);

	} else
	{
		uint8_t* yuv_data = frame->vo_frame.base[0];
		uint32_t n = (frame->width * frame->height) >> 2;

		__asm__ __volatile__(

			"movq (wmask), %%mm2\n\t" /* mm2 = [0 0xff 0 0xff 0 0xff 0 0xff] */
			"psllw $8, %%mm2\n\t" /* mm2 = [0xff 0 0xff 0 0xff 0 0xff 0] */
			"movq %3, %%mm3\n\t" /* mm3 = brightness */
			"movq %4, %%mm4\n\t" /* mm4 = contrast */
			"pxor %%mm7, %%mm7\n\t"
			".align 16\n"
			"1:\tmovq (%1), %%mm0\n\t" /* mm0 = [v2 y3 u2 y2 v0 y1 u0 y0] */
			"movq %%mm0, %%mm1\n\t"
			"pand (wmask), %%mm1\n\t" /* mm1 = [0 y3 0 y2 0 y1 0 y0] */
			"paddw %%mm3, %%mm1\n\t" /* y + brightness */
			"psllw $2, %%mm1\n\t" /* y << 2 */
			"pmulhw %%mm4, %%mm1\n\t" /* y * contrast */
			"packuswb %%mm1, %%mm1\n\t" /* mm1 = [y3 y2 y1 y0 y3 y2 y1 y0] */
			"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 y3 0 y2 0 y1 0 y0] */
			"pand %%mm2, %%mm0\n\t" /* mm0 = [v2 0 u2 0 v0 0 u0 0] */
			"por %%mm1, %%mm0\n\t" /* mm0 = [v2 y3 u2 y2 v0 y1 u0 y0] */
			"movq %%mm0, (%0)\n\t"
			"addl $8, %0\n\t"
			"addl $8, %1\n\t"
			"loop 1b\n\t"
			"emms\n\t"

			:: "r" (data), "r" (yuv_data), "c" (n),
			   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
			: "memory");
	}
}


static void
__dummy_yuy2_be_yuy2(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	int32_t bright = this->brightness.l_val;
	int32_t ctr    = this->contrast.l_val;
	
	if(!bright && ctr == 0x4000)
	{
		xine_fast_memcpy(data, frame->vo_frame.base[0],
					 pitch * frame->height);

	} else
	{
		uint8_t* yuv_data = frame->vo_frame.base[0];
		uint32_t n = (frame->width * frame->height) >> 1;

		do
		{
			register int y;

			y = *yuv_data + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*data = y;

			*(data + 1) = *(yuv_data + 1);

			y = *(yuv_data + 2) + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(data + 2) = y;

			*(data + 3) = *(yuv_data + 3);

			data     += 4;
			yuv_data += 4;

		} while(--n);
		
	}
}


static void
__mmx_yuy2_be_uyvy(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n = (frame->width * frame->height) >> 2;
	

	__asm__ __volatile__(

		"movq %3, %%mm3\n\t" /* mm3 = brightness */
		"movq %4, %%mm4\n\t" /* mm4 = contrast */
		".align 16\n"
		"1:\tmovq (%1), %%mm0\n\t" /* mm0 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"movq %%mm0, %%mm1\n\t"
		"pand (wmask), %%mm1\n\t" /* mm1 = [0 y3 0 y2 0 y1 0 y0] */
		"paddw %%mm3, %%mm1\n\t" /* y + brightness */
		"psllw $2, %%mm1\n\t" /* y << 2 */
		"pmulhw %%mm4, %%mm1\n\t" /* y * contrast */
		"packuswb %%mm1, %%mm1\n\t" /* mm1 = [y3 y2 y1 y0 y3 y2 y1 y0] */
		"pxor %%mm2, %%mm2\n\t"
		"punpcklbw %%mm1, %%mm2\n\t" /* mm2 = [y3 0 y2 0 y1 0 y0 0] */
		"psrlw $8, %%mm0\n\t" /* mm0 = [0 v2 0 u2 0 v0 0 u0] */
		"por %%mm2, %%mm0\n\t" /* mm0 = [y3 v2 y2 u2 y1 v0 y0 u0] */
		"movq %%mm0, (%0)\n\t"
		"addl $8, %0\n\t"
		"addl $8, %1\n\t"
		"loop 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (yuv_data), "c" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yuy2_be_uyvy(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* dest     = (uint8_t*) data;
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n        = (frame->width * frame->height) >> 2;
	int32_t bright    = this->brightness.l_val;
	int32_t ctr       = this->contrast.l_val;

	
	if(bright || ctr != 0x4000)
	{
		do
		{
			register int y;

			*dest = *(yuv_data + 1);

			y = *yuv_data + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(dest + 1) = y;

			*(dest + 2) = *(yuv_data + 3);

			y = *(yuv_data + 2) + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(dest + 3) = y;

			*(dest + 4) = *(yuv_data + 5);

			y = *(yuv_data + 4) + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(dest + 5) = y;

			*(dest + 6) = *(yuv_data + 7);

			y = *(yuv_data + 6) + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(dest + 7) = y;

			yuv_data += 8;
			dest     += 8;

		} while(--n);

	} else
	{
		do
		{
			*dest       = *(yuv_data + 1);
			*(dest + 1) = *yuv_data;
			*(dest + 2) = *(yuv_data + 3);
			*(dest + 3) = *(yuv_data + 2);
			*(dest + 4) = *(yuv_data + 5);
			*(dest + 5) = *(yuv_data + 4);
			*(dest + 6) = *(yuv_data + 7);
			*(dest + 7) = *(yuv_data + 6);

			yuv_data += 8;
			dest     += 8;

		} while(--n);
	}
}


/* NOT TESTED */
static void
__dummy_yuy2_be_yv12(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint8_t* y_off    = (uint8_t*) data;
	uint8_t* u_off    = (frame->surface->format == DSPF_YV12)
			     ? data + (pitch * frame->height)
			     : data + (pitch * frame->height) +
			       ((pitch * frame->height) >> 2);
	uint8_t* v_off    = (frame->surface->format == DSPF_YV12)
			    ? data + (pitch * frame->height) +
			      ((pitch * frame->height) >> 2)
			    : data + (pitch * frame->height);
	uint32_t line  = frame->width >> 2;
	uint32_t n     = (frame->width * frame->height) >> 3;
	uint32_t l     = frame->vo_frame.pitches[0];
	int32_t bright = this->brightness.l_val;
	int32_t ctr    = this->contrast.l_val;


	do
	{
		register int y;

		y = *yuv_data + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*y_off = y;

		y = *(yuv_data + 2) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + 1) = y;

		y = *(yuv_data + 4) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + 2) = y;

		y = *(yuv_data + 6) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + 3) = y;

		y = *(yuv_data + l) + bright;
		y = (y * ctr) >> 7;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + pitch) = y;

		y = *(yuv_data + l + 2) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + pitch + 1) = y;

		y = *(yuv_data + l + 4) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + pitch + 2) = y;

		y = *(yuv_data + l + 6) + bright;
		y = (y * ctr) >> 14;
		y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
		*(y_off + pitch + 3) = y;

		*u_off       = (*(yuv_data + 1) + *(yuv_data + l + 1)) >> 1;
		*(u_off + 1) = (*(yuv_data + 5) + *(yuv_data + l + 5)) >> 1;

		*v_off       = (*(yuv_data + 3) + *(yuv_data + l + 3)) >> 1;
		*(v_off + 1) = (*(yuv_data + 7) + *(yuv_data + l + 7)) >> 1;

		yuv_data += 8;
		y_off    += 4;
		u_off    += 2;
		v_off    += 2;

		if(!(--line))
		{
			line      = frame->width >> 2;
			yuv_data += l;
			y_off    += pitch;
		}

	} while(--n);

}


static void
__mmx_yuy2_be_rgb15(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n = (frame->width * frame->height) >> 2;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovq (%1), %%mm0\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"pand (wmask), %%mm2\n\t" /* mm2 = [0 y3 0 y2 0 y1 0 y0] */
		"paddw %3, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %4, %%mm2\n\t" /* y * contrast */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 v2 y3 u2 0 v0 y1 u0] */
		"pand (lmask), %%mm1\n\t" /* mm1 = [0 0 0 u2 0 0 0 u0] */
		"packssdw %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u0 0 u2 0 u0] */
		"punpcklwd %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u2 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"psrld $24, %%mm0\n\t" /* mm0 = [0 0 0 v2 0 0 0 v0] */
		"packssdw %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v0 0 v2 0 v0] */
		"punpcklwd %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v2 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm3\n\t"
		"pmulhw (yuv_factors), %%mm3\n\t" /* chroma_r */
		"paddw %%mm2, %%mm3\n\t" /* mm3 = [r3 | r2 | r1 | r0] */
		"packuswb %%mm3, %%mm3\n\t" /* mm3 = [r3 r2 r1 r0 r3 r2 r1 r0] */
		"pand (b5_mask), %%mm3\n\t" /* red 15 */
		"movq %%mm1, %%mm4\n\t"
		"pmulhw (yuv_factors + 24), %%mm4\n\t" /* chroma_b */
		"paddw %%mm2, %%mm4\n\t" /* mm4 = [b3 | b2 | b1 | b0] */
		"packuswb %%mm4, %%mm4\n\t" /* mm4 = [b3 b2 b1 b0 b3 b2 b1 b0] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g3 | g2 | g1 | g0] */
		"packuswb %%mm2, %%mm2\n\t" /* mm2 = [g3 g2 g1 g0 g3 g2 g1 g0] */
		"pand (b5_mask), %%mm2\n\t" /* green 15 */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 r3 0 r2 0 r1 0 r0] */
		"psllw $7, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm4\n\t" /* mm4 = [0 b3 0 b2 0 b1 0 b0] */
		"psrlw $3, %%mm4\n\t" /* blue 15 */
		"por %%mm4, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 g3 0 g2 0 g1 0 g0] */
		"psllw $2, %%mm2\n\t" /* green 15 */
		"por %%mm2, %%mm3\n\t" /* rgb15 */
		"movq %%mm3, (%0)\n\t" /* out 4 pixles */
		"addl $8, %0\n\t"
		"addl $8, %1\n\t"
		"decl %2\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (yuv_data), "q" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yuy2_be_rgb15(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint16_t* dest    = (uint16_t*) data;
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n     = (frame->width * frame->height) >> 1;
	int32_t bright = this->brightness.l_val;

	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;

		{
			register int u, v;

			u = *(yuv_data + 1);
			v = *(yuv_data + 3);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *yuv_data + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		*(dest++) = ((r << 10) | (g << 5) | b);

		y = *(yuv_data + 2) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		*(dest++) = ((r << 10) | (g << 5) | b);

		
		yuv_data += 4;

	} while(--n);

}


static void
__mmx_yuy2_be_rgb16(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n = (frame->width * frame->height) >> 2;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovq (%1), %%mm0\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"pand (wmask), %%mm2\n\t" /* mm2 = [0 y3 0 y2 0 y1 0 y0] */
		"paddw %3, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %4, %%mm2\n\t" /* y * contrast */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 v2 y3 u2 0 v0 y1 u0] */
		"pand (lmask), %%mm1\n\t" /* mm1 = [0 0 0 u2 0 0 0 u0] */
		"packssdw %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u0 0 u2 0 u0] */
		"punpcklwd %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u2 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"psrld $24, %%mm0\n\t" /* mm0 = [0 0 0 v2 0 0 0 v0] */
		"packssdw %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v0 0 v2 0 v0] */
		"punpcklwd %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v2 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm3\n\t"
		"pmulhw (yuv_factors), %%mm3\n\t" /* chroma_r */
		"paddw %%mm2, %%mm3\n\t" /* mm3 = [r3 | r2 | r1 | r0] */
		"packuswb %%mm3, %%mm3\n\t" /* mm3 = [r3 r2 r1 r0 r3 r2 r1 r0] */
		"pand (b5_mask), %%mm3\n\t" /* red 16 */
		"movq %%mm1, %%mm4\n\t"
		"pmulhw (yuv_factors + 24), %%mm4\n\t" /* chroma_b */
		"paddw %%mm2, %%mm4\n\t" /* mm4 = [b3 | b2 | b1 | b0] */
		"packuswb %%mm4, %%mm4\n\t" /* mm4 = [b3 b2 b1 b0 b3 b2 b1 b0] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g3 | g2 | g1 | g0] */
		"packuswb %%mm2, %%mm2\n\t" /* mm2 = [g3 g2 g1 g0 g3 g2 g1 g0] */
		"pand (b6_mask), %%mm2\n\t" /* green 16 */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 r3 0 r2 0 r1 0 r0] */
		"psllw $8, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm4\n\t" /* mm4 = [0 b3 0 b2 0 b1 0 b0] */
		"psrlw $3, %%mm4\n\t" /* blue 16 */
		"por %%mm4, %%mm3\n\t"
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 g3 0 g2 0 g1 0 g0] */
		"psllw $3, %%mm2\n\t"
		"por %%mm2, %%mm3\n\t" /* rgb16 */
		"movq %%mm3, (%0)\n\t" /* out 4 pixels */
		"addl $8, %0\n\t"
		"addl $8, %1\n\t"
		"decl %2\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (yuv_data), "q" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yuy2_be_rgb16(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint16_t* dest    = (uint16_t*) data;
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n     = (frame->width * frame->height) >> 1;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;

		{
			register int u, v;

			u = *(yuv_data + 1);
			v = *(yuv_data + 3);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *yuv_data + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		*(dest++) = ((r << 11) | (g << 5) | b);

		y = *(yuv_data + 2) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		*(dest++) = ((r << 11) | (g << 5) | b);

		
		yuv_data += 4;

	} while(--n);

}


static void
__mmx_yuy2_be_rgb24(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n = (frame->width * frame->height) >> 2;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovq (%1), %%mm0\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"pand (wmask), %%mm2\n\t" /* mm2 = [0 y3 0 y2 0 y1 0 y0] */
		"paddw %3, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %4, %%mm2\n\t" /* y * contrast */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 v2 y3 u2 0 v0 y1 u0] */
		"pand (lmask), %%mm1\n\t" /* mm1 = [0 0 0 u2 0 0 0 u0] */
		"packssdw %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u0 0 u2 0 u0] */
		"punpcklwd %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u2 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"psrld $24, %%mm0\n\t" /* mm0 = [0 0 0 v2 0 0 0 v0] */
		"packssdw %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v0 0 v2 0 v0] */
		"punpcklwd %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v2 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm3\n\t"
		"pmulhw (yuv_factors), %%mm3\n\t" /* chroma_r */
		"paddw %%mm2, %%mm3\n\t" /* mm3 = [r3 | r2 | r1 | r0] */
		"packuswb %%mm3, %%mm3\n\t" /* mm3 = [r3 r2 r1 r0 r3 r2 r1 r0] */
		"movq %%mm1, %%mm4\n\t"
		"pmulhw (yuv_factors + 24), %%mm4\n\t" /* chroma_b */
		"paddw %%mm2, %%mm4\n\t" /* mm4 = [b3 | b2 | b1 | b0] */
		"packuswb %%mm4, %%mm4\n\t" /* mm4 = [b3 b2 b1 b0 b3 b2 b1 b0] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g3 | g2 | g1 | g0] */
		"packuswb %%mm2, %%mm2\n\t" /* mm2 = [g3 g2 g1 g0 g3 g2 g1 g0] */
		"movq %%mm4, %%mm0\n\t" /* mm0 = [b3 b2 b1 b0 b3 b2 b1 b0] */
		"punpcklbw %%mm2, %%mm0\n\t" /* mm0 = [g3 b3 g2 b2 g1 b1 g0 b0] */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [g3 b3 g2 b2 g1 b1 g0 b0] */
		"punpcklbw %%mm3, %%mm4\n\t" /* mm4 = [r3 b3 r2 b2 r1 b1 r0 b0] */
		"psrld $8, %%mm4\n\t" /* mm4 = [0 r3 b3 r2 0 r1 b1 r0] */
		"punpcklwd %%mm4, %%mm0\n\t" /* mm0 = [0 r1 g1 b1 b1 r0 g0 b0] */
		"movq %%mm0, %%mm6\n\t" /* mm6 = [0 r1 g1 b1 b1 r0 g0 b0] */
		"psrlq $40, %%mm6\n\t" /* mm6 = [0 0 0 0 0 0 r1 g1] */
		"punpckhwd %%mm4, %%mm1\n\t" /* mm1 = [0 r3 g3 b3 b3 r2 g2 b2] */
		"punpcklwd %%mm1, %%mm6\n\t" /* mm6 = [b3 r2 0 0 g2 b2 r1 g1] */
		"punpckldq %%mm6, %%mm0\n\t" /* mm0 = [g2 b2 r1 g1 b1 r0 g0 b0] */
		"movq %%mm0, (%0)\n\t" /* out 2 + 2/3 pixels */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 0 r3 g3 0 b3 r2 g2] */
		"psrld $16, %%mm6\n\t" /* mm6 = [0 0 b3 r2 0 0 g2 b2] */
		"punpckhwd %%mm1, %%mm6\n\t" /* mm6 = [0 0 0 0 r3 g3 b3 r2] */
		"movd %%mm6, 8(%0)\n\t" /* out 1/3 + 1 pixels */
		"addl $12, %0\n\t"
		"addl $8, %1\n\t"
		"decl %2\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (yuv_data), "q" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


/* unrolling here seems to speed up */
static void
__dummy_yuy2_be_rgb24(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data  = frame->vo_frame.base[0];
	uint32_t n     = (frame->width * frame->height) >> 2;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;

		{
			register int u, v;

			u = *(yuv_data + 1);
			v = *(yuv_data + 3);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *yuv_data + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*data       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(yuv_data + 2) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 3) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 4) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 5) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		{
			register int u, v;

			u = *(yuv_data + 5);
			v = *(yuv_data + 7);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *(yuv_data + 4) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 6) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 7) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 8) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(yuv_data + 6) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 9)  = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 10) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 11) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		data     += 12;
		yuv_data += 8;

	} while(--n);

}


static void
__mmx_yuy2_be_rgb32(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n = (frame->width * frame->height) >> 2;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovq (%1), %%mm0\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"pand (wmask), %%mm2\n\t" /* mm2 = [0 y3 0 y2 0 y1 0 y0] */
		"paddw %3, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %4, %%mm2\n\t" /* y * contrast */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [v2 y3 u2 y2 v0 y1 u0 y0] */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 v2 y3 u2 0 v0 y1 u0] */
		"pand (lmask), %%mm1\n\t" /* mm1 = [0 0 0 u2 0 0 0 u0] */
		"packssdw %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u0 0 u2 0 u0] */
		"punpcklwd %%mm1, %%mm1\n\t" /* mm1 = [0 u2 0 u2 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"psrld $24, %%mm0\n\t" /* mm0 = [0 0 0 v2 0 0 0 v0] */
		"packssdw %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v0 0 v2 0 v0] */
		"punpcklwd %%mm0, %%mm0\n\t" /* mm0 = [0 v2 0 v2 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm3\n\t"
		"pmulhw (yuv_factors), %%mm3\n\t" /* chroma_r */
		"paddw %%mm2, %%mm3\n\t" /* mm3 = [r3 | r2 | r1 | r0] */
		"packuswb %%mm3, %%mm3\n\t" /* mm3 = [r3 r2 r1 r0 r3 r2 r1 r0] */
		"movq %%mm1, %%mm4\n\t"
		"pmulhw (yuv_factors + 24), %%mm4\n\t" /* chroma_b */
		"paddw %%mm2, %%mm4\n\t" /* mm4 = [b3 | b2 | b1 | b0] */
		"packuswb %%mm4, %%mm4\n\t" /* mm4 = [b3 b2 b1 b0 b3 b2 b1 b0] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g3 | g2 | g1 | g0] */
		"packuswb %%mm2, %%mm2\n\t" /* mm2 = [g3 g2 g1 g0 g3 g2 g1 g0] */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 r3 0 r2 0 r1 0 r0] */
		"punpcklbw %%mm2, %%mm4\n\t" /* mm4 = [g3 b3 g2 b2 g1 b1 g0 b0] */
		"movq %%mm4, %%mm0\n\t" /* mm0 = [g3 b3 g2 b2 g1 b1 g0 b0] */
		"punpcklwd %%mm3, %%mm0\n\t" /* mm0 = [0 r1 g1 b1 0 r0 g0 b0] */
		"movq %%mm0, (%0)\n\t" /* out first 2 pixels */
		"punpckhwd %%mm3, %%mm4\n\t" /* mm4 = [0 r3 g3 b3 0 r2 g2 b2] */
		"movq %%mm4, 8(%0)\n\t" /* out second 2 pixels */
		"addl $16, %0\n\t"
		"addl $8, %1\n\t"
		"decl %2\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (yuv_data), "q" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


/* unrolling here seems to speed up */
static void
__dummy_yuy2_be_rgb32(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data  = frame->vo_frame.base[0];
	uint32_t n     = (frame->width * frame->height) >> 2;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;

		{
			register int u, v;

			u = *(yuv_data + 1);
			v = *(yuv_data + 3);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *yuv_data + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*data       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(yuv_data + 2) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 4) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 5) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 6) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		{
			register int u, v;

			u = *(yuv_data + 5);
			v = *(yuv_data + 7);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *(yuv_data + 4) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 8)  = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 9)  = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 10) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(yuv_data + 6) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 12) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 13) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 14) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));


		data     += 16;
		yuv_data += 8;

	} while(--n);

}



static yuv_render_t yuy2_cc =
{
	yuy2:	__dummy_yuy2_be_yuy2,
	uyvy:	__dummy_yuy2_be_uyvy,
	yv12:	__dummy_yuy2_be_yv12,
	rgb15:	__dummy_yuy2_be_rgb15,
	rgb16:	__dummy_yuy2_be_rgb16,
	rgb24:  __dummy_yuy2_be_rgb24,
	rgb32:	__dummy_yuy2_be_rgb32
};






static void
__mmx_yv12_be_yuy2(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t n      = frame->height >> 1;
	uint32_t line;


	__asm__ __volatile__(
		"movq (%0), %%mm4\n\t" /* mm4 = brightness */
		"movq (%1), %%mm5\n\t" /* mm5 = contrast */
		"pxor %%mm7, %%mm7\n\t"
		:: "r" (this->brightness.mm_val), "r" (this->contrast.mm_val)
		: "memory");

	__asm__ __volatile__(

		".align 16\n"
		"1:\tmovl %4, %6\n\t"
		"shrl $2, %6\n\t"
		".align 16\n"
		"2:\tmovd (%1), %%mm0\n\t" /* mm0 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %%mm4, %%mm0\n\t" /* y + brightness */
		"psllw $2, %%mm0\n\t" /* y << 2 */
		"pmulhw %%mm5, %%mm0\n\t" /* y * contrast */
		"movd (%1, %4), %%mm1\n\t" /* mm1 = [0 0 0 0 y13 y12 y11 y10] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %%mm4, %%mm1\n\t" /* y + brightness */
		"psllw $2, %%mm1\n\t" /* y << 2 */
		"pmulhw %%mm5, %%mm1\n\t" /* y * contrast */
		"packuswb %%mm1, %%mm0\n\t" /* mm0 = [y13 y12 y11 y10 y03 y02 y01 y00] */
		"movd (%2), %%mm2\n\t" /* mm2 = [0 0 0 0 u3 u2 u1 u0] */
		"addl $2, %2\n\t" /* u_data += 2 */
		"movd (%3), %%mm3\n\t" /* mm3 = [0 0 0 0 v3 v2 v1 v0] */
		"addl $2, %3\n\t" /* v_data += 2 */
		"punpcklbw %%mm3, %%mm2\n\t" /* mm2 = [v3 u3 v2 u2 v1 u1 v0 u0] */
		"movq %%mm0, %%mm1\n\t" /* mm1 = [y13 y12 y11 y10 y03 y02 y01 y00] */
		"punpcklbw %%mm2, %%mm1\n\t" /* mm1 = [v1 y03 u1 y02 v0 y01 u0 y00] */
		"movq %%mm1, (%0)\n\t" /* out 2 pixels (line 1) */
		"punpckhdq %%mm0, %%mm0\n\t" /* mm0 = [y13 y12 y11 y10 y13 y12 y11 y10] */
		"punpcklbw %%mm2, %%mm0\n\t" /* mm0 = [v1 y13 u1 y12 v0 y11 u0 y10] */
		"movq %%mm0, (%0, %5)\n\t" /* out 2 pixels (line 2) */
		"addl $8, %0\n\t" /* data += 8 */
		"addl $4, %1\n\t" /* y_data += 4 */
		"decl %6\n\t"
		"jnz 2b\n\t"
		"addl %5, %0\n\t"
		"addl %4, %1\n\t"
		"decl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n)
		: "memory");

}


 static void
__dummy_yv12_be_yuy2(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = frame->height >> 1;
	int32_t bright = this->brightness.l_val;
	int32_t ctr    = this->contrast.l_val;


	do
	{
		uint32_t i;

		for(i = 0; i < line; i++)
		{
			*(data + 1) = u_data[i];
			*(data + 3) = v_data[i];

			if(bright || ctr != 0x4000)
			{
				register int y;

				y = *y_data + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*data = y;

				y = *(y_data + 1) + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 2) = y;

			} else
			{
				*data       = *y_data;
				*(data + 2) = *(y_data + 1);
			}

			data   += 4;
			y_data += 2;
		}

		for(i = 0; i < line; i++)
		{
			*(data + 1) = *(u_data++);
			*(data + 3) = *(v_data++);

			if(bright || ctr != 0x4000)
			{
				register int y;

				y = *y_data + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*data = y;

				y = *(y_data + 1) + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 2) = y;
			
			} else
			{
				*data       = *y_data;
				*(data + 2) = *(y_data + 1);
			}

			data   += 4;
			y_data += 2;
		}
	
	} while(--n);

}


static void
__mmx_yv12_be_uyvy(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t n      = frame->height >> 1;
	uint32_t line;


	__asm__ __volatile__(
		"movq (%0), %%mm5\n\t" /* mm5 = brightness */
		"movq (%1), %%mm6\n\t" /* mm6 = contrast */
		"pxor %%mm7, %%mm7\n\t"
		:: "r" (this->brightness.mm_val), "r" (this->contrast.mm_val)
		: "memory");

	__asm__ __volatile__(

		".align 16\n"
		"1:\tmovl %4, %6\n\t"
		"shrl $2, %6\n\t"
		".align 16\n"
		"2:\tmovd (%1), %%mm1\n\t" /* mm1 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %%mm5, %%mm1\n\t" /* y + brightness */
		"psllw $2, %%mm1\n\t" /* y << 2 */
		"pmulhw %%mm6, %%mm1\n\t" /* y * contrast */
		"movd (%1, %4), %%mm2\n\t" /* mm2 = [0 0 0 0 y13 y12 y10 y10] */
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %%mm5, %%mm2\n\t" /* y + brightness */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %%mm6, %%mm2\n\t" /* y * contrast */
		"packuswb %%mm2, %%mm1\n\t" /* mm1 = [y13 y12 y10 y10 y03 y02 y01 y00] */
		"movd (%2), %%mm3\n\t" /* mm3 = [0 0 0 0 u3 u2 u1 u0] */
		"addl $2, %2\n\t" /* u_data += 2 */
		"movd (%3), %%mm4\n\t" /* mm4 = [0 0 0 0 v3 v2 v1 v0] */
		"addl $2, %3\n\t" /* v_data += 2 */
		"punpcklbw %%mm4, %%mm3\n\t" /* mm3 = [v3 u3 v2 u2 v1 u1 v0 u0] */
		"movq %%mm3, %%mm0\n\t"
		"punpcklbw %%mm1, %%mm0\n\t" /* mm0 = [y03 v1 y02 u1 y01 v0 y00 u0] */
		"movq %%mm0, (%0)\n\t"
		"punpckhdq %%mm1, %%mm1\n\t" /* mm1 = [y13 y12 y10 y10 y13 y12 y10 y10] */
		"punpcklbw %%mm1, %%mm3\n\t" /* mm3 = [y13 v1 y12 u1 y11 v0 y10 u0] */
		"movq %%mm3, (%0, %5)\n\t"
		"addl $8, %0\n\t" /* data += 8 */
		"addl $4, %1\n\t" /* y_data += 4 */
		"decl %6\n\t"
		"jnz 2b\n\t"
		"addl %5, %0\n\t"
		"addl %4, %1\n\t"
		"decl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n)
		: "memory");

}


static void
__dummy_yv12_be_uyvy(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = frame->height >> 1;
	int32_t bright = this->brightness.l_val;
	int32_t ctr    = this->contrast.l_val;


	do
	{
		uint32_t i;

		for(i = 0; i < line; i++)
		{
			*data       = u_data[i];
			*(data + 2) = v_data[i];

			if(bright || ctr != 0x4000)
			{
				register int y;
			
				y = *y_data + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 1) = y;

				y = *(y_data + 1) + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 3) = y;

			} else
			{
				*(data + 1) = *y_data;
				*(data + 3) = *(y_data + 1);
			}

			data   += 4;
			y_data += 2;
		}

		for(i = 0; i < line; i++)
		{
			*data       = *(u_data++);
			*(data + 2) = *(v_data++);

			if(bright || ctr != 0x4000)
			{
				register int y;
				
				y = *y_data + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 1) = y;

				y = *(y_data + 1) + bright;
				y = (y * ctr) >> 14;
				y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
				*(data + 3) = y;

			} else
			{
				*(data + 1) = *y_data;
				*(data + 3) = *(y_data + 1);
			}
				

			data   += 4;
			y_data += 2;
		}
	
	} while(--n);

}


/* NOT TESTED */
static void
__mmx_yv12_be_yv12(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	if(!this->brightness.l_val &&
		this->contrast.l_val == 0x4000)
	{
		xine_fast_memcpy(data,
				frame->vo_frame.base[0],
				pitch * frame->height);
		data += (pitch * frame->height);

	} else
	{
		uint32_t n = (frame->width * frame->height) >> 2;

		__asm__ __volatile__(

			"movq %3, %%mm1\n\t" /* mm1 = brightness */
			"movq %4, %%mm2\n\t" /* mm2 = contrast */
			"pxor %%mm7, %%mm7\n\t"
			".align 16\n"
			"1:\tmovd (%1), %%mm0\n\t" /* mm0 = [0 0 0 0 y3 y2 y1 y0] */
			"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 y3 0 y2 0 y1 0 y0] */
			"paddw %%mm1, %%mm0\n\t" /* y + brightness */
			"psllw $2, %%mm0\n\t" /* y << 2 */
			"pmulhw %%mm2, %%mm0\n\t" /* y * contrast */
			"packuswb %%mm0, %%mm0\n\t" /* mm0 = [y3 y2 y1 y0 y3 y2 y1 y0] */
			"movd %%mm0, (%0)\n\t"
			"addl $4, %0\n\t"
			"addl $4, %1\n\t"
			"loop 1b\n\t"

			: "=&r" (data)
			: "r" (frame->vo_frame.base[0]), "c" (n),
			  "m" (*(this->brightness.mm_val)),
			  "m" (*(this->contrast.mm_val)), "0" (data)
			: "memory");
	}

	xine_fast_memcpy(data,
			(frame->surface->format == DSPF_YV12)
				? frame->vo_frame.base[1]
				: frame->vo_frame.base[2],
			frame->vo_frame.pitches[1] * (frame->height >> 1));
	data += (pitch * frame->height) >> 2;

	xine_fast_memcpy(data,
			(frame->surface->format == DSPF_YV12)
				? frame->vo_frame.base[2]
				: frame->vo_frame.base[1],
			frame->vo_frame.pitches[2] * (frame->height >> 1));
}


/* NOT TESTED */
static void
__dummy_yv12_be_yv12(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	int32_t bright = this->brightness.l_val;
	int32_t ctr    = this->contrast.l_val;


	if(!bright && ctr == 0x4000)
	{
		xine_fast_memcpy(data,
				frame->vo_frame.base[0],
				pitch * frame->height);
		data += (pitch * frame->height);

	} else
	{
		uint8_t* y_data = frame->vo_frame.base[0];
		uint32_t n = (frame->width * frame->height) >> 1;

		do
		{
			register int y;

			y = *y_data + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*data = y;

			y = *(y_data + 1) + bright;
			y = (y * ctr) >> 14;
			y = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);
			*(data + 1) = y;

			y_data += 2;
			data   += 2;

		} while(--n);
	}

	xine_fast_memcpy(data,
			(frame->surface->format == DSPF_YV12)
				? frame->vo_frame.base[1]
				: frame->vo_frame.base[2],
			frame->vo_frame.pitches[1] * (frame->height >> 1));
	data += (pitch * frame->height) >> 2;

	xine_fast_memcpy(data,
			(frame->surface->format == DSPF_YV12)
				? frame->vo_frame.base[2]
				: frame->vo_frame.base[1],
			frame->vo_frame.pitches[2] * (frame->height >> 1));
}


static void
__mmx_yv12_be_rgb15(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line = frame->width >> 2;
	uint32_t n    = (frame->width * frame->height) >> 3;
	
	
	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovd (%1), %%mm3\n\t" /* mm3 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %8, %%mm3\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm3\n\t" /* y << 2 */
		"pmulhw %9, %%mm3\n\t" /* y * contrast */
		"movd (%1, %4), %%mm2\n\t" /* mm2 = [0 0 0 0 y13 y12 y11 y10] */
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %8, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %9, %%mm2\n\t" /* y * contrast */
		"movd (%2), %%mm1\n\t" /* mm1 = [0 0 0 0 u3 u2 u1 u0] */
		"punpcklbw %%mm1, %%mm1\n\t" /* mm1 = [u3 u3 u2 u2 u1 u1 u0 u0] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 u1 0 u1 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"movd (%3), %%mm0\n\t" /* mm0 = [0 0 0 0 v3 v2 v1 v0] */
		"punpcklbw %%mm0, %%mm0\n\t" /* mm0 = [v3 v3 v2 v2 v1 v1 v0 v0] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 v1 0 v1 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm4\n\t" /* mm4 = [0 v1 0 v1 0 v0 0 v0] */
		"pmulhw (yuv_factors), %%mm4\n\t" /* chroma_r */
		"movq %%mm4, %%mm5\n\t"
		"paddw %%mm3, %%mm4\n\t" /* mm4 = [r03 | r02 | r01 | r00] */
		"paddw %%mm2, %%mm5\n\t" /* mm5 = [r13 | r12 | r11 | r10] */
		"packuswb %%mm5, %%mm4\n\t" /* mm4 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"pand (b5_mask), %%mm4\n\t" /* red 15 */
		"movq %%mm1, %%mm5\n\t" /* mm5 = [0 u1 0 u1 0 u0 0 u0] */
		"pmulhw (yuv_factors + 24), %%mm5\n\t" /* chroma_b */
		"movq %%mm5, %%mm6\n\t"
		"paddw %%mm3, %%mm5\n\t" /* mm5 = [b03 | b02 | b01 | b00] */
		"paddw %%mm2, %%mm6\n\t" /* mm6 = [b13 | b12 | b11 | b00] */
		"packuswb %%mm6, %%mm5\n\t" /* mm5 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm3\n\t" /* mm3 = [g03 | g02 | g01 | g00] */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g13 | g12 | g11 | g10] */
		"packuswb %%mm2, %%mm3\n\t" /* mm3 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"pand (b5_mask), %%mm3\n\t" /* green 15 */
		"movq %%mm4, %%mm0\n\t" /* mm0 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 r03 0 r02 0 r01 0 r00] */
		"psllw $7, %%mm0\n\t"
		"movq %%mm5, %%mm1\n\t" /* mm1 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 b03 0 b02 0 b01 0 b00] */
		"psrlw $3, %%mm1\n\t" /* blue 15 */
		"por %%mm1, %%mm0\n\t"
		"movq %%mm3, %%mm1\n\t" /* mm1 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 g03 0 g02 0 g01 0 g00] */
		"psllw $2, %%mm1\n\t"
		"por %%mm1, %%mm0\n\t" /* rgb15 */
		"movq %%mm0, (%0)\n\t" /* out 4 pixels (line 1) */
		"punpckhbw %%mm7, %%mm4\n\t" /* mm4 = [0 r13 0 r12 0 r11 0 r10] */
		"psllw $7, %%mm4\n\t"
		"punpckhbw %%mm7, %%mm5\n\t" /* mm5 = [0 b13 0 b12 0 b11 0 b10] */
		"psrlw $3, %%mm5\n\t" /* blue 15 */
		"por %%mm5, %%mm4\n\t"
		"punpckhbw %%mm7, %%mm3\n\t" /* mm3 = [0 g13 0 g12 0 g11 0 g10] */
		"psllw $2, %%mm3\n\t"
		"por %%mm3, %%mm4\n\t" /* rgb15 */
		"movq %%mm4, (%0, %5)\n\t" /* out 4 pixels (line 2) */
		"addl $8, %0\n\t"
		"addl $4, %1\n\t"
		"addl $2, %2\n\t"
		"addl $2, %3\n\t"
		"decl %6\n\t" /* --line */
		"jnz 2f\n\t"
		"movl %4, %6\n\t" /* if(!line) { */
		"shrl $2, %6\n\t" /* line = frame->width / 4 */
		"addl %5, %0\n\t" /* data += pitch */
		"addl %4, %1\n\t" /* y_data += frame->width } */
		".align 16\n"
		"2:\tdecl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yv12_be_rgb15(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint16_t* dest  = (uint16_t*) data;
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = (frame->width * frame->height) >> 2;
	uint32_t dp    = 0;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;
		uint32_t dn = dp + frame->width;

		{
			register int u, v;
			
			u = *(u_data++);
			v = *(v_data++);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *y_data + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dp] = ((r << 10) | (g << 5) | b);

		y = *(y_data + 1) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dp + 1] = ((r << 10) | (g << 5) | b);

		y = *(y_data + frame->width) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dn] = ((r << 10) | (g << 5) | b);

		y = *(y_data + frame->width + 1) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 3;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dn + 1] = ((r << 10) | (g << 5) | b);

		y_data += 2;
		dp     += 2;

		if(!(--line))
		{
			line    = frame->width >> 1;
			y_data += frame->width;
			dp     += frame->width;
		}

	} while(--n);

}


static void
__mmx_yv12_be_rgb16(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line = frame->width >> 2;
	uint32_t n    = (frame->width * frame->height) >> 3;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovd (%1), %%mm3\n\t" /* mm3 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %8, %%mm3\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm3\n\t" /* y << 2 */
		"pmulhw %9, %%mm3\n\t" /* y * contrast */
		"movd (%1, %4), %%mm2\n\t" /* mm2 = [0 0 0 0 y13 y12 y11 y10] */
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %8, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %9, %%mm2\n\t" /* y * contrast */
		"movd (%2), %%mm1\n\t" /* mm1 = [0 0 0 0 u3 u2 u1 u0] */
		"punpcklbw %%mm1, %%mm1\n\t" /* mm1 = [u3 u3 u2 u2 u1 u1 u0 u0] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 u1 0 u1 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"movd (%3), %%mm0\n\t" /* mm0 = [0 0 0 0 v3 v2 v1 v0] */
		"punpcklbw %%mm0, %%mm0\n\t" /* mm0 = [v3 v3 v2 v2 v1 v1 v0 v0] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 v1 0 v1 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm4\n\t" /* mm4 = [0 v1 0 v1 0 v0 0 v0] */
		"pmulhw (yuv_factors), %%mm4\n\t" /* chroma_r*/
		"movq %%mm4, %%mm5\n\t"
		"paddw %%mm3, %%mm4\n\t" /* mm4 = [r03 | r02 | r01 | r00] */
		"paddw %%mm2, %%mm5\n\t" /* mm5 = [r13 | r12 | r11 | r10] */
		"packuswb %%mm5, %%mm4\n\t" /* mm4 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"pand (b5_mask), %%mm4\n\t" /* red 16 */
		"movq %%mm1, %%mm5\n\t" /* mm5 = [0 u1 0 u1 0 u0 0 u0] */
		"pmulhw (yuv_factors + 24), %%mm5\n\t" /* chroma_b */
		"movq %%mm5, %%mm6\n\t"
		"paddw %%mm3, %%mm5\n\t" /* mm5 = [b03 | b02 | b01 | b00] */
		"paddw %%mm2, %%mm6\n\t" /* mm6 = [b13 | b12 | b11 | b00] */
		"packuswb %%mm6, %%mm5\n\t" /* mm5 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chroma_g */
		"psubw %%mm0, %%mm3\n\t" /* mm3 = [g03 | g02 | g01 | g00] */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g13 | g12 | g11 | g10] */
		"packuswb %%mm2, %%mm3\n\t" /* mm3 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"pand (b6_mask), %%mm3\n\t" /* green 16 */
		"movq %%mm4, %%mm0\n\t" /* mm0 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 r03 0 r02 0 r01 0 r00] */
		"psllw $8, %%mm0\n\t"
		"movq %%mm5, %%mm1\n\t" /* mm1 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 b03 0 b02 0 b01 0 b00] */
		"psrlw $3, %%mm1\n\t" /* blue 16 */
		"por %%mm1, %%mm0\n\t"
		"movq %%mm3, %%mm1\n\t" /* mm1 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 g03 0 g02 0 g01 0 g00] */
		"psllw $3, %%mm1\n\t"
		"por %%mm1, %%mm0\n\t" /* rgb16 */
		"movq %%mm0, (%0)\n\t" /* out 4 pixels (line 1) */
		"punpckhbw %%mm7, %%mm4\n\t" /* mm4 = [0 r13 0 r12 0 r11 0 r10] */
		"psllw $8, %%mm4\n\t"
		"punpckhbw %%mm7, %%mm5\n\t" /* mm5 = [0 b13 0 b12 0 b11 0 b10] */
		"psrlw $3, %%mm5\n\t" /* blue 16 */
		"por %%mm5, %%mm4\n\t"
		"punpckhbw %%mm7, %%mm3\n\t" /* mm3 = [0 g13 0 g12 0 g11 0 g10] */
		"psllw $3, %%mm3\n\t"
		"por %%mm3, %%mm4\n\t" /* rgb16 */
		"movq %%mm4, (%0, %5)\n\t" /* out 4 pixels (line 2) */
		"addl $8, %0\n\t"
		"addl $4, %1\n\t"
		"addl $2, %2\n\t"
		"addl $2, %3\n\t"
		"decl %6\n\t" /* --line */
		"jnz 2f\n\t"
		"movl %4, %6\n\t" /* if(!line) { */
		"shrl $2, %6\n\t" /* line = frame->width / 4 */
		"addl %5, %0\n\t" /* data += pitch */
		"addl %4, %1\n\t" /* y_data += frame->width } */
		".align 16\n"
		"2:\tdecl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yv12_be_rgb16(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint16_t* dest  = (uint16_t*) data;
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = (frame->width * frame->height) >> 2;
	uint32_t dp    = 0;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;
		uint32_t dn = dp + frame->width;

		{
			register int u, v;

			u = *(u_data++);
			v = *(v_data++);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *y_data + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dp] = ((r << 11) | (g << 5) | b);

		y = *(y_data + 1) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dp + 1] = ((r << 11) | (g << 5) | b);

		y = *(y_data + frame->width) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dn] = ((r << 11) | (g << 5) | b);

		y = *(y_data + frame->width + 1) + bright;

		r = (y + m1) >> 3;
		g = (y - m2) >> 2;
		b = (y + m3) >> 3;

		r = ((r < 0) ? 0 : ((r > 0x1f) ? 0x1f : r));
		g = ((g < 0) ? 0 : ((g > 0x3f) ? 0x3f : g));
		b = ((b < 0) ? 0 : ((b > 0x1f) ? 0x1f : b));

		dest[dn + 1] = ((r << 11) | (g << 5) | b);


		y_data += 2;
		dp     += 2;

		if(!(--line))
		{
			line    = frame->width >> 1;
			y_data += frame->width;
			dp     += frame->width;
		}

	} while(--n);

}


static void
__mmx_yv12_be_rgb24(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line = frame->width >> 2;
	uint32_t n    = (frame->width * frame->height) >> 3;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovd (%1), %%mm3\n\t" /* mm3 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %8, %%mm3\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm3\n\t" /* y << 2 */
		"pmulhw %9, %%mm3\n\t" /* y * contrast */
		"movd (%1, %4), %%mm2\n\t" /* mm2 = [0 0 0 0 y13 y12 y11 y10] */
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %8, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %9, %%mm2\n\t" /* y * contrast */
		"movd (%2), %%mm1\n\t" /* mm1 = [0 0 0 0 u3 u2 u1 u0] */
		"punpcklbw %%mm1, %%mm1\n\t" /* mm1 = [u3 u3 u2 u2 u1 u1 u0 u0] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 u1 0 u1 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"movd (%3), %%mm0\n\t" /* mm0 = [0 0 0 0 v3 v2 v1 v0] */
		"punpcklbw %%mm0, %%mm0\n\t" /* mm0 = [v3 v3 v2 v2 v1 v1 v0 v0] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 v1 0 v1 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm4\n\t" /* mm4 = [0 v1 0 v1 0 v0 0 v0] */
		"pmulhw (yuv_factors), %%mm4\n\t" /* chroma_r*/
		"movq %%mm4, %%mm5\n\t"
		"paddw %%mm3, %%mm4\n\t" /* mm4 = [r03 | r02 | r01 | r00] */
		"paddw %%mm2, %%mm5\n\t" /* mm5 = [r13 | r12 | r11 | r10] */
		"packuswb %%mm5, %%mm4\n\t" /* mm4 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"movq %%mm1, %%mm5\n\t" /* mm5 = [0 u1 0 u1 0 u0 0 u0] */
		"pmulhw (yuv_factors + 24), %%mm5\n\t" /* chroma_b */
		"movq %%mm5, %%mm6\n\t"
		"paddw %%mm3, %%mm5\n\t" /* mm5 = [b03 | b02 | b01 | b00] */
		"paddw %%mm2, %%mm6\n\t" /* mm6 = [b13 | b12 | b11 | b00] */
		"packuswb %%mm6, %%mm5\n\t" /* mm5 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chorma_g */
		"psubw %%mm0, %%mm3\n\t" /* mm3 = [g03 | g02 | g01 | g00] */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g13 | g12 | g11 | g10] */
		"packuswb %%mm2, %%mm3\n\t" /* mm3 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"movq %%mm5, %%mm0\n\t" /* mm0 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpcklbw %%mm3, %%mm0\n\t" /* mm0 = [g03 b03 g02 b02 g01 b01 g00 b00] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [g03 b03 g02 b02 g01 b01 g00 b00] */
		"movq %%mm5, %%mm1\n\t" /* mm1 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpcklbw %%mm4, %%mm1\n\t" /* mm1 = [r03 b03 r02 b02 r01 b01 r00 b00] */
		"psrld $8, %%mm1\n\t" /* mm1 = [0 r03 b03 r02 0 r01 b01 r00] */
		"punpcklwd %%mm1, %%mm0\n\t" /* mm0 = [0 r01 g01 b01 b01 r00 g00 b00] */
		"movq %%mm0, %%mm6\n\t" /* mm6 = [0 r01 g01 b01 b01 r00 g00 b00] */
		"psrlq $40, %%mm6\n\t" /* mm6 = [0 0 0 0 0 0 r01 g01] */
		"punpckhwd %%mm1, %%mm2\n\t" /* mm2 = [0 r03 g03 b03 b03 r02 g02 b02] */
		"punpcklwd %%mm2, %%mm6\n\t" /* mm6 = [b03 r02 0 0 g02 b02 r01 g01] */
		"punpckldq %%mm6, %%mm0\n\t" /* mm0 = [g02 b02 r01 g01 b01 r00 g00 b00] */
		"movq %%mm0, (%0)\n\t" /* out 2 + 2/3 pixels (line 1) */
		"psrld $8, %%mm2\n\t" /* mm1 = [0 0 r03 g03 0 b03 r02 g02] */
		"psrld $16, %%mm6\n\t" /* mm6 = [0 0 b03 r02 0 0 g02 b02] */
		"punpckhwd %%mm2, %%mm6\n\t" /* mm6 = [0 0 0 0 r03 g03 b03 r02] */
		"movd %%mm6, 8(%0)\n\t" /* out 1/3 + 1 pixels (line 1) */
		"movq %%mm5, %%mm0\n\t" /* mm0 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpckhbw %%mm3, %%mm0\n\t" /* mm0 = [g13 b13 g12 b12 g11 b11 g10 b10] */
		"movq %%mm0, %%mm2\n\t" /* mm2 = [g13 b13 g12 b12 g11 b11 g10 b10] */
		"punpckhbw %%mm4, %%mm5\n\t" /* mm5 = [r13 b13 r12 b12 r11 b11 r10 b10] */
		"psrld $8, %%mm5\n\t" /* mm5 = [0 r13 b13 r12 0 r11 b11 r10] */
		"punpcklwd %%mm5, %%mm0\n\t" /* mm0 = [0 r11 g11 b11 b11 r10 g10 b10] */
		"movq %%mm0, %%mm6\n\t" /* mm6 = [0 r11 g11 b11 b11 r10 g10 b10] */
		"psrlq $40, %%mm6\n\t" /* mm6 = [0 0 0 0 0 0 r11 b11] */
		"punpckhwd %%mm5, %%mm2\n\t" /* mm2 = [0 r13 g13 b13 b13 r12 g12 b12] */
		"punpcklwd %%mm2, %%mm6\n\t" /* mm6 = [b13 r12 0 0 g12 b12 r11 g11] */
		"punpckldq %%mm6, %%mm0\n\t" /* mm0 = [g12 b12 r11 g11 b11 r10 g10 b10] */
		"movq %%mm0, (%0, %5)\n\t" /* out 2 + 2/3 pixels (line 2) */
		"psrld $8, %%mm2\n\t" /* mm2 = [0 0 r13 g13 0 b13 r12 g12] */
		"psrld $16, %%mm6\n\t" /* mm6 = [0 0 b13 r12 0 0 g12 b12] */
		"punpckhwd %%mm2, %%mm6\n\t" /* mm6 = [0 0 0 0 r13 g13 b13 r12] */
		"movd %%mm6, 8(%0, %5)\n\t" /* out 1/3 + 1 pixels (line 2) */
		"addl $12, %0\n\t"
		"addl $4, %1\n\t"
		"addl $2, %2\n\t"
		"addl $2, %3\n\t"
		"decl %6\n\t" /* --line */
		"jnz 2f\n\t"
		"movl %4, %6\n\t" /* if(!line) { */
		"shrl $2, %6\n\t" /* line = frame->width / 4 */
		"addl %5, %0\n\t" /* data += pitch */
		"addl %4, %1\n\t" /* y_data += frame->width } */
		".align 16\n"
		"2:\tdecl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yv12_be_rgb24(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = (frame->width * frame->height) >> 2;
	int32_t bright = this->brightness.l_val;


	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;
		uint8_t* datan = data + pitch;

		{
			register int u, v;

			u = *(u_data++);
			v = *(v_data++);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *y_data + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*data       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + 1) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 3) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 4) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 5) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + frame->width) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*datan       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(datan + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(datan + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + frame->width + 1) + bright;

		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(datan + 3) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(datan + 4) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(datan + 5) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));


		data   += 6;
		y_data += 2;

		if(!(--line))
		{
			line    = frame->width >> 1;
			y_data += frame->width;
			data   += pitch;
		}

	} while(--n);

}


static void
__mmx_yv12_be_rgb32(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line = frame->width >> 2;
	uint32_t n    = (frame->width * frame->height) >> 3;


	__asm__ __volatile__(

		"pxor %%mm7, %%mm7\n\t"
		".align 16\n"
		"1:\tmovd (%1), %%mm3\n\t" /* mm3 = [0 0 0 0 y03 y02 y01 y00] */
		"punpcklbw %%mm7, %%mm3\n\t" /* mm3 = [0 y03 0 y02 0 y01 0 y00] */
		"paddw %8, %%mm3\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm3\n\t" /* y << 2 */
		"pmulhw %9, %%mm3\n\t" /* y * contrast */
		"movd (%1, %4), %%mm2\n\t" /* mm2 = [0 0 0 0 y13 y12 y11 y10] */
		"punpcklbw %%mm7, %%mm2\n\t" /* mm2 = [0 y13 0 y12 0 y11 0 y10] */
		"paddw %8, %%mm2\n\t" /* y + (brightness + gamma_correction) */
		"psllw $2, %%mm2\n\t" /* y << 2 */
		"pmulhw %9, %%mm2\n\t" /* y * contrast */
		"movd (%2), %%mm1\n\t" /* mm1 = [0 0 0 0 u3 u2 u1 u0] */
		"punpcklbw %%mm1, %%mm1\n\t" /* mm1 = [u3 u3 u2 u2 u1 u1 u0 u0] */
		"punpcklbw %%mm7, %%mm1\n\t" /* mm1 = [0 u1 0 u1 0 u0 0 u0] */
		"psubw (chroma_sub), %%mm1\n\t" /* u -= 128 */
		"psllw $2, %%mm1\n\t" /* u << 2 */
		"movd (%3), %%mm0\n\t" /* mm0 = [0 0 0 0 v3 v2 v1 v0] */
		"punpcklbw %%mm0, %%mm0\n\t" /* mm0 = [v3 v3 v2 v2 v1 v1 v0 v0] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 v1 0 v1 0 v0 0 v0] */
		"psubw (chroma_sub), %%mm0\n\t" /* v -= 128 */
		"psllw $2, %%mm0\n\t" /* v << 2 */
		"movq %%mm0, %%mm4\n\t" /* mm4 = [0 v1 0 v1 0 v0 0 v0] */
		"pmulhw (yuv_factors), %%mm4\n\t" /* chroma_r*/
		"movq %%mm4, %%mm5\n\t"
		"paddw %%mm3, %%mm4\n\t" /* mm4 = [r03 | r02 | r01 | r00] */
		"paddw %%mm2, %%mm5\n\t" /* mm5 = [r13 | r12 | r11 | r10] */
		"packuswb %%mm5, %%mm4\n\t" /* mm4 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"movq %%mm1, %%mm5\n\t" /* mm5 = [0 u1 0 u1 0 u0 0 u0] */
		"pmulhw (yuv_factors + 24), %%mm5\n\t" /* chroma_b */
		"movq %%mm5, %%mm6\n\t"
		"paddw %%mm3, %%mm5\n\t" /* mm5 = [b03 | b02 | b01 | b00] */
		"paddw %%mm2, %%mm6\n\t" /* mm6 = [b13 | b12 | b11 | b00] */
		"packuswb %%mm6, %%mm5\n\t" /* mm5 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"pmulhw (yuv_factors + 8), %%mm0\n\t"
		"pmulhw (yuv_factors + 16), %%mm1\n\t"
		"paddw %%mm1, %%mm0\n\t" /* chorma_g */
		"psubw %%mm0, %%mm3\n\t" /* mm3 = [g03 | g02 | g01 | g00] */
		"psubw %%mm0, %%mm2\n\t" /* mm2 = [g13 | g12 | g11 | g10] */
		"packuswb %%mm2, %%mm3\n\t" /* mm3 = [g13 g12 g11 g10 g03 g02 g01 g00] */
		"movq %%mm4, %%mm0\n\t" /* mm0 = [r13 r12 r11 r10 r03 r02 r01 r00] */
		"punpcklbw %%mm7, %%mm0\n\t" /* mm0 = [0 r03 0 r02 0 r01 0 r00] */
		"movq %%mm5, %%mm1\n\t" /* mm1 = [b13 b12 b11 b10 b03 b02 b01 b00] */
		"punpcklbw %%mm3, %%mm1\n\t" /* mm1 = [g03 b03 g02 b02 g01 b01 g00 b00] */
		"movq %%mm1, %%mm2\n\t" /* mm2 = [g03 b03 g02 b02 g01 b01 g00 b00] */
		"punpcklwd %%mm0, %%mm2\n\t" /* mm2 = [0 r01 g01 b01 0 r00 g00 b00] */
		"movq %%mm2, (%0)\n\t" /* out first 2 pixels (line 1) */
		"punpckhwd %%mm0, %%mm1\n\t" /* mm1 = [0 r03 g03 b03 0 r02 g02 b02] */
		"movq %%mm1, 8(%0)\n\t" /* out second 2 pixels (line 1) */
		"punpckhbw %%mm7, %%mm4\n\t" /* mm4 = [0 r13 0 r12 0 r11 0 r10] */
		"punpckhbw %%mm3, %%mm5\n\t" /* mm5 = [g13 b13 g12 b12 g11 b11 g10 b10] */
		"movq %%mm5, %%mm0\n\t" /* mm0 = [g13 b13 g12 b12 g11 b11 g10 b10] */
		"punpcklwd %%mm4, %%mm0\n\t" /* mm0 = [0 r11 g11 b11 0 r10 g10 b10] */
		"movq %%mm0, (%0, %5)\n\t" /* out first 2 pixels (line 2) */
		"punpckhwd %%mm4, %%mm5\n\t" /* mm5 = [0 r13 g13 b13 0 r12 g12 b12] */
		"movq %%mm5, 8(%0, %5)\n\t" /* out second 2 pixels (line 2) */
		"addl $16, %0\n\t"
		"addl $4, %1\n\t"
		"addl $2, %2\n\t"
		"addl $2, %3\n\t"
		"decl %6\n\t" /* --line */
		"jnz 2f\n\t"
		"movl %4, %6\n\t" /* if(!line) { */
		"shrl $2, %6\n\t" /* line = frame->width / 4 */
		"addl %5, %0\n\t" /* data += pitch */
		"addl %4, %1\n\t" /* y_data += frame->width } */
		".align 16\n"
		"2:\tdecl %7\n\t"
		"jnz 1b\n\t"
		"emms\n\t"

		:: "r" (data), "r" (y_data), "r" (u_data), "r" (v_data),
		   "q" (frame->width), "q" (pitch), "m" (line), "m" (n),
		   "m" (*(this->brightness.mm_val)), "m" (*(this->contrast.mm_val))
		: "memory");

}


static void
__dummy_yv12_be_rgb32(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint8_t* u_data = frame->vo_frame.base[1];
	uint8_t* v_data = frame->vo_frame.base[2];
	uint32_t line  = frame->width >> 1;
	uint32_t n     = (frame->width * frame->height) >> 2;
	int32_t bright = this->brightness.l_val;
	

	do
	{
		register int y;
		int m1, m2, m3;
		int r, g, b;
		uint8_t* datan = data + pitch;

		{
			register int u, v;

			u = *(u_data++);
			v = *(v_data++);

			m1 = v_red_table[v];
			m2 = v_green_table[v] + u_green_table[u];
			m3 = u_blue_table[u];
		}

		y = *y_data + bright;
		
		r = y + m1;
		g = y - m2;
		b = y + m3;

		*data       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + 1) + bright;
				
		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(data + 4) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(data + 5) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(data + 6) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + frame->width) + bright;
				
		r = y + m1;
		g = y - m2;
		b = y + m3;

		*datan       = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(datan + 1) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(datan + 2) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		y = *(y_data + frame->width + 1) + bright;
				
		r = y + m1;
		g = y - m2;
		b = y + m3;

		*(datan + 4) = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		*(datan + 5) = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
		*(datan + 6) = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));

		
		data   += 8;
		y_data += 2;

		if(!(--line))
		{
			line    = frame->width >> 1;
			y_data += frame->width;
			data   += pitch;
		}

	} while(--n);

}



static yuv_render_t yv12_cc =
{
	yuy2:	__dummy_yv12_be_yuy2,
	uyvy:	__dummy_yv12_be_uyvy,
	yv12:	__dummy_yv12_be_yv12,
	rgb15:	__dummy_yv12_be_rgb15,
	rgb16:	__dummy_yv12_be_rgb16,
	rgb24:  __dummy_yv12_be_rgb24,
	rgb32:	__dummy_yv12_be_rgb32
};







static uint32_t
dfb_get_capabilities(vo_driver_t* vo_driver)
{
	return(VO_CAP_YV12 | VO_CAP_YUY2);
}


static void
dfb_proc_frame(vo_frame_t* vo_frame)
{
	dfb_driver_t* this = NULL;
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;
	uint8_t* data;
	uint32_t pitch;
#ifdef DFB_DEBUG
	static int test = 8;
#endif
	
	TEST(vo_frame != NULL);
	TEST(frame->surface != NULL);

	vo_frame->proc_called = 1;

	this  = (dfb_driver_t*) vo_frame->driver;
	data  = (uint8_t*) frame->surface->back_buffer->system.addr;
	pitch = (uint32_t) frame->surface->back_buffer->system.pitch;

	SPEED(frame->realize(this, frame, data, pitch));

FAILURE:
	return;
}


static void
dfb_frame_dispose(vo_frame_t* vo_frame)
{
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;

	if(frame)
	{
		if(frame->surface)
			dfb_surface_unref(frame->surface);
		release(frame->chunks[0]);
		release(frame->chunks[1]);
		release(frame->chunks[2]);
		free(frame);
	}
}


static vo_frame_t*
dfb_alloc_frame(vo_driver_t* vo_driver)
{
	dfb_frame_t* frame = NULL;

	TEST(vo_driver != NULL);

	TEST(frame = (dfb_frame_t*) calloc(1, sizeof(dfb_frame_t)));

	pthread_mutex_init(&(frame->vo_frame.mutex), NULL);

	frame->vo_frame.proc_slice = NULL;
 	frame->vo_frame.proc_frame = dfb_proc_frame;
	frame->vo_frame.field      = NULL;
	frame->vo_frame.dispose    = dfb_frame_dispose;
	frame->vo_frame.driver     = vo_driver;
	frame->state.src_blend     = DSBF_SRCALPHA;
	frame->state.dst_blend     = DSBF_INVSRCALPHA;
	frame->state.modified      = SMF_ALL;

	D_MAGIC_SET(&(frame->state), CardState);

	return((vo_frame_t*) frame);

FAILURE:
	return(NULL);
}


static void
dfb_update_frame_format(vo_driver_t* vo_driver, vo_frame_t* vo_frame,
		uint32_t width, uint32_t height, double ratio,
		int format, int flags)
{
	dfb_driver_t* this   = (dfb_driver_t*) vo_driver;
	dfb_frame_t* frame   = (dfb_frame_t*) vo_frame;
	yuv_render_t* yuv_cc;

	TEST(vo_driver  != NULL);
	TEST(vo_frame   != NULL);
	TEST(this->main != NULL);
	TEST(this->main_data != NULL);

	/* width must be a multiple of 4 */
	frame->width  = width + ((width & 3) ? (4 - (width & 3)) : 0);
	/* height must be a multiple of 2 */
	frame->height = height + (height & 1);
	frame->format = format;
	
	if(frame->surface)
	{
		dfb_surface_unref(frame->surface);
		frame->surface = NULL;
	}
	
	dfb_surface_create(NULL, frame->width, frame->height,
				this->main_data->surface->format,
				CSP_SYSTEMONLY, DSCAPS_SYSTEMONLY,
				NULL, &(frame->surface));
	if(!frame->surface)
	{
		DBUG("couldn't create a surface for frame %p", frame);
		goto FAILURE;
	}

	frame->state.source    = frame->surface;
	frame->state.modified |= SMF_SOURCE;

	release(frame->chunks[0]);
	release(frame->chunks[1]);
	release(frame->chunks[2]);
	
	switch(format)
	{
		case XINE_IMGFMT_YUY2:
		{
			ONESHOT("video frame format is YUY2");
			yuv_cc = &yuy2_cc;
			frame->vo_frame.pitches[0] = frame->width << 1;
			frame->vo_frame.base[0] = (uint8_t*) xine_xmalloc_aligned(16,
							frame->vo_frame.pitches[0] * frame->height,
							&(frame->chunks[0]));
			TEST(frame->vo_frame.base[0] != NULL);
		}
		break;

		/* assume XINE_IMGFMT_YV12 */
		default:
		{
			ONESHOT("video frame format is YV12");
			yuv_cc = &yv12_cc;
			frame->vo_frame.pitches[0] = frame->width;
			frame->vo_frame.pitches[1] = frame->width >> 1;
			frame->vo_frame.pitches[2] = frame->width >> 1;
			frame->vo_frame.base[0] = (uint8_t*) xine_xmalloc_aligned(16,
							(frame->vo_frame.pitches[0] * frame->height),
							&(frame->chunks[0]));
			frame->vo_frame.base[1] = (uint8_t*) xine_xmalloc_aligned(16,
							frame->vo_frame.pitches[1] * (frame->height >> 1) + 2,
							&(frame->chunks[1]));
			frame->vo_frame.base[2] = (uint8_t*) xine_xmalloc_aligned(16,
							frame->vo_frame.pitches[2] * (frame->height >> 1) + 2,
							&(frame->chunks[2]));
			TEST(frame->vo_frame.base[0] != NULL);
			TEST(frame->vo_frame.base[1] != NULL);
			TEST(frame->vo_frame.base[2] != NULL);
		}
		break;
	}

	switch(frame->surface->format)
	{
		case DSPF_YUY2:
			frame->realize = yuv_cc->yuy2;
		break;

		case DSPF_UYVY:
			frame->realize = yuv_cc->uyvy;
		break;

		case DSPF_YV12:
		case DSPF_I420:
			frame->realize = yuv_cc->yv12;
		break;

		case DSPF_ARGB1555:
			frame->realize = yuv_cc->rgb15;
		break;

		case DSPF_RGB16:
			frame->realize = yuv_cc->rgb16;
		break;

		case DSPF_RGB24:
			frame->realize = yuv_cc->rgb24;
		break;

		case DSPF_RGB32:
		case DSPF_ARGB:
		case DSPF_AiRGB:
			frame->realize = yuv_cc->rgb32;
		break;
	
		default:
		break;
	}

	return;
	
FAILURE:
	if(frame->surface)
	{
		dfb_surface_unref(frame->surface);
		frame->surface = NULL;
	}
}


static void
dfb_overlay_clut_yuv2rgb(dfb_driver_t* this, dfb_frame_t* frame,
					vo_overlay_t* overlay)
{
	clut_t* clut;
	uint32_t i;

	if(!overlay->rgb_clut)
	{
		clut = (clut_t*) overlay->color;

		for(i = 0; i < (sizeof(overlay->color) / sizeof(clut_t)); i++)
		{
			DFBColor* color = (DFBColor*) &clut[i];
			int y, r, g, b;

			y = clut[i].y + this->correction.defined;

			r = y + v_red_table[clut[i].cb];
			g = y - v_green_table[clut[i].cb] -
				u_green_table[clut[i].cr];
			b = y + u_blue_table[clut[i].cr];

			color->r = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));
			color->g = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
			color->b = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		}

		overlay->rgb_clut++;
	}

	if(!overlay->clip_rgb_clut)
	{
		clut = (clut_t*) overlay->clip_color;

		for(i = 0; i < (sizeof(overlay->clip_color) / sizeof(clut_t)); i++)
		{
			DFBColor* color = (DFBColor*) &clut[i];
			int y, r, g, b;

			y = clut[i].y + this->correction.defined;
			
			r = y + v_red_table[clut[i].cb];
			g = y - v_green_table[clut[i].cb] -
				u_green_table[clut[i].cr];
			b = y + u_blue_table[clut[i].cr];

			color->r = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));
			color->g = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
			color->b = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		}

		overlay->clip_rgb_clut++;
	}
}


static void
dfb_overlay_blend(vo_driver_t* vo_driver, vo_frame_t* vo_frame,
			vo_overlay_t* overlay)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;
	uint32_t i, x, y;

	TEST(vo_driver != NULL);
	TEST(vo_frame  != NULL);
	TEST(overlay   != NULL);
	TEST(frame->surface != NULL);

	if(!overlay->rle)
		return;

	switch(frame->surface->format)
	{
		case DSPF_YUY2:
		case DSPF_UYVY:
		case DSPF_YV12:
		case DSPF_I420:
			ONESHOT("blending overlays over YUV surfaces is not supported");
		return;

		default:
		break;
	}

	dfb_overlay_clut_yuv2rgb(this, frame, overlay);

	frame->state.destination  = frame->surface;
	frame->state.drawingflags = DSDRAW_BLEND;
	frame->state.modified    |= (SMF_DESTINATION | SMF_DRAWING_FLAGS);

	/* based on X11 osd */
	for(i = 0, x= 0, y = 0; i < overlay->num_rle; i++)
	{
		uint32_t len   = overlay->rle[i].len;
		uint32_t color = overlay->rle[i].color & 0xff;

		while(len > 0)
		{
			DFBColor* c_palette = (DFBColor*) overlay->color;
			uint8_t*  t_palette = (uint8_t*) overlay->trans;
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if(y >= overlay->clip_top && y <= overlay->clip_bottom
					&& x <= overlay->clip_right)
			{
				if(x < overlay->clip_left &&
					(x + width + 1) >= overlay->clip_left)
				{
					width -= overlay->clip_left - x;
					len   += overlay->clip_left - x;
				} else
				if(x > overlay->clip_left)
				{
					c_palette = (DFBColor*) overlay->clip_color;
					t_palette = (uint8_t*) overlay->clip_trans;

					if((x + width - 1) > overlay->clip_right)
					{
						width -= overlay->clip_right - x;
						len   += overlay->clip_right - x;
					}
				}
			}

			if(t_palette[color])
			{
				DFBRectangle rect;
				
				frame->state.color     = c_palette[color];
				frame->state.color.a   = t_palette[color] * 17;
				frame->state.modified |= SMF_COLOR;

				rect.x = overlay->x + x;
				rect.y = overlay->y + y;
				rect.w = width;
				rect.h = 1;

				dfb_gfxcard_fillrectangle(&rect, &(frame->state));
			}

			x += width;

			if(x >= overlay->width)
			{
				x = 0;
				y++;
			}
		}
	}

FAILURE:
	return;
}


static void
dfb_display_frame(vo_driver_t* vo_driver, vo_frame_t* vo_frame)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;
	DFBRectangle dest_rect;
	DFBRegion used_area;

	TEST(vo_driver  != NULL);
	TEST(vo_frame   != NULL);
	TEST(frame->surface != NULL);

	this->output_cb(this->output_cdata, frame->width,
			frame->height, &dest_rect);

	used_area.x1 = dest_rect.x;
	used_area.y1 = dest_rect.y;
	used_area.x2 = dest_rect.x + dest_rect.w;
	used_area.y2 = dest_rect.y + dest_rect.h;

	if(dest_rect.x < 0)
		dest_rect.x = 0;
	dest_rect.x += this->main_data->area.wanted.x;

	if(dest_rect.y < 0)
		dest_rect.y = 0;
	dest_rect.y += this->main_data->area.wanted.y;

	{
		uint32_t maxw = this->main_data->surface->width - dest_rect.x;
		uint32_t maxh = this->main_data->surface->height - dest_rect.y;

		if(dest_rect.w < 1 || dest_rect.w > maxw)
		{
			dest_rect.x = this->main_data->area.wanted.x;
			dest_rect.w = this->main_data->area.wanted.w;
		}

		if(dest_rect.h < 1 || dest_rect.h > maxh)
		{
			dest_rect.y = this->main_data->area.wanted.y;
			dest_rect.h = this->main_data->area.wanted.h;
		}
	}

	if(!dfb_rectangle_intersect(&dest_rect,
			&(this->main_data->area.current)))
		goto FAILURE;

	frame->state.clip.x1     = dest_rect.x;
	frame->state.clip.y1     = dest_rect.y;
	frame->state.clip.x2     = dest_rect.x + dest_rect.w - 1;
	frame->state.clip.y2     = dest_rect.y + dest_rect.h - 1;
	frame->state.destination = this->main_data->surface;
	frame->state.modified   |= (SMF_CLIP | SMF_DESTINATION);


	if(dest_rect.w != frame->width ||
		dest_rect.h != frame->height)
	{
		DFBRectangle rect = {0, 0, frame->width, frame->height};

		dfb_gfxcard_stretchblit(&rect, &dest_rect, &(frame->state));
	} else
	{
		DFBRectangle rect = {0, 0, frame->width, frame->height};

		dfb_gfxcard_blit(&rect, dest_rect.x,
					dest_rect.y, &(frame->state));
	}

	if(this->frame_cb)
	{
		this->frame_cb(this->frame_cdata);
	} else
	if(this->main_data->caps & DSCAPS_FLIPPING)
	{
		this->main->Flip(this->main, &used_area, 0);
	}


FAILURE:
	if(vo_frame)
		vo_frame->free(vo_frame);
}


static int
dfb_get_property(vo_driver_t* vo_driver, int property)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;

	TEST(vo_driver != NULL);

	switch(property)
	{
		case VO_PROP_BRIGHTNESS:
		{
			DBUG("brightness is %i", this->brightness.l_val -
						this->correction.used);
			return(this->brightness.l_val - this->correction.used);
		}
		break;

		case VO_PROP_CONTRAST:
		{
			DBUG("contrast is %i", this->contrast.l_val);
			return(this->contrast.l_val);
		}
		break;

		case VO_PROP_MAX_NUM_FRAMES:
		{
			DBUG("maximum number of frames is %i", this->max_num_frames);
			return(this->max_num_frames);
		}
		break;

		default:
			DBUG("tryed to get unsupported property %i", property);
		break;
	}
	
FAILURE:
	return(0);
}


static int
dfb_set_property(vo_driver_t* vo_driver, int property, int value)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;

	TEST(vo_driver != NULL);

	switch(property)
	{
		case VO_PROP_BRIGHTNESS:
		{
			if(value > -129 && value < 128)
			{
				int brightness = value + this->correction.used;

				DBUG("setting brightness to %i", value);
				this->brightness.l_val     = brightness;
				this->brightness.mm_val[0] = brightness;
				this->brightness.mm_val[1] = brightness;
				this->brightness.mm_val[2] = brightness;
				this->brightness.mm_val[3] = brightness;
			}
		}
		break;

		case VO_PROP_CONTRAST:
		{
			if(value > -1 && value < 32768)
			{				
				DBUG("setting contrast to %i", value);
				this->contrast.l_val     = value;
				this->contrast.mm_val[0] = value;
				this->contrast.mm_val[1] = value;
				this->contrast.mm_val[2] = value;
				this->contrast.mm_val[3] = value;
			}
		}
		break;
		
		default:
			DBUG("tryed to set unsupported property %i", property);
		break;
	}

	return(value);
	
FAILURE:
	return(0);
}


static void
dfb_get_property_min_max(vo_driver_t* vo_driver,
			int property, int *min, int *max)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;

	TEST(vo_driver != NULL);

	switch(property)
	{
		case VO_PROP_BRIGHTNESS:
		{
			*min = -128;
			*max = +127;
		}
		break;

		case VO_PROP_CONTRAST:
		{
			*min = 0;
			*max = 0x7fff;
		}
		break;

		default:
		{
			DBUG("requested min/max for unsupported property %i", property);
			*min = 0;
			*max = 0;
		}
		break;
	}

FAILURE:
	return;
}


static int
dfb_gui_data_exchange(vo_driver_t* vo_driver,
				int data_type, void* data)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;

	TEST(vo_driver != NULL);

	switch(data_type)
	{
		/* update destination Surface */
		case XINE_GUI_SEND_DRAWABLE_CHANGED:
		{
			IDirectFBSurface* surface = (IDirectFBSurface*) data;

			if(!surface || !surface->priv)
			{
				fprintf(stderr, THIS ": bad surface\n");
				return(0);
			}

			if(this->main)
				this->main->Release(this->main);

			this->main      = surface;
			this->main_data = (IDirectFBSurface_data*) surface->priv;

			/* reset brightness value */
			this->brightness.l_val     -= this->correction.used;
			this->brightness.mm_val[0] -= this->correction.used;
			this->brightness.mm_val[1] -= this->correction.used;
			this->brightness.mm_val[2] -= this->correction.used;
			this->brightness.mm_val[3] -= this->correction.used;

			switch(this->main_data->surface->format)
			{
				case DSPF_YUY2:
				case DSPF_UYVY:
				case DSPF_YV12:
				case DSPF_I420:
				{
					DBUG("we have a new surface [format: YUV(%#x)]",
						this->main_data->surface->format);
					this->correction.used = 0;
				}
				break;

				case DSPF_ARGB1555:
				case DSPF_RGB16:
				case DSPF_RGB24:
				case DSPF_RGB32:
				case DSPF_ARGB:
				case DSPF_AiRGB:
				{
					DBUG("we have a new surface [format: RGB(%#x)]",
						this->main_data->surface->format);
					this->correction.used = this->correction.defined;
				}
				break;

				default:
				{
					SAY("unsupported surface format [%#x]",
						this->main_data->surface->format);
					this->main      = NULL;
					this->main_data = NULL;
					return(0);
				}
			}

			this->brightness.l_val     += this->correction.used;
			this->brightness.mm_val[0] += this->correction.used;
			this->brightness.mm_val[1] += this->correction.used;
			this->brightness.mm_val[2] += this->correction.used;
			this->brightness.mm_val[3] += this->correction.used;

			this->main->AddRef(this->main);

			return(1);
		}
		break;

		/* register DVFrameCallback */
		case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
		{
			dfb_frame_callback_t* frame_callback;

			frame_callback = (dfb_frame_callback_t*) data;

			this->frame_cb    = frame_callback->frame_cb;
			this->frame_cdata = frame_callback->cdata;

			DBUG("registered new DVFrameCallback");

			return(1);
		}
		break;
		
		default:
			DBUG("unknown data type %i", data_type);
		break;
	}
	
FAILURE:
	return(0);
}


static int
dfb_redraw_needed(vo_driver_t* vo_driver)
{
	/* not needed */
	return(0);
}


static void
dfb_dispose(vo_driver_t* vo_driver)
{
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;

	if(this)
	{
		if(this->main)
			this->main->Release(this->main);
		free(this);
	}
}


static vo_driver_t*
open_plugin(video_driver_class_t* vo_class, const void *vo_visual)
{
	dfb_driver_class_t* class = (dfb_driver_class_t*) vo_class;
	dfb_visual_t* visual      = (dfb_visual_t*) vo_visual;
	dfb_driver_t* this        = NULL;

	TEST(vo_class  != NULL);
	TEST(vo_visual != NULL);
	TEST(visual->output_cb != NULL);

	if(class->xine->verbosity)
		fprintf(stderr, "DFB [Unofficial DirectFB video driver]\n");

	TEST(this = (dfb_driver_t*) calloc(1, sizeof(dfb_driver_t)));

	this->vo_driver.get_capabilities     = dfb_get_capabilities;
	this->vo_driver.alloc_frame          = dfb_alloc_frame;
	this->vo_driver.update_frame_format  = dfb_update_frame_format;
	this->vo_driver.overlay_begin        = NULL;
	this->vo_driver.overlay_blend        = dfb_overlay_blend;
	this->vo_driver.overlay_end          = NULL;
	this->vo_driver.display_frame        = dfb_display_frame;
	this->vo_driver.get_property         = dfb_get_property;
	this->vo_driver.set_property         = dfb_set_property;
	this->vo_driver.get_property_min_max = dfb_get_property_min_max;
	this->vo_driver.gui_data_exchange    = dfb_gui_data_exchange;
	this->vo_driver.redraw_needed        = dfb_redraw_needed;
	this->vo_driver.dispose              = dfb_dispose;

	this->verbosity = class->xine->verbosity;

	{
		int accel = xine_mm_accel();

		if((accel & MM_MMX) == MM_MMX)
		{
			SAY("MMX detected and enabled");
			yuy2_cc.yuy2  = __mmx_yuy2_be_yuy2;
			yuy2_cc.uyvy  = __mmx_yuy2_be_uyvy;
			yuy2_cc.rgb15 = __mmx_yuy2_be_rgb15;
			yuy2_cc.rgb16 = __mmx_yuy2_be_rgb16;
			yuy2_cc.rgb24 = __mmx_yuy2_be_rgb24;
			yuy2_cc.rgb32 = __mmx_yuy2_be_rgb32;
			yv12_cc.yuy2  = __mmx_yv12_be_yuy2;
			yv12_cc.uyvy  = __mmx_yv12_be_uyvy;
			yv12_cc.yv12  = __mmx_yv12_be_yv12;
			yv12_cc.rgb15 = __mmx_yv12_be_rgb15;
			yv12_cc.rgb16 = __mmx_yv12_be_rgb16;
			yv12_cc.rgb24 = __mmx_yv12_be_rgb24;
			yv12_cc.rgb32 = __mmx_yv12_be_rgb32;
		}

	}

	{
		config_values_t* config = class->xine->config;

		if(config)
		{
			this->max_num_frames = config->register_num(config,
						"video.dfb.max_num_frames", 15,
						"Maximum number of allocated frames (at least 5)",
						NULL, 10, NULL, NULL);

			this->correction.defined = config->register_range(config,
						"video.dfb.gamma_correction", -16,
						-128, 127, "RGB gamma correction",
						NULL, 10, NULL, NULL);
		}
	}


	if(visual->surface)
	{
		this->main      = visual->surface;
		this->main_data = (IDirectFBSurface_data*) this->main->priv;

		if(!this->main_data)
		{
			fprintf(stderr, THIS ": bad surface\n");
			goto FAILURE;
		}

		switch(this->main_data->surface->format)
		{
			case DSPF_YUY2:
			case DSPF_UYVY:
			case DSPF_YV12:
			case DSPF_I420:
			{
				DBUG("surface format is YUV [%#x]",
					this->main_data->surface->format);
				this->correction.used = 0;
			}
			break;

			case DSPF_ARGB1555:
			case DSPF_RGB16:
			case DSPF_RGB24:
			case DSPF_RGB32:
			case DSPF_ARGB:
			case DSPF_AiRGB:
			{
				DBUG("surface format is RGB [%#x]",
					this->main_data->surface->format);
				this->correction.used = this->correction.defined;
			}
			break;

			default:
			{
				SAY("unsupported surface format [%#x]",
						this->main_data->surface->format);
				goto FAILURE;
			}
			break;
		}

		this->main->AddRef(this->main);
	}

	this->brightness.l_val     = this->correction.used;
	this->brightness.mm_val[0] = this->correction.used;
	this->brightness.mm_val[1] = this->correction.used;
	this->brightness.mm_val[2] = this->correction.used;
	this->brightness.mm_val[3] = this->correction.used;
	
	this->contrast.l_val     = 0x4000;
	this->contrast.mm_val[0] = 0x4000;
	this->contrast.mm_val[1] = 0x4000;
	this->contrast.mm_val[2] = 0x4000;
	this->contrast.mm_val[3] = 0x4000;

	this->output_cb    = visual->output_cb;
	this->output_cdata = visual->cdata;


	return(&(this->vo_driver));

FAILURE:
	release(this);
	return(NULL);
}


static char*
get_identifier(video_driver_class_t* vo_class)
{
	return("DFB");
}


static char*
get_description(video_driver_class_t* vo_class)
{
	return("Unofficial DirectFB video driver.");
}


static void
dispose_class(video_driver_class_t* vo_class)
{
	release(vo_class);
}


static void*
init_class(xine_t* xine, void* vo_visual)
{
	dfb_driver_class_t* class = NULL;

	TEST(xine != NULL);

	TEST(class = (dfb_driver_class_t*) calloc(1, sizeof(dfb_driver_class_t)));

	class->vo_class.open_plugin     = open_plugin;
	class->vo_class.get_identifier  = get_identifier;
	class->vo_class.get_description = get_description;
	class->vo_class.dispose         = dispose_class;
	class->xine                     = xine; 

	return(class);

FAILURE:
	return(NULL);
}



static vo_info_t vo_info_dfb =
{
	8,
	XINE_VISUAL_TYPE_DFB
};


plugin_info_t xine_plugin_info[] =
{
  /* type, API, "name", version, special_info, init_function */
  { PLUGIN_VIDEO_OUT, 18, "DFB", XINE_VERSION_CODE, &vo_info_dfb, init_class},
  { PLUGIN_NONE, 0, "", 0, NULL, NULL}
};

