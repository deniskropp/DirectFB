#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define Y_ADD      -16.0
#define UV_ADD     -128.0
#define Y_FACTOR    1.16400
#define VR_FACTOR   1.59600
#define VG_FACTOR  -0.81300
#define UG_FACTOR  -0.39100
#define UB_FACTOR   2.01800


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
     float       add;
     float       mul;
     float       scale;
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
          num = (num + gt->add) * gt->mul / gt->scale;
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
          .scale     = 1.0
     };

     /* headers */
     puts( "#ifndef __YUV_TABLES_H__" );
     puts( "#define __YUV_TABLES_H__" );
     puts( "" );
     
     /* luma */
     gt.type      = "static const unsigned int";
     gt.name      = "y_for_rgb";
     gt.range.max = 235;
     gt.add       = Y_ADD;
     gt.mul       = Y_FACTOR;
     dump_table( &gt );

     /* chroma */
     gt.type      = "static const int";
     gt.range.max = 240;
     gt.add       = UV_ADD;

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

     /* end headers */
     puts( "" );
     puts( "#endif /* __YUV_TABLES_H__ */" );

     return 0;
 }
