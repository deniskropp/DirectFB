/*
 * Copyright (C) 2004-2005 Claudio "KLaN" Ciccani <klan@users.sf.net>
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
 */


#ifndef __VIDEO_OUT_DFB_H__
#define __VIDEO_OUT_DFB_H__

#include <direct/types.h>


/**************************** Driver Interface ********************************/

typedef void (*DVOutputCallback) ( void         *cdata,
                                   int           width,
                                   int           height,
                                   double        ratio,
                                   DFBRectangle *dest_rect );

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


typedef struct {
     const char  *name;
     struct {
          short   off;
          short   min;
          short   max;
     } y;
     struct {
          short   off;
          short   min;
          short   max;
     } uv;
     int          v_for_r;
     int          v_for_g;
     int          u_for_g;
     int          u_for_b;
} DVProcMatrix;

typedef union {
     struct {
          uint8_t a;
          uint8_t r;
          uint8_t g;
          uint8_t b;
     } rgb;
     struct {
          uint8_t a;
          uint8_t y;
          uint8_t u;
          uint8_t v;
     } yuv;
} DVColor;

typedef struct {
     uint8_t      *plane[3];
     uint32_t      pitch[3];
     uint32_t      period[2];
     
     int           x;
     int           y;
     int           len;
} DVBlender;


typedef void (*DVProcFunc) ( dfb_frame_t *frame );

typedef void (*DVBlendFunc) ( DVBlender *blender,
                              DVColor   *color );


typedef enum {
     MF_NONE = 0x0000000,
     MF_B    = 0x00000001,  /* brightness */
     MF_C    = 0x00000002,  /* contrast   */
     MF_S    = 0x00000004,  /* saturation */
     MF_ALL  = 0x00000007
} MixerFlags;

struct dfb_frame_s {
     vo_frame_t   vo_frame;
     
     int          width;      /* multiple of 4 and at least  8 */
     int          height;     /* multiple of 2 and at least  2 */
     int          area;       /* multiple of 8 and at least 16 */
     int          in_format;
     int          out_format;
     bool         interlaced;

     CoreSurface *surface;
     uint8_t     *out_plane[3];
     uint32_t     out_pitch[3]; 
     void        *chunk[3];
     
     MixerFlags   mixer_set;
     bool         proc_needed;
};

struct dfb_driver_s {
     vo_driver_t             vo_driver;

     xine_t                 *xine;

     struct {
          int                max_num_frames;
          int                proc_matrix;
          bool               enable_mmx;
     } config;

     int                     accel;
     
     CardState               state;
     
     IDirectFBSurface       *dest;
     IDirectFBSurface_data  *dest_data;
     DFBSurfacePixelFormat   dest_format;
     
     IDirectFBSurface       *ovl;
     IDirectFBSurface_data  *ovl_data;
     int                     ovl_width;
     int                     ovl_height;
     bool                    ovl_changed;   
     
     DFBSurfacePixelFormat   frame_format;
     int                     frame_width;
     int                     frame_height;
     int                     frame_interlaced;
     
     struct {
          MixerFlags         set;
          int                b;
          int                c;
          int                s;
     } mixer;

     int                     deinterlace;
     int                     aspect_ratio;
     double                  output_ratio;
     
     DVOutputCallback        output_cb;
     void*                   output_cdata;

     DVFrameCallback         frame_cb;
     void*                   frame_cdata;
};

typedef struct {
     video_driver_class_t  vo_class;
     xine_t               *xine;
} dfb_driver_class_t;


/****************************** Internal Macros *******************************/

#define DFB_PFUNCTION_GEN_NAME( a, s, d )  __##a##_##s##_be_##d
#define DFB_BFUNCTION_GEN_NAME( a, d )     __##a##_blend_##d

#define DFB_PFUNCTION_NAME( a, s, d )  DFB_PFUNCTION_GEN_NAME( a, s, d )
#define DFB_BFUNCTION_NAME( a, d )     DFB_BFUNCTION_GEN_NAME( a, d )

#define DFB_PFUNCTION( s, d ) \
     void DFB_PFUNCTION_NAME( PACCEL, s, d ) ( dfb_frame_t *frame )

#define DFB_BFUNCTION( d ) \
     void DFB_BFUNCTION_NAME( PACCEL, d ) ( DVBlender *blender, \
                                            DVColor   *color )

#define DFB_PFUNCTION_ASSIGN( a, s, d, f ) \
     ProcFuncs[(#s=="yuy2")?0:1][DFB_PIXELFORMAT_INDEX(f)] = DFB_PFUNCTION_NAME(a,s,d)

#define DFB_BFUNCTION_ASSIGN( a, d, f ) \
     BlendFuncs[DFB_PIXELFORMAT_INDEX(f)] = DFB_BFUNCTION_NAME( a, d )


#ifdef min
# undef min
#endif
#define min( a, b )  (((a)<(b)) ? (a) : (b))

#ifdef max
# undef max
#endif
#define max( a, b )  (((a)>(b)) ? (a) : (b))

#ifdef CLAMP
# undef CLAMP
#endif
#define CLAMP( a, min, max )  (((a)<(min)) ? (min) : (((a)>(max)) ? (max) : (a)))

#ifdef __GNUC__
# define __aligned( n )  __attribute__((aligned((n))))
# if (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ > 2)
#  define __used  __attribute__((used))
# else
#  define __used
# endif
#else
# define __aligned( n )
#endif


#ifdef DFB_DEBUG

#include <sys/time.h>

static inline uint64_t
microsec( void )
{
     struct timeval t;
     gettimeofday( &t, NULL );
     return (t.tv_sec*1000000ull + t.tv_usec);
}

# define BENCH_BEGIN( area ) \
  {\
     static   uint32_t _elapsed = 0;\
     static   uint64_t _pixels  = 0;\
     volatile uint64_t _m;\
     uint32_t          _n       = (area);\
     _m = microsec();\

# define BENCH_END() \
     _m = microsec() - _m;\
     _elapsed += (uint32_t) _m;\
     _pixels  += _n;\
     if (_elapsed >= 300000) {\
          xprintf( this->xine, XINE_VERBOSITY_DEBUG,\
                   "video_out_dfb: %s() BENCH [%2d.%03d secs | %6lld.%03d K/Pixels]\n",\
                   __FUNCTION__, (_elapsed/1000000), (_elapsed/1000)%1000, \
                   _pixels/1000ull, (int) (_pixels%1000ull) );\
          _elapsed = 0;\
          _pixels  = 0;\
     }\
  }

#else /* ! DFB_DEBUG */

# define BENCH_BEGIN( area )
# define BENCH_END()         (void) this;

#endif /* DFB_DEBUG */
 

#endif /* __VIDEO_OUT_DFB_H__ */

