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
 */


#ifndef VIDEO_OUT_DFB_H
#define VIDEO_OUT_DFB_H



typedef struct dfb_frame_s  dfb_frame_t;
typedef struct dfb_driver_s dfb_driver_t;



struct dfb_frame_s
{
	vo_frame_t     vo_frame;

	int            width;
	int            height;

	char           proc_needed;
	
	CardState      state;
	CoreSurface*   surface;

	void*          chunks[3];

	void (*realize) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

};


typedef struct
{
	void (*yuy2) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

	void (*uyvy) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

	void (*yv12) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch); /* yv12 and i420 */

	void (*rgb15) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

	void (*rgb16) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);
	
	void (*rgb24) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

	void (*rgb32) (dfb_driver_t* this, dfb_frame_t* frame,
				uint8_t* data, uint32_t pitch);

} yuv_render_t;


typedef void (*DVOutputCallback) (void* cdata, int width, int height,
					DFBRectangle* dest_rect);


struct dfb_driver_s
{
	vo_driver_t            vo_driver;

	char                   verbosity;
	int                    max_num_frames;

	IDirectFBSurface*      main;
	IDirectFBSurface_data* main_data;

	struct
	{
		char defined;
		char used;

	} correction; /* gamma correction */

	struct
	{
		int   l_val;
		short mm_val[4];

	} brightness;

	struct
	{
		int   l_val;
		short mm_val[4];

	} contrast;

	
	DVOutputCallback       output_cb;
	void*                  output_cdata;

	DVFrameCallback        frame_cb;
	void*                  frame_cdata;
};


typedef struct
{
	video_driver_class_t  vo_class;

	xine_t*               xine;

} dfb_driver_class_t;


typedef struct
{
	IDirectFBSurface* surface;

	DVOutputCallback  output_cb;

	void*             cdata;

} dfb_visual_t;

/*
 *  Applications that want to use this driver must pass a dfb_visual_t
 *  to xine_open_video_driver(); 
 *  -> surface: is the destination surface (if it's NULL you must can set it
 *              using xine_port_send_gui_data() with XINE_GUI_SEND_DRAWABLE_CHANGED
 *              as second argument and the new surface as third argument)
 *  -> output_cb: this function is called before rendering each frame:
 *                . cdata is your private data
 *                . width is video width
 *                . height is video height
 *                . dest_rect is used to set video rendering size and 
 *                  position (the driver expects that you fill this)
 *  -> cdata: your private data (can be NULL)
 *
 */


typedef struct
{
	DVFrameCallback frame_cb;

	void*           cdata;

} dfb_frame_callback_t;

/*
 * You can register a DVFrameCallback using xine_port_send_gui_data()
 * with XINE_GUI_SEND_TRASLATE_GUI_TO_VIDEO as second argument and
 * a dfb_frame_callback_t* as third argument.
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



#endif /* VIDEO_OUT_DFB_H */

