/*
   Copyright (c) 2003 Andreas Robinson, All rights reserved.

   This library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2 of the License, or (at your option) any later version.
*/

#include "unichrome.h"
#include "uc_overlay.h"
#include "vidregs.h"
#include "mmio.h"

/**
 * Map hw settings for vertical scaling.
 *
 * @param sh        source height
 * @param dh        destination height
 * @param zoom      will hold vertical setting of zoom register.
 * @param mini      will hold vertical setting of mini register.
 *
 * @returns true if successful.
 *          false if the zooming factor is too large or small.
 *
 * @note Derived from VIA's V4L driver.
 *       See ddover.c, DDOVER_HQVCalcZoomHeight()
 */

bool uc_ovl_map_vzoom(int sh, int dh, __u32* zoom, __u32* mini)
{
    __u32 sh1, tmp, d;
    bool zoom_ok = true;

    if (sh == dh) { // No zoom
        // Do nothing
    }
    else if (sh < dh) { // Zoom in

        tmp = (sh * 0x0400) / dh;
        zoom_ok = !(tmp > 0x3ff);

        *zoom |= (tmp & 0x3ff) | V1_Y_ZOOM_ENABLE;
        *mini |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
    } 
    else { // sw > dh - Zoom out

        // Find a suitable divider (1 << d) = {2, 4, 8 or 16}

        sh1 = sh;
        for (d = 1; d < 5; d++) {
            sh1 >>= 1;
            if (sh1 <= dh) break;
        }
        if (d == 5) { // Too small.
            d = 4;
            zoom_ok = false;
        }

        *mini |= ((d<<1)-1) << 16;  // <= {1,3,5,7} << 16

        // Add scaling

        if (sh1 < dh)  {
            tmp = (sh1 * 0x400) / dh;
            *zoom |= ((tmp & 0x3ff) | V1_Y_ZOOM_ENABLE);
            *mini |= V1_Y_INTERPOLY | V1_YCBCR_INTERPOLY;
        }
    }

    return zoom_ok;
}


/**
 * Map hw settings for horizontal scaling.
 *
 * @param sw        source width
 * @param dw        destination width
 *
 * @param zoom      will hold horizontal setting of zoom register.
 * @param mini      will hold horizontal setting of mini register.
 * @param falign    will hold fetch aligment
 * @param dcount    will hold display count
 *
 * @returns true if successful.
 *          false if the zooming factor is too large or small.
 *
 * @note Derived from VIA's V4L driver.
 *       See ddover.c, DDOVER_HQVCalcZoomWidth() and DDOver_GetDisplayCount()
 */
bool uc_ovl_map_hzoom(int sw, int dw,  __u32* zoom, __u32* mini,
                      int* falign, int* dcount)
{
    __u32 tmp, sw1, d;
    int md; // Minify-divider
    bool zoom_ok = true;

    md = 1;
    *falign = 0;

    if (sw == dw) { // No zoom
        // Do nothing
    }
    else if (sw < dw) { // Zoom in

        tmp = (sw * 0x0800) / dw;
        zoom_ok = !(tmp > 0x7ff);

        *zoom |= ((tmp & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
        *mini |= V1_X_INTERPOLY;
    }
    else { // sw > dw - Zoom out

        // Find a suitable divider (1 << d) = {2, 4, 8 or 16}

        sw1 = sw;
        for (d = 1; d < 5; d++) {
            sw1 >>= 1;
            if (sw1 <= dw) break;
        }
        if (d == 5) { // Too small.
            d = 4;
            zoom_ok = false;
        }

        md = 1 << d;                    // <= {2,4,8,16}
        *falign = ((md<<1)-1) & 0xf;    // <= {3,7,15,15}
        *mini |= V1_X_INTERPOLY;
        *mini |= ((d<<1)-1) << 24;      // <= {1,3,5,7} << 24

        // Add scaling

        if (sw1 < dw) {
            //CLE bug
            //tmp = sw1*0x0800 / dw;
            tmp = (sw1 - 2) * 0x0800 / dw;                
            *zoom |= ((tmp & 0x7ff) << 16) | V1_X_ZOOM_ENABLE;
        }
    }

    *dcount = sw - md;

    return zoom_ok;
}


/**
 * @param falign    fetch alignment
 * @param format    overlay pixel format
 * @param sw        source width
 *
 * @returns qword pitch register setting
 *
 * @note Derived from VIA's V4L driver. See ddover.c, DDOver_GetFetch()
 * @note Only call after uc_ovl_map_hzoom()
 */
__u32 uc_ovl_map_qwpitch(int falign, DFBSurfacePixelFormat format, int sw)
{
    int fetch = 0;

    switch (format) {
    case DSPF_YV12:
        fetch = ALIGN_TO(sw, 32) >> 4;
        break;
    case DSPF_I420:
        fetch = (ALIGN_TO(sw, 16) >> 4) + 1;
        break;
    case DSPF_UYVY:
    case DSPF_YUY2:
        fetch = (ALIGN_TO(sw << 1, 16) >> 4) + 1;
        break;
    case DSPF_ARGB1555:
    case DSPF_RGB16:
        fetch = (ALIGN_TO(sw << 1, 16) >> 4) + 1;
        break;
    case DSPF_RGB32:
    case DSPF_ARGB:
        fetch = (ALIGN_TO(sw << 2, 16) >> 4) + 1;
        break;
    default:
        BUG("Unexpected pixelformat!");
        break;
    }

    if (fetch < 4) fetch = 4;

    // Note: Unsure if alignment is needed or is in the way.
    fetch = ALIGN_TO(fetch, falign + 1);
    return fetch << 20; // V12_QWORD_PER_LINE
}


/**
 * Map pixel format.
 *
 * @note Derived from VIA's V4L driver. See ddover.c, DDOver_GetV1Format()
 */
__u32 uc_ovl_map_format(DFBSurfacePixelFormat format)
{
    switch (format) {
    case DSPF_YV12:
    case DSPF_I420:
        return V1_COLORSPACE_SIGN | V1_YUV420;
    case DSPF_UYVY:
    case DSPF_YUY2:
        return V1_COLORSPACE_SIGN | V1_YUV422;
    case DSPF_ARGB1555:
        return V1_RGB15;
    case DSPF_RGB16:
        return V1_RGB16;
    case DSPF_RGB32:
    case DSPF_ARGB:
        return V1_RGB32;
    default :
        BUG("Unexpected pixelformat!");
        return V1_YUV422;
    }
}


/**
 * Map overlay window.
 *
 * @param scrw      screen width (eg. 800)
 * @param scrh      screen height (eg. 600)
 * @param win       destination window
 * @param sw        source surface width
 * @param sh        source surface height
 *
 * @param win_start will hold window start register setting
 * @param win_end   will hold window end register setting
 *
 * @parm ox         will hold new leftmost coordinate in source surface
 * @parm oy         will hold new topmost coordinate in source surface
 */
void uc_ovl_map_window(int scrw, int scrh, DFBRectangle* win, int sw, int sh,
                       __u32* win_start, __u32* win_end, int* ox, int* oy)
{
    int x1, y1, x2, y2;
    int x,y,dw,dh;      // These help making the code readable...

    *ox = 0;
    *oy = 0;
    *win_start = 0;
    *win_end = 0;

    x = win->x;
    y = win->y;
    dw = win->w;
    dh = win->h;

    // For testing the clipping
    //scrw -= 100;
    //scrh -= 100;

    // Handle invisible case.
    if ((x > scrw) || (y > scrh) || (x+dw < 0) || (y+dh < 0)) return;

    // Vertical clipping

    if ((y >= 0) && (y+dh < scrh)) {
        // No clipping
        y1 = y;
        y2 = y+dh-1;
    }
    else if ((y < 0) && (y+dh < scrh)) {
        // Top clip
        y1 = 0;
        y2 = y+dh-1;
        *oy = (int) (((float) (sh * -y)) / ((float) dh) + 0.5);
    }
    else if ((y >= 0) && (y+dh >= scrh)) {
        // Bottom clip
        y1 = y;
        y2 = scrh-1;
    }
    else { // if (y < 0) && (y+dh >= scrh)
        // Top and bottom clip
        y1 = 0;
        y2 = scrh-1;
        *oy = (int) (((float) (sh * -y)) / ((float) dh) + 0.5);
    }

    // Horizontal clipping

    if ((x >= 0) && (x+dw < scrw)) {
        // No clipping
        x1 = x;
        x2 = x+dw-1;
    }
    else if ((x < 0) && (x+dw < scrw)) {
        // Left clip
        x1 = 0;
        x2 = x+dw-1;
        *ox = (int) (((float) (sw * -x)) / ((float) dw) + 0.5);
    }
    else if ((x >= 0) && (x+dw >= scrw)) {
        // Right clip
        x1 = x;
        x2 = scrw-1;
    }
    else { // if (x < 0) && (x+dw >= scrw)
        // Left and right clip
        x1 = 0;
        x2 = scrw-1;
        *ox = (int) (((float) (sw * -x)) / ((float) dw) + 0.5);
    }

    *win_start = (x1 << 16) | y1;
    *win_end = (x2 << 16) | y2;

    // For testing the clipping
    //*win_start = ((x1+50) << 16) | (y1+50);
    //*win_end = ((x2+50) << 16) | (y2+50);
}


/**
 * Map overlay buffer address.
 *
 * @param format    pixel format
 * @param buf       Framebuffer address of surface (0 = start of framebuffer)
 * @param ox        leftmost pixel to show (used when clipping, else set to zero)
 * @param oy        topmost pixel to show (used when clipping, else set to zero)
 * @param w         total surface width (does *not* depend on the x parameter)
 * @param h         total surface height (does *not* depend on the y parameter)
 * @param pitch     source surface pitch (bytes per pixel)
 *
 * @param y_start   will hold start address of Y(UV) or RGB buffer
 * @param u_start   will hold start address of Cb buffer (planar modes only)
 * @param v_start   will hold start address of Cr buffer (planar modes only)
 *
 * @note Derived from VIA's V4L driver. See ddover.c,
 *       DDOver_GetSrcStartAddress() and DDOVer_GetYCbCrStartAddress()
 */
void uc_ovl_map_buffer(DFBSurfacePixelFormat format, __u32 buf,
                       int ox, int oy, int sw, int sh, int sp,
                       __u32* y_start, __u32* u_start, __u32* v_start)
{
    int swap_cb_cr = 0;

    __u32 tmp;
    __u32 y_offset, uv_offset = 0;

    switch (format) {

    case DSPF_YUY2:
    case DSPF_UYVY:
        y_offset = ((oy * sp) + ((ox << 1) & ~15));
        break;

    case DSPF_YV12:
        swap_cb_cr = 1;
    case DSPF_I420:
        y_offset = ((((oy & ~3) * sp) + ox) & ~31) ;
        if (oy > 0)
            uv_offset = (((((oy & ~3) >> 1) * sp) + ox) & ~31) >> 1;
        else
            uv_offset = y_offset >> 1;
        break;

    case DSPF_ARGB1555:
    case DSPF_RGB16:
        y_offset = (oy * sp) + ((ox * 16) >> 3);
        break;

    case DSPF_RGB32:
    case DSPF_ARGB:
        y_offset = (oy * sp) + ((ox * 32) >> 3);
        break;

    default:
        y_offset = 0;
        uv_offset = 0;
        BUG("Unexpected pixelformat!");
    }

    *y_start = buf + y_offset;

    if (u_start && v_start) {
        *u_start = buf + sp * sh + uv_offset;
        *v_start = buf + sp * sh + sp * (sh >> 2) + uv_offset;

        if (swap_cb_cr) {
            tmp = *u_start;
            *u_start = *v_start;
            *v_start = tmp;
        }
    }
}


/**
 * Map alpha mode and opacity.
 *
 * @param opacity   Alpha opacity: 0 = transparent, 255 = opaque.
 *                  -1 = Use alpha from underlying graphics.
 *
 * @returns alpha control register setting.
 *
 * @note: Unfortunately, if using alpha from underlying graphics,
 *        the video is opaque if alpha = 255 and transparent if = 0.
 *        The inverse would have made more sense ...
 *
 * @note: The hardware supports a separate alpha plane as well,
 *        but it is not implemented here.
 *
 * @note: Derived from ddmpeg.c, VIAAlphaWin()
 */

__u32 uc_ovl_map_alpha(int opacity)
{
    __u32 ctrl = 0x00080000;    // Not sure what this number is, supposedly 
                                // it is the "expire number divided by 4".

    if (opacity > 255) opacity = 255;

    if (opacity < 0) {
        ctrl |= ALPHA_WIN_BLENDING_GRAPHIC;
    }
    else {
        opacity = opacity >> 4; // Throw away bits 0 - 3
        ctrl |= (opacity << 12) | ALPHA_WIN_BLENDING_CONSTANT;
    }

    return ctrl; // V_ALPHA_CONTROL
}

/**
 * Calculate V1 control and fifo-control register values
 * @param format        pixel format
 * @param sw            source width
 * @param hwrev         CLE266 hardware revision
 * @param extfifo_on    set this true if the extended FIFO is enabled
 * @param control       will hold value for V1_CONTROL
 * @param fifo          will hold value for V1_FIFO_CONTROL
 */
void uc_ovl_map_v1_control(DFBSurfacePixelFormat format, int sw,
                           int hwrev, bool extfifo_on,
                           __u32* control, __u32* fifo)
{
    *control = V1_ENABLE | uc_ovl_map_format(format);

    if (hwrev == 0x10) {
        *control |= V1_EXPIRE_NUM_F;
    }
    else {
        if (extfifo_on) {
            *control |= V1_EXPIRE_NUM_A | V1_FIFO_EXTENDED;
        }
        else {
            *control |= V1_EXPIRE_NUM;
        }
    }

    if (format == DSPF_YV12) { // Test for DSPF_I420 too? V4L does not.
        //Minified video will be skewed without this workaround.
        if (sw <= 80) { //Fetch count <= 5
            *fifo = UC_MAP_V1_FIFO_CONTROL(16,0,0);
        }
        else {
            if (hwrev == 0x10)
                *fifo = UC_MAP_V1_FIFO_CONTROL(64,56,56);
            else
                *fifo = UC_MAP_V1_FIFO_CONTROL(16,12,8);
        }
    }
    else {
        if (hwrev == 0x10) {
            *fifo = UC_MAP_V1_FIFO_CONTROL(64,56,56);   // Default rev 0x10
        }
        else {
            if (extfifo_on)
                *fifo = UC_MAP_V1_FIFO_CONTROL(48,40,40);
            else
                *fifo = UC_MAP_V1_FIFO_CONTROL(32,29,16);   // Default
        }
    }
}
