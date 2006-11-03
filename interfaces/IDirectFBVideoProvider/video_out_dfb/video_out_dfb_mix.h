/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
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
 
 
typedef enum {
     MF_NONE = 0x0000000,
     MF_B    = 0x00000001,  /* brightness */
     MF_C    = 0x00000002,  /* contrast   */
     MF_S    = 0x00000004,  /* saturation */
     MF_ALL  = 0x00000007
} MixerModifiedFlags;
 
 
static void
vo_dfb_update_mixing( dfb_driver_t *this, MixerModifiedFlags flags )
{
     int i;
     
     if (flags & (MF_B | MF_C)) {
          int b = this->mixer.b;
          int c = this->mixer.c;
          
          if (b == 0 && c == 128) {
               if (this->mixer.l_csc) {
                    free( this->mixer.l_csc );
                    this->mixer.l_csc = NULL;
               }
          }
          else {
               if (!this->mixer.l_csc) {
                    this->mixer.l_csc = malloc( 256 );
                    if (!this->mixer.l_csc) {
                         lprintf( "memory allocation failed!!!\n" );
                         return;
                    }
               }
          
               for (i = 0; i < 256; i++) {
                    int v = (((i - 16) * c) >> 7) + b + 16;
                    this->mixer.l_csc[i] = CLAMP( v, 0, 255 );
               }
          }
     }
     
     if (flags & MF_S) {
          int s = this->mixer.s;
          
          if (s == 128) {
               if (this->mixer.c_csc) {
                    free( this->mixer.c_csc );
                    this->mixer.c_csc = NULL;
               }
          }
          else {
               if (!this->mixer.c_csc) {
                    this->mixer.c_csc = malloc( 256 );
                    if (!this->mixer.c_csc) {
                         lprintf( "memory allocation failed!!!\n" );
                         return;
                    }
               }
          
               for (i = 0; i < 256; i++) {
                    int v = (((i - 128) * s) >> 7) + 128;
                    this->mixer.c_csc[i] = CLAMP( v, 0, 255 );
               }
          }
     }
}   
 

static void
vo_dfb_mix_yuy2( dfb_driver_t *this, dfb_frame_t *frame )
{
     uint8_t *l_csc = this->mixer.l_csc;
     uint8_t *c_csc = this->mixer.c_csc;
     uint8_t *src   = frame->vo_frame.base[0];
     int      pitch = frame->vo_frame.pitches[0];
     int      x, y;
     
     for (y = 0; y < frame->height; y++) {
          for (x = 0; x < frame->width; x++) {
               if (l_csc) {
#ifdef WORDS_BIGENDIAN
                    src[x*4+1] = l_csc[src[x*4+1]];
                    src[x*4+3] = l_csc[src[x*4+3]];
#else
                    src[x*4+0] = l_csc[src[x*4+0]];
                    src[x*4+2] = l_csc[src[x*4+2]];
#endif
               }
               if (c_csc) {
#ifdef WORDS_BIGENDIAN
                    src[x*4+0] = c_csc[src[x*4+0]];
                    src[x*4+2] = c_csc[src[x*4+2]];
#else
                    src[x*4+1] = c_csc[src[x*4+1]];
                    src[x*4+3] = c_csc[src[x*4+3]];
#endif
               }
          }
          src += pitch;
     }
}

static void
vo_dfb_mix_yv12( dfb_driver_t *this, dfb_frame_t *frame )
{
     uint8_t *l_csc = this->mixer.l_csc;
     uint8_t *c_csc = this->mixer.c_csc;
     int      x, y;
     
     if (l_csc) {
          uint8_t *srcy  = frame->vo_frame.base[0];
          int      pitch = frame->vo_frame.pitches[0];
          
          for (y = 0; y < frame->height; y++) {
               for (x = 0; x < frame->width; x++)
                    srcy[x] = l_csc[srcy[x]];
               srcy += pitch;
          }
     }
     
     if (c_csc) {
          uint8_t *srcu  = frame->vo_frame.base[1];
          uint8_t *srcv  = frame->vo_frame.base[2];
          int      pitch = frame->vo_frame.pitches[1];
          
          for (y = 0; y < frame->height/2; y++) {
               for (x = 0; x < frame->width/2; x++) {
                    srcu[x] = c_csc[srcu[x]];
                    srcv[x] = c_csc[srcv[x]];
               }
               srcu += pitch;
               srcv += pitch;
          }
     }
}
     
