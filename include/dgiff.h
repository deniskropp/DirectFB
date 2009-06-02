/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org>,
              Ville Syrjälä <syrjala@sci.fi> and
              Claudio Ciccani <klan@users.sf.net>.

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

#ifndef __DGIFF_H__
#define __DGIFF_H__

#include <inttypes.h>

#define DGIFF_FLAG_LITTLE_ENDIAN   0x01

typedef struct {
     unsigned char  magic[5];      /* "DGIFF" magic */

     unsigned char  major;         /* Major version number */
     unsigned char  minor;         /* Minor version number */

     unsigned char  flags;         /* Some flags like endianess */

     /* From now on endianess matters... */

     uint32_t       num_faces;

     uint32_t       __pad;
} DGIFFHeader;

typedef struct {
     int32_t        next_face;     /* byte offset from this to next face */

     int32_t        size;

     int32_t        ascender;
     int32_t        descender;
     int32_t        height;

     int32_t        max_advance;

     uint32_t       pixelformat;

     uint32_t       num_glyphs;
     uint32_t       num_rows;

     uint32_t       __pad;
} DGIFFFaceHeader;

typedef struct {
     uint32_t       unicode;

     uint32_t       row;

     int32_t        offset;
     int32_t        width;
     int32_t        height;

     int32_t        left;
     int32_t        top;
     int32_t        advance;
} DGIFFGlyphInfo;

typedef struct {
     int32_t        width;
     int32_t        height;
     int32_t        pitch;         /* Preferably 8 byte aligned */

     uint32_t       __pad;

     /* Raw pixel data follows, "height * pitch" bytes. */
} DGIFFGlyphRow;

#endif

