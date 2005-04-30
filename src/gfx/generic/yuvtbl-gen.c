/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2005  convergence GmbH.

   All rights reserved.

   Written by Claudio Ciccani <klan@users.sf.net>.

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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* YCbCr->RGB:
 * Standard ITU Rec. BT.601 derived coefficients.
 */
#define Y_FACTOR    1.16400
#define VR_FACTOR   1.59600
#define VG_FACTOR  -0.81300
#define UG_FACTOR  -0.39100
#define UB_FACTOR   2.01800

/* RGB->YCbCr:
 * ITU Rec. BT.601 defines the following formula for EyEcbEcr:
 *   
 *   Ey  = 0.299 * R + 0.587 * G + 0.114 * B
 *   Ecb = (B - Ey) * 0.564
 *   Ecr = (R - Ey) * 0.713
 *   
 * Relationship between EyEcbEcr and YCbCr:
 *
 *   Y   = Ey  * 219 / 255 + 16
 *   Cb  = Ecb * 224 / 255 + 128
 *   Cr  = Ecr * 224 / 255 + 128
 *
 * Therefore, to convert RGB to YCbCr, we compute Ey from R G B,
 * then we derive Y Cb Cr from Ey using 3 lookup tables.
 */
#define ECB_FACTOR   0.56400
#define ECR_FACTOR   0.71300


typedef struct {
     const char *type;
     const char *name;
     int         size;
     struct {
          int    min;
          int    max;
     } range;
     struct {
          int    min;
          int    max;
     } clamp;
     float       sub;
     float       mul;
     float       add;
} GenTable;
     

#define N_PER_LINE 10

static void
dump_table( GenTable *gt )
{
     int i, n;
     
     printf( "\n%s %s[%i] = {\n", gt->type, gt->name, gt->size );

     for (i = 0, n = 0; i < gt->size; i++) {
          float num;
          
          num = (i < gt->range.min)
                ? gt->range.min
                : ((i > gt->range.max) ? gt->range.max : i);
          num = ((num - gt->sub) * gt->mul) + gt->add;
          num = (num < gt->clamp.min) 
                ? gt->clamp.min
                : ((num > gt->clamp.max) ? gt->clamp.max : num);
          
          if (n == 0)
               printf( "     " );
          
          printf( "%4i", (int) num );
          
          if (i < (gt->size-1)) {
               if (++n == N_PER_LINE) {
                    printf( ",\n" );
                    n = 0;
               } else
                    printf( ", " );
          } else
               printf( "\n" );
     }

     printf( "};\n\n" );
}

int
main( int argc, char **argv )
{
     GenTable gt = {
          .size      = 256,
          .range.min = 16,
          .clamp.min = -1000,
          .clamp.max = +1000,
     };

     /* headers */
     puts( "#ifndef __YUV_TABLES_H__" );
     puts( "#define __YUV_TABLES_H__" );
     puts( "" );
     
/* YUV->RGB */
     /* luma */
     gt.type      = "const __u16";
     gt.name      = "y_for_rgb";
     gt.range.max = 235;
     gt.sub       = 16;
     gt.mul       = Y_FACTOR; 
     gt.add       = 0;
     dump_table( &gt );

     /* chroma */
     gt.type      = "const __s16";
     gt.range.max = 240;
     gt.sub       = 128;

     gt.name      = "cr_for_r";
     gt.mul       = VR_FACTOR;
     dump_table( &gt );

     gt.name      = "cr_for_g";
     gt.mul       = VG_FACTOR;
     dump_table( &gt );

     gt.name      = "cb_for_g";
     gt.mul       = UG_FACTOR;
     dump_table( &gt );

     gt.name      = "cb_for_b";
     gt.mul       = UB_FACTOR;
     dump_table( &gt );

/* RGB->YUV */
     gt.size      = 256;
     gt.range.min = 0;
     gt.range.max = 255; 
     gt.clamp.min = 16;
     gt.sub       = 0;
     
     /* luma */
     gt.type      = "const __u16";
     gt.name      = "y_from_ey";
     gt.clamp.max = 235;
     gt.mul       = 219.0/255.0;
     gt.add       = 16;
     dump_table( &gt );

     /* chroma */
     gt.size      = 512;
     gt.range.max = 511;
     gt.clamp.max = 240;
     gt.sub       = 255;
     gt.add       = 128;
     
     /* cb */
     gt.type = "static const __u16";
     gt.name = "_cb_from_bey";
     gt.mul  = ECB_FACTOR * 224.0 / 255.0;
     dump_table( &gt );

     /* cr */
     gt.type = "static const __u16";
     gt.name = "_cr_from_rey";
     gt.mul  = ECR_FACTOR * 224.0 / 255.0;
     dump_table( &gt );

     puts( "const __u16 *cb_from_bey = &_cb_from_bey[255];" );
     puts( "const __u16 *cr_from_rey = &_cr_from_rey[255];" );
     
     /* end headers */
     puts( "" );
     puts( "#endif /* __YUV_TABLES_H__ */" );

     return 0;
 }
