/*
   (c) Copyright 2006  Denis Oliver Kropp <dok@directfb.org>

   All rights reserved.

   This file is subject to the terms and conditions of the MIT License:

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify, merge,
   publish, distribute, sublicense, and/or sell copies of the Software,
   and to permit persons to whom the Software is furnished to do so,
   subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __DFIFF_H__
#define __DFIFF_H__

#include <inttypes.h>

#define DFIFF_FLAG_LITTLE_ENDIAN   0x01

typedef struct {
     unsigned char magic[5];      /* "DFIFF" magic */

     unsigned char major;         /* Major version number */
     unsigned char minor;         /* Minor version number */

     unsigned char flags;         /* Some flags like endianess */

     /* From now on endianess matters... */

     uint32_t                 width;
     uint32_t                 height;
     DFBSurfacePixelFormat    format;
     uint32_t                 pitch;
} DFIFFHeader;


#endif

