/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002       convergence GmbH.

   All rights reserved.

   Written by Antonino Daplas <adaplas@pol.net>

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
#include <asm/types.h>

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <sys/mman.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <malloc.h>

#include <directfb.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/screens.h>

#include <core/accelerators.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

/* need fb handle to get accel, MMIO programming in the i810 is useless */
#include <fbdev/fbdev.h>
#include <gfx/convert.h>
#include <gfx/util.h>
#include <misc/conf.h>
#include <misc/util.h>

#include <core/graphics_driver.h>

DFB_GRAPHICS_DRIVER( i810 )

#include "i810.h"


#define TIMER_LOOP     1000000000
#define BUFFER_PADDING 2
#define MMIO_SIZE      512 * 1024

#define I810_SUPPORTED_DRAWINGFLAGS (DSDRAW_NOFX)

#define I810_SUPPORTED_DRAWINGFUNCTIONS   \
                 (DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE | DFXL_FILLTRIANGLE)

#define I810_SUPPORTED_BLITTINGFLAGS   \
                 (DSBLIT_SRC_COLORKEY | DSBLIT_DST_COLORKEY)

#define I810_SUPPORTED_BLITTINGFUNCTIONS  \
                 (DFXL_BLIT)

static void
i810_lring_enable(I810DriverData *i810drv, u32 mode)
{
	u32 tmp;

	tmp = i810_readl(i810drv->mmio_base, LRING + 12);
	tmp = (!mode) ? tmp & ~1 : tmp | 1;

	i810_writel(i810drv->mmio_base, LRING + 12, tmp);
}


static inline void
i810_wait_for_blit_idle(I810DriverData *i810drv,
			I810DeviceData *i810dev )
{
	u32 count = 0;
		
	if (i810dev != NULL)
		i810dev->idle_calls++;
	
	while ((i810_readw(i810drv->mmio_base, INSTDONE) & 0x7b) != 0x7b &&
	       count++ < TIMER_LOOP) {
		if (i810dev != NULL)
			i810dev->idle_waitcycles++;
	}
	
	if (count >= TIMER_LOOP) {
		if (i810dev != NULL)
			i810dev->idle_timeoutsum++;
		D_BUG("warning: idle timeout exceeded");
	}
}

static void
i810_init_ringbuffer(I810DriverData *i810drv,
		     I810DeviceData *i810dev )
{
	u32 tmp1, tmp2;
	
	i810_wait_for_blit_idle(i810drv, i810dev);
	i810_lring_enable(i810drv, 0);

	i810_writel(i810drv->mmio_base, LRING, 0);
	i810_writel(i810drv->mmio_base, LRING + 4, 0);
	i810dev->cur_tail = 0;

	tmp2 = i810_readl(i810drv->mmio_base, LRING + 8) & ~RBUFFER_START_MASK;
	tmp1 = i810dev->lring_bind.pg_start * 4096;
	i810_writel(i810drv->mmio_base, LRING + 8, tmp2 | tmp1);

	tmp1 = i810_readl(i810drv->mmio_base, LRING + 12);
	tmp1 &= ~RBUFFER_SIZE_MASK;
	tmp2 = (RINGBUFFER_SIZE - 4096) & RBUFFER_SIZE_MASK;
	i810_writel(i810drv->mmio_base, LRING + 12, tmp1 | tmp2);
	i810_lring_enable(i810drv, 1);
}

static inline int
i810_wait_for_space(I810DriverData *i810drv,
		    I810DeviceData *i810dev,
		    u32             space   )
{
	u32 head, count = TIMER_LOOP, tail, tries = 0;

	i810dev->waitfifo_calls++;

	tail = i810dev->cur_tail;

	space += BUFFER_PADDING;
	space <<= 2;
	i810dev->waitfifo_sum += space;

	while (count--) {
		i810dev->fifo_waitcycles++;
		head = i810_readl(i810drv->mmio_base, LRING + 4) & RBUFFER_HEAD_MASK;	
		if ((tail == head) ||
		    (tail > head && (RINGBUFFER_SIZE - tail + head) >= space) ||
		    (tail < head && (head - tail) >= space)) {
			if (!tries)
				i810dev->fifo_cache_hits++;
			return 0;	
		}
		tries++;
	}
	D_BUG("warning: buffer space timout error");
	return 1;
}


static void
i810FlushTextureCache(void *drv, void *dev)
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;

	if (BEGIN_LRING(i810drv, i810dev, 2)) return;

	PUT_LRING(PARSER | FLUSH);
	PUT_LRING(NOP);

	END_LRING(i810drv);
}

static void
i810EngineSync(void *drv, void *dev)
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;

	i810_wait_for_blit_idle(i810drv, i810dev);
}

/*
 * Set State routines
 */
static inline void
i810_set_src(I810DriverData *i810drv,
	     I810DeviceData *i810dev,
	     CardState      *state)
{
	CoreSurface   *source = state->source;
	SurfaceBuffer *buffer = source->front_buffer;

	if (i810dev->i_src)
		return;
	i810dev->srcaddr = dfb_gfxcard_memory_physical((GraphicsDevice *) i810dev,
						       buffer->video.offset);
	i810dev->srcpitch = buffer->video.pitch;

	i810dev->i_src = 1;
}

static inline void
i810_set_dest(I810DriverData *i810drv,
	      I810DeviceData *i810dev,
	      CardState      *state)
{
	CoreSurface   *destination = state->destination;
	SurfaceBuffer *buffer      = destination->back_buffer;
	
	if (i810dev->i_dst)
		return;
	i810dev->destaddr = dfb_gfxcard_memory_physical((GraphicsDevice *) i810dev,
							buffer->video.offset);
	i810dev->destpitch = buffer->video.pitch;
	
	switch (destination->format) {
	case DSPF_LUT8:
		i810dev->pixeldepth = 1;
		i810dev->blit_color = BPP8;
		break;
	case DSPF_ARGB1555:
		i810dev->pixeldepth = 2;
		i810dev->blit_color = BPP16;
		break;
	case DSPF_RGB16:
		i810dev->pixeldepth = 2;
		i810dev->blit_color = BPP16;
		break;
	case DSPF_RGB24:
		i810dev->pixeldepth = 3;
		i810dev->blit_color = BPP24;
		break;
	default:
		D_BUG("unexpected pixelformat~");
	}
	i810dev->i_dst = 1;
}
					
static inline void
i810_set_colorkey(I810DriverData *i810drv,
		  I810DeviceData *i810dev,
		  CardState      *state)
{
	if (i810dev->i_colorkey)
		return;

	i810dev->colorkey_bit = 0;
	if (state->blittingflags & DSBLIT_SRC_COLORKEY) {
		i810dev->colorkey_bit = 1 << 8;
		i810dev->colorkey = state->src_colorkey;
	}
	else {
		i810dev->colorkey_bit = 7 << 8;
		i810dev->colorkey = state->dst_colorkey;
	}
	
	i810dev->i_colorkey = 1;
}

static inline void
i810_set_color(I810DriverData *i810drv,
	       I810DeviceData *i810dev,
	       CardState      *state)
{
	if (i810dev->i_color)
		return;

	switch (state->destination->format) {
	case DSPF_LUT8:
		i810dev->color_value = state->color_index;
		break;
	case DSPF_ARGB1555:
		i810dev->color_value = PIXEL_ARGB1555(state->color.a,
                                                      state->color.r,
						      state->color.g,
						      state->color.b);
		break;
	case DSPF_RGB16:
		i810dev->color_value = PIXEL_RGB16(state->color.r,
						   state->color.g,
						   state->color.b);
		break;
	case DSPF_RGB24:
		i810dev->color_value = PIXEL_RGB32(state->color.r,
						   state->color.g,
						   state->color.b);
		break;
	default:
		D_BUG("unexpected pixelformat~");
	}
	i810dev->i_color = 1;
}

static inline void
i810_set_clip(I810DriverData *i810drv,
	      I810DeviceData *i810dev,
	      DFBRegion      *clip     )
{
	if (i810dev->i_clip)
		return;

	i810dev->clip_x1 = clip->x1;
	i810dev->clip_x2 = clip->x2 + 1;
	i810dev->clip_y1 = clip->y1;
	i810dev->clip_y2 = clip->y2 + 1;

	i810dev->i_clip = 1;
}
	
static void
i810CheckState(void *drv, void *dev,
	       CardState *state, DFBAccelerationMask accel )
{
	switch (state->destination->format) {
	case DSPF_LUT8:
	case DSPF_ARGB1555:
	case DSPF_RGB16:
	case DSPF_RGB24:
		break;
	default:
		return;
	}

	if (!(accel & ~I810_SUPPORTED_DRAWINGFUNCTIONS) &&
	    !(state->drawingflags & ~I810_SUPPORTED_DRAWINGFLAGS))
		state->accel |= I810_SUPPORTED_DRAWINGFUNCTIONS;
	
	if (!(accel & ~I810_SUPPORTED_BLITTINGFUNCTIONS) &&
	    !(state->blittingflags & ~I810_SUPPORTED_BLITTINGFLAGS)) {
		if (state->source->format == state->destination->format)
			state->accel |= I810_SUPPORTED_BLITTINGFUNCTIONS;
	}
}

static void
i810SetState( void *drv, void *dev,
	      GraphicsDeviceFuncs *funcs,
	      CardState *state, DFBAccelerationMask accel )
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;

	if (state->modified) {
		if ((state->modified & SMF_SOURCE) && state->source)
			i810dev->i_src = 0;
		if (state->modified & SMF_DESTINATION)
			i810dev->i_dst = 0;
		if (state->modified & SMF_COLOR)
			i810dev->i_color = 0;
		if (state->modified & SMF_CLIP)
			i810dev->i_clip = 0;
		if (state->modified & SMF_SRC_COLORKEY ||
		    state->modified & SMF_DST_COLORKEY) {
			i810dev->i_colorkey = 0;
		}
	}

	switch (accel) {
	case DFXL_DRAWRECTANGLE:
	case DFXL_FILLRECTANGLE:
	case DFXL_FILLTRIANGLE:
		i810_set_dest(i810drv, i810dev, state);
		i810_set_color(i810drv, i810dev, state);
		i810_set_clip(i810drv, i810dev, &state->clip);
		state->set |= DFXL_FILLRECTANGLE | DFXL_DRAWRECTANGLE;
		break;
	case DFXL_BLIT:
		i810_set_src( i810drv, i810dev, state);
		i810_set_dest(i810drv, i810dev, state);
		i810_set_color(i810drv, i810dev, state);
		i810_set_clip(i810drv, i810dev, &state->clip);
		if (state->blittingflags & DSBLIT_SRC_COLORKEY ||
		    state->blittingflags & DSBLIT_DST_COLORKEY)
			i810_set_colorkey(i810drv, i810dev, state);
		state->set |= DFXL_BLIT;
		break;
	default:
		D_BUG("unexpected drawing/blitting function");
	}
	state->modified = 0;
}

static bool
i810FillRectangle( void *drv, void *dev, DFBRectangle *rect )
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;
	u32 dest;

	
	if (rect->x < i810dev->clip_x1)
		rect->x = i810dev->clip_x1;
	if (i810dev->clip_x2 < rect->x + rect->w)
		rect->w = i810dev->clip_x2 - rect->x;
	if (rect->y < i810dev->clip_y1)
		rect->y = i810dev->clip_y1;
	if (i810dev->clip_y2 < rect->y + rect->h)
		rect->h = i810dev->clip_y2 - rect->y;

	rect->x *= i810dev->pixeldepth;
	rect->w *= i810dev->pixeldepth;
	dest = i810dev->destaddr + rect->x + (rect->y * i810dev->destpitch);
	
	if (BEGIN_LRING(i810drv, i810dev, 6)) return false;

	PUT_LRING(BLIT | COLOR_BLT | 3);
	PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch | SOLIDPATTERN |
		  DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING(rect->h << 16 | rect->w);
	PUT_LRING(dest);
	PUT_LRING(i810dev->color_value);
	PUT_LRING(NOP);

	END_LRING(i810drv);
	
	return true;
}

static bool
i810DrawRectangle( void *drv, void *dev, DFBRectangle *rect )
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;
	u32 dest;


	if (rect->x < i810dev->clip_x1)
		rect->x = i810dev->clip_x1;
	if (i810dev->clip_x2 < rect->x + rect->w)
		rect->w = i810dev->clip_x2 - rect->x;
	if (rect->y < i810dev->clip_y1)
		rect->y = i810dev->clip_y1;
	if (i810dev->clip_y2 < rect->y + rect->h)
		rect->h = i810dev->clip_y2 - rect->y;



	rect->x *= i810dev->pixeldepth;
	rect->w *= i810dev->pixeldepth;

	if (BEGIN_LRING(i810drv, i810dev, 20)) return false;

	/* horizontal line 1 */
	dest = i810dev->destaddr + rect->x + (rect->y * i810dev->destpitch);
	PUT_LRING(BLIT | COLOR_BLT | 3);
	PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch | SOLIDPATTERN |
		  DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING(1 << 16 | rect->w);
	PUT_LRING(dest);
	PUT_LRING(i810dev->color_value);

	/* vertical line 1 */
	PUT_LRING(BLIT | COLOR_BLT | 3);
	PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch | SOLIDPATTERN |
		  DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING(rect->h << 16 | i810dev->pixeldepth);
	PUT_LRING(dest);
	PUT_LRING(i810dev->color_value);

	/* vertical line 2 */
	dest += rect->w;
	PUT_LRING(BLIT | COLOR_BLT | 3);
	PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch | SOLIDPATTERN |
                              DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING(rect->h << 16 | i810dev->pixeldepth);
	PUT_LRING(dest);
	PUT_LRING(i810dev->color_value);

	/* horizontal line 2 */
	dest -= rect->w;
	dest += rect->h * i810dev->destpitch;
	PUT_LRING(BLIT | COLOR_BLT | 3);
	PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch | SOLIDPATTERN |
		  DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING(1 << 16 | rect->w);
	PUT_LRING(dest);
	PUT_LRING(i810dev->color_value);

	END_LRING(i810drv);
	
	return true;
}

static bool
i810Blit( void *drv, void *dev, DFBRectangle *rect, int dx, int dy )
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;
	int xdir = INCREMENT, spitch = 0, dpitch = 0, src, dest;

	if (dx < i810dev->clip_x1) {
		rect->w = MIN((i810dev->clip_x2 - i810dev->clip_x1),
			      (dx + rect->w) - i810dev->clip_x1);
		rect->x += i810dev->clip_x1 - dx;
		dx = i810dev->clip_x1;
	}
	if (i810dev->clip_x2 < dx + rect->w)
		rect->w = i810dev->clip_x2 - dx;

	if (dy < i810dev->clip_y1) {
		rect->h = MIN((i810dev->clip_y2 - i810dev->clip_y1),
			      (dy + rect->h) - i810dev->clip_y1);
		rect->y += i810dev->clip_y1 - dy;
		dy = i810dev->clip_y1;
	}
	if (i810dev->clip_y2 < dy + rect->h)
		rect->h = i810dev->clip_y2 - dy;

	rect->x *= i810dev->pixeldepth;
	dx *= i810dev->pixeldepth;
	rect->w *= i810dev->pixeldepth;

	spitch = i810dev->srcpitch;
	dpitch = i810dev->destpitch;

	if (i810dev->srcaddr == i810dev->destaddr) {
		if (dx > rect->x && dx < rect->x + rect->w) {
			xdir = DECREMENT;
			rect->x += rect->w - 1;
			dx += rect->w - 1;
		}
		if (dy > rect->y && dy < rect->y + rect->h) {
			i810dev->srcpitch = (-(i810dev->srcpitch)) & 0xFFFF;
			i810dev->destpitch = (-(i810dev->destpitch)) & 0xFFFF;
			rect->y += rect->h - 1;
			dy += rect->h - 1;
		}
	}

	src = i810dev->srcaddr + rect->x + (rect->y * spitch);
	dest = i810dev->destaddr + dx + (dy * dpitch);

	BEGIN_LRING(i810drv, i810dev, 8);

	PUT_LRING(BLIT | FULL_BLIT | 0x6 | i810dev->colorkey_bit);
	PUT_LRING(xdir | PAT_COPY_ROP << 16 | i810dev->destpitch |
		  DYN_COLOR_EN | i810dev->blit_color);
	PUT_LRING((rect->h << 16) | rect->w);
	PUT_LRING(dest);
	PUT_LRING(i810dev->srcpitch);
	PUT_LRING(src);
	PUT_LRING(i810dev->colorkey);
	PUT_LRING((u32) i810drv->pattern_base);

	END_LRING(i810drv);
	
	return true;
}

/*
 * The software rasterizer when rendering non-axis aligned
 * edges uses line spanning.  In our case, it will use
 * FillRect to render a 1 pixel-high rectangle.  However,
 * this would be slow in the i810 since for each rectangle,
 * an ioctl has to be done which is very slow.  As a temporary
 * replacement, I'll include a SpanLine function that will
 * not do an ioctl for every line.  This should be
 * significantly faster.
 */

/* borrowed heavily and shamelessly from gfxcard.c */

typedef struct {
   int xi;
   int xf;
   int mi;
   int mf;
   int _2dy;
} DDA;

#define SETUP_DDA(xs,ys,xe,ye,dda)         \
     do {                                  \
          int dx = xe - xs;                \
          int dy = ye - ys;                \
          dda.xi = xs;                     \
          if (dy != 0) {                   \
               dda.mi = dx / dy;           \
               dda.mf = 2*(dx % dy);       \
               dda.xf = -dy;               \
               dda._2dy = 2*dy;            \
               if (dda.mf < 0) {           \
                    dda.mf += ABS(dy)*2;   \
                    dda.mi--;              \
               }                           \
          }                                \
          else {                           \
               dda.mi   = 0;               \
               dda.mf   = 0;               \
               dda.xf   = 0;               \
               dda._2dy = 0;               \
          }                                \
     } while (0)


#define INC_DDA(dda)                       \
     do {                                  \
          dda.xi += dda.mi;                \
          dda.xf += dda.mf;                \
          if (dda.xf > 0) {                \
               dda.xi++;                   \
               dda.xf -= dda._2dy;         \
          }                                \
     } while (0)



/*
 * The i810fill_tri function takes advantage of the buffer.
 * It will fill up the buffer until it's done  rendering the
 * triangle.
 */
static inline bool
i810fill_tri( DFBTriangle    *tri,
	      I810DriverData *i810drv,
	      I810DeviceData *i810dev )
{
	int y, yend;

	DDA dda1, dda2;
	u32 total, dest = 0;

	y = tri->y1;
	yend = tri->y3;

	if (y < i810dev->clip_y1)
		y = i810dev->clip_y1;
	if (yend > i810dev->clip_y2)
		yend = i810dev->clip_y2;


	SETUP_DDA(tri->x1, tri->y1, tri->x3, tri->y3, dda1);
	SETUP_DDA(tri->x1, tri->y1, tri->x2, tri->y2, dda2);
	
	total = (yend - y) * 5;
	if (total + BUFFER_PADDING > RINGBUFFER_SIZE/4) {
		D_BUG("fill_triangle: buffer size is too small\n");
		return false;
	}
	
	BEGIN_LRING(i810drv, i810dev, total);

	while (y < yend) {
		DFBRectangle rect;

		if (y == tri->y2) {
			if (tri->y2 == tri->y3)
				return true;
			SETUP_DDA(tri->x2, tri->y2, tri->x3, tri->y3, dda2);
		}
		
		rect.w = ABS(dda1.xi - dda2.xi);
		rect.x = MIN(dda1.xi, dda2.xi);
		
		if (rect.w > 0) {
			rect.y = y;
			dest = i810dev->destaddr + (y * i810dev->destpitch) + (rect.x * i810dev->pixeldepth);
			PUT_LRING(BLIT | COLOR_BLT | 3);
			PUT_LRING(COLOR_COPY_ROP << 16 | i810dev->destpitch |
				  SOLIDPATTERN | DYN_COLOR_EN | i810dev->blit_color);
			PUT_LRING(1 << 16 | rect.w * i810dev->pixeldepth);
			PUT_LRING(dest);
			PUT_LRING(i810dev->color_value);
		}

		INC_DDA(dda1);
		INC_DDA(dda2);
		
		y++;
	}
	END_LRING(i810drv);
	return true;
}

static bool
i810FillTriangle( void *drv, void *dev, DFBTriangle *tri)
{
	I810DriverData *i810drv = (I810DriverData *) drv;
	I810DeviceData *i810dev = (I810DeviceData *) dev;
	bool err = true;

	dfb_sort_triangle(tri);
	
	if (tri->y3 - tri->y1 > 0)
		err = i810fill_tri(tri, i810drv, i810dev);

	return err;
}

static int
driver_probe( GraphicsDevice *device )
{
     switch (dfb_gfxcard_get_accelerator( device )) {
          case FB_ACCEL_I810:          /* Intel 810 */
		  return 1;
     }
     return 0;
}


static void
driver_get_info( GraphicsDevice     *device,
                 GraphicsDriverInfo *info )
{
     /* fill driver info structure */
     snprintf( info->name,
               DFB_GRAPHICS_DRIVER_INFO_NAME_LENGTH,
               "Intel 810/810E/810-DC100/815 Driver" );

     snprintf( info->vendor,
               DFB_GRAPHICS_DRIVER_INFO_VENDOR_LENGTH,
               "Tony Daplas" );

     snprintf( info->url,
               DFB_GRAPHICS_DRIVER_INFO_URL_LENGTH,
               "http://i810fb.sourceforge.net" );

     snprintf( info->license,
               DFB_GRAPHICS_DRIVER_INFO_LICENSE_LENGTH,
               "LGPL" );

     info->version.major = 0;
     info->version.minor = 5;

     info->driver_data_size = sizeof (I810DriverData);
     info->device_data_size = sizeof (I810DeviceData);
}

static void
i810_release_resource( I810DriverData *idrv, I810DeviceData *idev )
{
     agp_unbind unbind;

     if (idrv->flags & I810RES_STATE_SAVE) {
          i810_writel( idrv->mmio_base, LP_RING, idev->lring1 );
          i810_writel( idrv->mmio_base, LP_RING + RING_HEAD, idev->lring2 );
          i810_writel( idrv->mmio_base, LP_RING + RING_START, idev->lring3 );
          i810_writel( idrv->mmio_base, LP_RING + RING_LEN, idev->lring4 );
     }

     if (idrv->flags & I810RES_MMAP) {
          munmap((void *) idrv->aper_base, idev->info.aper_size * 1024 * 1024);
          idrv->flags &= ~I810RES_MMAP;
     }

     if (idrv->flags & I810RES_LRING_BIND) {
          unbind.key = idev->lring_bind.key;
          ioctl(idrv->agpgart, AGPIOC_UNBIND, &unbind);
     }

     if (idrv->flags & I810RES_LRING_ACQ)
          ioctl(idrv->agpgart, AGPIOC_DEALLOCATE, idev->lring_mem.key);

     if (idrv->flags & I810RES_OVL_BIND) {
          unbind.key = idev->ovl_bind.key;
          ioctl(idrv->agpgart, AGPIOC_UNBIND, &unbind);
     }

     if (idrv->flags & I810RES_OVL_ACQ)
          ioctl(idrv->agpgart, AGPIOC_DEALLOCATE, idev->ovl_mem.key);

     if (idrv->flags & I810RES_GART_ACQ) {
          ioctl(idrv->agpgart, AGPIOC_RELEASE);
          idrv->flags &= ~I810RES_GART_ACQ;
     }

     if (idrv->flags & I810RES_GART) {
          close(idrv->agpgart);
          idrv->flags &= ~I810RES_GART;
     }
}

static DFBResult
i810_agp_setup( GraphicsDevice *device,
                I810DriverData *idrv,
                I810DeviceData *idev )
{
     idrv->agpgart = open("/dev/agpgart", O_RDWR);
     if (idrv->agpgart == -1)
          return DFB_IO;
     D_FLAGS_SET( idrv->flags, I810RES_GART );


     if (ioctl(idrv->agpgart, AGPIOC_ACQUIRE)) {
          D_PERROR( "I810/AGP: AGPIOC_ACQUIRE failed!\n" );
          return DFB_IO;
     }
     D_FLAGS_SET( idrv->flags, I810RES_GART_ACQ );


     if (!idev->initialized) {
          agp_setup setup;

          setup.agp_mode = 0;
          if (ioctl(idrv->agpgart, AGPIOC_SETUP, &setup)) {
               D_PERROR( "I810/AGP: AGPIOC_SETUP failed!\n" );
               return DFB_IO;
          }
     
          if (ioctl(idrv->agpgart, AGPIOC_INFO, &idev->info)) {
               D_PERROR( "I810/AGP: AGPIOC_INFO failed!\n" );
               return DFB_IO;
          }
     }


     idrv->aper_base = mmap( NULL, idev->info.aper_size * 1024 * 1024, PROT_WRITE,
                             MAP_SHARED, idrv->agpgart, 0 );
     if (idrv->aper_base == MAP_FAILED) {
          D_PERROR( "I810/AGP: mmap() failed!\n" );
          i810_release_resource( idrv, idev );
          return DFB_IO;
     }
     D_FLAGS_SET( idrv->flags, I810RES_MMAP );


     if (!idev->initialized) {
          __u32 base;

          /* We'll attempt to bind at fb_base + fb_len + 1 MB,
          to be safe */
          base = dfb_gfxcard_memory_physical(device, 0) - idev->info.aper_base;
          base += dfb_gfxcard_memory_length();
          base += (1024 * 1024);
     
          idev->lring_mem.pg_count = RINGBUFFER_SIZE/4096;
          idev->lring_mem.type = AGP_NORMAL_MEMORY;
          if (ioctl(idrv->agpgart, AGPIOC_ALLOCATE, &idev->lring_mem)) {
               D_PERROR( "I810/AGP: AGPIOC_ALLOCATE failed!\n" );
               i810_release_resource( idrv, idev );
               return DFB_IO;
          }
          D_FLAGS_SET( idrv->flags, I810RES_LRING_ACQ );
     
          idev->lring_bind.key = idev->lring_mem.key;
          idev->lring_bind.pg_start = base/4096;
          if (ioctl(idrv->agpgart, AGPIOC_BIND, &idev->lring_bind)) {
               D_PERROR( "I810/AGP: AGPIOC_BIND failed!\n" );
               i810_release_resource( idrv, idev );
               return DFB_IO;
          }
          D_FLAGS_SET( idrv->flags, I810RES_LRING_BIND );
     
          idev->ovl_mem.pg_count = 1;
          idev->ovl_mem.type = AGP_PHYSICAL_MEMORY;
          if (ioctl(idrv->agpgart, AGPIOC_ALLOCATE, &idev->ovl_mem)) {
               D_PERROR( "I810/AGP: AGPIOC_ALLOCATE failed!\n" );
               i810_release_resource( idrv, idev );
               return DFB_IO;
          }
          D_FLAGS_SET( idrv->flags, I810RES_OVL_ACQ );
     
          idev->ovl_bind.key = idev->ovl_mem.key;
          idev->ovl_bind.pg_start = (base + RINGBUFFER_SIZE)/4096;
          if (ioctl(idrv->agpgart, AGPIOC_BIND, &idev->ovl_bind)) {
               D_PERROR( "I810/AGP: AGPIOC_BIND failed!\n" );
               i810_release_resource( idrv, idev );
               return DFB_IO;
          }
          D_FLAGS_SET( idrv->flags, I810RES_OVL_BIND );
     }


     if (idrv->flags & I810RES_GART_ACQ) {
          ioctl(idrv->agpgart, AGPIOC_RELEASE);
          idrv->flags &= ~I810RES_GART_ACQ;
     }


     idrv->lring_base   = idrv->aper_base + idev->lring_bind.pg_start * 4096;
     idrv->ovl_base     = idrv->aper_base + idev->ovl_bind.pg_start * 4096;
     idrv->pattern_base = idrv->ovl_base + 1024;

     if (!idev->initialized) {
          memset((void *) idrv->ovl_base, 0xff, 1024);
          memset((void *) idrv->pattern_base, 0xff, 4096 - 1024);

          idev->lring1 = 0;//i810_readl(idrv->mmio_base, LP_RING);
          idev->lring2 = 0;//i810_readl(idrv->mmio_base, LP_RING + RING_HEAD);
          idev->lring3 = 0;//i810_readl(idrv->mmio_base, LP_RING + RING_START);
          idev->lring4 = 0;//i810_readl(idrv->mmio_base, LP_RING + RING_LEN);

          D_FLAGS_SET( idrv->flags, I810RES_STATE_SAVE );
     }

     idev->initialized = true;

     return DFB_OK;
}

static DFBResult
driver_init_driver( GraphicsDevice      *device,
                    GraphicsDeviceFuncs *funcs,
                    void                *driver_data,
                    void                *device_data,
                    CoreDFB             *core )
{
     DFBResult       ret;
     I810DriverData *idrv = driver_data;
     I810DeviceData *idev = device_data;

     idrv->idev = device_data;

     idrv->mmio_base = (volatile __u8*) dfb_gfxcard_map_mmio( device, 0, -1 );
     if (!idrv->mmio_base)
          return DFB_IO;

     ret = i810_agp_setup( device, idrv, idev );
     if (ret) {
          dfb_gfxcard_unmap_mmio( device, idrv->mmio_base, -1 );
          return ret;
     }

     idrv->info = idev->info;


     funcs->CheckState         = i810CheckState;
     funcs->SetState           = i810SetState;
     funcs->EngineSync         = i810EngineSync;
     funcs->FlushTextureCache  = i810FlushTextureCache;

     funcs->FillRectangle      = i810FillRectangle;
     funcs->DrawRectangle      = i810DrawRectangle;
     funcs->Blit               = i810Blit;
     funcs->FillTriangle       = i810FillTriangle;

     dfb_layers_register( dfb_screens_at(DSCID_PRIMARY), driver_data, &i810OverlayFuncs );

     return DFB_OK;
}

static DFBResult
driver_init_device( GraphicsDevice     *device,
                    GraphicsDeviceInfo *device_info,
                    void               *driver_data,
                    void               *device_data )
{
     I810DriverData *idrv = driver_data;
     I810DeviceData *idev = device_data;

     /* fill device info */
     snprintf( device_info->name,
               DFB_GRAPHICS_DEVICE_INFO_NAME_LENGTH, "810/810E/810-DC100/815" );

     snprintf( device_info->vendor,
               DFB_GRAPHICS_DEVICE_INFO_VENDOR_LENGTH, "Intel" );

     device_info->caps.flags    = CCF_CLIPPING;
     device_info->caps.accel    = I810_SUPPORTED_DRAWINGFUNCTIONS |
                                  I810_SUPPORTED_BLITTINGFUNCTIONS;
     device_info->caps.drawing  = I810_SUPPORTED_DRAWINGFLAGS;
     device_info->caps.blitting = I810_SUPPORTED_BLITTINGFLAGS;

     device_info->limits.surface_byteoffset_alignment = 32 * 4;
     device_info->limits.surface_pixelpitch_alignment = 32;

     dfb_config->pollvsync_after = 1;

     i810_init_ringbuffer( idrv, idev );

     return DFB_OK;
}

static void
driver_close_device( GraphicsDevice *device,
		     void           *driver_data,
		     void           *device_data )
{
	I810DeviceData *i810dev = (I810DeviceData *) device_data;
	I810DriverData *i810drv = (I810DriverData *) driver_data;

    i810ovlOnOff( i810drv, i810dev, false );

    i810_wait_for_blit_idle( i810drv, i810dev );
    i810_lring_enable( i810drv, 0 );

    i810_release_resource( i810drv, i810dev );


	D_DEBUG( "DirectFB/I810: DMA Buffer Performance Monitoring:\n");
	D_DEBUG( "DirectFB/I810:  %9d DMA buffer size in KB\n",
		  RINGBUFFER_SIZE/1024 );
	D_DEBUG( "DirectFB/I810:  %9d i810_wait_for_blit_idle calls\n",
		  i810dev->idle_calls );
	D_DEBUG( "DirectFB/I810:  %9d i810_wait_for_space calls\n",
		  i810dev->waitfifo_calls );
	D_DEBUG( "DirectFB/I810:  %9d BUFFER transfers (i810_wait_for_space sum)\n",
		  i810dev->waitfifo_sum );
	D_DEBUG( "DirectFB/I810:  %9d BUFFER wait cycles (depends on GPU/CPU)\n",
		  i810dev->fifo_waitcycles );
	D_DEBUG( "DirectFB/I810:  %9d IDLE wait cycles (depends on GPU/CPU)\n",
		  i810dev->idle_waitcycles );
	D_DEBUG( "DirectFB/I810:  %9d BUFFER space cache hits(depends on BUFFER size)\n",
		  i810dev->fifo_cache_hits );
	D_DEBUG( "DirectFB/I810:  %9d BUFFER timeout sum (possible hardware crash)\n",
		  i810dev->fifo_timeoutsum );
	D_DEBUG( "DirectFB/I810:  %9d IDLE timeout sum (possible hardware crash)\n",
		  i810dev->idle_timeoutsum );
	D_DEBUG( "DirectFB/I810: Conclusion:\n" );
	D_DEBUG( "DirectFB/I810:  Average buffer transfers per i810_wait_for_space "
		  "call: %.2f\n",
		  i810dev->waitfifo_sum/(float)(i810dev->waitfifo_calls) );
	D_DEBUG( "DirectFB/I810:  Average wait cycles per i810_wait_for_space call:"
		  " %.2f\n",
		  i810dev->fifo_waitcycles/(float)(i810dev->waitfifo_calls) );
	D_DEBUG( "DirectFB/I810:  Average wait cycles per i810_wait_for_blit_idle call:"	
		  " %.2f\n",
		  i810dev->idle_waitcycles/(float)(i810dev->idle_calls) );
	D_DEBUG( "DirectFB/I810:  Average buffer space cache hits: %02d%%\n",
		  (int)(100 * i810dev->fifo_cache_hits/
			(float)(i810dev->waitfifo_calls)) );

}

static void
driver_close_driver( GraphicsDevice *device,
                     void           *driver_data )
{
	I810DriverData *idrv = (I810DriverData *) driver_data;

	dfb_gfxcard_unmap_mmio( device, idrv->mmio_base, -1);

    if (idrv->flags & I810RES_MMAP) {
         munmap((void *) idrv->aper_base, idrv->info.aper_size * 1024 * 1024);
         idrv->flags &= ~I810RES_MMAP;
    }

    if (idrv->flags & I810RES_GART_ACQ) {
         ioctl(idrv->agpgart, AGPIOC_RELEASE);
         idrv->flags &= ~I810RES_GART_ACQ;
    }

    if (idrv->flags & I810RES_GART) {
         close(idrv->agpgart);
         idrv->flags &= ~I810RES_GART;
    }
}

