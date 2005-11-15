#ifndef __UC_OVERLAY_H__
#define __UC_OVERLAY_H__

#define UC_OVL_CAPS (DLCAPS_SURFACE | DLCAPS_OPACITY | DLCAPS_SCREEN_LOCATION \
    | DLCAPS_DEINTERLACING | DLCAPS_DST_COLORKEY | DLCAPS_LEVELS \
    | DLCAPS_FIELD_PARITY )
/*    | DLCAPS_BRIGHTNESS | DLCAPS_CONTRAST              \
    | DLCAPS_SATURATION | DLCAPS_HUE)
    */
#define UC_OVL_OPTIONS (DLOP_DEINTERLACING | DLOP_DST_COLORKEY \
    | DLOP_FIELD_PARITY | DLOP_OPACITY )

#define ALIGN_TO(v, n) (((v) + (n-1)) & ~(n-1))
#define UC_MAP_V1_FIFO_CONTROL(depth, pre_thr, thr) \
    (((depth)-1) | ((thr) << 8) | ((pre_thr) << 24))

// Actions for uc_ovl_update()

#define UC_OVL_FLIP     1
#define UC_OVL_CHANGE   2
#define UC_OVL_FIELD    4

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
    DFBColorAdjustment adj;         // Color adjustment (brightness etc)
    DFBColor dst_key;               // Destination color key
    bool dstkey_enabled;            // Destination color key is enabled
};

typedef struct _UcOverlayData {

    // TODO: initialize the variables!!!

    __u8                hwrev;       // Unichrome revision
    int                 scrwidth;    // Current screen width

    bool                extfifo_on;  // True when we're using the extended fifo.
    __u8                mclk_save[3];

    struct uc_ovl_vinfo v1;          // Video overlay V1

    CoreLayerRegionConfig config;
    
    bool                deinterlace;
    int                 field;

    CoreSurface        *surface;
    
    int                 opacity_primary; // overlay opacity if primary is logically
                                         // above or -1 if primary has alpha channel

} UcOverlayData;


// Video engine - mapping functions (uc_ovl_hwmap.c)

bool uc_ovl_map_vzoom(int sh, int dh, __u32* zoom, __u32* mini);
bool uc_ovl_map_hzoom(int sw, int dw,  __u32* zoom, __u32* mini,
                      __u32* falign, __u32* dcount);
__u32 uc_ovl_map_qwpitch(int falign, DFBSurfacePixelFormat format, int sw);
__u32 uc_ovl_map_format(DFBSurfacePixelFormat format);
void uc_ovl_map_window(int scrw, int scrh, DFBRectangle* win, int sw, int sh,
                       __u32* win_start, __u32* win_end, int* ox, int* oy);
void uc_ovl_map_buffer(DFBSurfacePixelFormat format, __u32 buf,
                       int x, int y, int w, int h, int pitch, int field,
                       __u32* y_start, __u32* u_start, __u32* v_start);
__u32 uc_ovl_map_alpha(int opacity);
void uc_ovl_map_v1_control(DFBSurfacePixelFormat format, int sw,
                           int hwrev, bool extfifo_on,
                           __u32* control, __u32* fifo);
__u32 uc_ovl_map_fifo(__u8 depth, __u8 pre_thr, __u8 thr);
void uc_ovl_map_adjustment(DFBColorAdjustment* adj, __u32* a1, __u32* a2);
__u32 uc_ovl_map_colorkey(DFBColor* c);

// Video engine - setting functions (uc_ovl_hwset.c)

void uc_ovl_setup_fifo(UcOverlayData* ucovl, int scrwidth);
void uc_ovl_vcmd_wait(volatile __u8* vio);
DFBResult uc_ovl_update(UcDriverData* ucdrv,
                        UcOverlayData* ucovl, int action,
                        CoreSurface* surface);
DFBResult uc_ovl_set_adjustment(CoreLayer *layer,
                                void *driver_data,
                                void *layer_data,
                                DFBColorAdjustment *adj);

#endif // __UC_OVERLAY_H__
