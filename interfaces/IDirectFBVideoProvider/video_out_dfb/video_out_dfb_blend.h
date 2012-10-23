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

typedef void (*BlendSpanFunc) ( dfb_frame_t *frame, 
                                int          x,
                                int          y,
                                int          w,
                                clut_t       clut,
                                uint8_t      alpha );



static void 
vo_dfb_blend_overlay( dfb_driver_t *this,
                      dfb_frame_t  *frame,
                      vo_overlay_t *overlay )
{
#define MAX_RECTS 100
     DFBRegion     clip;
     DFBRectangle  rects[MAX_RECTS];
     int           n_rects = 0;
     DFBColor      p_color = { 0, 0, 0, 0 };
     int           x, y, i;
     
     if (!overlay->rgb_clut) {
          for (i = 0; i < OVL_PALETTE_SIZE; i++) {
               clut_t    clut  = ((clut_t*)overlay->color)[i];
               uint8_t   alpha = overlay->trans[i];
               DFBColor *color = &((DFBColor*)overlay->color)[i];
               
               YCBCR_TO_RGB( clut.y, clut.cb, clut.cr,
                             color->r, color->g, color->b );
               color->a = alpha | (alpha<<4);
          }
          overlay->rgb_clut++;
     }
     
     if (!overlay->hili_rgb_clut) {
          for (i = 0; i < OVL_PALETTE_SIZE; i++) {
               clut_t    clut  = ((clut_t*)overlay->hili_color)[i];
               uint8_t   alpha = overlay->hili_trans[i];
               DFBColor *color = &((DFBColor*)overlay->hili_color)[i];
               
               YCBCR_TO_RGB( clut.y, clut.cb, clut.cr,
                             color->r, color->g, color->b );
               color->a = alpha | (alpha<<4);
          }
          overlay->hili_rgb_clut++;
     }
  
     clip.x1 = overlay->x;
     clip.y1 = overlay->y;
     clip.x2 = clip.x1 + overlay->width - 1;
     clip.y2 = clip.y1 + overlay->height - 1;
     this->ovl->SetClip( this->ovl, &clip );
     
     if (dfb_region_region_intersects( &this->ovl_region, &clip )) {
          /* overlapping overlays */
          this->ovl->SetPorterDuff( this->ovl, DSPD_ADD );
          this->ovl->SetDrawingFlags( this->ovl, DSDRAW_BLEND );
     } 
     else {
          this->ovl->SetDrawingFlags( this->ovl, DSDRAW_NOFX );
     }
     
     dfb_region_region_union( &this->ovl_region, &clip );
  
     for (x = 0, y= 0, i = 0; i < overlay->num_rle; i++) {
          int idx = overlay->rle[i].color;
          int len = overlay->rle[i].len;
    
          while (len > 0) {
               DFBColor color = ((DFBColor*)overlay->color)[idx];
               int      width;
      
               if ((len+x) > overlay->width) {
                    width = overlay->width - x;
                    len  -= width;
               } else {
                    width = len;
                    len   = 0;
               }
      
               if (y >= overlay->hili_top    &&
                   y <= overlay->hili_bottom &&
                   x <= overlay->hili_right)
               {
                    if (x < overlay->hili_left && (x+width-1) >= overlay->hili_left) {
                         width -= overlay->hili_left - x;
                         len += overlay->hili_left - x;
                    }
                    else if (x > overlay->hili_left)  {
                         color = ((DFBColor*)overlay->hili_color)[idx];
          
                         if ((x+width-1) > overlay->hili_right) {
                              width -= overlay->hili_right - x;
                              len   += overlay->hili_right - x;
                         }
                    }
               }
        
               if (color.a) {
                    if (n_rects == MAX_RECTS || !DFB_COLOR_EQUAL(p_color,color)) {
                         if (n_rects) {
                              lprintf( "flushing %d rect(s).\n", n_rects );
                              this->ovl->FillRectangles( this->ovl, rects, n_rects );
                              n_rects = 0;
                         }
                         this->ovl->SetColor( this->ovl, color.r, color.g,
                                                         color.b, color.a );
                         p_color = color;
                    }
        
                    rects[n_rects].x = x + overlay->x;
                    rects[n_rects].y = y + overlay->y;
                    rects[n_rects].w = width;
                    rects[n_rects].h = 1;
                    if (n_rects &&
                        rects[n_rects-1].x == rects[n_rects].x &&
                        rects[n_rects-1].w == rects[n_rects].w &&
                        rects[n_rects-1].y+rects[n_rects-1].h == rects[n_rects].y)
                         rects[--n_rects].h++;  
               
                    n_rects++;
               }
      
               x += width;
               if (x == overlay->width) {
                    if (++y == overlay->height)
                         goto end;
                    x = 0;
               }
          }
     }
  
end:
     if (n_rects) {
          lprintf ("flushing %d remaining rect(s).\n", n_rects);
          this->ovl->FillRectangles( this->ovl, rects, n_rects );
     }
}

                           
static void
blend_span_yuy2( dfb_frame_t *frame,
                 int          x,
                 int          y,
                 int          w,
                 clut_t       clut,
                 uint8_t      alpha )
{
     uint16_t *d = (uint16_t*)(frame->vo_frame.base[0] + 
                               y * frame->vo_frame.pitches[0] + x*2);
     
     if (alpha < 15) {
          uint32_t a = (alpha | (alpha<<4)) + 1;
          uint32_t b = 256 - a;
          uint32_t s;
          uint32_t t;
          
          s = (clut.y  | (clut.y <<16)) * a;
#ifdef WORDS_BIGENDIAN
          t = (clut.cr | (clut.cb<<16)) * a;
#else
          t = (clut.cb | (clut.cr<<16)) * a;
#endif
          
          if (x & 1) {
               *d = (((*d & 0xff) * b + clut.y  * a) >> 8) |
                    (((*d >> 8)   * b + clut.cr * a) & 0xff);
               d++;
               w--;
          }
          
          for (; w > 1; w -= 2) {
               uint32_t p = *((uint32_t*)d);
               
               p = ((((p & 0x00ff00ff) * b + s) >> 8) & 0x00ff00ff) |
                   ((((p & 0xff00ff00) >> 8) * b + t) & 0xff00ff00);
               
               *((uint32_t*)d) = p;
               d += 2;
          }
          
          if (w & 1) {
               *d = (((*d & 0xff) * b + clut.y * a) >> 8) |
                    (((*d >> 8) * b + clut.cb * a) & 0xff00);
          }
     }
     else {
#ifdef WORDS_BIGENDIAN
          uint32_t s = clut.y | (clut.cr<<8) | (clut.y<<16) | (clut.cb<<24);
#else
          uint32_t s = clut.y | (clut.cb<<8) | (clut.y<<16) | (clut.cr<<24);
#endif     
          
          if (x & 1) {
               *d = clut.y | (clut.cr<<8);
               d++;
               w--;
          }
          
          for (; w > 1; w -= 2) {
               *((uint32_t*)d) = s;
               d += 2;
          }
          
          if (w & 1)
               *d = clut.y | (clut.cb<<8);
     }
}

static void
blend_span_yv12( dfb_frame_t *frame,
                 int          x,
                 int          y,
                 int          w,
                 clut_t       clut,
                 uint8_t      alpha )
{
     uint8_t *d = frame->vo_frame.base[0] + 
                  y * frame->vo_frame.pitches[0] + x;
     int      i;
     
     if (alpha < 15) {
          uint32_t a = (alpha | (alpha<<4)) + 1;
          uint32_t b = 256 - a;
          uint32_t s = clut.y * a;
          
          for (i = 0; i < w; i++)
               d[i] = (d[i] * b + s) >> 8;
               
          if (y & 1) {
               x /= 2; y /= 2; w /= 2;
               
               d = frame->vo_frame.base[1] +
                   y * frame->vo_frame.pitches[1] + x;
               s = clut.cb * a;
                   
               for (i = 0; i < w; i++)
                    d[i] = (d[i] * b + s) >> 8;
                    
               d = frame->vo_frame.base[2] + 
                   y * frame->vo_frame.pitches[2] + x;
               s = clut.cr * a;
               
               for (i = 0; i < w; i++)
                    d[i] = (d[i] * b + s) >> 8;
          }
     }
     else {
          memset( d, clut.y, w );
          
          if (y & 1) {
               x /= 2; y /= 2; w /= 2;
               
               memset( frame->vo_frame.base[1] +
                       y * frame->vo_frame.pitches[1] + x, clut.cb, w );
               memset( frame->vo_frame.base[2] + 
                       y * frame->vo_frame.pitches[2] + x, clut.cr, w );
          }
     }
}

static void
vo_dfb_blend_frame( dfb_driver_t *this, 
                    dfb_frame_t  *frame,
                    vo_overlay_t *overlay )
{
     BlendSpanFunc blendfunc;                              
     DFBRegion     clip;
     int           x, y, i;
     
     blendfunc = (frame->format == DSPF_YUY2)
                 ? blend_span_yuy2 : blend_span_yv12;
     
     clip.x1 = overlay->x;
     clip.y1 = overlay->y;
     clip.x2 = clip.x1 + overlay->width - 1;
     clip.y2 = clip.y1 + overlay->height - 1;
     
     if (clip.x1 < 0) {
          clip.x2 += clip.x1;
          clip.x1  = 0;
     }
     if (clip.y1 < 0) {
          clip.y2 += clip.y1;
          clip.y1  = 0;
     } 
     if (clip.x2 >= frame->width)
          clip.x2 = frame->width - 1;
     if (clip.y2 >= frame->height)
          clip.y2 = frame->height - 1;
     
     for (x = 0, y= 0, i = 0; i < overlay->num_rle; i++) {
          int idx = overlay->rle[i].color;
          int len = overlay->rle[i].len;
    
          while (len > 0) {
               clut_t  clut  = ((clut_t*)overlay->color)[idx];
               uint8_t alpha = overlay->trans[idx];
               int     width;
      
               if ((len+x) > overlay->width) {
                    width = overlay->width - x;
                    len  -= width;
               } else {
                    width = len;
                    len   = 0;
               }
      
               if (y >= overlay->hili_top    &&
                   y <= overlay->hili_bottom &&
                   x <= overlay->hili_right)
               {
                    if (x < overlay->hili_left && (x+width-1) >= overlay->hili_left) {
                         width -= overlay->hili_left - x;
                         len += overlay->hili_left - x;
                    }
                    else if (x > overlay->hili_left)  {
                         clut  = ((clut_t*)overlay->hili_color)[idx];
                         alpha = overlay->hili_trans[idx];
          
                         if ((x+width-1) > overlay->hili_right) {
                              width -= overlay->hili_right - x;
                              len   += overlay->hili_right - x;
                         }
                    }
               }
        
               if (alpha) {
                    x += overlay->x;
                    y += overlay->y;
                    if (y >= clip.y1 && y <= clip.y2 &&
                        x >= clip.x1 && x <= clip.x2)
                    {
                         int w = MIN( width, clip.x2 - x + 1 );
                         blendfunc( frame, x, y, w, clut, alpha );
                    }
                    x -= overlay->x;
                    y -= overlay->y;
               }
      
               x += width;
               if (x == overlay->width) {
                    if (++y == overlay->height)
                         return;
                    x = 0;
               }
          }
     }
}

