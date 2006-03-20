/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI Radeon cards written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
 
#ifndef __RADEON_STATE_H__
#define __RADEON_STATE_H__

/* R100 state funcs */
void r100_set_destination   ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_source        ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_clip          ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_drawing_color ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_src_colorkey  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_blend_function( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_drawingflags  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r100_set_blittingflags ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
                              
/* R200 state funcs */
void r200_set_destination   ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_source        ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_clip          ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_drawing_color ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_src_colorkey  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_blend_function( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_drawingflags  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r200_set_blittingflags ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
                              
/* R300 state funcs */
void r300_set_destination   ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_source        ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_clip          ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_drawing_color ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_blitting_color( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_src_colorkey  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_blend_function( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_drawingflags  ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
void r300_set_blittingflags ( RadeonDriverData *rdrv,
                              RadeonDeviceData *rdev,
                              CardState        *state );
                              
                              
#define RADEON_IS_SET( flag ) \
     ((rdev->set & SMF_##flag) == SMF_##flag)

#define RADEON_SET( flag ) \
     rdev->set |= SMF_##flag

#define RADEON_UNSET( flag ) \
     rdev->set &= ~(SMF_##flag)
     
     
static inline __u32
radeon_buffer_offset( RadeonDeviceData *rdev, SurfaceBuffer *buffer )
{
     switch (buffer->storage) {
          case CSS_VIDEO:
               return buffer->video.offset + rdev->fb_offset;
          case CSS_AUXILIARY:
               return buffer->video.offset + rdev->agp_offset;
          default:
               D_BUG( "unknown buffer storage" );
               break;
     }
     
     return 0;
}

                                                        
#endif /* __RADEON_STATE_H__ */
