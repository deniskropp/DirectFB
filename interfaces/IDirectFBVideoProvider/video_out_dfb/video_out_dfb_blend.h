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


#define PACCEL generic


#ifdef WORDS_BIGENDIAN
# define W0( d )  *((uint16_t*)(d)+1)
# define W1( d )  *((uint16_t*)(d)+0)
#else
# define W0( d )  *((uint16_t*)(d)+0)
# define W1( d )  *((uint16_t*)(d)+1)
#endif


static
DFB_BFUNCTION( yuy2 )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  y = color->yuv.y;
     uint32_t  u = color->yuv.u << 8;
     uint32_t  v = color->yuv.v << 8;
     int       x = blender->x;
     int       w = blender->len;
     int       n;
    
     if (color->yuv.a < 0xff) {
          uint32_t a0 = (color->yuv.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          y *= a0;
          u *= a0;
          v *= a0;
          
          if (x & 1) {
               *D =  ((y + ((*D & 0x00ff) * a1)) >> 16) |
                    (((v + ((*D & 0xff00) * a1)) >> 16) & 0xff00);
               D++;
               w--;
          }

          for (n = w/2; n--;) {
               register uint32_t Dpix;

#ifdef WORDS_BIGENDIAN
               Dpix  = ((y + ((*(D+1) & 0x00ff) * a1)) >> 16);              // __ __ __ y1
               Dpix |= ((v + ((*(D+1) & 0xff00) * a1)) >> 16) & 0x0000ff00; // __ __ cr y1
               Dpix |= ((y + ((*(D+0) & 0x00ff) * a1))      ) & 0x00ff0000; // __ y0 cr y1
               Dpix |= ((u + ((*(D+0) & 0xff00) * a1))      ) & 0xff000000; // cb y0 cr y1
#else
               Dpix  = ((y + ((*(D+0) & 0x00ff) * a1)) >> 16);              // __ __ __ y0
               Dpix |= ((u + ((*(D+0) & 0xff00) * a1)) >> 16) & 0x0000ff00; // __ __ cb y0
               Dpix |= ((y + ((*(D+1) & 0x00ff) * a1))      ) & 0x00ff0000; // __ y1 cb y0
               Dpix |= ((v + ((*(D+1) & 0xff00) * a1))      ) & 0xff000000; // cr y1 cb y0 
#endif
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) {
               *D =  ((y + ((*D & 0x00ff) * a1)) >> 16) |
                    (((u + ((*D & 0xff00) * a1)) >> 16) & 0xff00);
          }
     }
     else {
          uint32_t Dpix = (y <<  0) | (u << (YUY2_CB_SHIFT-8)) |
                          (y << 16) | (v << (YUY2_CR_SHIFT-8));
          
          if (x & 1) {
               *D++ = (y | v);
               w--;
          }

          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = (y | u);
     }
}

static
DFB_BFUNCTION( uyvy )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  y = color->yuv.y << 8;
     uint32_t  u = color->yuv.u;
     uint32_t  v = color->yuv.v;
     int       x = blender->x;
     int       w = blender->len;
     int       n;
     
     if (color->yuv.a < 0xff) {
          uint32_t a0 = (color->yuv.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          y *= a0;
          u *= a0;
          v *= a0;
          
          if (x & 1) {
               *D = (((v + ((*D & 0x00ff) * a1)) >> 16)         ) |
                    (((y + ((*D & 0xff00) * a1)) >> 16) & 0xff00);
               D++;
               w--;
          }

          for (n = w/2; n--;) {
               register uint32_t Dpix;

#ifdef WORDS_BIGENDIAN
               Dpix  = ((v + ((*(D+1) & 0x00ff) * a1)) >> 16);              // __ __ __ cr
               Dpix |= ((y + ((*(D+1) & 0xff00) * a1)) >> 16) & 0x0000ff00; // __ __ y1 cr
               Dpix |= ((u + ((*(D+0) & 0x00ff) * a1))      ) & 0x00ff0000; // __ cb y1 cr
               Dpix |= ((y + ((*(D+0) & 0xff00) * a1))      ) & 0xff000000; // y0 cb y1 cr
#else
               Dpix  = ((u + ((*(D+0) & 0x00ff) * a1)) >> 16);              // __ __ __ cb
               Dpix |= ((y + ((*(D+0) & 0xff00) * a1)) >> 16) & 0x0000ff00; // __ __ y0 cb
               Dpix |= ((v + ((*(D+1) & 0x00ff) * a1))      ) & 0x00ff0000; // __ cr y0 cb
               Dpix |= ((y + ((*(D+1) & 0xff00) * a1))      ) & 0xff000000; // y1 cr y0 cb
#endif
               
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) { 
               *D = (((u + ((*D & 0x00ff) * a1)) >> 16)         ) |
                    (((y + ((*D & 0xff00) * a1)) >> 16) & 0xff00);
          }
     }
     else {
          uint32_t Dpix = (u << UYVY_CB_SHIFT) | (y <<  0) |
                          (v << UYVY_CR_SHIFT) | (y << 16);
          
          if (x & 1) {
               *D++ = (v | y);
               w--;
          }

          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = (u | y);
     }
}

static
DFB_BFUNCTION( yv12 )
{
     uint8_t  *Dy = blender->plane[0] + blender->x;
     uint8_t  *Du = blender->plane[1] + blender->x/2;
     uint8_t  *Dv = blender->plane[2] + blender->x/2;
     uint32_t  y = color->yuv.y;
     uint32_t  u = color->yuv.u;
     uint32_t  v = color->yuv.v;
     int       w = blender->len;
     int       n;
     
     if (color->yuv.a < 0xff) {
          uint32_t a0 = (color->yuv.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          y *= a0;

          for (n = w; n--;) {
               *Dy = (y + (*Dy * a1)) >> 16;
               Dy++;
          }

          if (blender->y & 1) {
               u *= a0;
               v *= a0;
               
               for (n = w/2; n--;) {
                    *Du = (u + (*Du * a1)) >> 16;
                    Du++;
                    *Dv = (v + (*Dv * a1)) >> 16;
                    Dv++;
               }

               if (w & 1) {
                    *Du = (u + (*Du * a1)) >> 16;
                    *Dv = (v + (*Dv * a1)) >> 16;
               }
          }
     }
     else {
          memset( Dy, y, w );
          
          if (blender->y & 1) {
               memset( Du, u, (w+1)/2 );
               memset( Dv, v, (w+1)/2 );
          }
     }    
}

static
DFB_BFUNCTION( nv12 )
{
     uint8_t  *Dy  = blender->plane[0] + blender->x;
     uint16_t *Duv = (uint16_t*) blender->plane[1] + blender->x/2;
     uint32_t  y   = color->yuv.y;
     uint32_t  u   = color->yuv.u;
     uint32_t  v   = color->yuv.v << 8;
     int       w   = blender->len;
     int       n;
     
     if (color->yuv.a < 0xff) {
          uint32_t a0 = (color->yuv.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          y *= a0;

          for (n = w; n--;) {
               *Dy = (y + (*Dy * a1)) >> 16;
               Dy++;
          }

          if (blender->y & 1) {
               u *= a0;
               v *= a0;

               if ((uint32_t)Duv & 2) { 
                    *Duv = (((u + ((*Duv & 0x00ff) * a1)) >> 16)         ) |
                           (((v + ((*Duv & 0xff00) * a1)) >> 16) & 0xff00);
                    Duv++;
                    w -= 2;
               }
               
               for (n = w/4; n--;) {
                    register uint32_t Dpix;

                    Dpix  = ((u + ((W0(Duv) & 0x00ff) * a1)) >> 16);
                    Dpix |= ((v + ((W0(Duv) & 0xff00) * a1)) >> 16) & 0x0000ff00;
                    Dpix |= ((u + ((W1(Duv) & 0x00ff) * a1))      ) & 0x00ff0000;
                    Dpix |= ((v + ((W1(Duv) & 0xff00) * a1))      ) & 0xff000000;
                    
                    *((uint32_t*)Duv) = Dpix;
                    Duv += 2;
               }

               if (w & 2) {
                    *Duv = (((u + ((*Duv & 0x00ff) * a1)) >> 16)         ) |
                           (((v + ((*Duv & 0xff00) * a1)) >> 16) & 0xff00);
               }
          }
     }
     else {
          memset( Dy, y, w );
          
          if (blender->y & 1) {
               register uint32_t Dpix = u | v | ((u|v) << 16);
               
               if ((uint32_t)Duv & 2) {
                    *Duv++ = Dpix;
                    w -= 2;
               }
               
               for (n = w/4; n--;) {
                    *((uint32_t*)Duv) = Dpix;
                    Duv += 2;
               }

               if (w & 2)
                    *Duv = Dpix;
          }
     }    
}

static
DFB_BFUNCTION( rgb332 )
{
     uint8_t  *D = blender->plane[0] + blender->x;
     uint32_t  r = (color->rgb.r & 0xe0);
     uint32_t  g = (color->rgb.g & 0xe0) >> 3;
     uint32_t  b = (color->rgb.b & 0xc0) >> 6;
     int       n = blender->len;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          r *= a0;
          g *= a0;
          b *= a0;
          
          while (n--) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x03) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x1c) * a1)) >> 16) & 0x1c;
               Dpix |= ((r + ((*D & 0xe0) * a1)) >> 16) & 0xe0;
               
               *D++ = Dpix;
          }
     } else
          memset( D, (r | g | b), n );
}

static
DFB_BFUNCTION( argb2554 )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  a = (color->rgb.a & 0xc0) << 8;
     uint32_t  r = (color->rgb.r & 0xf8) << 6;
     uint32_t  g = (color->rgb.g & 0xf8) << 1;
     uint32_t  b = (color->rgb.b & 0xf0) >> 4;
     int       w = blender->len;
     int       n;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          a *= a0;
          r *= a0;
          g *= a0;
          b *= a0;

          if ((uint32_t)D & 2) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x01f0) * a1)) >> 16) & 0x01f0;
               Dpix |= ((r + ((*D & 0x3e00) * a1)) >> 16) & 0x3e00;
               Dpix |= ((a + ((*D & 0xc000) * a1)) >> 16) & 0xc000;

               *D++ = Dpix;
               w--;
          }               
          
          for (n = w/2; n--;) {
               register uint32_t Dpix;

               Dpix  = ((b + ((W0(D) & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((W0(D) & 0x01f0) * a1)) >> 16) & 0x000001f0;
               Dpix |= ((r + ((W0(D) & 0x3e00) * a1)) >> 16) & 0x00003e00;
               Dpix |= ((a + ((W0(D) & 0xc000) * a1)) >> 16) & 0x0000c000;
               
               Dpix |= ((b + ((W1(D) & 0x000f) * a1))      ) & 0x000f0000;
               Dpix |= ((g + ((W1(D) & 0x01f0) * a1))      ) & 0x01f00000;
               Dpix |= ((r + ((W1(D) & 0x3e00) * a1))      ) & 0x3e000000;
               Dpix |= ((a + ((W1(D) & 0xc000) * a1))      ) & 0xc0000000;
               
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x01f0) * a1)) >> 16) & 0x01f0;
               Dpix |= ((r + ((*D & 0x3e00) * a1)) >> 16) & 0x3e00;
               Dpix |= ((a + ((*D & 0xc000) * a1)) >> 16) & 0xc000;

               *D = Dpix;
          }
     }
     else {
          uint32_t Dpix = (a | r | g | b);

          Dpix |= Dpix << 16;

          if ((uint32_t)D & 2) {
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = Dpix;
     }
}

static
DFB_BFUNCTION( argb4444 )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  a = (color->rgb.a & 0xf0) << 8;
     uint32_t  r = (color->rgb.r & 0xf0) << 4;
     uint32_t  g = (color->rgb.g & 0xf0);
     uint32_t  b = (color->rgb.b & 0xf0) >> 4;
     int       w = blender->len;
     int       n;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          a *= a0;
          r *= a0;
          g *= a0;
          b *= a0;

          if ((uint32_t)D & 2) { 
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x00f0) * a1)) >> 16) & 0x00f0;
               Dpix |= ((r + ((*D & 0x0f00) * a1)) >> 16) & 0x0f00;
               Dpix |= ((a + ((*D & 0xf000) * a1)) >> 16) & 0xf000;
               
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               register uint32_t Dpix;

               Dpix  = ((b + ((W0(D) & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((W0(D) & 0x00f0) * a1)) >> 16) & 0x000000f0;
               Dpix |= ((r + ((W0(D) & 0x0f00) * a1)) >> 16) & 0x00000f00;
               Dpix |= ((a + ((W0(D) & 0xf000) * a1)) >> 16) & 0x0000f000;
               
               Dpix |= ((b + ((W1(D) & 0x000f) * a1))      ) & 0x000f0000;
               Dpix |= ((g + ((W1(D) & 0x00f0) * a1))      ) & 0x00f00000;
               Dpix |= ((r + ((W1(D) & 0x0f00) * a1))      ) & 0x0f000000;
               Dpix |= ((a + ((W1(D) & 0xf000) * a1))      ) & 0xf0000000;
               
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x000f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x00f0) * a1)) >> 16) & 0x00f0;
               Dpix |= ((r + ((*D & 0x0f00) * a1)) >> 16) & 0x0f00;
               Dpix |= ((a + ((*D & 0xf000) * a1)) >> 16) & 0xf000;
               
               *D = Dpix;
          }
     }
     else {
          uint32_t Dpix = (a | r | g | b);

          Dpix |= Dpix << 16;

          if ((uint32_t)D & 2) {
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = Dpix;
     }
}

static
DFB_BFUNCTION( argb1555 )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  a = (color->rgb.a & 0x80) << 8;
     uint32_t  r = (color->rgb.r & 0xf8) << 7;
     uint32_t  g = (color->rgb.g & 0xf8) << 2;
     uint32_t  b = (color->rgb.b & 0xf8) >> 3;
     int       w = blender->len;
     int       n;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          r *= a0;
          g *= a0;
          b *= a0;

          if ((uint32_t)D & 2) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x03e0) * a1)) >> 16) & 0x03e0;
               Dpix |= ((r + ((*D & 0x7c00) * a1)) >> 16) & 0x7c00;
               Dpix |= (a > 127) ? 0x8000 : (*D & 0x8000);
               
               *D++ = Dpix;
               w--;
          }    
          
          for (n = w/2; n--;) {
               register uint32_t Dpix;

               Dpix  = ((b + ((W0(D) & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((W0(D) & 0x03e0) * a1)) >> 16) & 0x000003e0;
               Dpix |= ((r + ((W0(D) & 0x7c00) * a1)) >> 16) & 0x00007c00;
               Dpix |= ((a > 127) ? 0x00008000 : ((W0(D) & 0x8000)));

               Dpix |= ((b + ((W1(D) & 0x001f) * a1))      ) & 0x001f0000;
               Dpix |= ((g + ((W1(D) & 0x03e0) * a1))      ) & 0x03e00000;
               Dpix |= ((r + ((W1(D) & 0x7c00) * a1))      ) & 0x7c000000;
               Dpix |= ((a > 127) ? 0x80000000 : ((W1(D) & 0x8000)<<16));
               
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x03e0) * a1)) >> 16) & 0x03e0;
               Dpix |= ((r + ((*D & 0x7c00) * a1)) >> 16) & 0x7c00;
               Dpix |= ((a > 127) ? 0x8000 : (*D & 0x8000));
               
               *D = Dpix;
          }    
     }
     else {
          uint32_t Dpix = (a | r | g | b);

          Dpix |= Dpix << 16;

          if ((uint32_t)D & 2) {
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = Dpix;
     }
}

static
DFB_BFUNCTION( rgb16 )
{
     uint16_t *D = (uint16_t*) blender->plane[0] + blender->x;
     uint32_t  r = (color->rgb.r & 0xf8) << 8;
     uint32_t  g = (color->rgb.g & 0xfc) << 3;
     uint32_t  b = (color->rgb.b & 0xf8) >> 3;
     int       w = blender->len;
     int       n;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          r *= a0;
          g *= a0;
          b *= a0;

          if ((uint32_t)D & 2) { 
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x07e0) * a1)) >> 16) & 0x07e0;
               Dpix |= ((r + ((*D & 0xf800) * a1)) >> 16) & 0xf800;
               
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               register uint32_t Dpix;

               Dpix  = ((b + ((W0(D) & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((W0(D) & 0x07e0) * a1)) >> 16) & 0x000007e0;
               Dpix |= ((r + ((W0(D) & 0xf800) * a1)) >> 16) & 0x0000f800;

               Dpix |= ((b + ((W1(D) & 0x001f) * a1))      ) & 0x001f0000;
               Dpix |= ((g + ((W1(D) & 0x07e0) * a1))      ) & 0x07e00000;
               Dpix |= ((r + ((W1(D) & 0xf800) * a1))      ) & 0xf8000000;
               
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1) {
               register uint32_t Dpix;

               Dpix  = ((b + ((*D & 0x001f) * a1)) >> 16);
               Dpix |= ((g + ((*D & 0x07e0) * a1)) >> 16) & 0x07e0;
               Dpix |= ((r + ((*D & 0xf800) * a1)) >> 16) & 0xf800;
               
               *D = Dpix;
          }
     }
     else {
          uint32_t Dpix = (r | g | b);

          Dpix |= Dpix << 16;

          if ((uint32_t)D & 2) {
               *D++ = Dpix;
               w--;
          }
          
          for (n = w/2; n--;) {
               *((uint32_t*)D) = Dpix;
               D += 2;
          }

          if (w & 1)
               *D = Dpix;
     }
}

static
DFB_BFUNCTION( rgb24 )
{
     uint8_t  *D = blender->plane[0] + blender->x*3;
     uint32_t  r = color->rgb.r;
     uint32_t  g = color->rgb.g;
     uint32_t  b = color->rgb.b;
     int       n = blender->len;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          r *= a0;
          g *= a0;
          b *= a0;
          
          while (n--) {
               *(D+0) = (b + (*(D+0) * a1)) >> 16;
               *(D+1) = (g + (*(D+1) * a1)) >> 16;
               *(D+2) = (r + (*(D+2) * a1)) >> 16;
               
               D += 3;
          }
     }
     else {
          while (n--) {
               *(D+0) = b;
               *(D+1) = r;
               *(D+2) = g;
               
               D += 3;
          }
     }
}

static
DFB_BFUNCTION( rgb32 )
{
     uint32_t *D = (uint32_t*) blender->plane[0] + blender->x;
     uint32_t  r = color->rgb.r;
     uint32_t  g = color->rgb.g;
     uint32_t  b = color->rgb.b;
     int       n = blender->len;
     
     if (color->rgb.a < 0xff) {
          uint32_t a0 = (color->rgb.a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
          
          r *= a0;
          g *= a0;
          b *= a0;
          
          while (n--) {
               register uint32_t Dpix;

               Dpix  = ((b + (((*D & 0x000000ff)      ) * a1)) >> 16);
               Dpix |= ((g + (((*D & 0x0000ff00) >>  8) * a1)) >>  8) & 0x0000ff00;
               Dpix |= ((r + (((*D & 0x00ff0000) >> 16) * a1))      ) & 0x00ff0000;

               *D++ = Dpix;
          }
     }
     else {
          uint32_t Dpix = (r << 16) | (g << 8) | b;
          
          while (n--)
               *D++ = Dpix;
     }
}

static
DFB_BFUNCTION( argb )
{
     uint32_t *D = (uint32_t*) blender->plane[0] + blender->x;
     uint32_t  a = color->rgb.a;
     uint32_t  r = color->rgb.r;
     uint32_t  g = color->rgb.g;
     uint32_t  b = color->rgb.b;
     int       n = blender->len;
     
     if (a < 0xff) {
          uint32_t a0 = (a << 16) / 0xff;
          uint32_t a1 = 0x10000 - a0;
         
          a *= a0;
          r *= a0;
          g *= a0;
          b *= a0;
          
          while (n--) {
               register uint32_t Dpix;
               
               Dpix  = ((b + (((*D & 0x000000ff)      ) * a1)) >> 16);
               Dpix |= ((g + (((*D & 0x0000ff00) >>  8) * a1)) >>  8) & 0x0000ff00;
               Dpix |= ((r + (((*D & 0x00ff0000) >> 16) * a1))      ) & 0x00ff0000;
               Dpix |= ((a + (((*D & 0xff000000) >> 24) * a1)) <<  8) & 0xff000000;

               *D++ = Dpix;
          }
     }
     else {
          uint32_t Dpix = (a << 24) | (r << 16) | (g << 8) | b;

          while (n--)
               *D++ = Dpix;
     }
}


#undef PACCEL

