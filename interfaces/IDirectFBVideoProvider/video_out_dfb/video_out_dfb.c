/*
 * Copyright (C) 2004-2006 Claudio Ciccani <klan@users.sf.net>
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

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#include <directfb.h>
#include <directfb_version.h>
#include <directfb_util.h>

#include <direct/util.h>

#include <idirectfb.h>

#include <gfx/convert.h>

#define LOG_MODULE "video_out_dfb"
#define LOG_VERBOSE

#include <xine.h>
#include <xine/xine_internal.h>
#include <xine/xineutils.h>
#include <xine/video_out.h>
#include <xine/alphablend.h>

#include "video_out_dfb.h"
#include "video_out_dfb_mix.h"
#include "video_out_dfb_blend.h"


static uint32_t
vo_dfb_get_capabilities( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;
     uint32_t      caps = VO_CAP_YV12 | VO_CAP_YUY2;

     if (this->ovl)
          caps |= VO_CAP_UNSCALED_OVERLAY;

     lprintf( "capabilities = 0x%08x\n", caps );     
     return caps;
}

static void
vo_dfb_proc_frame( vo_frame_t *vo_frame )
{
     dfb_driver_t *this  = (dfb_driver_t*) vo_frame->driver;
     dfb_frame_t  *frame = (dfb_frame_t*)  vo_frame;

     _x_assert( frame->surface != NULL );

     frame->vo_frame.proc_called = 1;
     
     if (this->mixer.l_csc || this->mixer.c_csc) {
          switch (frame->format) {
               case DSPF_YUY2:
                    vo_dfb_mix_yuy2( this, frame );
                    break;
               case DSPF_YV12:
                    vo_dfb_mix_yv12( this, frame );
                    break;
               default:
                    break;
          }
     }
}

static void
vo_dfb_frame_field( vo_frame_t *vo_frame,
                    int         which_field )
{
     /* not needed */
}

static void
vo_dfb_frame_dispose( vo_frame_t *vo_frame )
{
     dfb_frame_t *frame = (dfb_frame_t*) vo_frame;

     if (frame) {
          if (frame->surface) {
               frame->surface->Unlock( frame->surface );
               frame->surface->Release( frame->surface );
          }
          free( frame );
     }
}

static vo_frame_t*
vo_dfb_alloc_frame( vo_driver_t *vo_driver )
{
     dfb_frame_t *frame;

     frame = (dfb_frame_t*) xine_xmalloc( sizeof(dfb_frame_t) );
     if (!frame) {
          lprintf( "frame allocation failed!!!\n" );
          return NULL;
     }

     pthread_mutex_init( &frame->vo_frame.mutex, NULL );

     frame->vo_frame.proc_slice = NULL;
     frame->vo_frame.proc_frame = vo_dfb_proc_frame;
     frame->vo_frame.field      = vo_dfb_frame_field;
     frame->vo_frame.dispose    = vo_dfb_frame_dispose;
     frame->vo_frame.driver     = vo_driver;

     return (vo_frame_t*) frame;
}  

static void
vo_dfb_update_frame_format( vo_driver_t *vo_driver,
                            vo_frame_t  *vo_frame,
                            uint32_t     width,
                            uint32_t     height,
                            double       ratio,
                            int          format,
                            int          flags )
{
     dfb_driver_t  *this  = (dfb_driver_t*) vo_driver;
     dfb_frame_t   *frame = (dfb_frame_t*)  vo_frame;

     _x_assert( this->dest != NULL );
     
     format = (format == XINE_IMGFMT_YUY2) ? DSPF_YUY2 : DSPF_YV12;
     
     if (frame->surface == NULL   ||
         frame->format  != format ||
         frame->width   != width  ||
         frame->height  != height)
     {
          DFBSurfaceDescription dsc;
          DFBResult             ret;

          lprintf( "reformatting frame %p\n", frame );
          
          if (frame->surface) {
               frame->surface->Unlock( frame->surface );
               frame->surface->Release( frame->surface );
               frame->surface = NULL;
          }
          
          dsc.flags       = DSDESC_CAPS | DSDESC_WIDTH | 
                            DSDESC_HEIGHT | DSDESC_PIXELFORMAT;
          dsc.caps        = DSCAPS_SYSTEMONLY | DSCAPS_INTERLACED;
          dsc.width       = (width + 7) & ~7;
          dsc.height      = (height + 1) & ~1;
          dsc.pixelformat = format;
          
          ret = idirectfb_singleton->CreateSurface( idirectfb_singleton,
                                                    &dsc, &frame->surface );
          if (ret) {
               DirectFBError( "IDirectFB::CreateSurface()", ret );
               return;
          }
          
          frame->surface->Lock( frame->surface, DSLF_WRITE,
                                (void*)&frame->vo_frame.base[0],
                                (int *)&frame->vo_frame.pitches[0] );
                                
          if (format == DSPF_YV12) {
               frame->vo_frame.pitches[1] =
               frame->vo_frame.pitches[2] = frame->vo_frame.pitches[0]/2;
               frame->vo_frame.base[2]    = frame->vo_frame.base[0] + 
                                            dsc.height * frame->vo_frame.pitches[0];
               frame->vo_frame.base[1]    = frame->vo_frame.base[2] +
                                            dsc.height/2 * frame->vo_frame.pitches[2];
          }
          
          frame->format = format;
          frame->width  = width;
          frame->height = height;
     }
}

static void
vo_dfb_overlay_begin( vo_driver_t *vo_driver,
                      vo_frame_t  *vo_frame,
                      int          changed )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     if (this->ovl) {
          int w, h;
          
          this->ovl->GetSize( this->ovl, &w, &h );

          if (changed || this->ovl_width != w || this->ovl_height != h) {
               lprintf( "overlay changed, clearing subpicture surface\n" );
               
               this->ovl->SetClip( this->ovl, NULL );
               this->ovl->Clear( this->ovl, 0, 0, 0, 0 );

               this->ovl_width   = w;
               this->ovl_height  = h;
               this->ovl_region  = (DFBRegion) { 0, 0, 0, 0 };
               this->ovl_changed = 1;
          }
     }
}          
 
static void
vo_dfb_overlay_blend( vo_driver_t  *vo_driver,
                      vo_frame_t   *vo_frame,
                      vo_overlay_t *overlay )
{
     dfb_driver_t *this    = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame   = (dfb_frame_t*) vo_frame;
     int           use_ovl = 0;

     _x_assert( frame->surface != NULL );
     _x_assert( overlay->rle != NULL );

     if (this->ovl) {
          if (overlay->unscaled || 
             (frame->width  == this->ovl_width &&
              frame->height == this->ovl_height))
          {
               if (!this->ovl_changed)
                    return;
               use_ovl = 1;
          }
     }
     
     if (use_ovl)        
          vo_dfb_blend_overlay( this, frame, overlay );
     else
          vo_dfb_blend_frame( this, frame, overlay );
}

static void
vo_dfb_overlay_end( vo_driver_t *vo_driver,
                    vo_frame_t  *vo_frame )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     if (this->ovl && this->ovl_changed)
          this->ovl->Flip( this->ovl, NULL, 0 );
     
     this->ovl_changed = 0;
}          

static int
vo_dfb_redraw_needed( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     if (this->dest) {
          int w, h;
          this->dest->GetSize( this->dest, &w, &h );
          if (this->dest_width != w || this->dest_height != h) {
               lprintf( "redraw needed\n" );
               return 1;
          }
     }     
     
     return 0;
}

static inline double
aspect_ratio( dfb_driver_t *this, dfb_frame_t *frame )
{
     switch (this->aspect_ratio) {
          case XINE_VO_ASPECT_AUTO:
               return frame->vo_frame.ratio ? : 1.0;
          case XINE_VO_ASPECT_SQUARE:
               return (double)frame->width/(double)frame->height;
          case XINE_VO_ASPECT_4_3:
               return 4.0/3.0;
          case XINE_VO_ASPECT_ANAMORPHIC:
               return 16.0/9.0;
          case XINE_VO_ASPECT_DVB:
               return 2.0;
          default:
               break;
     }
     
     return (double)(this->aspect_ratio >> 16) /
            (double)(this->aspect_ratio & 0xffff);
}

static void
vo_dfb_display_frame( vo_driver_t *vo_driver,
                      vo_frame_t  *vo_frame )
{
     dfb_driver_t *this      = (dfb_driver_t*) vo_driver;
     dfb_frame_t  *frame     = (dfb_frame_t*) vo_frame;
     DFBRectangle  dst_rect  = { 0, 0, 0, 0 };
     DFBRectangle  src_rect  = { 0, 0, frame->width, frame->height };

     _x_assert( frame->surface != NULL );
     
     if (this->output_cb) {
          this->output_cb( this->output_cdata, frame->width, frame->height,
                           aspect_ratio( this, frame ), frame->format, &dst_rect );
     }

     if (this->dest) {
          this->dest->GetSize( this->dest, 
                              &this->dest_width, &this->dest_height );

          if (this->deinterlace) {
               frame->surface->SetField( frame->surface, this->deinterlace-1 );
               this->dest->SetBlittingFlags( this->dest, DSBLIT_DEINTERLACE );
          } else {
               this->dest->SetBlittingFlags( this->dest, DSBLIT_NOFX );
          }
     
          //frame->surface->Unlock( frame->surface );
     
          this->dest->StretchBlit( this->dest, 
                                   frame->surface, &src_rect, &dst_rect );
     
          //frame->surface->Lock( frame->surface, DSLF_WRITE,
          //                     (void*)&frame->vo_frame.base[0],
          //                     (int *)&frame->vo_frame.pitches[0] );

          if (this->frame_cb)
               this->frame_cb( this->frame_cdata );
     }

     frame->vo_frame.free( &frame->vo_frame );
}

static int
vo_dfb_get_property( vo_driver_t *vo_driver,
                    int          property )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property) {
          case VO_PROP_INTERLACED:
               lprintf( "interlaced is %d\n", this->deinterlace );
               return this->deinterlace;

          case VO_PROP_ASPECT_RATIO:
               lprintf( "aspect ratio is %d\n", this->aspect_ratio );
               return this->aspect_ratio;
                   
          case VO_PROP_BRIGHTNESS:
               lprintf( "brightness is %d\n", this->mixer.b );
               return this->mixer.b;
         
          case VO_PROP_CONTRAST:
               lprintf( "contrast is %d\n", this->mixer.c );
               return this->mixer.c;
         
          case VO_PROP_SATURATION:
               lprintf( "saturation is %d\n", this->mixer.s );
               return this->mixer.s;
         
          case VO_PROP_MAX_NUM_FRAMES:
               lprintf( "maximum number of frames is %d\n",
                        this->max_num_frames );
               return this->max_num_frames;
         
          case VO_PROP_WINDOW_WIDTH:
               if (this->ovl || this->dest) {
                    IDirectFBSurface *surface = this->ovl ? : this->dest;
                    int               w;
                    surface->GetSize( surface, &w, NULL );
                    lprintf( "window width is %d\n", w );
                    return w;
               }
               break;

          case VO_PROP_WINDOW_HEIGHT:
               if (this->ovl || this->dest) {
                    IDirectFBSurface *surface = this->ovl ? : this->dest;
                    int               h;
                    surface->GetSize( surface, NULL, &h );
                    lprintf( "window height is %d\n", h );
                    return h;
               }
               break;
               
          default:
               lprintf( "tried to get unsupported property %d\n", property );
               break;
     }
     
     return 0;
}

static int
vo_dfb_set_property( vo_driver_t *vo_driver,
                     int          property,
                     int          value )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (property) {
          case VO_PROP_INTERLACED:
               if (value >= 0 && value <= 2) {
                    lprintf( "setting deinterlacing to %d\n", value );
                    this->deinterlace = value;
               }
               break;

          case VO_PROP_ASPECT_RATIO:
               lprintf( "setting aspect ratio to %d\n", value );
               this->aspect_ratio = value;
               break;
          
          case VO_PROP_BRIGHTNESS:
               if (value >= -128 && value <= 127) {
                    if (this->mixer.b != value) {
                         lprintf( "setting brightness to %d\n", value );
                         this->mixer.b = value; 
                         vo_dfb_update_mixing( this, MF_B );
                    }
               }
               break;

          case VO_PROP_CONTRAST:
               if (value >= 0 && value <= 255) {
                    if (this->mixer.c != value ) {
                         lprintf( "setting contrast to %d\n", value );
                         this->mixer.c = value;
                         vo_dfb_update_mixing( this, MF_C );
                    }
               }
               break;

          case VO_PROP_SATURATION:
               if (value >= 0 && value <= 255) {
                    if (this->mixer.s != value) {
                         lprintf( "setting saturation to %d\n", value );
                         this->mixer.s = value;
                         vo_dfb_update_mixing( this, MF_S );
                    }
               }
               break;
          
          default:
               lprintf( "tried to set unsupported property %d\n", property );
               return 0;
     }

     return value;
}

static void
vo_dfb_get_property_min_max( vo_driver_t *vo_driver,
                             int          property,
                             int         *min,
                             int         *max )
{
     switch (property) {
          case VO_PROP_INTERLACED:
               *min =  0;
               *max = +2;
               break;

          case VO_PROP_ASPECT_RATIO:
               *min =  0;
               *max = +0xffffffff;
               break;
               
          case VO_PROP_BRIGHTNESS:
               *min = -128;
               *max = +127;
               break;

          case VO_PROP_CONTRAST:
               *min = 0;
               *max = 255;
               break;

          case VO_PROP_SATURATION:
               *min = 0;
               *max = 255;
               break;

          default:
               lprintf( "requested min/max for unsupported property %d\n",
                        property );
               *min = 0;
               *max = 0;
               break;
     }
}

static inline int
vo_dfb_set_destination( dfb_driver_t     *this,
                        IDirectFBSurface *surface )
{
     DFBResult ret;

     if (this->dest) {
          this->dest->Release( this->dest );
          this->dest = NULL;
     }
     
     if (surface) {
          ret = surface->GetSubSurface( surface, NULL, &this->dest );
          if (ret) {
               DirectFBError( "IDirectFBSurface::GetSubSurface()", ret );
               return 0;
          }
     }
     
     this->dest_width  = 0;
     this->dest_height = 0;
     
     return 1;
}

static inline int
vo_dfb_set_subpicture( dfb_driver_t     *this,
                       IDirectFBSurface *surface )
{
     DFBResult ret;
     
     if (this->ovl) {
          this->ovl->Release( this->ovl );
          this->ovl = NULL;
     }

     if (surface) {
          ret = surface->GetSubSurface( surface, NULL, &this->ovl );
          if (ret) {
               DirectFBError( "IDirectFBSurface::GetSubSurface()", ret );
               return 0;
          }
     }     

     this->ovl_width  = 0;
     this->ovl_height = 0;

     return 1;
}

static int
vo_dfb_gui_data_exchange( vo_driver_t *vo_driver,
                          int          data_type,
                          void        *data )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;

     switch (data_type) {
          /* update destination surface (deprecated) */
          case XINE_GUI_SEND_DRAWABLE_CHANGED:
               if (data) {
                    IDirectFBSurface *surface = (IDirectFBSurface*) data;
                    
                    if (this->dest != surface)
                         return vo_dfb_set_destination( this, surface );
                    return 1;
               }
               break;

          /* update visual */
          case XINE_GUI_SEND_SELECT_VISUAL:
               if (data) {
                    dfb_visual_t *visual = (dfb_visual_t*) data;

                    this->output_cb    = visual->output_cb;
                    this->output_cdata = visual->output_cdata;
                    this->frame_cb     = visual->frame_cb;
                    this->frame_cdata  = visual->frame_cdata;
                    
                    if (this->dest != visual->destination) {
                         if (!vo_dfb_set_destination( this, visual->destination ))
                              return 0;
                    }
                    
                    if (this->ovl != visual->subpicture) {
                         if (!vo_dfb_set_subpicture( this, visual->subpicture ))
                              return 0;
                    }
                    
                    return 1;
               }
               break;

          /* register/unregister DVFrameCallback (deprecated) */
          case XINE_GUI_SEND_TRANSLATE_GUI_TO_VIDEO:
               if (data) {
                    this->frame_cb    = ((dfb_framecallback_t*)data)->frame_cb;
                    this->frame_cdata = ((dfb_framecallback_t*)data)->cdata;
               } else {
                    this->frame_cb    = NULL;
                    this->frame_cdata = NULL;
               }
               lprintf( "%s DVFrameCallback\n",
                        (this->frame_cb) ? "registered new" : "unregistered" );
               return 1;
          
          default:
               lprintf( "unknown data type %i", data_type );
               break;
     }
     
     return 0;
}

static void
vo_dfb_dispose( vo_driver_t *vo_driver )
{
     dfb_driver_t *this = (dfb_driver_t*) vo_driver;
     
     if (this) {
          if (this->ovl)
               this->ovl->Release( this->ovl );
          if (this->dest)
               this->dest->Release( this->dest );
          
          if (this->mixer.l_csc)
               free( this->mixer.l_csc );
          if (this->mixer.c_csc)
               free( this->mixer.c_csc );
               
          free( this );
     }
}

static vo_driver_t*
open_plugin( video_driver_class_t *vo_class,
             const void           *vo_visual )
{
     dfb_driver_class_t *class  = (dfb_driver_class_t*) vo_class;
     dfb_visual_t       *visual = (dfb_visual_t*) vo_visual;
     dfb_driver_t       *this   = NULL;

     this = (dfb_driver_t*) xine_xmalloc( sizeof(dfb_driver_t) );
     if (!this) {
          lprintf( "memory allocation failed!!!\n" );
          return NULL;
     }
 
     this->vo_driver.get_capabilities     = vo_dfb_get_capabilities;
     this->vo_driver.alloc_frame          = vo_dfb_alloc_frame;
     this->vo_driver.update_frame_format  = vo_dfb_update_frame_format;
     this->vo_driver.overlay_begin        = vo_dfb_overlay_begin;
     this->vo_driver.overlay_blend        = vo_dfb_overlay_blend;
     this->vo_driver.overlay_end          = vo_dfb_overlay_end;
     this->vo_driver.display_frame        = vo_dfb_display_frame;
     this->vo_driver.get_property         = vo_dfb_get_property;
     this->vo_driver.set_property         = vo_dfb_set_property;
     this->vo_driver.get_property_min_max = vo_dfb_get_property_min_max;
     this->vo_driver.gui_data_exchange    = vo_dfb_gui_data_exchange;
     this->vo_driver.redraw_needed        = vo_dfb_redraw_needed;
     this->vo_driver.dispose              = vo_dfb_dispose; 
     this->xine                           = class->xine;

     this->max_num_frames = this->xine->config->register_num( 
                                   this->xine->config,
                                   "video.dfb.max_num_frames", 15,
                                   "Maximum number of allocated frames (at least 5)",
                                   NULL, 10, NULL, NULL );

     if (visual) {
          if (visual->destination) {
               if (!vo_dfb_set_destination( this, visual->destination )) {
                    free( this );
                    return NULL;
               }
          }
     
          if (visual->subpicture)
               vo_dfb_set_subpicture( this, visual->subpicture );
               
          this->output_cb    = visual->output_cb;
          this->output_cdata = visual->output_cdata;
          this->frame_cb     = visual->frame_cb;
          this->frame_cdata  = visual->frame_cdata;
     }
          
     this->mixer.b   =  0;
     this->mixer.c   = +128;
     this->mixer.s   = +128;
     
     return &this->vo_driver;
}

static char*
get_identifier( video_driver_class_t *vo_class )
{
     return "DFB";
}

static char*
get_description( video_driver_class_t *vo_class)
{
     return "generic DirectFB video output driver";
}

static void
dispose_class( video_driver_class_t *vo_class )
{
     free( vo_class );
}

static void*
init_class( xine_t *xine,
            void   *vo_visual )
{
     dfb_driver_class_t *class;
     const char         *error;

     error = DirectFBCheckVersion( DIRECTFB_MAJOR_VERSION,
                                   DIRECTFB_MINOR_VERSION,
                                   DIRECTFB_MICRO_VERSION );
     if (error) {
          fprintf( stderr, "video_out_dfb: %s !!!\n", error );
          return NULL;
     }                   

     class = (dfb_driver_class_t*) xine_xmalloc( sizeof(dfb_driver_class_t) );
     if (!class) {
          lprintf( "memory allocation failed!!!\n" );
          return NULL;
     }

     class->vo_class.open_plugin     = open_plugin;
     class->vo_class.get_identifier  = get_identifier;
     class->vo_class.get_description = get_description;
     class->vo_class.dispose         = dispose_class;
     class->xine                     = xine;

     return class;
}

static vo_info_t vo_info_dfb = {
     8,
     XINE_VISUAL_TYPE_DFB
};


plugin_info_t xine_plugin_info[] = {
     /* type, API, "name", version, special_info, init_function */
     { PLUGIN_VIDEO_OUT, VIDEO_OUT_DRIVER_IFACE_VERSION, "DFB",
                    XINE_VERSION_CODE, &vo_info_dfb, init_class },
     { PLUGIN_NONE, 0, "", 0, NULL, NULL }
};

