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
 *  TODO: speed up at 24bpp and 32bpp
 *
 */

#include <config.h>

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
#include "video_out_dfb_blend.h"
#ifdef ARCH_X86
#include "video_out_dfb_mmx.h"
#endif


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
 *
 * I'm using this formula for YUV->RGB:
 *
 *   R = Y + (1.40200 * (V - 128))
 *   G = Y - (0.71414 * (V - 128)) - (0.34414 * (U - 128))
 *   B = Y + (1.77200 * (U - 128))
 *
 *
 * Conversion is done as follows:
 *
 *   r = y + v_red_table[v];
 *   g = y - (v_green_table[v] + u_green_table[u]);
 *   b = y + u_blue_table[u];
 *
 * then values are clamped to 0-256.
 *
 * You can play with the "video.dfb.gamma_correction"
 * entry in ~/.xine/config to adjust colors conversion:
 * this variable specifies a value that is added to y
 * (default is -16 because many encoders use a range
 *  of 16-255 for luminance).
 *
 */




static void
__dummy_yuy2_be_yuy2(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint32_t n        = (frame->width * frame->height) >> 1;
	int32_t bright    = this->brightness.l_val;
	int32_t ctr       = this->contrast.l_val;


	do
	{
		register int y;

		y = *yuv_data + bright;
		y *= ctr;
		y >>= 14;
		*data = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);

		*(data + 1) = *(yuv_data + 1);

		y = *(yuv_data + 2) + bright;
		y *= ctr;
		y >>= 14;
		*(data + 2) = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);

		*(data + 3) = *(yuv_data + 3);

		data     += 4;
		yuv_data += 4;

	} while(--n);
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
			y *= ctr;
			y >>= 14;
			*(dest + 1) = (y < 0) ? 0 :
				      ((y > 0xff) ? 0xff : y);

			*(dest + 2) = *(yuv_data + 3);

			y = *(yuv_data + 2) + bright;
			y *= ctr;
			y >>= 14;
			*(dest + 3) = (y < 0) ? 0 :
				      ((y > 0xff) ? 0xff : y);

			*(dest + 4) = *(yuv_data + 5);

			y = *(yuv_data + 4) + bright;
			y *= ctr;
			y >>= 14;
			*(dest + 5) = (y < 0) ? 0 :
				      ((y > 0xff) ? 0xff : y);

			*(dest + 6) = *(yuv_data + 7);

			y = *(yuv_data + 6) + bright;
			y *= ctr;
			y >>= 14;
			*(dest + 7) = (y < 0) ? 0 :
				      ((y > 0xff) ? 0xff : y);

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


static void
__dummy_yuy2_be_yv12(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* yuv_data = frame->vo_frame.base[0];
	uint8_t* y_off    = (uint8_t*) data;
	uint8_t* u_off;
	uint8_t* v_off;
	uint32_t line     = frame->width >> 2;
	uint32_t n        = (frame->width * frame->height) >> 3;
	uint32_t l        = frame->vo_frame.pitches[0];
	int32_t bright    = this->brightness.l_val;
	int32_t ctr       = this->contrast.l_val;
	
	
	if(frame->surface->format == DSPF_YV12)
	{
		v_off = (uint8_t*) data + (pitch * frame->height);
		u_off = (uint8_t*) v_off + ((pitch * frame->height) >> 2);
	} else
	{
		u_off = (uint8_t*) data + (pitch * frame->height);
		v_off = (uint8_t*) u_off + ((pitch * frame->height) >> 2);
	}

	if(!bright && ctr == 0x4000)
	{
		do
		{
			*y_off       = *yuv_data;
			*(y_off + 1) = *(yuv_data + 2);
			*(y_off + 2) = *(yuv_data + 4);
			*(y_off + 3) = *(yuv_data + 6);
			
			*(y_off + pitch)     = *(yuv_data + l);
			*(y_off + pitch + 1) = *(yuv_data + l + 2);
			*(y_off + pitch + 2) = *(yuv_data + l + 4);
			*(y_off + pitch + 3) = *(yuv_data + l + 6);

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
	} else
	{
		do
		{
			register int y;

			y = *yuv_data + bright;
			y *= ctr;
			y >>= 14;
			*y_off = (y < 0) ? 0 :
				 ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + 2) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + 1) = (y < 0) ? 0 :
				       ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + 4) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + 2) = (y < 0) ? 0 :
				       ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + 6) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + 3) = (y < 0) ? 0 :
				       ((y > 0xff) ? 0xff : y);

			y = *( yuv_data + l) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + pitch) = (y < 0) ? 0 :
				           ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + l + 2) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + pitch + 1) = (y < 0) ? 0 :
				               ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + l + 4) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + pitch + 2) = (y < 0) ? 0 :
				               ((y > 0xff) ? 0xff : y);

			y = *(yuv_data + l + 6) + bright;
			y *= ctr;
			y >>= 14;
			*(y_off + pitch + 3) = (y < 0) ? 0 :
				               ((y > 0xff) ? 0xff : y);

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
}


static void
__dummy_yuy2_be_rgb8(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
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

		r = y + m1;
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*data = ((r & 0xe0) | (g & 0x1c) | b);

		y = *(yuv_data + 2) + bright;

		r = y + m1;
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*(data + 1) = ((r & 0xe0) | (g & 0x1c) | b);


		yuv_data += 4;
		data     += 2;

	} while(--n);

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
	rgb8:   __dummy_yuy2_be_rgb8,
	rgb15:	__dummy_yuy2_be_rgb15,
	rgb16:	__dummy_yuy2_be_rgb16,
	rgb24:  __dummy_yuy2_be_rgb24,
	rgb32:	__dummy_yuy2_be_rgb32
};






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
				y *= ctr;
				y >>= 14;
				*data = (y < 0) ? 0 :
					((y > 0xff) ? 0xff : y);

				y = *(y_data + 1) + bright;
				y *= ctr;
				y >>= 14;
				*(data + 2) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);

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
				y *= ctr;
				y >>= 14;
				*data = (y < 0) ? 0 :
					((y > 0xff) ? 0xff : y);

				y = *(y_data + 1) + bright;
				y *= ctr;
				y >>= 14;
				*(data + 2) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);
			
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
				y *= ctr;
				y >>= 14;
				*(data + 1) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);

				y = *(y_data + 1) + bright;
				y *= ctr;
				y >>= 14;
				*(data + 3) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);

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
				y *= ctr;
				y >>= 14;
				*(data + 1) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);

				y = *(y_data + 1) + bright;
				y *= ctr;
				y >>= 14;
				*(data + 3) = (y < 0) ? 0 :
					      ((y > 0xff) ? 0xff : y);

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


static void
__dummy_yv12_be_yv12(dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch)
{
	uint8_t* y_data = frame->vo_frame.base[0];
	uint32_t n      = (frame->width * frame->height) >> 1;
	int32_t bright  = this->brightness.l_val;
	int32_t ctr     = this->contrast.l_val;


	do
	{
		register int y;

		y = *y_data + bright;
		y *= ctr;
		y >>= 14;
		*data = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);

		y = *(y_data + 1) + bright;
		y *= ctr;
		y >>= 14;
		*(data + 1) = (y < 0) ? 0 : ((y > 0xff) ? 0xff : y);

		y_data += 2;
		data   += 2;

	} while(--n);

	xine_fast_memcpy(data, (frame->surface->format == DSPF_YV12)
				? frame->vo_frame.base[2]
				: frame->vo_frame.base[1],
			(pitch * frame->height) >> 2);
	data += (pitch * frame->height) >> 2;

	xine_fast_memcpy(data, (frame->surface->format == DSPF_YV12)
			       ? frame->vo_frame.base[1]
			       : frame->vo_frame.base[2],
			(pitch * frame->height) >> 2);
}


static void
__dummy_yv12_be_rgb8(dfb_driver_t* this, dfb_frame_t* frame,
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
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*data = ((r & 0xe0) | (g & 0x1c) | b);

		y = *(y_data + 1) + bright;

		r = y + m1;
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*(data + 1) = ((r & 0xe0) | (g & 0x1c) | b);

		y = *(y_data + frame->width) + bright;

		r = y + m1;
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*(data + pitch) = ((r & 0xe0) | (g & 0x1c) | b);

		y = *(y_data + frame->width + 1) + bright;

		r = y + m1;
		g = (y - m2) >> 3;
		b = (y + m3) >> 6;

		r = ((r < 0) ? 0 : ((r > 0xff) ? 0xe0 : r));
		g = ((g < 0) ? 0 : ((g > 0x1f) ? 0x1c : g));
		b = ((b < 0) ? 0 : ((b > 0x03) ? 0x03 : b));

		*(data + pitch + 1) = ((r & 0xe0) | (g & 0x1c) | b);

		y_data += 2;
		data   += 2;

		if(!(--line))
		{
			line    = frame->width >> 1;
			y_data += frame->width;
			data   += pitch;
		}

	} while(--n);

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
	rgb8:   __dummy_yv12_be_rgb8,
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
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;
#ifdef DFB_DEBUG
	static int test = 8;
#endif
	
	TEST(vo_frame != NULL);
	TEST(frame->surface != NULL);

	vo_frame->proc_called = 1;

	if(frame->proc_needed)
	{
		uint8_t* data;
		uint32_t pitch;

		data  = (uint8_t*) frame->surface->back_buffer->system.addr;
		pitch = (uint32_t) frame->surface->back_buffer->system.pitch;

		SPEED(frame->realize((dfb_driver_t*) vo_frame->driver,
					 frame, data, pitch));
	}

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
	dfb_driver_t* this = (dfb_driver_t*) vo_driver;
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;
	yuv_render_t* yuv_cc;

	TEST(vo_driver  != NULL);
	TEST(vo_frame   != NULL);
	TEST(this->main != NULL);
	TEST(this->main_data != NULL);

	/* width must be a multiple of 4 */
	frame->width  = width + ((width & 3) ? (4 - (width & 3)) : 0);
	/* height must be a multiple of 2 */
	frame->height = height + (height & 1);

	frame->proc_needed = 1;

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
	
	if(format == XINE_IMGFMT_YUY2)
	{
		ONESHOT("video frame format is YUY2");

		yuv_cc = &yuy2_cc;
		vo_frame->pitches[0] = frame->width << 1;

		if(frame->surface->format == DSPF_YUY2 &&
		   !this->brightness.l_val && this->contrast.l_val == 0x4000)
		{
			frame->proc_needed = 0;
			vo_frame->base[0]  = (uint8_t*)
					frame->surface->back_buffer->system.addr;
		} else
		{
			vo_frame->base[0] = (uint8_t*) xine_xmalloc_aligned(16,
							vo_frame->pitches[0] *
							frame->height,
							&(frame->chunks[0]));
			TEST(vo_frame->base[0] != NULL);
		}

	} else /* assume XINE_IMGFMT_YV12 */
	{
		ONESHOT("video frame format is YV12");

		yuv_cc = &yv12_cc;
		vo_frame->pitches[0] = frame->width;
		vo_frame->pitches[1] = frame->width >> 1;
		vo_frame->pitches[2] = frame->width >> 1;

		if((frame->surface->format == DSPF_YV12 ||
			frame->surface->format == DSPF_I420) &&
		    !this->brightness.l_val && this->contrast.l_val == 0x4000)
		{
			frame->proc_needed = 0;
			vo_frame->base[0]  = (uint8_t*)
					frame->surface->back_buffer->system.addr;
			if(frame->surface->format == DSPF_YV12)
			{
				vo_frame->base[2] = (uint8_t*) vo_frame->base[0] +
							(frame->width * frame->height);
				vo_frame->base[1] = (uint8_t*) vo_frame->base[2] +
							((frame->width * frame->height) >> 2);
			} else
			{
				vo_frame->base[1] = (uint8_t*) vo_frame->base[0] +
							(frame->width * frame->height);
				vo_frame->base[2] = (uint8_t*) vo_frame->base[1] +
							((frame->width * frame->height) >> 2);
			}
		} else
		{
			vo_frame->base[0] = (uint8_t*) xine_xmalloc_aligned(16,
							vo_frame->pitches[0] *
							frame->height,
							&(frame->chunks[0]));
			vo_frame->base[1] = (uint8_t*) xine_xmalloc_aligned(16,
							vo_frame->pitches[1] *
							(frame->height >> 1) + 2,
							&(frame->chunks[1]));
			vo_frame->base[2] = (uint8_t*) xine_xmalloc_aligned(16,
							vo_frame->pitches[2] *
							(frame->height >> 1) + 2,
							&(frame->chunks[2]));
			TEST(vo_frame->base[0] != NULL);
			TEST(vo_frame->base[1] != NULL);
			TEST(vo_frame->base[2] != NULL);
		}

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

		case DSPF_RGB332:
			frame->realize = yuv_cc->rgb8;
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
dfb_overlay_blend(vo_driver_t* vo_driver, vo_frame_t* vo_frame,
			vo_overlay_t* overlay)
{
	dfb_frame_t* frame = (dfb_frame_t*) vo_frame;

	TEST(vo_frame  != NULL);
	TEST(overlay   != NULL);
	TEST(frame->surface != NULL);

	if(!overlay->rle)
		return;

	switch(frame->surface->format)
	{
		case DSPF_YUY2:
			dfb_overlay_blend_yuy2(frame, overlay);
		break;

		case DSPF_UYVY:
			dfb_overlay_blend_uyvy(frame, overlay);
		break;

		case DSPF_YV12:
 		case DSPF_I420:
			dfb_overlay_blend_yv12(frame, overlay);
		break;

		default:
			dfb_overlay_blend_rgb(frame, overlay);
		break;
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

	if(dest_rect.w < 1 || dest_rect.h < 1)
		dest_rect = this->main_data->area.wanted;

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

				case DSPF_RGB332:
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

#ifdef ARCH_X86
	{
		int accel = xine_mm_accel();

		if((accel & MM_MMX) == MM_MMX)
		{
			SAY("MMX detected and enabled");
			yuy2_cc.yuy2  = __mmx_yuy2_be_yuy2;
			yuy2_cc.uyvy  = __mmx_yuy2_be_uyvy;
			yuy2_cc.yv12  = __mmx_yuy2_be_yv12;
			yuy2_cc.rgb8  = __mmx_yuy2_be_rgb8;
			yuy2_cc.rgb15 = __mmx_yuy2_be_rgb15;
			yuy2_cc.rgb16 = __mmx_yuy2_be_rgb16;
			yuy2_cc.rgb24 = __mmx_yuy2_be_rgb24;
			yuy2_cc.rgb32 = __mmx_yuy2_be_rgb32;
			yv12_cc.yuy2  = __mmx_yv12_be_yuy2;
			yv12_cc.uyvy  = __mmx_yv12_be_uyvy;
			yv12_cc.yv12  = __mmx_yv12_be_yv12;
			yv12_cc.rgb8  = __mmx_yv12_be_rgb8;
			yv12_cc.rgb15 = __mmx_yv12_be_rgb15;
			yv12_cc.rgb16 = __mmx_yv12_be_rgb16;
			yv12_cc.rgb24 = __mmx_yv12_be_rgb24;
			yv12_cc.rgb32 = __mmx_yv12_be_rgb32;
		}

	}
#endif

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

			case DSPF_RGB332:
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


	return((vo_driver_t*) this);

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
  { PLUGIN_VIDEO_OUT, VIDEO_OUT_DRIVER_IFACE_VERSION, "DFB",
	  XINE_VERSION_CODE, &vo_info_dfb, init_class},
  { PLUGIN_NONE, 0, "", 0, NULL, NULL}
};

