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


#define THIS  "video_out_dfb"



typedef struct dfb_frame_s  dfb_frame_t;
typedef struct dfb_driver_s dfb_driver_t;


typedef void (*DVProcFunc) ( dfb_driver_t *this,
                             dfb_frame_t  *frame,
                             uint8_t      *dst,
                             uint32_t      pitch );

typedef const DVProcFunc* DVProcFuncs;

typedef void (*DVOutputCallback) ( void         *cdata,
                                   int           width,
                                   int           height,
                                   double        ratio,
                                   DFBRectangle *dest_rect );


struct dfb_frame_s
{
     vo_frame_t   vo_frame;
     
     struct
     {
          int  cur;
          int  prev;
     } width;

     struct
     {
          int  cur;
          int  prev;
     } height;

     struct
     {
          int  cur;
          int  prev;
     } imgfmt;

     struct
     {
          int  cur;
          int  prev;
     } dstfmt;

     double       ratio;

     CoreSurface *surface;
     void        *chunks[3];
     
     int          proc_needed;
     DVProcFunc   procf;
};



/* Mixer Modified Flags */
#define MMF_B  0x00000001
#define MMF_C  0x00000002
#define MMF_S  0x00000004
#define MMF_A  (MMF_B|MMF_C|MMF_S)


struct dfb_driver_s
{
     vo_driver_t            vo_driver;

     char                   verbosity;
     int                    max_num_frames;

     CardState              state;
     IDirectFBSurface      *dest;
     IDirectFBSurface_data *dest_data;

     struct
     {
          char              defined;
          char              used;
     } correction;

     struct
     {
          int               modified;
          int               b;
          int               c;
          int               s;
     } mixer;

     struct
     {
          int               accel;
          DVProcFuncs       funcs[2];
     } proc;

     DVOutputCallback       output_cb;
     void*                  output_cdata;

     DVFrameCallback        frame_cb;
     void*                  frame_cdata;
};


typedef struct
{
     video_driver_class_t  vo_class;

     xine_t               *xine;

} dfb_driver_class_t;


typedef struct
{
     IDirectFBSurface *surface;

     DVOutputCallback  output_cb;

     void             *cdata;

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
 *                . ratio is video aspect ratio
 *                . dest_rect is used to set video rendering size and 
 *                  position (the driver expects that you fill this)
 *  -> cdata: your private data (can be NULL)
 *
 */


typedef struct
{
     DVFrameCallback  frame_cb;

     void            *cdata;

} dfb_frame_callback_t;

/*
 * You can register a DVFrameCallback using xine_port_send_gui_data()
 * with XINE_GUI_SEND_TRASLATE_GUI_TO_VIDEO as second argument and
 * a dfb_frame_callback_t* as third argument.
 *
 */


/************************** Internal Macros ******************************/


#define DFB_PFUNCTION_GEN_NAME( a, s, d )  __##a##_##s##_be_##d

#define DFB_PFUNCTION_NAME( a, s, d )  DFB_PFUNCTION_GEN_NAME( a, s, d )

#define DFB_PFUNCTION( s, d ) \
void  DFB_PFUNCTION_NAME( PACCEL, s, d ) ( dfb_driver_t *this,  \
                                           dfb_frame_t  *frame, \
                                           uint8_t      *dst,   \
                                           uint32_t      pitch )

#define SAY( fmt, ... ) \
{\
     if (this->verbosity)\
          fprintf( stderr, THIS ": " fmt "\n", ## __VA_ARGS__ );\
}

#define DBUG( fmt, ... ) \
{\
     if (this->verbosity == XINE_VERBOSITY_DEBUG)\
          fprintf( stderr, THIS ": " fmt "\n", ## __VA_ARGS__ );\
}

#define ONESHOT( fmt, ... ) \
{\
     if (this->verbosity)\
     {\
          static int one = 1;\
          if (one)\
          {\
               fprintf( stderr, THIS ": " fmt "\n", ## __VA_ARGS__ );\
               one = 0;\
          }\
     }\
}

#define release( ptr ) \
{\
     if (ptr)\
          free( ptr );\
     ptr = NULL;\
}


#ifdef DFB_DEBUG

# include <direct/clock.h>

# define SPEED( f ) \
  {\
       static int test = 15;\
       if (test)\
       {\
            long long t = direct_clock_get_micros();\
            f;\
            t = direct_clock_get_micros() - t;\
            fprintf( stderr, THIS ": [%lli micros.] speed test\n", t );\
            test--;\
       } else\
            f;\
  }

#else /* ! DFB_DEBUG */

# define SPEED( f )  f;

#endif /* DFB_DEBUG */
 


#endif /* VIDEO_OUT_DFB_H */

