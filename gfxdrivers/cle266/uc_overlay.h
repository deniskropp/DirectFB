#ifndef __UC_OVERLAY_H__
#define __UC_OVERLAY_H__

#define UC_OVL_CAPS (DLCAPS_SURFACE | DLCAPS_OPACITY | DLCAPS_SCREEN_LOCATION)
#define UC_OVL_OPTIONS DLOP_NONE

#define ALIGN_TO(v, n) (((v) + (n-1)) & ~(n-1))
#define UC_MAP_V1_FIFO_CONTROL(depth, pre_thr, thr) \
    (((depth)-1) | ((thr) << 8) | ((pre_thr) << 24))

// Actions for uc_ovl_update()

#define UC_OVL_FLIP     1
#define UC_OVL_CHANGE   2

/** Overlay layer data. */
struct uc_ovl_vinfo {
    bool isenabled;                 // True when visible
    DFBRectangle win;               // Layer screen rectangle.
    DFBDisplayLayerConfig cfg;      // Layer configuration
    int ox, oy;                     // Top-left visible corner (the offset)
                                    // in the source surface
    __u8 opacity;                   // Layer opacity
    int level;                      // Position in the DirectFB layer stack
                                    // < 0 = underlay mode, > 0 = overlay mode
};

typedef struct _UcOverlayData {

    // TODO: initialize the variables!!!

    __u8                hwrev;       // CLE266 revision
    volatile __u8*      hwregs;      // Hardware register base
    int                 scrwidth;    // Current screen width

    bool                extfifo_on;  // True when we're using the extended fifo.
    __u8                mclk_save[3];

    struct uc_ovl_vinfo v1;          // Video overlay V1

} UcOverlayData;


// Video engine - mapping functions (uc_ovl_hwmap.c)

bool uc_ovl_map_vzoom(int sh, int dh, __u32* zoom, __u32* mini);
bool uc_ovl_map_hzoom(int sw, int dw,  __u32* zoom, __u32* mini,
                      int* falign, int* dcount);
__u32 uc_ovl_map_qwpitch(int falign, DFBSurfacePixelFormat format, int sw);
__u32 uc_ovl_map_format(DFBSurfacePixelFormat format);
void uc_ovl_map_window(int scrw, int scrh, DFBRectangle* win, int sw, int sh,
                       __u32* win_start, __u32* win_end, int* ox, int* oy);
void uc_ovl_map_buffer(DFBSurfacePixelFormat format, __u32 buf,
                       int x, int y, int w, int h, int pitch,
                       __u32* y_start, __u32* u_start, __u32* v_start);
__u32 uc_ovl_map_alpha(int opacity);
void uc_ovl_map_v1_control(DFBSurfacePixelFormat format, int sw,
                           int hwrev, bool extfifo_on,
                           __u32* control, __u32* fifo);
__u32 uc_ovl_map_fifo(__u8 depth, __u8 pre_thr, __u8 thr);

// Video engine - setting functions (uc_ovl_hwset.c)

void uc_ovl_setup_fifo(UcOverlayData* ucovl, int scrwidth);
void uc_ovl_vcmd_wait(volatile __u8* vio);
DFBResult uc_ovl_update(UcOverlayData* ucovl, int action,
                        CoreSurface* surface);


#endif // __UC_OVERLAY_H__
