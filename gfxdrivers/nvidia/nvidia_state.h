#ifndef __NVIDIA_STATE_H__
#define __NVIDIA_STATE_H__


void nv_set_destination   ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_source        ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_clip          ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_drawing_color ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_blitting_color( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_blend_function( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_drawingflags  ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );
void nv_set_blittingflags ( NVidiaDriverData *nvdrv,
                            NVidiaDeviceData *nvdev,
                            CardState        *state );

#endif /* __NVIDIA_STATE_H__ */
