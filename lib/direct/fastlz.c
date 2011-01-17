/*
   (c) Copyright 2001-2011  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

/*  
  FastLZ - lightning-fast lossless compression library

  Copyright (C) 2007 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2006 Ariya Hidayat (ariya@kde.org)
  Copyright (C) 2005 Ariya Hidayat (ariya@kde.org)

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
  THE SOFTWARE.
*/

#include <config.h>

#include <direct/compiler.h>
#include <direct/fastlz.h>

/**********************************************************************************************************************/

#define MAX_COPY       32
#define MAX_LEN       264  /* 256 + 8 */
#define MAX_DISTANCE 8192

/*
 * Prevent accessing more than 8-bit at once, except on x86 architectures.
 */
#if !defined(FASTLZ_STRICT_ALIGN)
#define FASTLZ_STRICT_ALIGN
#if defined(__i386__) || defined(__386)  /* GNU C, Sun Studio */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__i486__) || defined(__i586__) || defined(__i686__) /* GNU C */
#undef FASTLZ_STRICT_ALIGN
#elif defined(_M_IX86) /* Intel, MSVC */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__386)
#undef FASTLZ_STRICT_ALIGN
#elif defined(_X86_) /* MinGW */
#undef FASTLZ_STRICT_ALIGN
#elif defined(__I86__) /* Digital Mars */
#undef FASTLZ_STRICT_ALIGN
#endif
#endif

#if !defined(FASTLZ_STRICT_ALIGN)
#define FASTLZ_READU16(p) (*(const u16*)(p))
#else
#define FASTLZ_READU16(p) ((p)[0] | (p)[1]<<8)
#endif

#define HASH_LOG  13
#define HASH_SIZE (1<< HASH_LOG)
#define HASH_MASK  (HASH_SIZE-1)
#define HASH_FUNCTION(v,p) { v = FASTLZ_READU16(p); v ^= FASTLZ_READU16(p+1)^(v>>(16-HASH_LOG));v &= HASH_MASK; }

/**********************************************************************************************************************/

int
direct_fastlz_compress( const void *input,
                        int         length,
                        void       *output )
{
     const u8* ip = (const u8*) input;
     const u8* ip_bound = ip + length - 2;
     const u8* ip_limit = ip + length - 12;
     u8* op = (u8*) output;

     const u8* htab[HASH_SIZE];
     const u8** hslot;
     u32 hval;

     u32 copy;

     /* sanity check */
     if (D_UNLIKELY(length < 4)) {
          if (length) {
               /* create literal copy only */
               *op++ = length-1;
               ip_bound++;
               while (ip <= ip_bound)
                    *op++ = *ip++;
               return length+1;
          }
          else
               return 0;
     }

     /* initializes hash table */
     for (hslot = htab; hslot < htab + HASH_SIZE; hslot++)
          *hslot = ip;

     /* we start with literal copy */
     copy = 2;
     *op++ = MAX_COPY-1;
     *op++ = *ip++;
     *op++ = *ip++;

     /* main loop */
     while (D_LIKELY(ip < ip_limit)) {
          const u8* ref;
          u32 distance;

          /* minimum match length */
          u32 len = 3;

          /* comparison starting-point */
          const u8* anchor = ip;

          /* find potential match */
          HASH_FUNCTION(hval,ip);
          hslot = htab + hval;
          ref = htab[hval];

          /* calculate distance to the match */
          distance = anchor - ref;

          /* update hash table */
          *hslot = anchor;

          /* is this a match? check the first 3 bytes */
          if (distance==0 || 
              (distance >= MAX_DISTANCE) ||
              *ref++ != *ip++ || *ref++!=*ip++ || *ref++!=*ip++)
               goto literal;

          /* last matched byte */
          ip = anchor + len;

          /* distance is biased */
          distance--;

          if (!distance) {
               /* zero distance means a run */
               u8 x = ip[-1];
               while (ip < ip_bound)
                    if (*ref++ != x) break;
                    else ip++;
          }
          else
               for (;;) {
                    /* safe because the outer check against ip limit */
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    if (*ref++ != *ip++) break;
                    while (ip < ip_bound)
                         if (*ref++ != *ip++) break;
                    break;
               }

          /* if we have copied something, adjust the copy count */
          if (copy)
               /* copy is biased, '0' means 1 byte copy */
               *(op-copy-1) = copy-1;
          else
               /* back, to overwrite the copy count */
               op--;

          /* reset literal counter */
          copy = 0;

          /* length is biased, '1' means a match of 3 bytes */
          ip -= 3;
          len = ip - anchor;

          /* encode the match */
          if (D_UNLIKELY(len > MAX_LEN-2))
               while (len > MAX_LEN-2) {
                    *op++ = (7 << 5) + (distance >> 8);
                    *op++ = MAX_LEN - 2 - 7 -2; 
                    *op++ = (distance & 255);
                    len -= MAX_LEN-2;
               }

          if (len < 7) {
               *op++ = (len << 5) + (distance >> 8);
               *op++ = (distance & 255);
          }
          else {
               *op++ = (7 << 5) + (distance >> 8);
               *op++ = len - 7;
               *op++ = (distance & 255);
          }

          /* update the hash at match boundary */
          HASH_FUNCTION(hval,ip);
          htab[hval] = ip++;
          HASH_FUNCTION(hval,ip);
          htab[hval] = ip++;

          /* assuming literal copy */
          *op++ = MAX_COPY-1;

          continue;

literal:
          *op++ = *anchor++;
          ip = anchor;
          copy++;
          if (D_UNLIKELY(copy == MAX_COPY)) {
               copy = 0;
               *op++ = MAX_COPY-1;
          }
     }

     /* left-over as literal copy */
     ip_bound++;
     while (ip <= ip_bound) {
          *op++ = *ip++;
          copy++;
          if (copy == MAX_COPY) {
               copy = 0;
               *op++ = MAX_COPY-1;
          }
     }

     /* if we have copied something, adjust the copy length */
     if (copy)
          *(op-copy-1) = copy-1;
     else
          op--;

     return op - (u8*)output;
}

int
direct_fastlz_decompress( const void *input,
                          int         length,
                          void       *output,
                          int         maxout )
{
     const u8* ip = (const u8*) input;
     const u8* ip_limit  = ip + length;
     u8* op = (u8*) output;
     u8* op_limit = op + maxout;
     u32 ctrl = (*ip++) & 31;
     int loop = 1;

     do {
          const u8* ref = op;
          u32 len = ctrl >> 5;
          u32 ofs = (ctrl & 31) << 8;

          if (ctrl >= 32) {
               len--;
               ref -= ofs;

               if (len == 7-1)
                    len += *ip++;

               ref -= *ip++;

               if (D_UNLIKELY(op + len + 3 > op_limit))
                    return 0;

               if (D_UNLIKELY(ref-1 < (u8 *)output))
                    return 0;

               if (D_LIKELY(ip < ip_limit))
                    ctrl = *ip++;
               else
                    loop = 0;

               if (ref == op) {
                    /* optimize copy for a run */
                    u8 b = ref[-1];
#if 0
                    *op++ = b;
                    *op++ = b;
                    *op++ = b;
                    for (; len; --len)
                         *op++ = b;
#else
                    memset( op, b, len+3 );
                    op += len+3;
#endif
               }
               else {
#if 0
#if !defined(FASTLZ_STRICT_ALIGN)
                    const u16* p;
                    u16* q;
#endif
                    /* copy from reference */
                    ref--;
                    *op++ = *ref++;
                    *op++ = *ref++;
                    *op++ = *ref++;

#if !defined(FASTLZ_STRICT_ALIGN)
                    /* copy a byte, so that now it's word aligned */
                    if (len & 1) {
                         *op++ = *ref++;
                         len--;
                    }

                    /* copy 16-bit at once */
                    q = (u16*) op;
                    op += len;
                    p = (const u16*) ref;
                    for (len>>=1; len > 4; len-=4) {
                         *q++ = *p++;
                         *q++ = *p++;
                         *q++ = *p++;
                         *q++ = *p++;
                    }
                    for (; len; --len)
                         *q++ = *p++;
#else
                    for (; len; --len)
                         *op++ = *ref++;
#endif
#else
                    ref--;
                    memcpy( op, ref, len+3 );
                    op  += len+3;
                    ref += len+3;
#endif
               }
          }
          else {
               ctrl++;

               if (D_UNLIKELY(op + ctrl > op_limit))
                    return 0;
               if (D_UNLIKELY(ip + ctrl > ip_limit))
                    return 0;

#if 0
               *op++ = *ip++; 
               for (--ctrl; ctrl; ctrl--)
                    *op++ = *ip++;
#else
               memcpy( op, ip, ctrl );
               op += ctrl;
               ip += ctrl;
#endif

               loop = D_LIKELY(ip < ip_limit);
               if (loop)
                    ctrl = *ip++;
          }
     }
     while (D_LIKELY(loop));

     return op - (u8*)output;
}

