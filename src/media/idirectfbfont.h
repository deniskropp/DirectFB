/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __IDIRECTFBFONT_H__
#define __IDIRECTFBFONT_H__

#include <directfb.h>

#include <direct/filesystem.h>

#include <core/coretypes.h>

typedef enum {
     IDFBFONT_CONTEXT_CONTENT_TYPE_UNKNOWN,

     IDFBFONT_CONTEXT_CONTENT_TYPE_MALLOCED,
     IDFBFONT_CONTEXT_CONTENT_TYPE_MAPPED,
     IDFBFONT_CONTEXT_CONTENT_TYPE_MEMORY
} IDirectFBFont_ProbeContextContentType;

/*
 * probing context
 */
typedef struct {
     /* Only set if databuffer is created from file.
        deprecated - use memory location below. */
     const char                            *filename;

     /* if !=NULL, pointer to the file content */
     unsigned char                         *content;
     unsigned int                           content_size;
     IDirectFBFont_ProbeContextContentType  content_type;
} IDirectFBFont_ProbeContext;

DFBResult
IDirectFBFont_CreateFromBuffer( IDirectFBDataBuffer       *buffer,
                                CoreDFB                   *core,
                                const DFBFontDescription  *desc,
                                IDirectFBFont            **interface_ptr );
                                
/**********************************************************************************************************************/

/*
 * private data struct of IDirectFBFont
 * used by implementors of IDirectFBFont
 */
typedef struct {
     int                                    ref;       /* reference counter    */
     CoreFont                              *font;      /* pointer to core font */
     unsigned char                         *content;   /* possible allocation, free at intf. close */
     unsigned int                           content_size;
     IDirectFBFont_ProbeContextContentType  content_type;

     DFBTextEncodingID                      encoding;  /* text encoding */
} IDirectFBFont_data;

/*
 * common code to construct the interface (internal usage only)
 */
DFBResult IDirectFBFont_Construct( IDirectFBFont *thiz, CoreFont *font );

/*
 * deinitialize font and its surfaces
 */
void IDirectFBFont_Destruct( IDirectFBFont *thiz );

#endif
