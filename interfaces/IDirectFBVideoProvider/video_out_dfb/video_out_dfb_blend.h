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

#ifndef VIDEO_OUT_DFB_BLEND_H
#define VIDEO_OUT_DFB_BLEND_H



/* CLUT == Color LookUp Table */
typedef struct
{
	uint8_t cb    : 8;
	uint8_t cr    : 8;
	uint8_t y     : 8;
	uint8_t foo   : 8;

} __attribute__ ((packed)) clut_t;






static void
dfb_overlay_blend_yuy2(dfb_frame_t* frame, vo_overlay_t* overlay)
{
	uint8_t* data  = (uint8_t*) frame->surface->back_buffer->system.addr;
	uint32_t pitch = (uint32_t) frame->surface->back_buffer->system.pitch;
	DFBRegion clip;
	uint32_t i, x, y;

	clip.x1 = overlay->x + overlay->clip_left;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y2 = overlay->y + overlay->clip_bottom;

	for(i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t* dest  = data + (y * pitch) + (x << 1);
		int32_t len    = overlay->rle[i].len;
		uint32_t index = overlay->rle[i].color & 0xff;

		while(len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if(y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if(x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if(x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if(alpha >= 15)
			{
				uint32_t w = width;
				uint32_t n;

				if(x & 1)
				{
					*(dest++) = color.y;
					*(dest++) = color.cr;
					w--;
				}

				n = w >> 1;

				while(n--)
				{
					*dest       = color.y;
					*(dest + 1) = color.cb;
					*(dest + 2) = color.y;
					*(dest + 3) = color.cr;

					dest += 4;
				}

				if(w & 1)
				{
					*(dest++) = color.y;
					*(dest++) = color.cb;
				}

			} else
			if(alpha)
			{
				uint32_t w      = width;
				uint32_t ialpha = 15 - alpha;
				uint32_t ya, cra, cba;
				uint32_t n;

				ya  = color.y * alpha;
				cra = color.cr * alpha;
				cba = color.cb * alpha;

				if(x & 1)
				{
					register int b;

					b = (ya  + (*dest * ialpha)) / 15;
					*(dest++) = b;

					b = (cra + (*dest * ialpha)) / 15;
					*(dest++) = b;

					w--;
				}

				n = w >> 1;

				while(n--)
				{
					register int b;

					b = (ya  + (*dest * ialpha)) / 15;
					*dest = b;

					b = (cba + (*(dest + 1) * ialpha)) / 15;
					*(dest + 1) = b;

					b = (ya  + (*(dest + 2) * ialpha)) / 15;
					*(dest + 2) = b;

					b = (cra + (*(dest + 3) * ialpha)) / 15;
					*(dest + 3) = b;

					dest += 4;
				}

				if(w & 1)
				{
					register int b;

					b = (ya  + (*dest * ialpha)) / 15;
					*(dest++) = b;

					b = (cba + (*dest * ialpha)) / 15;
					*(dest++) = b;
				}

			} else
			{
				dest += width << 1;
			}

			x += width;

			if(x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;
				dest = data + (y * pitch) + (x << 1);
			}
		}
	}
}


static void
dfb_overlay_blend_uyvy(dfb_frame_t* frame, vo_overlay_t* overlay)
{
	uint8_t* data  = (uint8_t*) frame->surface->back_buffer->system.addr;
	uint32_t pitch = (uint32_t) frame->surface->back_buffer->system.pitch;
	DFBRegion clip;
	uint32_t i, x, y;

	clip.x1 = overlay->x + overlay->clip_left;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y2 = overlay->y + overlay->clip_bottom;

	for(i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t* dest  = data + (y * pitch) + (x << 1);
		int32_t len    = overlay->rle[i].len;
		uint32_t index = overlay->rle[i].color & 0xff;

		while(len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if(y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if(x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if(x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if(alpha >= 15)
			{
				uint32_t w = width;
				uint32_t n;

				if(x & 1)
				{
					*(dest++) = color.cr;
					*(dest++) = color.y;
					w--;
				}

				n = w >> 1;

				while(n--)
				{
					*dest       = color.cb;
					*(dest + 1) = color.y;
					*(dest + 2) = color.cr;
					*(dest + 3) = color.y;

					dest += 4;
				}

				if(w & 1)
				{
					*(dest++) = color.cb;
					*(dest++) = color.y;
				}

			} else
			if(alpha)
			{
				uint32_t w      = width;
				uint32_t ialpha = 15 - alpha;
				uint32_t ya, cra, cba;
				uint32_t n;

				ya  = color.y * alpha;
				cra = color.cr * alpha;
				cba = color.cb * alpha;

				if(x & 1)
				{
					register int b;

					b = (cra + (*dest * ialpha)) / 15;
					*(dest++) = b;

					b = (ya + (*dest * ialpha)) / 15;
					*(dest++) = b;

					w--;
				}

				n = w >> 1;

				while(n--)
				{
					register int b;

					b = (cba +  (*dest * ialpha)) / 15;
					*dest = b;

					b = (ya  + (*(dest + 1) * ialpha)) / 15;
					*(dest + 1) = b;

					b = (cra +  (*(dest + 2) * ialpha)) / 15;
					*(dest + 2) = b;

					b = (ya  + (*(dest + 3) * ialpha)) / 15;
					*(dest + 3) = b;

					dest += 4;
				}

				if(w & 1)
				{
					register int b;
					
					b = (cba + (*dest * ialpha)) / 15;
					*(dest++) = b;

					b = (ya  + (*dest * ialpha)) / 15;
					*(dest++) = b;
				}

			} else
			{
				dest += width << 1;
			}

			x += width;

			if(x >= (overlay->x + overlay->width))
			{
				x = overlay->x;
				y++;
				dest = data + (y * pitch) + (x << 1);
			}
		}
	}
}


static void
dfb_overlay_blend_yv12(dfb_frame_t* frame, vo_overlay_t* overlay)
{
	uint8_t* yuv[3];
	uint32_t pitch[2];
	DFBRegion clip;
	uint32_t i, x, y;

	yuv[0]  = (uint8_t*) frame->surface->back_buffer->system.addr;

	if(frame->surface->format == DSPF_YV12)
	{
		yuv[2]  = (uint8_t*) yuv[0] + (frame->width * frame->height);
		yuv[1]  = (uint8_t*) yuv[2] + ((frame->width * frame->height) >> 2);
	} else
	{
		yuv[1]  = (uint8_t*) yuv[0] + (frame->width * frame->height);
		yuv[2]  = (uint8_t*) yuv[1] + ((frame->width * frame->height) >> 2);;
	}

	pitch[0] = frame->width;
	pitch[1] = frame->width >> 1;

	yuv[0] += overlay->y * pitch[0];
	yuv[1] += (overlay->y >> 1) * pitch[1];
	yuv[2] += (overlay->y >> 1) * pitch[1];

	clip.x1 = overlay->x + overlay->clip_left;
	clip.y1 = overlay->y + overlay->clip_top;
	clip.x2 = overlay->x + overlay->clip_right;
	clip.y2 = overlay->y + overlay->clip_bottom;

	for(i = 0, x = overlay->x, y = overlay->y; i < overlay->num_rle; i++)
	{
		uint8_t* y_data = yuv[0] + x;
		uint8_t* u_data = yuv[1] + (x >> 1);
		uint8_t* v_data = yuv[2] + (x >> 1);
		int32_t len     = overlay->rle[i].len;
		uint32_t index  = overlay->rle[i].color & 0xff;

		while(len > 0)
		{
			clut_t   color = ((clut_t*) overlay->color)[index];
			uint8_t  alpha = overlay->trans[index];
			uint32_t width;

			width = (len > overlay->width)
				 ? overlay->width : len;
			len -= width;

			if(y >= clip.y1 && y <= clip.y2 && x <= clip.x2)
			{
				if(x < clip.x1 && (x + width - 1) >= clip.x1)
				{
					width -= clip.x1 - x;
					len   += clip.x1 - x;
				} else
				if(x > clip.x1)
				{
					color = ((clut_t*) overlay->clip_color)[index];
					alpha = overlay->clip_trans[index];

					if((x + width - 1) > clip.x2)
					{
						width -= clip.x2 - x;
						len   += clip.x2 - x;
					}
				}
			}

			if(alpha >= 15)
			{
				memset(y_data, color.y, width);
				y_data += width;

				if(y & 1)
				{
					memset(u_data, color.cb, (width + 1) >> 1);
					u_data += width >> 1;
					memset(v_data, color.cr, (width + 1) >> 1);
					v_data += width >> 1;
				}

			} else
			if(alpha)
			{
				uint32_t ialpha = 15 - alpha;
				uint32_t ya     = color.y * alpha;;
				uint32_t n;

				n = width;

				do
				{
					*y_data = (ya + (*y_data * ialpha)) / 15;
					y_data++;

				} while(--n);

				if(y & 1)
				{
					uint32_t cba = color.cb * alpha;
					uint32_t cra = color.cr * alpha;

					n = width >> 1;

					while(n--)
					{
						*u_data = (cba + (*u_data * ialpha)) / 15;
						u_data++;
						*v_data = (cra + (*v_data * ialpha)) / 15;
						v_data++;
					}

					if(width & 1)
					{
						*u_data = (cba + (*u_data * ialpha)) / 15;
						*v_data = (cra + (*v_data * ialpha)) / 15;
					}
				}

			} else
			{
				y_data += width;
				if(y & 1)
				{
					u_data += width >> 1;
					v_data += width >> 1;
				}
			}

			x += width;

			if(x >= (overlay->x + overlay->width))
			{
				yuv[0] += pitch[0];
				y_data  = yuv[0] + overlay->x;

				if(y & 1)
				{
					yuv[1] += pitch[1];
					u_data  = yuv[1] + (overlay->x >> 1);
					yuv[2] += pitch[1];
					v_data  = yuv[2] + (overlay->x >> 1);
				}

				x = overlay->x;
				y++;
			}
		}
	}
}


static void
dfb_overlay_blend_rgb(dfb_frame_t* frame, vo_overlay_t* overlay)
{
	uint32_t i, x, y;

	if(!overlay->rgb_clut)
	{
		clut_t* clut = (clut_t*) overlay->color;

		for(i = 0; i < (sizeof(overlay->color) / sizeof(clut_t)); i++)
		{
			DFBColor* color = (DFBColor*) &clut[i];
			int r, g, b;

			r = clut[i].y + v_red_table[clut[i].cr];
			g = clut[i].y - v_green_table[clut[i].cr] -
					u_green_table[clut[i].cb];
			b = clut[i].y + u_blue_table[clut[i].cb];

			color->r = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));
			color->g = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
			color->b = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		}

		overlay->rgb_clut++;
	}

	if(!overlay->clip_rgb_clut)
	{
		clut_t* clut = (clut_t*) overlay->clip_color;

		for(i = 0; i < (sizeof(overlay->clip_color) / sizeof(clut_t)); i++)
		{
			DFBColor* color = (DFBColor*) &clut[i];
			int r, g, b;

			r = clut[i].y + v_red_table[clut[i].cr];
			g = clut[i].y - v_green_table[clut[i].cr] -
					u_green_table[clut[i].cb];
			b = clut[i].y + u_blue_table[clut[i].cb];

			color->r = ((r < 0) ? 0 : ((r > 0xff) ? 0xff : r));
			color->g = ((g < 0) ? 0 : ((g > 0xff) ? 0xff : g));
			color->b = ((b < 0) ? 0 : ((b > 0xff) ? 0xff : b));
		}

		overlay->clip_rgb_clut++;
	}

	frame->state.destination  = frame->surface;
	frame->state.drawingflags = DSDRAW_BLEND;
	frame->state.modified    |= (SMF_DESTINATION | SMF_DRAWING_FLAGS);

	for(i = 0, x= 0, y = 0; i < overlay->num_rle; i++)
	{
		int32_t len    = overlay->rle[i].len;
		uint32_t index = overlay->rle[i].color & 0xff;

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
					(x + width - 1) >= overlay->clip_left)
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

			if(t_palette[index])
			{
				DFBRectangle rect;
				
				frame->state.color     = c_palette[index];
				frame->state.color.a   = t_palette[index] * 17;
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
}



#endif /* VIDEO_OUT_DFB_BLEND_H */

