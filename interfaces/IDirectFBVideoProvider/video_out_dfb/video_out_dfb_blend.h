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
 *  based on x11osd.c by Miguel Freitas
 *
 */



/* CLUT == Color LookUp Table */
typedef struct
{
	uint8_t cb    : 8;
	uint8_t cr    : 8;
	uint8_t y     : 8;
	uint8_t foo   : 8;

} __attribute__ ((packed)) clut_t;





static inline uint8_t*
_skip_pixels( uint8_t *dst, int w, int x )
{
	if (x & 1)
	{
		dst += 2;
		w--;
	}
	
	dst += w >> 1;

	if (w & 1)
		dst += 2;

	return dst;
}

static inline void
_skip_planes( uint8_t* dst[], int w, int y )
{
	dst[0] += w;

	if (y & 1)
	{
		dst[1] += w >> 1;
		dst[2] += w >> 2;
	}
}


static inline uint8_t*
_store_yuy2( uint8_t *dst, clut_t color, int w, int x )
{
	uint32_t pix = (color.y | (color.cb << 8) | 
			(color.y << 16) | (color.cr << 24));
	int      n   = w >> 1;

	if (x & 1)
	{
		*dst++ = color.y;
		*dst++ = color.cr;
		n = --w >> 1;
	}

	while (n--)
	{
		*((uint32_t*) dst) = pix;
 		dst += 4;
	}

	if (w & 1)
	{
		*dst++ = color.y;
		*dst++ = color.cb;
	}

	return dst;
}

static inline uint8_t*
_store_uyvy( uint8_t *dst, clut_t color, int w, int x )
{
	uint32_t pix = (color.cb | (color.y << 8) |
			(color.cr << 16) | (color.y << 24));
	int      n   = w >> 1;

	if (x & 1)
	{
		*dst++ = color.cr;
		*dst++ = color.y;
		n = --w >> 1;
	}

	while (n--)
	{
		*((uint32_t*) dst) = pix;
		dst += 4;
	}

	if (w & 1)
	{
		*dst++ = color.cb;
		*dst++ = color.y;
	}

	return dst;
}

static inline void
_store_yv12( uint8_t* dst[], clut_t color, int w, int y )
{
	memset( dst[0], color.y, w );
	dst[0] += w;

	if (y & 1)
	{
		memset( dst[1], color.cb, (w + 1) >> 1 );
		dst[1] += w >> 1;
		memset( dst[2], color.cr, (w + 1) >> 1 );
		dst[2] += w >> 1;
	}
}


static inline uint8_t*
_blend_yuy2( uint8_t *dst, clut_t color, int a, int w, int x )
{
	int aa = (a * 4369) + 1;
	int ia = 0x10000 - aa;
	int y  = color.y  * aa;
	int cb = color.cb * aa;
	int cr = color.cr * aa;
	int n  = w >> 1;

	if (x & 1)
	{
		*dst = (y  + (*dst * ia)) >> 16;
		dst++;
		*dst = (cr + (*dst * ia)) >> 16;
		dst++;
		n = --w >> 1;
	}

	while (n--)
	{
		register uint32_t pix;

		pix  = ( y  + (*dst       * ia)) >> 16;
		pix |= ((cb + (*(dst + 1) * ia)) >>  8) & 0x0000ff00;
		pix |= ((y  + (*(dst + 2) * ia))      ) & 0x00ff0000;
		pix |= ((cr + (*(dst + 3) * ia)) <<  8) & 0xff000000;

		*((uint32_t*) dst) = pix;
		dst += 4;
	}

	if (w & 1)
	{
		*dst = (y  + (*dst * ia)) >> 16;
		dst++;
		*dst = (cb + (*dst * ia)) >> 16;
		dst++;
	}

	return dst;
}

static inline uint8_t*
_blend_uyvy( uint8_t *dst, clut_t color, int a, int w, int x )
{
	int aa = (a * 4369) + 1;
	int ia = 0x10000 - aa;
	int y  = color.y  * aa;
	int cb = color.cb * aa;
	int cr = color.cr * aa;
	int n  = w >> 1;

	if (x & 1)
	{
		*dst = (cr + (*dst * ia)) >> 16;
		dst++;
		*dst = (y  + (*dst * ia)) >> 16;
		dst++;
		n = --w >> 1;
	}

	while (n--)
	{
		register uint32_t pix;

		pix  = ( cb + (*dst       * ia)) >> 16;
		pix |= ((y  + (*(dst + 1) * ia)) >>  8) & 0x0000ff00;
		pix |= ((cr + (*(dst + 2) * ia))      ) & 0x00ff0000;
		pix |= ((y  + (*(dst + 3) * ia)) <<  8) & 0xff000000;

		*((uint32_t*) dst) = pix;
		dst += 4;
	}

	if (w & 1)
	{
		*dst = (cb + (*dst * ia)) >> 16;
		dst++;
		*dst = (y  + (*dst * ia)) >> 16;
		dst++;
	}

	return dst;
}

static inline void
_blend_yv12( uint8_t *dst[], clut_t color, int a, int w, int y )
{
	uint8_t *dy = dst[0];
	uint8_t *du = dst[1];
	uint8_t *dv = dst[2];
	int      aa = (a * 4369) + 1;
	int      ia = 0x10000 - aa;
	int      yy = color.y * aa;
	int      cb;
	int      cr;
	int      n  = w;

	do
	{
		*dy = (yy + (*dy * ia)) >> 16;
		dy++;
		
	} while (--n);
	
	if (y & 1)
	{
		cb = color.cb * aa;
		cr = color.cr * aa;
		
		for (n = w >> 1; n--; )
		{
			*du = (cb + (*du * ia)) >> 16;
			du++;
			*dv = (cr + (*dv * ia)) >> 16;
			dv++;
		}

		if (w & 1)
		{
			*du = (cb + (*du * ia)) >> 16;
			*dv = (cr + (*dv * ia)) >> 16;
		}
	}

	dst[0] = dy;
	dst[1] = du;
	dst[2] = dv;
}




static void
dfb_overlay_blend_yuy2( dfb_frame_t *frame, vo_overlay_t *overlay )
{
	uint8_t   *src   = (uint8_t*) frame->surface->back_buffer->system.addr;
	uint32_t   pitch = (uint32_t) frame->surface->back_buffer->system.pitch;
	DFBRegion  clip;
	int        i, x, y;

	clip.x1 = overlay->x + overlay->clip_left;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.y2 = overlay->y + overlay->clip_bottom;

	if (clip.x2 > frame->width.cur)
		clip.x2 = frame->width.cur;

	if (clip.y2 > frame->height.cur)
		clip.y2 = frame->height.cur;

	src += overlay->y * pitch;

	for (i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t *dst   = src + (x << 1);
		int      len   = overlay->rle[i].len;
		int      index = overlay->rle[i].color & 0xff;

		while (len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if (y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if (x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if (x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if ((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if (alpha >= 15)
				dst = _store_yuy2( dst, color, width, x );
			else if (alpha)
				dst = _blend_yuy2( dst, color, alpha, width, x );
			else
				dst = _skip_pixels( dst, width, x );

			x += width;

			if (x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;

				if (y > frame->height.cur)
					return;

				src += pitch;
				dst  = src + (x << 1);
			}
		}
	}
}


static void
dfb_overlay_blend_uyvy( dfb_frame_t *frame, vo_overlay_t *overlay )
{
	uint8_t   *src    = (uint8_t*) frame->surface->back_buffer->system.addr;
	uint32_t   pitch  = (uint32_t) frame->surface->back_buffer->system.pitch;
	DFBRegion  clip;
	int        i, x, y;

	clip.x1 = overlay->x + overlay->clip_left;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.y2 = overlay->y + overlay->clip_bottom;

	if (clip.x2 > frame->width.cur)
		clip.x2 = frame->width.cur;

	if (clip.y2 > frame->height.cur)
		clip.y2 = frame->height.cur;

	src += overlay->y * pitch;

	for (i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t *dst   = src + (x << 1);
		int      len   = overlay->rle[i].len;
		int      index = overlay->rle[i].color & 0xff;

		while (len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if (y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if (x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if (x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if ((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if (alpha >= 15)
				dst = _store_uyvy( dst, color, width, x );
			else if (alpha)
				dst = _blend_uyvy( dst, color, alpha, width, x );
			else
				dst = _skip_pixels( dst, width, x );
			
			x += width;

			if (x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;

				if (y > frame->height.cur)
					return;

				src += pitch;
				dst  = src + (x << 1);
			}
		}
	}
}


static void
dfb_overlay_blend_yv12( dfb_frame_t *frame, vo_overlay_t *overlay )
{
	uint8_t   *src[3]   = {frame->surface->back_buffer->system.addr, };
	uint32_t   pitch[2] = {frame->width.cur, frame->width.cur >> 1};
	DFBRegion  clip;
	int        i, x, y;

	if (frame->dstfmt.cur == DSPF_YV12)
	{
		src[2]  = src[0] + (pitch[0] * frame->height.cur);
		src[1]  = src[2] + (pitch[1] * (frame->height.cur >> 1));
	} else
	{
		src[1]  = src[0] + (pitch[0] * frame->height.cur);
		src[2]  = src[1] + (pitch[1] * (frame->height.cur >> 1));
	}

	src[0] += overlay->y * pitch[0];
	src[1] += (overlay->y >> 1) * pitch[1];
	src[2] += (overlay->y >> 1) * pitch[1];

	clip.x1 = overlay->x + overlay->clip_left;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.y2 = overlay->y + overlay->clip_bottom;

	if (clip.x2 > frame->width.cur)
		clip.x2 = frame->width.cur;

	if (clip.y2 > frame->height.cur)
		clip.y2 = frame->height.cur;

	for (i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t *dst[3] = 
		{
			[0] = src[0] + x,
			[1] = src[1] + (x >> 1),
			[2] = src[2] + (x >> 1)
		};
		int      len    = overlay->rle[i].len;
		int      index  = overlay->rle[i].color & 0xff;

		while (len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if (y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if (x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if (x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if ((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if (alpha >= 15)
				_store_yv12( dst, color, width, y );
			else if (alpha)
				_blend_yv12( dst, color, alpha, width, y );
			else
				_skip_planes( dst, width, y );

			x += width;

			if (x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;

				if (y > frame->height.cur)
					return;

				src[0] += pitch[0];
				dst[0]  = src[0] + x;

				if (y & 1)
				{
					src[1] += pitch[1];
					dst[1]  = src[1] + (x >> 1);
					src[2] += pitch[1];
					dst[2]  = src[2] + (x >> 1);
				}
			}
		}
	}
}


static void
dfb_overlay_blend_rgb( dfb_frame_t *frame, vo_overlay_t *overlay )
{
	dfb_driver_t *this   = (dfb_driver_t*) frame->vo_frame.driver;
	DFBRegion     clip;
	int           i, x, y;

	if (!overlay->rgb_clut)
	{
		clut_t *clut = (clut_t*) overlay->color;

		/* avoid applying color adjustments */
		for (i = 0; i < (sizeof( overlay->color ) / sizeof( clut_t )); i++)
		{
			DFBColor *color   = (DFBColor*) &clut[i];
			int       y, u, v;
			int       r, g, b;

			y = clut[i].y + this->correction.used;
			u = clut[i].cb - 128;
			v = clut[i].cr - 128;
			
			r = y + ((v * V_RED_FACTOR) >> 14);
			g = y - (((v * V_GREEN_FACTOR) + (u * U_GREEN_FACTOR)) >> 14);
			b = y + ((u * U_BLUE_FACTOR) >> 14);

			color->r = (r < 0) ? 0 : ((r > 0xff) ? 0xff : r);
			color->g = (g < 0) ? 0 : ((g > 0xff) ? 0xff : g);
			color->b = (b < 0) ? 0 : ((b > 0xff) ? 0xff : b);
			color->a = overlay->trans[i] * 17;
		}

		overlay->rgb_clut++;
	}

	if (!overlay->clip_rgb_clut)
	{
		clut_t *clut = (clut_t*) overlay->clip_color;

		/* avoid applying color adjustments */
		for (i = 0; i < (sizeof( overlay->clip_color ) / sizeof( clut_t )); i++)
		{
			DFBColor *color   = (DFBColor*) &clut[i];
			int       y, u, v;
			int       r, g, b;

			y = clut[i].y  + this->correction.used;
			u = clut[i].cb - 128;
			v = clut[i].cr - 128;

			r = y + ((v * V_RED_FACTOR) >> 14);
			g = y - (((v * V_GREEN_FACTOR) + (u * U_GREEN_FACTOR)) >> 14);
			b = y + ((u * U_BLUE_FACTOR) >> 14);

			color->r = (r < 0) ? 0 : ((r > 0xff) ? 0xff : r);
			color->g = (g < 0) ? 0 : ((g > 0xff) ? 0xff : g);
			color->b = (b < 0) ? 0 : ((b > 0xff) ? 0xff : b);
			color->a = overlay->clip_trans[i] * 17;
		}

		overlay->clip_rgb_clut++;
	}

	clip.x1 = overlay->x + overlay->clip_left;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.y2 = overlay->y + overlay->clip_bottom;

	if (clip.x2 > frame->width.cur)
		clip.x2 = frame->width.cur;

	if (clip.y2 > frame->height.cur)
		clip.y2 = frame->height.cur;

	this->state.clip.x1      = 0;
	this->state.clip.x2      = frame->width.cur - 1;
	this->state.clip.y1      = 0;
	this->state.clip.y2      = frame->height.cur - 1;
	this->state.destination  = frame->surface;
	this->state.modified    |= (SMF_CLIP | SMF_DESTINATION);

	for (i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		int len   = overlay->rle[i].len;
		int index = overlay->rle[i].color & 0xff;

		while (len > 0)
		{
			DFBColor *palette = (DFBColor*) overlay->color;
			uint32_t  width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if (y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if (x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if (x > clip.x1)
				{
					palette = (DFBColor*) overlay->clip_color;

					if ((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if (palette[index].a)
			{
				DFBRectangle rect;
				
				this->state.color     = palette[index];
				this->state.modified |= SMF_COLOR;

				rect.x = x;
				rect.y = y;
				rect.w = width;
				rect.h = 1;

				dfb_gfxcard_fillrectangle( &rect, &this->state );
			}

			x += width;

			if (x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;

				if (y > frame->height.cur)
					goto end;
			}
		}
	}

end:
	this->state.destination  = this->dest_data->surface;
	this->state.modified    |= SMF_DESTINATION;
}



