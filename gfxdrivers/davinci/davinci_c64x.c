/*
   TI Davinci driver - C64X+ DSP Library

   (c) Copyright 2007  Telio AG

   Written by Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

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

//#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <asm/types.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include <direct/clock.h>
#include <direct/debug.h>
#include <direct/messages.h>
#include <direct/util.h>

#include "davinci_c64x.h"

/**********************************************************************************************************************/

#define C64X_DEVICE "/dev/c64x"
#define C64X_QLEN   direct_page_align( sizeof(c64xTaskControl) )
#define C64X_MLEN   direct_page_align( 0x2000000 )

/**********************************************************************************************************************/
/*   Benchmarking or Testing                                                                                       */
/**********************************************************************************************************************/

static void
bench_mem( const char *name,
           void       *ptr,
           int         length,
           bool        copy,
           bool        from )
{
     int       i, num;
     long long t1, t2, dt, total;
     char      buf[0x100];

     if (length > sizeof(buf))
          length = sizeof(buf);

     num = 0x2000000 / length;

     t1 = direct_clock_get_abs_micros();

     if (copy) {
          if (from)
               for (i=0; i<num; i++)
                    memcpy( buf, ptr, length );
          else
               for (i=0; i<num; i++)
                    memcpy( ptr, buf, length );
     }
     else
          for (i=0; i<num; i++)
               memset( ptr, 0, length );

     t2 = direct_clock_get_abs_micros();

     dt    = t2 - t1;
     total = i * length;

     D_INFO( "Davinci/C64X: MEMORY BENCHMARK on %-7s - %-15s   %lld.%03lld MB/sec\n",
             name, copy ? from ? "memcpy() from" : "memcpy() to" : "memset()",
             total / dt, (total * 1000 / dt) % 1000 );
}

#define DVA_BLOCK_WORD( val, index, EOB )    (((val) << 16) | (((index)&0x3f) << 1) | ((EOB) ? 1 : 0))

static inline void
bench_load_block( DavinciC64x *c64x )
{
     int       i, num;
     long long t1, t2, dt, total;
     //int       length = 0x10000;

     num = 0x10000;// / length;

     short *dst = c64x->mem + 0x01000000;
     u8    *duv = c64x->mem + 0x01100000;
     u8    *duy = c64x->mem + 0x01300000;
     int   *src = c64x->mem + 0x01200000;

     src[0] = DVA_BLOCK_WORD( 100, 0, 1 );
     src[1] = DVA_BLOCK_WORD( 200, 0, 0 );
     src[2] = DVA_BLOCK_WORD( 210, 1, 0 );
     src[3] = DVA_BLOCK_WORD( 220, 2, 1 );
     src[4] = DVA_BLOCK_WORD( 300, 0, 1 );
     src[5] = DVA_BLOCK_WORD( 400, 0, 0 );
     src[6] = DVA_BLOCK_WORD( 410, 1, 1 );
     src[7] = DVA_BLOCK_WORD( 500, 0, 0 );
     src[8] = DVA_BLOCK_WORD( 510, 63, 1 );
     src[9] = DVA_BLOCK_WORD( 600, 63, 1 );

     printf("\n");
     printf("\n");

     memset( dst, 0x55, 0x100000 );

     for (i=0; i<10; i++) {
          printf("0x%08x  (%d, %d, %d)\n", (u32)src[i], src[i] >> 16, (src[i] >> 1) & 0x3f, src[i] & 1);
     }

     printf("\n");

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_LOAD_BLOCK | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x1200000;
          task->c64x_arg[1] = 10;
          task->c64x_arg[2] = 0x3f;

          c64x_submit_task( c64x );
     }

     davinci_c64x_blit_16( c64x, 0x8f000000, 0, 0xf06180, 0, 384, 1 );
     davinci_c64x_blit_16( c64x, 0x8f100000, 0, 0xf06480, 0, 384/2, 1 );

     davinci_c64x_put_uyvy_16x16( c64x, 0x8f300000, 32, 0xf06160, 0 );

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     for (i=0; i<384; i++) {
          printf("%5d ", dst[i] );
          if (i%8==7) {
               printf("\n");
          }
          if (i%64==63) {
               printf("\n");
          }
     }

     printf("\n\n");


     for (i=0; i<384; i++) {
          printf("%3d ", duv[i] );
          if (i%8==7) {
               printf("\n");
          }
          if (i%64==63) {
               printf("\n");
          }
     }

     printf("\n\n");

     for (i=0; i<16*16*2; i++) {
          printf("%02x ", duy[i]);

          if (i%32==31) {
               printf("\n");
          }
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;// * length;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "block_load()", total * 1000000ULL / dt );
}

static inline void
bench_dezigzag( DavinciC64x *c64x )
{
     int       i, num;
     long long t1, t2, dt, total;
     //int       length = 0x10000;

     num = 0x200000;// / length;

     short *p = c64x->mem + 0x1000000;

     for (i=0; i<64; i++) {
          p[i] = i;
          printf("%3d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_DEZIGZAG | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8f000000+0x200000;
          task->c64x_arg[1] = 0x8f000000+0x000000;
          //task->c64x_arg[2] = length/4;

          c64x_submit_task( c64x );
     }

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     p = c64x->mem + 0x1200000;
     for (i=0; i<64; i++) {
          printf("%3d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     dt    = t2 - t1;
     total = num;// * length;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "de_zigzag()", total * 1000000ULL / dt );
}

#define DUMP_PIXELS 1

static inline void
bench_blend_argb( DavinciC64x *c64x, int sub )
{
     int       i, num;
     long long t1, t2, dt, total;

     num = 1;//0x20000;

     u32 *src = c64x->mem + 0x1000000;
     u32 *dst = c64x->mem + 0x1200000;

     printf( "\nTESTING BLEND_32 SUB %d\n", sub );

     printf( "\nSOURCE               " );

     for (i=0; i<DUMP_PIXELS; i++) {
          src[i] = (i << 26) | ((i & 0x30) << 20) | (i * 0x010204 + 3);

          if (!i)
               src[i] = 0xc0c08001;

          printf("%02x %02x %02x %02x  ", src[i] >> 24, (src[i] >> 16) & 0xff, (src[i] >> 8) & 0xff, src[i] & 0xff);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf( "\nDESTINATION          " );

     for (i=0; i<DUMP_PIXELS; i++) {
          dst[i] = i * 0x04040404;

          if (!i)
               dst[i] = 0xe0e0e0e0;

          printf("%02x %02x %02x %02x  ", dst[i] >> 24, (dst[i] >> 16) & 0xff, (dst[i] >> 8) & 0xff, dst[i] & 0xff);
          if (i%8==7) {
               printf("\n");
          }
     }

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_BLEND_32 | C64X_FLAG_TODO | (sub << 16);

          task->c64x_arg[0] = 0x8f000000+0x200000;
          task->c64x_arg[1] = 32;
          task->c64x_arg[2] = 0x8f000000+0x000000;
          task->c64x_arg[3] = 32;
          task->c64x_arg[4] = 8;
          task->c64x_arg[5] = 8;
          task->c64x_arg[6] = 0x80;

          c64x_submit_task( c64x );
     }

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     printf( "\n\nDESTINATION (AFTER)  " );

     for (i=0; i<DUMP_PIXELS; i++) {
          printf("%02x %02x %02x %02x  ", dst[i] >> 24, (dst[i] >> 16) & 0xff, (dst[i] >> 8) & 0xff, dst[i] & 0xff);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "blend_32(8x8)", total * 1000000ULL / dt );
}

static inline void
bench_fetch_uyvy( DavinciC64x *c64x, bool interleave, int xoff, int yoff )
{
     int       i, x, y, num;
     long long t1, t2, dt, total;

     num = 1;//0x20000;

     u8 *yuv = c64x->mem + 0x1000000;
     u8 *src = c64x->mem + 0x1200000;

     printf("\n");
     printf("\n\n.======================== Testing fetch_uyvy (inter %d, xoff %d, yoff %d) ========================.\n",
            interleave, xoff, yoff);
     printf("\n");
     printf( "SOURCE (20x20)\n" );

     for (y=0; y<20; y++) {
          for (x=0; x<40; x++) {
               int val = (x & 1) ? (x * 4 + y*0x10) : (x/4 + 0x40 + (x&2) * 0x10 + y*0x08);

               src[y*1440 + x] = val;

               printf("%02x ", val&0xff);
          }

          printf("\n");
     }

     printf("\n");

     memset( yuv, 0x55, 0x100000 );


     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = (19 << 2) | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8f000000+0x000000;
          task->c64x_arg[1] = 0x8f000000+0x200000 + yoff*1440 + xoff * 2;
          task->c64x_arg[2] = 1440;
          task->c64x_arg[3] = 16;
          task->c64x_arg[4] = interleave ? 1 : 0;

          c64x_submit_task( c64x );
     }

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     printf( "\n\nDESTINATION (17x18 / [9x9 9x9])\n" );

     for (y=0; y<18; y++) {
          for (x=0; x<17; x++) {
               printf("%02x ", yuv[y*32 + x]);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<9; y++) {
          for (x=0; x<9; x++) {
               printf("%02x ", yuv[y*32 + x + 32*18]);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<9; y++) {
          for (x=0; x<9; x++) {
               printf("%02x ", yuv[y*32 + x + 32*18+16]);
          }

          printf("\n");
     }

     printf("\n");

     printf("\n\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "blend_fetch_uyvy(16x16)", total * 1000000ULL / dt );
}

static inline void
bench_put_mc( DavinciC64x *c64x, bool interleave )
{
     int       x, y, i, num;
     long long t1, t2, dt, total;

     num = 1;//720/16*576/16;

     u8 *dst = c64x->mem + 0x1000000;
     u8 *src = c64x->mem + 0x1200000;

     printf("\n");
     printf("\n\n.======================== Testing put_mc (%d) ========================.\n", interleave);
     printf("\n");
     printf("SOURCE (16x16 / [8x8 8x8]\n");

     for (y=0; y<16; y++) {
          for (x=0; x<16; x++) {
               u8 val = (x << 4) + y;
               src[y*16 + x] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16 + 8] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     memset( dst, 0x55, 0x100000 );

     davinci_c64x_blit_32( c64x, C64X_MC_BUFFER_Y, 16, 0x8e000000+0x1200000, 16, 4, 24 );
     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_PUT_MC_UYVY_16x16 | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x1000000;
          task->c64x_arg[1] = 1440;
          task->c64x_arg[2] = interleave ? 1 : 0;

          c64x_submit_task( c64x );
     }

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     printf("\n");
     printf("DESTINATION (16x16 UYVY)\n");

     for (y=0; y<16; y++) {
          for (x=0; x<32; x++)
               printf("%02x ", dst[y*1440 + x]);

          printf("\n");
     }

     printf("\n\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "put_mc_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_put_sum( DavinciC64x *c64x, bool interleave )
{
     int       x, y, i, num;
     long long t1, t2, dt, total;

     num = 1;//720/16*576/16;

     u8  *dst   = c64x->mem + 0x1000000;
     u8  *src   = c64x->mem + 0x1200000;
     u32 *words = c64x->mem + 0x1100000;

     printf("\n");
     printf("\n\n.======================== Testing put_sum (%d) ========================.\n", interleave);
     printf("\n");
     printf("WORDS (6x IDCT with one value)\n");

     words[0] = DVA_BLOCK_WORD(   0, 0, 1 );
     words[1] = DVA_BLOCK_WORD(  50, 0, 1 );
     words[2] = DVA_BLOCK_WORD( 100, 0, 1 );
     words[3] = DVA_BLOCK_WORD( 150, 0, 1 );
     words[4] = DVA_BLOCK_WORD( 200, 0, 1 );
     words[5] = DVA_BLOCK_WORD( 250, 0, 1 );

     printf("\n");
     printf("\n");

     memset( dst, 0x55, 0x100000 );

     for (i=0; i<6; i++) {
          printf("0x%08x  (%d, %d, %d)\n", (u32)words[i], words[i] >> 16, (words[i] >> 1) & 0x3f, words[i] & 1);
     }

     {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_LOAD_BLOCK | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x1100000;
          task->c64x_arg[1] = 6;
          task->c64x_arg[2] = 0x3f;

          c64x_submit_task( c64x );
     }

     printf("\n");
     printf("SOURCE (16x16 / [8x8 8x8]\n");

     for (y=0; y<16; y++) {
          for (x=0; x<16; x++) {
               u8 val = (x << 4) + y;
               src[y*16 + x] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16 + 8] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     memset( dst, 0x55, 0x100000 );

     davinci_c64x_blit_32( c64x, C64X_MC_BUFFER_Y, 16, 0x8e000000+0x1200000, 16, 4, 24 );
     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_PUT_SUM_UYVY_16x16 | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x1000000;
          task->c64x_arg[1] = 1440;
          task->c64x_arg[2] = interleave ? 1 : 0;

          c64x_submit_task( c64x );
     }

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     printf("\n");
     printf("DESTINATION (16x16 UYVY)\n");

     for (y=0; y<16; y++) {
          for (x=0; x<32; x++)
               printf("%02x ", dst[y*1440 + x]);

          printf("\n");
     }

     printf("\n\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "put_sum_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_sat_mc( DavinciC64x *c64x )
{
     int       x, y, i, num;
     long long t1, t2, dt, total;

     num = 1;//720/16*576/16;

     u8 *dst = c64x->mem + 0x1000000;
     u8 *src = c64x->mem + 0x1200000;

     printf("\n");
     printf("\n\n.======================== Testing sat_mc ========================.\n");
     printf("\n");
     printf("SOURCE (16x16 / [8x8 8x8]\n");

     for (y=0; y<16; y++) {
          for (x=0; x<16; x++) {
               u8 val = (x << 4) + y;
               src[y*16 + x] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++) {
               u8 val = (x << 4) + y*2;
               src[y*16 + x + 16*16 + 8] = val;
               printf("%02x ", val);
          }

          printf("\n");
     }

     printf("\n");

     memset( dst, 0x55, 0x100000 );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = (57 << 2) | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x1000000;
          task->c64x_arg[1] = 0x8e000000+0x1200000;
          task->c64x_arg[2] = 16;

          c64x_submit_task( c64x );
     }

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     printf("\n");
     printf("DESTINATION (16x16 / [8x8 8x8]\n");

     for (y=0; y<16; y++) {
          for (x=0; x<16; x++)
               printf("%02x ", dst[y*16 + x]);

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++)
               printf("%02x ", dst[y*16 + x + 16*16]);

          printf("\n");
     }

     printf("\n");

     for (y=0; y<8; y++) {
          for (x=0; x<8; x++)
               printf("%02x ", dst[y*16 + x + 16*16 + 8]);

          printf("\n");
     }

     printf("\n\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "sat_mc_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_uyvy_1( DavinciC64x *c64x )
{
     int       i, num;
     long long t1, t2, dt, total;

     num = 720/16*576/16;

     u8 *u = c64x->mem + 0x1200000;
     u8 *p = c64x->mem + 0x1000000;

     printf("\n");
     printf("\n");

     for (i=0; i<256; i++) {
          p[i] = i - 128;
          printf("Y%-3d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[256+i] = i-32;
          printf("U%-3d ", p[256+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[320+i] = i-32;
          printf("V%-3d ", p[320+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     for (i=0; i<384; i++) {
          printf("%4d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     memset( u, 0x55, 720*576*2 );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_PUT_UYVY_16x16 | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8f000000+0x200000+i*16*16*2;
          task->c64x_arg[1] = 720 * 2;
          task->c64x_arg[2] = 0x8f000000;
          task->c64x_arg[3] = 0;

          c64x_submit_task( c64x );
     }

     printf("\n");

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     for (i=0; i<16*16*2; i++) {
          printf("%02x ", u[i/32*720*2 + i%32]);

          if (i%32==31) {
               printf("\n");
          }
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "put_uyvy_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_uyvy_2( DavinciC64x *c64x )
{
     int       i, num;
     long long t1, t2, dt, total;

     num = 0x10000;//720/16*576/16;

     u8 *u = c64x->mem + 0x0200000;
     u8 *p = c64x->mem + 0x0000000;

     printf("\n");
     printf("\n");

     for (i=0; i<256; i++) {
          p[i] = i/8;
          printf("Y%-3d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[256+i] = i/8 + 128;
          printf("U%-3d ", p[256+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[320+i] = i/8 + 240;
          printf("V%-3d ", p[320+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     for (i=0; i<384; i++) {
          printf("%4d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     memset( u, 0x55, 720*576*2 );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_PUT_UYVY_16x16 | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000+0x200000;//+i*16*16*2;
          task->c64x_arg[1] = 720 * 2;
          task->c64x_arg[2] = 0x8e000000;
          task->c64x_arg[3] = 0;

          c64x_submit_task( c64x );
     }

     printf("\n");

     davinci_c64x_write_back_all( c64x );
     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     for (i=0; i<16*16*2; i++) {
          printf("%02x ", u[i/32*720*2 + i%32]);

          if (i%32==31) {
               printf("\n");
          }
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "put_uyvy_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_uyvy_3( DavinciC64x *c64x )
{
     int       i, num;
     long long t1, t2, dt, total;

     num = 720/16*576/16;

     u8 *u = c64x->mem + 0x1200000;
     u8 *p = c64x->mem + 0x1000000;

     printf("\n");
     printf("\n");

     for (i=0; i<256; i++) {
          p[i] = i%8;
          printf("Y%-3d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[256+i] = i%8 + 128;
          printf("U%-3d ", p[256+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     for (i=0; i<64; i++) {
          p[320+i] = i%8 + 240;
          printf("V%-3d ", p[320+i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     for (i=0; i<384; i++) {
          printf("%4d ", p[i]);
          if (i%8==7) {
               printf("\n");
          }
     }

     printf("\n");

     memset( u, 0x55, 720*576*2 );

     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_PUT_UYVY_16x16 | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8f000000+0x200000+i*16*16*2;
          task->c64x_arg[1] = 720 * 2;
          task->c64x_arg[2] = 0x8f000000;
          task->c64x_arg[3] = 0;

          c64x_submit_task( c64x );
     }

     printf("\n");

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     for (i=0; i<16*16*2; i++) {
          printf("%02x ", u[i/32*720*2 + i%32]);

          if (i%32==31) {
               printf("\n");
          }
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     D_INFO( "Davinci/C64X: BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "put_uyvy_16x16()", total * 1000000ULL / dt );
}

static inline void
bench_mc( DavinciC64x *c64x, int func, int w, int h, bool avg, const char *name )
{
     int       i, x, y, num;
     long long t1, t2, dt, total;

     num = 0x1;//0000;

     u8 *dst = c64x->mem + 0x1200000;
     u8 *dsr = c64x->mem + 0x1100000;
     u8 *src = c64x->mem + 0x1000000;

     printf("\n\n.============ Testing %s ============.\n", name);
     printf("\n");
     printf("SRC REF\n");

     for (y=0; y<h+1; y++) {
          for (x=0; x<w+1; x++) {
               src[x+y*32] = x*y;
               printf("%-3d ", src[x+y*32]);
          }
          printf("\n");
     }

     printf("\n");

     printf("DST REF\n");

     for (y=0; y<h; y++) {
          for (x=0; x<w; x++) {
               dsr[x+y*32] = w*h-1-x*y;
               printf("%-3d ", dsr[x+y*32]);
          }
          printf("\n");
     }

     printf("\n");


     for (i=0; i<0x100000; i++) {
          dst[i] = i;
     }


     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = (func << 2) | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000 + 0x01200000;
          task->c64x_arg[1] = 32;
          task->c64x_arg[2] = 0x8e000000 + 0x01000000;
          task->c64x_arg[3] = 0x8e000000 + 0x01100000;
          task->c64x_arg[4] = 32;
          task->c64x_arg[5] = h;

          c64x_submit_task( c64x );
     }

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     printf("-> DST\n");

     for (y=0; y<h; y++) {
          for (x=0; x<w; x++) {
               printf("%-3d ", dst[x+y*32]);
          }
          printf("\n");
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     printf( "BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             name, total * 1000000ULL / dt );
}

static inline void
bench_div( DavinciC64x *c64x, u32 nom, u32 den )
{
     c64xTask *task = c64x_get_task( c64x );

     printf("\n\n.============ Testing div ============.\n");
     printf("\n");

     task->c64x_function = (63 << 2) | C64X_FLAG_TODO;

     task->c64x_arg[0] = nom;
     task->c64x_arg[1] = den;

     c64x_submit_task( c64x );

     davinci_c64x_wait_low( c64x );

     printf("%x / %x = %x\n\n\n", nom, den, task->c64x_return);
}

static inline void
bench_dither_argb( DavinciC64x *c64x )
{
     int       i, x, y, num, w = 8, h = 17;
     long long t1, t2, dt, total;

     num = 0x10000;

     u16 *dr  = c64x->mem + 0x1200000;
     u8  *da  = c64x->mem + 0x1100000;
     u32 *src = c64x->mem + 0x1000000;

     printf("\n\n.======================== Testing dither_argb ========================.\n");
     printf("\n");
     printf("SOURCE ARGB\n");

     for (y=0; y<h-1; y++) {
          for (x=0; x<w; x++) {
               src[x+y*32] = 0x10101010 * y + 0x888888 * x;
               printf("%08x ", src[x+y*32]);
          }
          printf("\n");
     }
     for (x=0; x<w; x++) {
          src[x+(h-1)*32] = 0xffffffff;
          printf("%08x ", src[x+y*32]);
     }

     printf("\n");
     printf("\n");

     memset( dr, 0x55, 0x100000 );
     memset( da, 0x55, 0x100000 );


     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_DITHER_ARGB | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000 + 0x01200000;
          task->c64x_arg[1] = 0x8e000000 + 0x01100000;
          task->c64x_arg[2] = 64;
          task->c64x_arg[3] = 0x8e000000 + 0x01000000;
          task->c64x_arg[4] = 128;
          task->c64x_arg[5] = w;
          task->c64x_arg[6] = h;

          c64x_submit_task( c64x );
     }

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();

     printf("-> DST RGB\n");

     for (y=0; y<h; y++) {
          for (x=0; x<w; x++) {
               printf("    %04x ", dr[x+y*32]);
          }
          printf("\n");
     }

     printf("\n");

     printf("-> DST ALPHA\n");

     for (y=0; y<h; y++) {
          for (x=0; x<w; x++) {
               if (x&1)
                    printf(" %x       ", da[x/2+y*64] & 0xF);
               else
                    printf(" %x       ", da[x/2+y*64] >> 4);
          }
          printf("\n");
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     printf( "BENCHMARK on DSP - %-15s   %lld Calls/sec\n",
             "dither_argb", total * 1000000ULL / dt );
}

static inline void
bench_stretch_32_up( DavinciC64x *c64x )
{
     int       i, x, y, num;
     long long t1, t2, dt, total;

     num = 1;//0x10000;

     u32 *dst = c64x->mem + 0x1200000;
     u32 *src = c64x->mem + 0x1000000;

     printf("\n\n.======================== Testing stretch_32_up ========================.\n");
     printf("\n");
     printf("SOURCE ARGB (10x5)\n");

     for (y=0; y<5; y++) {
          for (x=0; x<10; x++) {
               src[x+y*64/4] = 0x10010203 * x + 0x04202020 * (y + 1);
               printf("%08x ", src[x+y*64/4]);
          }
          printf("\n");
     }

     printf("\n");
     printf("\n");

     memset( dst, 0x55, 0x100000 );


     t1 = direct_clock_get_abs_micros();

     for (i=0; i<num; i++) {
          c64xTask *task = c64x_get_task( c64x );

          task->c64x_function = C64X_STRETCH_32_up | C64X_FLAG_TODO;

          task->c64x_arg[0] = 0x8e000000 + 0x1200000;
          task->c64x_arg[1] = 0x8e000000 + 0x1000000;
          task->c64x_arg[2] = 128      | (64       << 16);
          task->c64x_arg[3] = 20       | (6        << 16);
          task->c64x_arg[4] = 10       | (5        << 16);
          task->c64x_arg[5] = 19       | (5        << 16);
          task->c64x_arg[6] = 0        | (0        << 16);

          c64x_submit_task( c64x );
     }

     davinci_c64x_write_back_all( c64x );

     davinci_c64x_wait_low( c64x );

     t2 = direct_clock_get_abs_micros();


     printf("-> DESTINATION (20x6)\n");

     for (y=0; y<6; y++) {
          for (x=0; x<20; x++) {
               printf("%08x ", dst[x+y*128/4]);
          }
          printf("\n");
     }

     printf("\n");

     dt    = t2 - t1;
     total = num;

     printf( "BENCHMARK on DSP - stretch_32_up   %lld Calls/sec\n", total * 1000000ULL / dt );
}

static inline void
run_benchmarks( const char *name,
                void       *ptr,
                int         length )
{
     bench_mem( name, ptr, length, false, false );
     bench_mem( name, ptr, length, true,  false );
     bench_mem( name, ptr, length, true,  true  );
}

/**********************************************************************************************************************/
/*   Public Functions                                                                                                 */
/**********************************************************************************************************************/

DFBResult
davinci_c64x_open( DavinciC64x *c64x )
{
     DFBResult  ret;
     int        fd;
     void      *map_m;
     void      *map_q = NULL;

     mknod( C64X_DEVICE, 0666 | S_IFCHR, makedev( 400, 0 ) );

     fd = open( C64X_DEVICE, O_RDWR );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Davinci/C64X: Opening '%s' failed!\n", C64X_DEVICE );
          return ret;
     }

     map_q = mmap( NULL, C64X_QLEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0 );
     if (map_q == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "Davinci/C64X: Mapping %lu bytes at %lu via '%s' failed!\n", C64X_QLEN, 0UL, C64X_DEVICE );
          goto error;
     }

//     run_benchmarks( "Queue", map_q, C64X_QLEN );

     map_m = mmap( NULL, C64X_MLEN, PROT_READ | PROT_WRITE, MAP_SHARED, fd, C64X_QLEN );
     if (map_m == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "Davinci/C64X: Mapping %lu bytes at %lu via '%s' failed!\n", C64X_MLEN, C64X_QLEN, C64X_DEVICE );
          goto error;
     }

//     run_benchmarks( "Memory", map_m, C64X_MLEN );

     c64x->fd  = fd;
     c64x->ctl = map_q;
     c64x->mem = map_m;
     c64x->QueueL = map_m + (0x8fe00000 - 0x8e000000);

     D_INFO( "Davinci/C64X: Low ARM %d / DSP %d, High ARM %d / DSP %d\n",
             c64x->ctl->QL_arm, c64x->ctl->QL_dsp, c64x->ctl->QH_arm, c64x->ctl->QH_dsp );

     D_MAGIC_SET( c64x, DavinciC64x );

if (getenv("C64X_TEST")) {
//     bench_dsp( c64x );
     bench_load_block( c64x );
//     bench_uyvy_1( c64x );
//     bench_uyvy_2( c64x );
//     bench_uyvy_3( c64x );
//     bench_blend_argb( c64x, 0 );
//     bench_blend_argb( c64x, 1 );
//     bench_blend_argb( c64x, 2 );
//     bench_blend_argb( c64x, 3 );

//     bench_dither_argb( c64x );
     bench_stretch_32_up( c64x );

#if 0
     bench_fetch_uyvy( c64x, false, 0, 0 );
     bench_fetch_uyvy( c64x, false, 1, 0 );
     bench_fetch_uyvy( c64x, false, 0, 1 );
     bench_fetch_uyvy( c64x, false, 1, 1 );
     bench_fetch_uyvy( c64x, true, 0, 0 );
     bench_fetch_uyvy( c64x, true, 1, 0 );
     bench_fetch_uyvy( c64x, true, 0, 1 );
     bench_fetch_uyvy( c64x, true, 1, 1 );
#endif

//     bench_put_mc( c64x, false );
//     bench_put_mc( c64x, true );

     bench_put_sum( c64x, false );
     bench_put_sum( c64x, true );

//     bench_sat_mc( c64x );

#if 0
     bench_mc( c64x, 32, 8, 8, false, "mc_put_o_8" );
     bench_mc( c64x, 33, 8, 8, false, "mc_put_x_8" );
     bench_mc( c64x, 34, 8, 8, false, "mc_put_y_8" );
     bench_mc( c64x, 35, 8, 8, false, "mc_put_xy_8" );
     bench_mc( c64x, 36, 16, 16, false, "mc_put_o_16" );
     bench_mc( c64x, 37, 16, 16, false, "mc_put_x_16" );
     bench_mc( c64x, 38, 16, 16, false, "mc_put_y_16" );
     bench_mc( c64x, 39, 16, 16, false, "mc_put_xy_16" );
#endif

#if 0
     bench_mc( c64x, 40, 8, 8, true, "mc_avg_o_8" );
     bench_mc( c64x, 41, 8, 8, true, "mc_avg_x_8" );
     bench_mc( c64x, 42, 8, 8, true, "mc_avg_y_8" );
     bench_mc( c64x, 43, 8, 8, true, "mc_avg_xy_8" );
     bench_mc( c64x, 44, 16, 16, true, "mc_avg_o_16" );
     bench_mc( c64x, 45, 16, 16, true, "mc_avg_x_16" );
     bench_mc( c64x, 46, 16, 16, true, "mc_avg_y_16" );
     bench_mc( c64x, 47, 16, 16, true, "mc_avg_xy_16" );
#endif

#if 0
     bench_div( c64x, 1, 3 );
     bench_div( c64x, 1000, 333 );
     bench_div( c64x, 1000, 334 );
     bench_div( c64x, 6666, 2222 );
     bench_div( c64x, 1234, 1234 );
     bench_div( c64x, 4000, 0 );
     bench_div( c64x, 5000, 0 );
     bench_div( c64x, 10000, 3 );
     bench_div( c64x, 14, 3 );
     bench_div( c64x, 0x10000, 0x1000 );
     bench_div( c64x, 0x1000, 0x100 );
     bench_div( c64x, 0x100000, 2 );
#endif
}

     return DFB_OK;


error:
     if (map_q)
          munmap( map_q, C64X_QLEN );

     close( fd );

     return ret;
}

DFBResult
davinci_c64x_close( DavinciC64x *c64x )
{
     D_MAGIC_ASSERT( c64x, DavinciC64x );

     munmap( (void*) c64x->mem, C64X_MLEN );
     munmap( (void*) c64x->ctl, C64X_QLEN );

     close( c64x->fd );

     D_MAGIC_CLEAR( c64x );

     return DFB_OK;
}

