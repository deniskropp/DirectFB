/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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

#include <directfb.h>

#include "misc/util.h"

#include "clip.h"

#define REGION_CODE(x,y,cx1,cx2,cy1,cy2) ( ( (y) > (cy2) ? 8 : 0) | \
                                           ( (y) < (cy1) ? 4 : 0) | \
                                           ( (x) > (cx2) ? 2 : 0) | \
                                           ( (x) < (cx1) ? 1 : 0) )


int clip_line( DFBRegion *clip, DFBRegion *line )
{
     unsigned char region_code1 = REGION_CODE( line->x1, line->y1,
                                               clip->x1,
                                               clip->x2,
                                               clip->y1,
                                               clip->y2 );

     unsigned char region_code2 = REGION_CODE( line->x2, line->y2,
                                               clip->x1,
                                               clip->x2,
                                               clip->y1,
                                               clip->y2 );

     while (region_code1 | region_code2) {
          if (region_code1 & region_code2)

               return 0;  /* line completely outside the clipping rectangle */


          if (region_code1) {
               if (region_code1 & 8) { /* divide line at bottom*/
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y2;
               }
               else
                    if (region_code1 & 4) { /* divide line at top*/
                    line->x1 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y1 = clip->y1;
               }
               else
                    if (region_code1 & 2) { /* divide line at right*/
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x2;
               }
               else
                    if (region_code1 & 1) { /* divide line at right*/
                    line->y1 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x1 = clip->x1;
               }
               region_code1 = REGION_CODE( line->x1, line->y1,
                                           clip->x1,
                                           clip->x2,
                                           clip->y1,
                                           clip->y2 );
          }
          else {
               if (region_code2 & 8) {  /* divide line at bottom*/
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y2 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y2;
               }
               else
                    if (region_code2 & 4) { /* divide line at top*/
                    line->x2 = line->x1 +(line->x2-line->x1) * (clip->y1 - line->y1) / (line->y2-line->y1);
                    line->y2 = clip->y1;
               }
               else
                    if (region_code2 & 2) { /* divide line at right*/
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x2 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x2;
               }
               else
                    if (region_code2 & 1) { /* divide line at right*/
                    line->y2 = line->y1 +(line->y2-line->y1) * (clip->x1 - line->x1) / (line->x2-line->x1);
                    line->x2 = clip->x1;
               }
               region_code2 = REGION_CODE( line->x2, line->y2, clip->x1,
                                                     clip->x2,
                                                     clip->y1,
                                                     clip->y2 );
          }
     }

     return 1; /* successfully clipped or clipping not neccessary */
}

unsigned int clip_rectangle( DFBRegion    *clip,
                             DFBRectangle *rect )
{
     unsigned int result = 0x1F;  /* returns bit flags for clipped edges  */

     if ((clip->x1 >= rect->x + rect->w) ||
         (clip->x2 < rect->x) ||
         (clip->y1 >= rect->y + rect->h) ||
         (clip->y2 < rect->y)) {
          return 0;
     }

     if (clip->x1 > rect->x) {
          rect->w += rect->x - clip->x1;
          rect->x = clip->x1;
          result &= ~1;
     }

     if (clip->y1 > rect->y) {
          rect->h += rect->y - clip->y1;
          rect->y = clip->y1;
          result &= ~2;
     }

     if (clip->x2 < rect->x + rect->w - 1) {
          rect->w = clip->x2 - rect->x + 1;
          result &= ~4;
     }

     if (clip->y2 < rect->y + rect->h - 1) {
          rect->h = clip->y2 - rect->y + 1;
          result &= ~8;
     }

     return result;
}

int clip_triangle_precheck( DFBRegion   *clip,
                            DFBTriangle *tri )
{
    int x, y, w, h;
  
    x = MIN (MIN (tri->x1, tri->x2), tri->x3);
    y = MIN (MIN (tri->y1, tri->y2), tri->y3);
    w = MAX (MAX (tri->x1, tri->x2), tri->x3) - x;
    h = MAX (MAX (tri->y1, tri->y2), tri->y3) - y;
    
    if (clip->x1 > x ||
        clip->x2 < x + w ||
        clip->y1 > y ||
        clip->y2 < y + h)
      return 0;
    
    return 1;
}

int clip_blit_precheck( DFBRegion *clip,
                        int w, int h, int dx, int dy )
{
     if (w < 1 || h < 1 ||
         (clip->x1 >= dx + w) ||
         (clip->x2 < dx) ||
         (clip->y1 >= dy + h) ||
         (clip->y2 < dy))
     {
          return 0;
     }

     return 1;
}

void clip_blit( DFBRegion *clip, DFBRectangle *srect,
                int *dx, int *dy )
{
     if (clip->x1 > *dx ) {
          srect->w = MIN( (clip->x2 - clip->x1) + 1,
                    (*dx + srect->w) - clip->x1);

          srect->x+= clip->x1 - *dx;
          *dx = clip->x1;
     }
     else if (clip->x2 < *dx + srect->w - 1) {
          srect->w = clip->x2 - *dx + 1;
     }

     if (clip->y1 > *dy ) {
          srect->h = MIN( (clip->y2 - clip->y1) + 1,
                          (*dy + srect->h) - clip->y1);
          srect->y+= clip->y1 - *dy;
          *dy = clip->y1;
     }
     else if (clip->y2 < *dy + srect->h - 1) {
          srect->h = clip->y2 - *dy + 1;
     }
}

void clip_stretchblit( DFBRegion *clip,
                       DFBRectangle *srect, DFBRectangle *drect )
{
     DFBRectangle orig_dst = *drect;

     clip_rectangle( clip, drect );

     if (drect->x != orig_dst.x)
          srect->x += (int)( (drect->x - orig_dst.x) *
                             (srect->w / (float)orig_dst.w) + 0.5f );

     if (drect->y != orig_dst.y)
          srect->y += (int)( (drect->y - orig_dst.y) *
                             (srect->h / (float)orig_dst.h) + 0.5f );
     
     if (drect->w != orig_dst.w)
          srect->w = ICEIL(srect->w * (drect->w / (float)orig_dst.w));

     if (drect->h != orig_dst.h)
          srect->h = ICEIL(srect->h * (drect->h / (float)orig_dst.h));
}

