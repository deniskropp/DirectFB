/*
   Copyright (C) 2004-2006 Claudio Ciccani <klan@users.sf.net>

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



#ifndef __VIDEO_OUT_DFB_H__
#define __VIDEO_OUT_DFB_H__


/**************************** Driver Interface ********************************/

typedef void (*DVOutputCallback) ( void                  *cdata,
                                   int                    width,
                                   int                    height,
                                   double                 ratio,
                                   DFBSurfacePixelFormat  format,
                                   DFBRectangle          *dest_rect );

typedef struct {
     IDirectFBSurface *destination;
     IDirectFBSurface *subpicture;

     DVOutputCallback  output_cb;
     void             *output_cdata;

     DVFrameCallback  frame_cb;
     void            *frame_cdata;
} dfb_visual_t;

typedef struct {
     DVFrameCallback  frame_cb;
     void            *cdata;
} dfb_framecallback_t;


/*
 * Well, now you want to use this driver with your application.
 * How to make things work?
 * Quite simple:
 * > Read Xine's HackersGuide, if you haven't already done it.
 * > Copy the above data somewhere in your progam.
 * > To initialize the driver:
 *   1) Fill the dfb_visual_t:
 *      - dfb_visual_t::destination   is the destination surface;
 *      - dfb_visual_t::subpicture    is an optional surface for the OSD, the
 *                                    driver will render overlays on it
 *                                    (NOTE: the surface is automatically flipped);
 *      - dfb_visual_t::output_cb     is called before displaying a frame,
 *                                    you have to fill the destination rectangle;
 *      - dfb_visual_t::output_cdata  is your private data for the output callback;
 *      - dfb_visual_t::frame_cb      is an optional frame callback;
 *      - dfb_visual_t::frame_cdata   is your private data for the frame callback.
 *   2) Call xine_open_video_driver(), passing it the dfb_visual_t:
 *        xine_open_video_driver( xine_t *,
 *                                "DFB",
 *                                XINE_VISUAL_TYPE_DFB,
 *                                dfb_visual_t * )
 *      at least the output callback must be set.
 * > To change the visual:
 *        xine_port_send_gui_data( xine_video_port_t *,
 *                                 XINE_GUI_SEND_SELECT_VISUAL,
 *                                 dfb_visual_t * )
 *   in this case, at least destination surface and output callback must be set.
 * 
 * Old way to do things:
 *   -  to change the destination surface:
 *        xine_port_send_gui_data( xine_video_port_t *,
 *                                 XINE_GUI_SEND_DRAWABLE_CHANGED,
 *                                 IDirectFBSurface * )
 *   - to register a new frame callback:
 *        xine_port_send_gui_data( xine_video_port_t *,
 *                                 XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO,
 *                                 dfb_framecallback_t * )
 *
 */


/****************************** Internal Types ********************************/

typedef struct dfb_frame_s  dfb_frame_t;
typedef struct dfb_driver_s dfb_driver_t;


struct dfb_frame_s {
     vo_frame_t             vo_frame;
     
     DFBSurfacePixelFormat  format;
     int                    width;
     int                    height;

     IDirectFBSurface      *surface;
};

struct dfb_driver_s {
     vo_driver_t             vo_driver;

     xine_t                 *xine;

     int                     max_num_frames;
     
     IDirectFBSurface       *dest;
     int                     dest_width;
     int                     dest_height;
     
     IDirectFBSurface       *ovl;
     int                     ovl_width;
     int                     ovl_height;
     DFBRegion               ovl_region;
     int                     ovl_changed;
     
     struct {
          int                b;
          int                c;
          int                s;
          uint8_t           *l_csc; /* luma */
          uint8_t           *c_csc; /* chroma */
     } mixer;

     int                     deinterlace;
     int                     aspect_ratio;
     
     DVOutputCallback        output_cb;
     void*                   output_cdata;

     DVFrameCallback         frame_cb;
     void*                   frame_cdata;
};

typedef struct {
     video_driver_class_t  vo_class;
     xine_t               *xine;
} dfb_driver_class_t;


#endif /* __VIDEO_OUT_DFB_H__ */

