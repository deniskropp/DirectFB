/* 
 * Copyright (C) 2006 Texas Instruments Inc
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option)any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * File: davincifb.h	
 */

#ifndef DAVINVI_VPBE_H
#define DAVINVI_VPBE_H

/* include Linux files */
#include <linux/fb.h>

/* define the custom FBIO_WAITFORVSYNC ioctl */
#define FBIO_WAITFORVSYNC	_IOW('F', 0x20, u_int32_t)
#define FBIO_SETATTRIBUTE       _IOW('F', 0x21, struct fb_fillrect)

/* Backported IOCTLS. */
#define FBIO_SETPOSX                            _IOW('F', 0x22, u_int32_t)
#define FBIO_SETPOSY                            _IOW('F', 0x23, u_int32_t)
#define FBIO_SETZOOM            		_IOW('F', 0x24, struct zoom_params)
#define FBIO_GETSTD                             _IOR('F', 0x25, u_int32_t)

typedef struct zoom_params {
	u_int32_t window_id;
	u_int32_t zoom_h;
	u_int32_t zoom_v;
} zoom_params_t;

#define	RAM_CLUT_SIZE	256*3
#define FBIO_ENABLE_DISABLE_WIN		\
	_IOW('F', 0x30, unsigned char)
#define FBIO_SET_BITMAP_BLEND_FACTOR	\
	_IOW('F', 0x31, vpbe_bitmap_blend_params_t)
#define FBIO_SET_BITMAP_WIN_RAM_CLUT    \
	_IOW('F', 0x32, unsigned char)*RAM_CLUT_SIZE)
#define FBIO_ENABLE_DISABLE_ATTRIBUTE_WIN \
	_IOW('F', 0x33, unsigned int)
#define FBIO_GET_BLINK_INTERVAL		\
	_IOR('F', 0x34, vpbe_blink_option_t)
#define FBIO_SET_BLINK_INTERVAL         \
	_IOW('F', 0x35, vpbe_blink_option_t)
#define FBIO_GET_VIDEO_CONFIG_PARAMS    \
	_IOR('F', 0x36, vpbe_video_config_params_t)
#define FBIO_SET_VIDEO_CONFIG_PARAMS    \
	_IOW('F', 0x37, vpbe_video_config_params_t)
#define FBIO_GET_BITMAP_CONFIG_PARAMS   \
	_IOR('F', 0x38, vpbe_bitmap_config_params_t)
#define FBIO_SET_BITMAP_CONFIG_PARAMS   \
	_IOW('F', 0x39, vpbe_bitmap_config_params_t)
#define FBIO_SET_DCLK                   \
	_IOW('F', 0x40, vpbe_dclk_t)
#define FBIO_SET_INTERFACE		\
	_IOW('F', 0x41, unsigned char)
#define FBIO_GET_INTERFACE		\
	_IOR('F', 0x42, unsigned char)
#define FBIO_QUERY_TIMING		\
	_IOWR('F', 0x43, struct vpbe_mode_info)
#define FBIO_SET_TIMING			\
	_IOW('F', 0x44, struct vpbe_fb_videomode)
#define FBIO_GET_TIMING                 \
	_IOR('F', 0x45, struct vpbe_fb_videomode)
#define FBIO_SET_VENC_CLK_SOURCE	\
	_IOW('F', 0x46, unsigned char)
#define FBIO_SET_BACKG_COLOR            \
	_IOW('F', 0x47, vpbe_backg_color_t)
#define FBIO_ENABLE_DISPLAY		\
	_IOW('F', 0x48, unsigned char)
#define FBIO_SETPOS            		\
	_IOW('F', 0x49, u_int32_t)
#define FBIO_SET_CURSOR         	\
	_IOW('F', 0x50, struct fb_cursor)

/* 
 * Defines and Constants
 */
#ifdef __KERNEL__
#define DAVINCIFB_DEVICE "davincifb"
#define DAVINCIFB_DRIVER "davincifb"

#define MULTIPLE_BUFFERING      1

#ifdef MULTIPLE_BUFFERING
#define DOUBLE_BUF      2
#define TRIPLE_BUF      3
#else
#define DOUBLE_BUF      1
#define TRIPLE_BUF      1
#endif

/* usage:	if (is_win(info->fix.id, OSD0)) ... */
#define is_win(name, x) ((strcmp(name, x ## _FBNAME) == 0) ? 1 : 0)

/*
 * display controller register I/O routines
 */
u32 dispc_reg_in(u32 offset);
u32 dispc_reg_out(u32 offset, u32 val);
u32 dispc_reg_merge(u32 offset, u32 val, u32 mask);

#endif				/*__KERNEL__*/

/*  Error return codes  */
#define VPBE_INVALID_PARA_VALUE         700
#define VPBE_WRONG_WINDOW_ID            701
#define VPBE_CURRENTLY_IN_REQUIRED_MODE 702
#define VPBE_INSUFFICIENT_CLUT_VALUES   703
#define VPBE_CLUT_WRITE_TIMEOUT         704
#define VPBE_VID0_BUF_ADR_NULL          705
#define VPBE_WINDOW_NOT_DISABLED        706
#define VPBE_WINDOW_NOT_ENABLED         707

#ifndef __KERNEL__
/*  Window ID definations */
#define OSD0      0
#define VID0      1
#define OSD1      2
#define VID1      3
#endif

/* There are 4 framebuffers, each represented by an fb_info and
 * a dm_win_info structure */
#define OSD0_FBNAME "dm_osd0_fb"
#define OSD1_FBNAME "dm_osd1_fb"
#define VID0_FBNAME "dm_vid0_fb"
#define VID1_FBNAME "dm_vid1_fb"

/*  FIXME: Digital LCD RGB matrix coefficients */
#define DLCD_DGY_VAL    0
#define DLCD_DRV_VAL    0
#define DLCD_DGU_VAL    0
#define DLCD_DBU_VAL		0

/* Defines for bitmap format */
#define VPBE_BITMAP_BIT_1	1
#define VPBE_BITMAP_BIT_2	2
#define VPBE_BITMAP_BIT_4	4
#define VPBE_BITMAP_BIT_8	8
#define VPBE_BITMAP_RGB565	16
#define VPBE_VIDEO_YUV422 	16
#define VPBE_VIDEO_RGB888 	24

/* Defines foe cursor parameter validation*/
#define MAX_CURSOR_WIDTH	0x3FF
#define MAX_CURSOR_HEIGHT	0x1FF
#define MAX_CURSOR_LINEWIDTH    7

#define BASEX		0x80
#define BASEY		0x12
#define BASEX_DLCD		0x59
#define BASEY_DLCD		0x22

/*
 * Enumerations 
 */
/*  Enum for blending factor  */
typedef enum vpbe_blend_factor {
	OSD_CONTRIBUTION_ZERO = 0,
	OSD_CONTRIBUTION_1_BY_8 = 1,
	OSD_CONTRIBUTION_2_BY_8 = 2,
	OSD_CONTRIBUTION_3_BY_8 = 3,
	OSD_CONTRIBUTION_4_BY_8 = 4,
	OSD_CONTRIBUTION_5_BY_8 = 5,
	OSD_CONTRIBUTION_6_BY_8 = 6,
	OSD_CONTRIBUTION_ONE = 7
} vpbe_blend_factor_t;

/*  Enum for Boolean variables  */
typedef enum {
	SET_0 = 0,
	SET_1 = 1
} CB_CR_ORDER, ATTRIBUTE, ROM_RAM_CLUT;

/*  Defines for Display Interface */
#define  PRGB		0
#define  COMPOSITE      1
#define  SVIDEO    	2
#define  COMPONENT 	3
#define  RGB       	4
#define  YCC16     	5
#define  YCC8      	6
#define  SRGB      	7
#define  EPSON     	8
#define  CASIO1G   	9
#define  UDISP     	10
#define  STN       	11
#define VPBE_MAX_INTERFACES	12

/*  Defines for Display Mode */
#define  LCD    0
#define  NTSC	1
#define  PAL    2
#define  P525   3
#define  P625   4

#define DEFAULT_MODE 0
#define  P480   0
#define  P400   1
#define  P350   2
#define NON_EXISTING_MODE 255
/*  Enable/Disable enum */
typedef enum {
	VPBE_DISABLE = 0,
	VPBE_ENABLE = 1
} ATTENUATION, TRANSPARENCY, EXPANSION, BLINKING;

typedef enum clk_source {
	CLK_SOURCE_CLK27 = 0,
	CLK_SOURCE_CLK54 = 1,
	CLK_SOURCE_VPBECLK = 2
} CLK_SOURCE;

/*
 * Structures and Union Definitions
 */

/*  Structure for transparency and the blending factor for the bitmap window  */
typedef struct vpbe_bitmap_blend_params {
	unsigned int colorkey;	/* color key to be blend */
	unsigned int enable_colorkeying;	/* enable color keying */
	unsigned int bf;	/* valid range from 0 to 7 only. */
} vpbe_bitmap_blend_params_t;

/*  Structure for window expansion  */
typedef struct vpbe_win_expansion {
	EXPANSION horizontal;
	EXPANSION vertical;	/* 1: Enable 0:disable */
} vpbe_win_expansion_t;

/*  Structure for OSD window blinking options */
typedef struct vpbe_blink_option {
	BLINKING blinking;	/* 1: Enable blinking 0: Disable */
	unsigned int interval;	/* Valid only if blinking is 1 */
} vpbe_blink_option_t;

/*  Structure for DCLK parameters */
typedef struct vpbe_dclk {
	unsigned char dclk_pattern_width;
	unsigned int dclk_pattern0;
	unsigned int dclk_pattern1;
	unsigned int dclk_pattern2;
	unsigned int dclk_pattern3;
} vpbe_dclk_t;

/*  Structure for display format  */
typedef struct vpbe_display_format {
	unsigned char interface;	/* Output interface type */
	unsigned char mode;	/* output mode */
} vpbe_display_format_t;

/*  Structure for background color  */
typedef struct vpbe_backg_color {
	unsigned char clut_select;	/* 2: RAM CLUT 1:ROM1 CLUT 0:ROM0 CLUT */
	unsigned char color_offset;	/* index of color */
} vpbe_backg_color_t;

/*  Structure for Video window configurable parameters  */
typedef struct vpbe_video_config_params {
	CB_CR_ORDER cb_cr_order;	/*Cb/Cr order in input data for a pixel. */
	/*    0: cb cr  1:  cr cb */
	vpbe_win_expansion_t exp_info;	/* HZ/VT Expansion enable disable */
} vpbe_video_config_params_t;

/*Union of structures giving the CLUT index for the 1, 2, 4 bit bitmap values.*/
typedef union vpbe_clut_idx {
	struct _for_4bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char bitmap_val_1;
		unsigned char bitmap_val_2;
		unsigned char bitmap_val_3;
		unsigned char bitmap_val_4;
		unsigned char bitmap_val_5;
		unsigned char bitmap_val_6;
		unsigned char bitmap_val_7;
		unsigned char bitmap_val_8;
		unsigned char bitmap_val_9;
		unsigned char bitmap_val_10;
		unsigned char bitmap_val_11;
		unsigned char bitmap_val_12;
		unsigned char bitmap_val_13;
		unsigned char bitmap_val_14;
		unsigned char bitmap_val_15;
	} for_4bit_bimap;
	struct _for_2bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char dummy0[4];
		unsigned char bitmap_val_1;
		unsigned char dummy1[4];
		unsigned char bitmap_val_2;
		unsigned char dummy2[4];
		unsigned char bitmap_val_3;
	} for_2bit_bimap;
	struct _for_1bit_bimap {
		unsigned char bitmap_val_0;
		unsigned char dummy0[14];
		unsigned char bitmap_val_1;
	} for_1bit_bimap;
} vpbe_clut_idx_t;

/*  Structure for bitmap window configurable parameters */
typedef struct vpbe_bitmap_config_params {
	/* Only for bitmap width = 1,2,4 bits */
	vpbe_clut_idx_t clut_idx;
	/* Attenuation value for YUV o/p for bitmap window */
	unsigned char attenuation_enable;
	/* 0: ROM DM270, 1:ROM DM320, 2:RAM CLUT */
	unsigned char clut_select;
} vpbe_bitmap_config_params_t;

/*  Unioun for video/OSD configuration parameters  */
typedef union vpbe_conf_params {

	struct vpbe_video_params {
		CB_CR_ORDER cb_cr_order;
		/* HZ/VT Expansion enable disable */
		vpbe_win_expansion_t exp_info;
	} video_params;

	struct vpbe_bitmap_params {
		/* Attenuation value for YUV o/p */
		ATTENUATION attenuation_enable;
		/* 0: ROM DM270, 1: ROM DM320, 2:RAM CLUT */
		unsigned char clut_select;
		/* Only for bitmap width = 1,2,4 bits */
		vpbe_clut_idx_t clut_idx;
		/* 0: OSD window is bitmap window */
		/* 1: OSD window is attribute window */
		ATTRIBUTE enable_attribute;
		/* To hold bps value. 
		   Used to switch back from attribute to bitmap. */
		unsigned int stored_bits_per_pixel;
		/* Blending information */
		vpbe_bitmap_blend_params_t blend_info;
		/* OSD Blinking information */
		vpbe_blink_option_t blink_info;
	} bitmap_params;

} vpbe_conf_params_t;

typedef struct vpbe_video_params vpbe_video_params_t;
typedef struct vpbe_bitmap_params vpbe_bitmap_params_t;

/* Structure to hold window position */
typedef struct vpbe_window_position {
	unsigned int xpos;	/* X position of the window */
	unsigned int ypos;	/* Y position of the window */
} vpbe_window_position_t;

#ifdef __KERNEL__
/*  Structure for each window */
typedef struct vpbe_dm_win_info {
	struct fb_info info;
	vpbe_window_position_t win_pos;	/* X,Y position of window */
	/* Size of window is already there in var_info structure. */

	dma_addr_t fb_base_phys;	/*framebuffer area */
	unsigned int fb_base;	/*window memory pointer */
	unsigned int fb_size;	/*memory size */
	unsigned int pseudo_palette[17];
	int alloc_fb_mem;
	/*flag to identify if framebuffer area is fixed or not */
	unsigned long sdram_address;
	struct vpbe_dm_info *dm;
	unsigned char window_enable;	/*Additions for all windows */
	zoom_params_t zoom;	/*Zooming parameters */
	unsigned char field_frame_select;	/*To select Field or frame */
	unsigned char numbufs;	/*Number of buffers valid 2 or 3 */
	vpbe_conf_params_t conf_params;
	/*window configuration parameter union pointer */
} vpbe_dm_win_info_t;
#endif				/*__KERNEL__*/

/*
 *  Videmode structure for display interface and mode settings
 */
typedef struct vpbe_fb_videomode {
	unsigned char name[10];	/* Mode name ( NTSC , PAL) */
	unsigned int vmode;	/* FB_MODE_INTERLACED or FB_MODE_NON_INTERLACED */
	unsigned int xres;	/* X Resolution of the display */
	unsigned int yres;	/* Y Resolution of the display */
	unsigned int fps;	/* frames per second */
	/* Timing Parameters applicable for std = 0 only */
	unsigned int left_margin;
	unsigned int right_margin;
	unsigned int upper_margin;
	unsigned int lower_margin;
	unsigned int hsync_len;
	unsigned int vsync_len;
	unsigned int sync;	/* 0: hsync -ve/vsync -ve */
	/*1: hsync -ve/vsync +ve */
	/*2: hsync +ve/vsync -ve */
	/*3: hsync +ve/vsync +ve */
	unsigned int basepx;	/* Display x,y start position */
	unsigned int basepy;
/*  1= Mode s available in modelist 0=Mode is not available in modelist */
	unsigned int std;
} vpbe_fb_videomode_t;

/* Structure to interface videomode to application*/
typedef struct vpbe_mode_info {
	vpbe_fb_videomode_t vid_mode;
	unsigned char interface;
	unsigned char mode_idx;
} vpbe_mode_info_t;

#ifdef __KERNEL__
/* 
 * Structure for the driver holding information of windows, 
 *  memory base addresses etc.
 */
typedef struct vpbe_dm_info {
	vpbe_dm_win_info_t *osd0;
	vpbe_dm_win_info_t *osd1;
	vpbe_dm_win_info_t *vid0;
	vpbe_dm_win_info_t *vid1;

/* to map the registers */
	dma_addr_t mmio_base_phys;
	unsigned int mmio_base;
	unsigned int mmio_size;

	wait_queue_head_t vsync_wait;
	unsigned int vsync_cnt;
	int timeout;

	/* this is the function that configures the output device (NTSC/PAL/LCD)
	 * for the required output format (composite/s-video/component/rgb)
	 */
	void (*output_device_config) (void);

	struct device *dev;

	vpbe_backg_color_t backg;	/* background color */
	vpbe_dclk_t dclk;	/*DCLK parameters */
	vpbe_display_format_t display;	/*Display interface and mode */
	vpbe_fb_videomode_t videomode;	/*Cuurent videomode */
	char ram_clut[256][3];	/*RAM CLUT array */
	struct fb_cursor cursor;	/* cursor config params from fb.h */
/*Flag that indicates whether any of the display is enabled or not*/
	int display_enable;
} vpbe_dm_info_t;

/*
 * Functions Definitions for 'davincifb' module
 */
int vpbe_mem_alloc_window_buf(vpbe_dm_win_info_t *);
int vpbe_mem_release_window_buf(vpbe_dm_win_info_t *);
void init_display_function(vpbe_display_format_t *);
int vpbe_mem_alloc_struct(vpbe_dm_win_info_t **);
void set_vid0_default_conf(void);
void set_vid1_default_conf(void);
void set_osd0_default_conf(void);
void set_osd1_default_conf(void);
void set_cursor_default_conf(void);
void set_dm_default_conf(void);
void set_win_enable(char *, unsigned int);
int within_vid0_limits(u32, u32, u32, u32);
void vpbe_set_display_default(void);
#ifdef __KERNEL__
void set_win_position(char *, u32, u32, u32, u32);
void change_win_param(int);
void set_interlaced(char *, unsigned int);
#endif /* __KERNEL__ */

/*
 *	Function definations for 'osd' module
 */

int vpbe_enable_window(vpbe_dm_win_info_t *);
int vpbe_disable_window(vpbe_dm_win_info_t *);
int vpbe_vid_osd_select_field_frame(u8 *, u8);
int vpbe_bitmap_set_blend_factor(u8 *, vpbe_bitmap_blend_params_t *);
int vpbe_bitmap_set_ram_clut(void);
int vpbe_enable_disable_attribute_window(u32);
int vpbe_get_blinking(u8 *, vpbe_blink_option_t *);
int vpbe_set_blinking(u8 *, vpbe_blink_option_t *);
int vpbe_set_vid_params(u8 *, vpbe_video_config_params_t *);
int vpbe_get_vid_params(u8 *, vpbe_video_config_params_t *);
int vpbe_bitmap_get_params(u8 *, vpbe_bitmap_config_params_t *);
int vpbe_bitmap_set_params(u8 *, vpbe_bitmap_config_params_t *);
int vpbe_set_cursor_params(struct fb_cursor *);
int vpbe_set_vid_expansion(vpbe_win_expansion_t *);
int vpbe_set_dclk(vpbe_dclk_t *);
int vpbe_set_display_format(vpbe_display_format_t *);
int vpbe_set_backg_color(vpbe_backg_color_t *);
int vpbe_set_interface(u8);
int vpbe_query_mode(vpbe_mode_info_t *);
int vpbe_set_mode(struct vpbe_fb_videomode *);
int vpbe_set_venc_clk_source(u8);
void set_vid0_default_conf(void);
void set_osd0_default_conf(void);
void set_vid1_default_conf(void);
void set_osd1_default_conf(void);
void set_cursor_default_conf(void);
void set_dm_default_conf(void);
/*
 * Function definations for 'venc' module
 */

void davincifb_ntsc_composite_config(void);
void davincifb_ntsc_svideo_config(void);
void davincifb_ntsc_component_config(void);
void davincifb_pal_composite_config(void);
void davincifb_pal_svideo_config(void);
void davincifb_pal_component_config(void);

void vpbe_davincifb_ntsc_rgb_config(void);
void vpbe_davincifb_pal_rgb_config(void);
void vpbe_davincifb_525p_component_config(void);
void vpbe_davincifb_625p_component_config(void);

void vpbe_enable_venc(int);
void vpbe_enable_dacs(int);
/*
 * Function definations for 'dlcd' module
 */
void vpbe_davincifb_480p_prgb_config(void);
void vpbe_davincifb_400p_prgb_config(void);
void vpbe_davincifb_350p_prgb_config(void);
void vpbe_set_display_timing(struct vpbe_fb_videomode *);

void vpbe_enable_lcd(int);
/*
 * Following functions are not implemented
 */
void vpbe_davincifb_default_ycc16_config(void);
void vpbe_davincifb_default_ycc8_config(void);
void vpbe_davincifb_default_srgb_config(void);
void vpbe_davincifb_default_epson_config(void);
void vpbe_davincifb_default_casio_config(void);
void vpbe_davincifb_default_UDISP_config(void);
void vpbe_davincifb_default_STN_config(void);
#endif				/*__KERNEL__*/

#endif				/* End of #ifndef DAVINCI_VPBE_H */
