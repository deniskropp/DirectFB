// Shared header file for uc_hwmap.c and uc_hwset.c.

#ifndef __UC_HW_H__
#define __UC_HW_H__

#include <core/coredefs.h>

#include "unichrome.h"
#include "uc_fifo.h"

// GPU - mapping functions (uc_hwmap.c)

/// Map a DirectFB destination surface pixel format to the hw. (3D)
static inline int uc_map_dst_format( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_ARGB1555: return HC_HDBFM_ARGB1555;
          case DSPF_RGB16:    return HC_HDBFM_RGB565;
          case DSPF_RGB32:    return HC_HDBFM_ARGB0888;
          case DSPF_ARGB:     return HC_HDBFM_ARGB8888;

          default:
               D_BUG( "unexpected pixel format" );
     }

     return 0;
}

/// Map a DirectFB source surface pixel format to the hw. (3D)
static inline int uc_map_src_format_3d( DFBSurfacePixelFormat format )
{
     switch (format) {
          case DSPF_ARGB1555: return HC_HTXnFM_ARGB1555;
          case DSPF_RGB16:    return HC_HTXnFM_RGB565;
          case DSPF_RGB32:    return HC_HTXnFM_ARGB0888;
          case DSPF_ARGB:     return HC_HTXnFM_ARGB8888;
          case DSPF_A8:       return HC_HTXnFM_A8;
          case DSPF_LUT8:     return HC_HTXnFM_Index8;

          default:
               D_BUG( "unexpected pixel format" );
     }

     return 0;
}

inline void uc_map_blending_fn( struct uc_hw_alpha      *hwalpha,
                                DFBSurfaceBlendFunction  sblend,
                                DFBSurfaceBlendFunction  dblend,
                                DFBSurfacePixelFormat    dformat );

inline void uc_map_blitflags  ( struct uc_hw_texture    *tex,
                                DFBSurfaceBlittingFlags  bflags,
                                DFBSurfacePixelFormat    sformat );

// GPU - setting functions (uc_hwset.c)

inline void uc_set_blending_fn( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_texenv     ( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_clip       ( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_destination( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_source_2d  ( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_source_3d  ( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_color_2d   ( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

inline void uc_set_colorkey_2d( UcDriverData *ucdrv,
                                UcDeviceData *ucdev,
                                CardState    *state );

#endif // __UC_HW_H__

