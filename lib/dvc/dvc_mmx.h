/*
   (C) Copyright 2007 Claudio Ciccani <klan@directfb.org>.

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
   
   DVC - DirectFB Video Converter
*/

#ifndef __DVC_MMX_H__
#define __DVC_MMX_H__

#define aligned(x) __attribute__((aligned(x)))


static void YCbCr_to_RGB_Proc_MMX( DVCContext *ctx )
{  
     static const s16 aligned(8) sub0[4] = {  16,  16,  16,  16 }; 
     static const s16 aligned(8) sub1[4] = { 128, 128, 128, 128 };
     static const s16 aligned(8) mul[20] = {
                0x253F,  0x253F,  0x253F,  0x253F, // Y       Coeff.
                0x3312,  0x3312,  0x3312,  0x3312, // V Red   Coeff.
                0x4093,  0x4093,  0x4093,  0x4093, // U Blue  Coeff.
               -0x1A04, -0x1A04, -0x1A04, -0x1A04, // V Green Coeff.
               -0x0C83, -0x0C83, -0x0C83, -0x0C83  // U Green Coeff.
     };

     DVCColor *D = ctx->buf[0];
     int       w;
         
     __asm__ __volatile__ (
          "shr           $2,    %1\n\t"
          "jz            2f       \n\t"
          "pxor       %%mm7, %%mm7\n\t"
          ".align 16\n"
          "1:\n\t"
          "movq        (%0), %%mm1\n\t" // a1 y1 u1 v1 a0 y0 u0 v0
          "movq       8(%0), %%mm2\n\t" // a3 y3 u3 v3 a2 y2 u2 v2
          "movq       %%mm1, %%mm5\n\t" // a1 y1 u1 v1 a0 y0 u0 v0
          "punpcklbw  %%mm2, %%mm1\n\t" // a2 a0 y2 y0 u2 u0 v2 v0
          "punpckhbw  %%mm2, %%mm5\n\t" // a3 a1 y3 y1 u3 u1 v3 v1
          "movq       %%mm1, %%mm3\n\t" // a2 a0 y2 y0 u2 u0 v2 v0
          "punpcklbw  %%mm5, %%mm1\n\t" // u3 u2 u1 u0 v3 v2 v1 v0
          "punpckhbw  %%mm5, %%mm3\n\t" // a3 a2 a1 a0 y3 y2 y1 y0
          "movq       %%mm1, %%mm0\n\t" // u3 u2 u1 u0 v3 v2 v1 v0
          "movq       %%mm3, %%mm4\n\t" // a3 a2 a1 a0 y3 y2 y1 y0
          "punpcklbw  %%mm7, %%mm1\n\t" // 00 v3 00 v2 00 v1 00 v0
          "punpckhbw  %%mm7, %%mm0\n\t" // 00 u3 00 u2 00 u1 00 u0
          "punpcklbw  %%mm7, %%mm3\n\t" // 00 y3 00 y2 00 y1 00 y0
          "punpckhdq  %%mm4, %%mm4\n\t" // a3 a2 a1 a0 a3 a2 a1 a0
          /* mm0 = u, mm1 = v, mm3 = y, mm4 = a */
          "psubw         %2, %%mm3\n\t" // y -= 16
          "psllw         $3, %%mm3\n\t" // precision
          "pmulhw      (%4), %%mm3\n\t"
          "psubw         %3, %%mm1\n\t" // v -= 128 
          "psllw         $3, %%mm1\n\t" // precision
          "movq       %%mm1, %%mm2\n\t" // 00 v3 00 v2 00 v1 00 v0
          "pmulhw     8(%4), %%mm2\n\t" // vr
          "psubw         %3, %%mm0\n\t" // u -= 128 
          "psllw         $3, %%mm0\n\t" // precision
          "movq       %%mm0, %%mm5\n\t" // 00 u3 00 u2 00 u1 00 u0
          "pmulhw    16(%4), %%mm5\n\t" // ub
          "paddw      %%mm3, %%mm2\n\t" // 00 r3 00 r2 00 r1 00 r0
          "paddw      %%mm3, %%mm5\n\t" // 00 b3 00 b2 00 b1 00 b0
          "pmulhw    24(%4), %%mm1\n\t" // vg
          "packuswb   %%mm2, %%mm2\n\t" // r3 r2 r1 r0 r3 r2 r1 r0
          "packuswb   %%mm5, %%mm5\n\t" // b3 b2 b1 b0 b3 b2 b1 b0
          "pmulhw    32(%4), %%mm0\n\t" // ug
          "paddw      %%mm1, %%mm3\n\t" // y + vg
          "paddw      %%mm0, %%mm3\n\t" // 00 g3 00 g2 00 g1 00 g0
          "packuswb   %%mm3, %%mm3\n\t" // g3 g2 g1 g0 g3 g2 g1 g0
           /* mm5 = b, mm3 = g, mm2 = r, mm4 = a */
          "punpcklbw  %%mm4, %%mm2\n\t" // a3 r3 a2 r2 a1 r1 a0 r0
          "punpcklbw  %%mm3, %%mm5\n\t" // g3 b3 g2 b2 g1 b1 g0 b0
          "movq       %%mm5, %%mm0\n\t" // g3 b3 g2 b2 g1 b1 g0 b0
          "punpckhwd  %%mm2, %%mm5\n\t" // a3 r3 g3 b3 a2 r2 g2 b2
          "punpcklwd  %%mm2, %%mm0\n\t" // a1 r1 g1 b1 a0 r0 g0 b0
          "movq       %%mm5, 8(%0)\n\t" 
          "movq       %%mm0,  (%0)\n\t"
          "add          $16,    %0\n\t"
          "dec           %1       \n\t"
          "jnz           1b       \n\t"
          "emms                   \n\t"
          "2:"     
          : "=&D" (D)
          : "c" (ctx->sw), "m" (*sub0), "m" (*sub1), "r" (mul), "0" (D)
          : "memory" );

     for (w = ctx->sw&3; w; w--) {
          YCBCR_TO_RGB( D->YUV.y, D->YUV.u, D->YUV.v,
                        D->RGB.r, D->RGB.g, D->RGB.b );
          D++;
     }
}

static void Load_RGB32_LE_MMX( DVCContext *ctx )
{
     static const u64 alpha = 0xff000000ff000000ull;
     
     u32      *S = ctx->src[0];
     DVCColor *D = ctx->buf[0];
     int       w;
     
     __asm__ __volatile__(
          "shr          $2,    %2\n\t"
          "jz           2f       \n\t"
          "movq         %3, %%mm2\n\t"
          ".align 16\n"
          "1:\n\t"
          "movq       (%0), %%mm0\n\t"
          "movq      8(%0), %%mm1\n\t"
          "por       %%mm2, %%mm0\n\t"
          "por       %%mm2, %%mm1\n\t"
          "movq      %%mm0,  (%1)\n\t"
          "movq      %%mm1, 8(%1)\n\t"
          "add         $16,    %0\n\t"
          "add         $16,    %1\n\t"
          "dec          %2       \n\t"
          "jnz          1b       \n\t"
          "emms                  \n\t"
          "2:"
          : "=&S" (S), "=&D" (D)
          : "c" (ctx->sw), "m" (alpha), "0" (S), "1" (D)
          : "memory" );
          
     for (w = ctx->sw&3; w; w--) {
          D->RGB.r = *S >> 16;
          D->RGB.g = *S >>  8;
          D->RGB.b = *S;
          S++;
          D++;
     }
}

static void Load_YUV422_MMX( DVCContext *ctx )
{
     static const u64 alpha = 0xffffffffffffffffull;
         
     u8       *Sy  = ctx->src[0];
     u8       *Su  = ctx->src[1];
     u8       *Sv  = ctx->src[2];
     DVCColor *D   = ctx->buf[0];
     int       i;

     __asm__ __volatile__(
          "shr          $3,     %4\n\t"
          "jz           2f        \n\t"
          "movq         %5,  %%mm7\n\t"
          ".align 16\n"
          "1:\n\t"
          "movq       (%0),  %%mm0\n\t" // y7 y6 y5 y4 y3 y2 y1 y0
          "movd       (%1),  %%mm1\n\t" // 00 00 00 00 u3 u2 u1 u0
          "movd       (%2),  %%mm2\n\t" // 00 00 00 00 v3 v2 v1 v0
          "punpcklbw %%mm1,  %%mm1\n\t" // u3 u3 u2 u2 u1 u1 u0 u0
          "punpcklbw %%mm2,  %%mm2\n\t" // v3 v3 v2 v2 v1 v1 v0 v0
          "movq      %%mm0,  %%mm3\n\t" // y7 y6 y5 y4 y3 y2 y1 y0
          "movq      %%mm2,  %%mm4\n\t" // v3 v3 v2 v2 v1 v1 v0 v0
          "punpcklbw %%mm7,  %%mm0\n\t" // ff y3 ff y2 ff y1 ff y0
          "punpckhbw %%mm7,  %%mm3\n\t" // ff y7 ff y6 ff y5 ff y4
          "punpcklbw %%mm1,  %%mm2\n\t" // u1 v1 u1 v1 u0 v0 u0 v0
          "punpckhbw %%mm1,  %%mm4\n\t" // u3 v3 u3 v3 u2 v2 u2 v2
          "movq      %%mm2,  %%mm5\n\t" // u1 v1 u1 v1 u0 v0 u0 v0
          "movq      %%mm4,  %%mm6\n\t" // u3 v3 u3 v3 u2 v2 u2 v2
          "punpcklwd %%mm0,  %%mm2\n\t" // ff y1 u0 v0 ff y0 u0 v0
          "punpcklwd %%mm3,  %%mm4\n\t" // ff y5 u2 v2 ff y4 u2 v2
          "punpckhwd %%mm0,  %%mm5\n\t" // ff y3 u1 v1 ff y2 u1 v1
          "punpckhwd %%mm3,  %%mm6\n\t" // ff y7 u3 v3 ff y6 u3 v3
          "movq      %%mm2,   (%3)\n\t"
          "movq      %%mm5,  8(%3)\n\t"
          "movq      %%mm4, 16(%3)\n\t"
          "movq      %%mm6, 24(%3)\n\t"
          "add          $8,     %0\n\t"
          "add          $4,     %1\n\t"
          "add          $4,     %2\n\t"
          "add         $32,     %3\n\t"
          "dec          %4        \n\t"
          "jnz          1b        \n\t"
          "emms                   \n\t"
          "2:"
          : "=&r" (Sy), "=&r" (Su), "=&r" (Sv), "=&D" (D)
          : "c" (ctx->sw), "m" (alpha),
            "0" (Sy), "1" (Su), "2" (Sv), "3" (D)
          : "memory");
     
     for (i = 0; i < (ctx->sw&7); i++) {
          D[i].YUV.y = Sy[i];
          D[i].YUV.u = Su[i>>1];
          D[i].YUV.v = Sv[i>>1];
     }
}

static void Store_RGB16_LE_MMX( DVCContext *ctx )
{
     static const u64 aligned(8) dth5[2] = { 0x0602060206020602ull,
                                             0x0004000400040004ull };
     static const u64 aligned(8) dth6[2] = { 0x0103010301030103ull,
                                             0x0200020002000200ull };
     static const u64 aligned(8) msk5[1] = { 0xf8f8f8f8f8f8f8f8ull };
     static const u64 aligned(8) msk6[1] = { 0xfcfcfcfcfcfcfcfcull };
     
     DVCColor *S = ctx->buf[0];
     u16      *D = ctx->dst[0];
     int       w = ctx->dw;
     
     if (w > 15) {
          const u64 *d5x = &dth5[ctx->dy&1];
          const u64 *d6x = &dth6[ctx->dy&1];
          int        n;
          
          while ((long)D & 6) {
               *D++ = ((S->RGB.r & 0xf8) << 8) |
                      ((S->RGB.g & 0xfc) << 3) |
                      ((S->RGB.b       ) >> 3);
               S++;
               w--;
          }
          
          n = w >> 3;
          w &= 7;
     
          __asm__ __volatile__(
               "pxor       %%mm7, %%mm7\n\t"
               ".align 16\n"
               "1:\n\t"
               "movq        (%0), %%mm0\n\t" // a1 r1 g1 b1 a0 r0 g0 b0
               "movq       8(%0), %%mm1\n\t" // a3 r3 g3 b3 a2 r2 g2 b2
               "movq      16(%0), %%mm2\n\t" // a5 r5 g5 b5 a4 r4 g4 b4
               "movq      24(%0), %%mm3\n\t" // a7 r7 g7 b7 a6 r6 g6 b6
               "movq       %%mm0, %%mm4\n\t" // a1 r1 g1 b1 a0 r0 g0 b0
               "movq       %%mm2, %%mm6\n\t" // a5 r5 g5 b5 a4 r4 g4 b4
               "punpcklbw  %%mm1, %%mm0\n\t" // a2 a0 r2 r0 g2 g0 b2 b0
               "punpckhbw  %%mm1, %%mm4\n\t" // a3 a1 r3 r1 g3 g1 b3 b1
               "punpcklbw  %%mm3, %%mm2\n\t" // a6 a4 r6 r4 g6 g4 b6 b4
               "punpckhbw  %%mm3, %%mm6\n\t" // a7 a5 r7 r5 g7 g5 b7 b5
               "movq       %%mm0, %%mm1\n\t" // a2 a0 r2 r0 g2 g0 b2 b0
               "movq       %%mm2, %%mm3\n\t" // a6 a4 r6 r4 g6 g4 b6 b4
               "punpcklbw  %%mm4, %%mm0\n\t" // g3 g2 g1 g0 b3 b1 b2 b0
               "punpckhbw  %%mm4, %%mm1\n\t" // a3 a2 a1 a0 r3 r2 r1 r0
               "punpcklbw  %%mm6, %%mm2\n\t" // g7 g6 g5 g4 b7 b6 b5 b4
               "punpckhbw  %%mm6, %%mm3\n\t" // a7 a6 a5 a4 r7 r6 r5 r4
               "movq       %%mm0, %%mm4\n\t" // g3 g2 g1 g0 b3 b1 b2 b0
               "punpckldq  %%mm2, %%mm0\n\t" // b7 b6 b5 b4 b3 b1 b2 b0
               "punpckhdq  %%mm2, %%mm4\n\t" // g7 g6 g5 g4 g3 g2 g1 g0
               "punpckldq  %%mm3, %%mm1\n\t" // r7 r6 r5 r4 r3 r2 r1 r0
               "paddusb       %3, %%mm0\n\t" // dither b
               "paddusb       %4, %%mm4\n\t" // dither g
               "paddusb       %3, %%mm1\n\t" // dither r
               "pand          %6, %%mm4\n\t" // g & 0xfc
               "pand          %5, %%mm1\n\t" // r & 0xf8
               "movq       %%mm0, %%mm2\n\t" // b7 b6 b5 b4 b3 b1 b2 b0
               "movq       %%mm4, %%mm3\n\t" // g7 g6 g5 g4 g3 g2 g1 g0
               "movq       %%mm1, %%mm5\n\t" // r7 r6 r5 r4 r3 r2 r1 r0
               "punpcklbw  %%mm7, %%mm2\n\t" // 00 b3 00 b2 00 b1 00 b0
               "punpcklbw  %%mm7, %%mm3\n\t" // 00 g3 00 g2 00 g1 00 g0
               "punpcklbw  %%mm7, %%mm5\n\t" // 00 r3 00 r2 00 r1 00 r0
               "psrlw         $3, %%mm2\n\t" // b >> 3
               "psllw         $3, %%mm3\n\t" // g << 3
               "psllw         $8, %%mm5\n\t" // r << 8
               "por        %%mm3, %%mm2\n\t" // b | g
               "por        %%mm5, %%mm2\n\t" // b | g | r
               "punpckhbw  %%mm7, %%mm0\n\t" // 00 b7 00 b6 00 b5 00 b4
               "punpckhbw  %%mm7, %%mm4\n\t" // 00 g7 00 g6 00 g5 00 g4
               "punpckhbw  %%mm7, %%mm1\n\t" // 00 r7 00 r6 00 r5 00 r4
               "psrlw         $3, %%mm0\n\t" // b >> 3
               "psllw         $3, %%mm4\n\t" // g << 3
               "psllw         $8, %%mm1\n\t" // r << 8
               "por        %%mm4, %%mm0\n\t" // b | g
               "por        %%mm1, %%mm0\n\t" // b | g | r
               "movq       %%mm2,  (%1)\n\t" // out 0..3 pixels
               "movq       %%mm0, 8(%1)\n\t" // out 4..7 pixels
               "add          $32,    %0\n\t"
               "add          $16,    %1\n\t"
               "dec           %2       \n\t"
               "jnz           1b       \n\t"
               "emms"
               : "=&S" (S), "=&D" (D), "=&c" (n)
               : "m" (*d5x), "m" (*d6x), "m" (*msk5), "m" (*msk6),
                 "0" (S), "1" (D), "2" (n)
               : "memory" );
     }
     
     while (w) {
          *D++ = ((S->RGB.r & 0xf8) << 8) |
                 ((S->RGB.g & 0xfc) << 3) |
                 ((S->RGB.b       ) >> 3);
          S++;
          w--;
     }
}

static void ScaleH_Up_Proc_MMX( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[0];
     u32 *D = (u32*)ctx->buf[0] + ctx->dw - 1;
     int  i = ctx->h_scale * (ctx->dw - 1);
     int  j = i & ~0xffff;
     int  n = ctx->dw;
     
     for (; n && i >= j; n--) {
          *D-- = S[i>>16];
          i -= ctx->h_scale;
     }

     __asm__ __volatile__(
          "pxor       %%mm7, %%mm7\n\t"
          "shr           $1,    %1\n\t"
          "jz            2f\n\t"
          ".align 16\n"
          "1:\n\t"
          "mov           %2, %%eax\n\t"
          "movd          %2, %%mm4\n\t"
          "shr          $16, %%eax\n\t"
          "punpcklwd  %%mm4, %%mm4\n\t"
#ifdef ARCH_X86_64
          "movq (%3,%%rax,4), %%mm0\n\t"
#else
          "movq (%3,%%eax,4), %%mm0\n\t"
#endif
          "punpckldq  %%mm4, %%mm4\n\t"
          "movq       %%mm0, %%mm1\n\t"
          "sub           %4,    %2\n\t"
          "psrlw         $1, %%mm4\n\t"
          "mov           %2, %%eax\n\t"
          "movd          %2, %%mm5\n\t"
          "shr          $16, %%eax\n\t"
          "punpcklwd  %%mm5, %%mm5\n\t"
#ifdef ARCH_X86_64
          "movq (%3,%%rax,4), %%mm2\n\t"
#else
          "movq (%3,%%eax,4), %%mm2\n\t"
#endif
          "punpckldq  %%mm5, %%mm5\n\t"
          "movq       %%mm2, %%mm3\n\t"
          "psrlw         $1, %%mm5\n\t"
          "punpcklbw  %%mm7, %%mm0\n\t"
          "punpckhbw  %%mm7, %%mm1\n\t"
          "punpcklbw  %%mm7, %%mm2\n\t"
          "punpckhbw  %%mm7, %%mm3\n\t"
          "psubw      %%mm0, %%mm1\n\t"
          "psubw      %%mm2, %%mm3\n\t"
          "psllw         $1, %%mm1\n\t"
          "psllw         $1, %%mm3\n\t"
          "pmulhw     %%mm4, %%mm1\n\t"
          "sub           %4,    %2\n\t"
          "sub           $8,    %0\n\t"
          "pmulhw     %%mm5, %%mm3\n\t"
          "paddw      %%mm1, %%mm0\n\t"
          "paddw      %%mm3, %%mm2\n\t"
          "packuswb   %%mm0, %%mm2\n\t"
          "movq       %%mm2, 4(%0)\n\t"
          "dec           %1\n\t"
          "jnz           1b\n\t"
          ".align 8\n"
          "2:\n\t"
          "testb         $1,    %5\n\t"
          "jz            3f\n\t"
          "movd          %2, %%mm4\n\t"
          "shr          $16,    %2\n\t"
          "punpcklwd  %%mm4, %%mm4\n\t"
#ifdef ARCH_X86_64
          "movq   (%3,%q2,4), %%mm0\n\t"
#else
          "movq   (%3,%2,4), %%mm0\n\t"
#endif
          "punpckldq  %%mm4, %%mm4\n\t"
          "movq       %%mm0, %%mm1\n\t" 
          "psrlw         $1, %%mm4\n\t"     
          "punpcklbw  %%mm7, %%mm0\n\t"
          "punpckhbw  %%mm7, %%mm1\n\t"
          "psubw      %%mm0, %%mm1\n\t"
          "psllw         $1, %%mm1\n\t"
          "pmulhw     %%mm4, %%mm1\n\t"
          "paddw      %%mm1, %%mm0\n\t"
          "packuswb   %%mm0, %%mm0\n\t"
          "movd       %%mm0,  (%0)\n\t"
          "3:\n\t"
          "emms"
          : "=&D" (D), "=&r" (n), "=&r" (i)
          : "S" (S), "rm" (ctx->h_scale), "m" (n), "0" (D), "1" (n), "2" (i)
          : "eax", "memory" );
}

static void ScaleV_Up_Proc_MMX( DVCContext *ctx )
{
     u32 *S = (u32*)ctx->buf[1];
     u32 *D = (u32*)ctx->buf[0];
     
     if (ctx->s_v & 0xffff) {
          __asm__ __volatile__(
               "movd          %3, %%mm4\n\t"
               "pxor       %%mm7, %%mm7\n\t"
               "punpcklwd  %%mm4, %%mm4\n\t"
               "punpckldq  %%mm4, %%mm4\n\t"
               "shr           $1,    %2\n\t"
               "jz            2f\n\t"
               ".align 16\n"
               "1:\n\t"
               "movq        (%0), %%mm0\n\t"
               "movq        (%1), %%mm1\n\t"
               "movq       %%mm0, %%mm2\n\t"
               "movq       %%mm1, %%mm3\n\t"
               "punpcklbw  %%mm7, %%mm0\n\t"
               "punpcklbw  %%mm7, %%mm1\n\t"
               "punpckhbw  %%mm7, %%mm2\n\t"
               "punpckhbw  %%mm7, %%mm3\n\t"
               "psubw      %%mm1, %%mm0\n\t"
               "psubw      %%mm3, %%mm2\n\t"
               "psllw         $1, %%mm0\n\t"
               "psllw         $1, %%mm2\n\t"
               "pmulhw     %%mm4, %%mm0\n\t"
               "add           $8,    %1\n\t"
               "add           $8,    %0\n\t"
               "pmulhw     %%mm4, %%mm2\n\t"
               "paddw      %%mm1, %%mm0\n\t"
               "paddw      %%mm3, %%mm2\n\t"
               "packuswb   %%mm2, %%mm0\n\t"
               "movq       %%mm0,-8(%0)\n\t"
               "dec           %2\n\t"
               "jnz           1b\n\t"
               ".align 8\n"
               "2:\n\t"
               "testb         $1,    %4\n\t"
               "jz            3f\n\t"
               "movd        (%0), %%mm0\n\t"
               "movd        (%1), %%mm1\n\t"
               "punpcklbw  %%mm7, %%mm0\n\t"
               "punpcklbw  %%mm7, %%mm1\n\t"
               "psubw      %%mm1, %%mm0\n\t"
               "psllw         $1, %%mm0\n\t"
               "pmulhw     %%mm4, %%mm0\n\t"
               "paddw      %%mm1, %%mm0\n\t"
               "packuswb   %%mm0, %%mm0\n\t"
               "movd       %%mm0,  (%0)\n\t"
               "3:\n\t" 
               "emms"
               : "=&D" (D), "=&S" (S)
               : "c" (ctx->dw), "q" ((ctx->s_v & 0xffff)>>1), "m" (ctx->dw), "0" (D), "1" (S)
               : "memory" );
     }
}


#define CPUID( i, a, b, c, d )                     \
    __asm__ __volatile__(                          \
         "push   %%ebx    \n\t"                    \
         "cpuid           \n\t"                    \
         "mov    %%ebx, %1\n\t"                    \
         "pop    %%ebx"                            \
         : "=a" (a), "=r" (b), "=c" (c), "=d" (d)  \
         : "a" (i)                                 \
         : "cc" )

static void init_mmx( void )
{
     static int initialized = 0;
     bool       have_mmx;
     long       a, b, c, d;
     
     if (initialized)
          return;
          
     initialized = 1;
     
     if (!dfb_config->mmx)
          return;
     
#if defined(__amd64__) || defined(__x86_64__)
     have_mmx = true;
#else
     have_mmx = false;
     
     /* Check whether CPUID is supported. */
     __asm__ __volatile__( 
          "pushf               \n\t"
          "pop           %0    \n\t"
          "mov           %0, %1\n\t"
          "xor    $0x200000, %0\n\t"
          "push          %0    \n\t"
          "popf                \n\t"
          "pushf               \n\t"
          "pop           %0    \n\t"
          :  "=a" (a), "=r" (b)
          :: "cc" );

     if (a == b)
          return;
          
     CPUID( 0, a, b, c, d );
     if (a >= 1) {
         CPUID( 1, a, b, c, d );
         have_mmx = ((d & 0x800000) != 0);
     }
     
     if (!have_mmx) {
          CPUID( 0x80000000, a, b, c, d );
          if (a >= 0x80000001) {
               CPUID( 0x80000001, a, b, c, d );
               have_mmx = ((d & 0x80000000) != 0);
          }
     }
#endif

     if (!have_mmx)
          return;
     
     Load_Proc[DVC_PIXELFORMAT_INDEX(DVCPF_RGB32_LE)] = Load_RGB32_LE_MMX;
     Load_Proc[DVC_PIXELFORMAT_INDEX(DVCPF_YUV422)]   = Load_YUV422_MMX;
     Load_Proc[DVC_PIXELFORMAT_INDEX(DVCPF_YUV420)]   = Load_YUV422_MMX;
     
     Store_Proc[DVC_PIXELFORMAT_INDEX(DVCPF_RGB16_LE)] = Store_RGB16_LE_MMX;
     
     YCbCr_to_RGB_Proc = YCbCr_to_RGB_Proc_MMX;
     ScaleH_Up_Proc    = ScaleH_Up_Proc_MMX;
     ScaleV_Up_Proc    = ScaleV_Up_Proc_MMX;
}


#endif /* __DVC_MMX_H__ */
