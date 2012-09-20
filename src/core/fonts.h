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

#ifndef __FONTS_H__
#define __FONTS_H__

#include <pthread.h>

#include <fusion/lock.h>

#include <directfb.h>

#include <core/coretypes.h>

#include <core/state.h>



typedef struct {
     unsigned int             height;
     DFBSurfacePixelFormat    pixel_format;
     DFBSurfaceCapabilities   surface_caps;
} DFBFontCacheType;


DFBResult dfb_font_manager_create        ( CoreDFB                 *core,
                                           DFBFontManager         **ret_manager );
DFBResult dfb_font_manager_destroy       ( DFBFontManager          *manager );
DFBResult dfb_font_manager_init          ( DFBFontManager          *manager,
                                           CoreDFB                 *core );
DFBResult dfb_font_manager_deinit        ( DFBFontManager          *manager );

DFBResult dfb_font_manager_lock          ( DFBFontManager          *manager );
DFBResult dfb_font_manager_unlock        ( DFBFontManager          *manager );

DFBResult dfb_font_manager_get_cache     ( DFBFontManager          *manager,
                                           const DFBFontCacheType  *type,
                                           DFBFontCache           **ret_cache );

DFBResult dfb_font_manager_remove_lru_row( DFBFontManager          *manager );

DFBResult dfb_font_cache_create          ( DFBFontManager          *manager,
                                           const DFBFontCacheType  *type,
                                           DFBFontCache           **ret_cache );
DFBResult dfb_font_cache_destroy         ( DFBFontCache            *cache );
DFBResult dfb_font_cache_init            ( DFBFontCache            *cache,
                                           DFBFontManager          *manager,
                                           const DFBFontCacheType  *type );
DFBResult dfb_font_cache_deinit          ( DFBFontCache            *cache );
DFBResult dfb_font_cache_get_row         ( DFBFontCache            *cache,
                                           unsigned int             width,
                                           DFBFontCacheRow        **ret_row );

DFBResult dfb_font_cache_row_create      ( DFBFontCache            *cache,
                                           DFBFontCacheRow        **ret_row );
DFBResult dfb_font_cache_row_destroy     ( DFBFontCacheRow         *row );
DFBResult dfb_font_cache_row_init        ( DFBFontCacheRow         *row,
                                           DFBFontCache            *cache );
DFBResult dfb_font_cache_row_deinit      ( DFBFontCacheRow         *row );





/*
 * glyph struct
 */
struct _CoreGlyphData {
     DirectLink       link;

     CoreFont        *font;

     unsigned int     index;
     unsigned int     layer;

     CoreSurface     *surface;              /* contains bitmap of glyph         */
     int              start;                /* x offset of glyph in surface     */
     int              width;                /* width of the glyphs bitmap       */
     int              height;               /* height of the glyphs bitmap      */
     int              left;                 /* x offset of the glyph            */
     int              top;                  /* y offset of the glyph            */
     int              xadvance;             /* placement of next glyph          */
     int              yadvance;

     int              magic;

     DFBFontCacheRow *row;

     bool             inserted;
     bool             retry;
};

#define CORE_GLYPH_DATA_DEBUG_AT(Domain, data)                                       \
     do {                                                                            \
          D_DEBUG_AT( Domain, "  -> index    %d\n", (data)->index );                 \
          D_DEBUG_AT( Domain, "  -> layer    %d\n", (data)->layer );                 \
          D_DEBUG_AT( Domain, "  -> row      %p\n", (data)->row );                   \
          D_DEBUG_AT( Domain, "  -> surface  %p\n", (data)->surface );               \
          D_DEBUG_AT( Domain, "  -> start    %d\n", (data)->start );                 \
          D_DEBUG_AT( Domain, "  -> width    %d\n", (data)->width );                 \
          D_DEBUG_AT( Domain, "  -> height   %d\n", (data)->height );                \
          D_DEBUG_AT( Domain, "  -> left     %d\n", (data)->left );                  \
          D_DEBUG_AT( Domain, "  -> top      %d\n", (data)->top );                   \
          D_DEBUG_AT( Domain, "  -> xadvance %d\n", (data)->xadvance );              \
          D_DEBUG_AT( Domain, "  -> yadvance %d\n", (data)->yadvance );              \
     } while (0)

typedef struct {
     DFBResult   (* GetCharacterIndex) ( CoreFont       *thiz,
                                         unsigned int    character,
                                         unsigned int   *ret_index );

     DFBResult   (* DecodeText)        ( CoreFont       *thiz,
                                         const void     *text,
                                         int             length,
                                         unsigned int   *ret_indices,
                                         int            *ret_num );
} CoreFontEncodingFuncs;

typedef struct {
     DirectLink                   link;

     DFBTextEncodingID            encoding;
     char                        *name;
     const CoreFontEncodingFuncs *funcs;

     int                          magic;
} CoreFontEncoding;


typedef enum {
     CFF_NONE             = 0x00000000,

     CFF_SUBPIXEL_ADVANCE = 0x00000001,

     CFF_ALL              = 0x00000001,
} CoreFontFlags;


#define DFB_FONT_MAX_LAYERS 2

/*
 * font struct
 */

struct _CoreFont {
     CoreDFB                      *core;

     DFBFontManager               *manager;

     DFBFontDescription            description;   /* original description used to create the font */
     char                         *url;

     DFBSurfaceBlittingFlags       blittingflags;
     DFBSurfacePixelFormat         pixel_format;
     DFBSurfaceCapabilities        surface_caps;

     DFBFontAttributes             attributes;


     struct {
          DirectHash              *glyph_hash;    /* infos about loaded glyphs        */
          CoreGlyphData           *glyph_data[128];
     } layers[DFB_FONT_MAX_LAYERS];

     int                           height;        /* font height                      */

     int                           ascender;      /* a positive value, the distance
                                                     from the baseline to the top     */
     int                           descender;     /* a negative value, the distance
                                                     from the baseline to the bottom  */
     int                           maxadvance;    /* width of largest character       */

     float                         up_unit_x;     /* unit vector pointing 'up' in for */
     float                         up_unit_y;     /* this font's rotation             */

     const CoreFontEncodingFuncs  *utf8;          /* for default encoding, DTEID_UTF8 */
     CoreFontEncoding            **encodings;     /* for other encodings              */
     DFBTextEncodingID             last_encoding; /* dynamic allocation impl. helper  */

     void                         *impl_data;     /* a pointer used by the impl.      */

     DFBResult                  (* GetGlyphData) ( CoreFont      *thiz,
                                                   unsigned int   index,
                                                   CoreGlyphData *data );

     DFBResult                  (* RenderGlyph)  ( CoreFont      *thiz,
                                                   unsigned int   index,
                                                   CoreGlyphData *data );

     DFBResult                  (* GetKerning)   ( CoreFont      *thiz,
                                                   unsigned int   prev,
                                                   unsigned int   current,
                                                   int           *ret_x,
                                                   int           *ret_y );


     int                           magic;

     int                           underline_position;
     int                           underline_thickness;

     CoreFontFlags                 flags;
};

#define CORE_FONT_DEBUG_AT(Domain, font)                                             \
     do {                                                                            \
          D_DEBUG_AT( Domain, "  -> ascender  %d\n", (font)->ascender );             \
          D_DEBUG_AT( Domain, "  -> descender %d\n", (font)->descender );            \
          D_DEBUG_AT( Domain, "  -> height    %d\n", (font)->height );               \
     } while (0)

/*
 * allocates and initializes a new font structure
 */
DFBResult dfb_font_create( CoreDFB                   *core,
                           const DFBFontDescription  *description,
                           const char                *url,
                           CoreFont                 **ret_font );

/*
 * destroy all data in the CoreFont struct
 */
void dfb_font_destroy( CoreFont *font );

/*
 * dispose resources that can be recreated, mainly glyph cache surfaces
 */
DFBResult dfb_font_dispose( CoreFont *font );

/*
 * lock the font before accessing it
 */
static inline void
dfb_font_lock( CoreFont *font )
{
     D_MAGIC_ASSERT( font, CoreFont );

     dfb_font_manager_lock( font->manager );
}

/*
 * unlock the font after access
 */
static inline void
dfb_font_unlock( CoreFont *font )
{
     D_MAGIC_ASSERT( font, CoreFont );

     dfb_font_manager_unlock( font->manager );
}

/*
 * loads glyph data from font
 */
DFBResult dfb_font_get_glyph_data( CoreFont        *font,
                                   unsigned int     index,
                                   unsigned int     layer,
                                   CoreGlyphData  **glyph_data );


/*
 * Called by font module to register encoding implementations.
 *
 * The encoding can be DTEID_UTF8 or DTEID_OTHER, where in the
 * latter case the actual id will be allocated dynamically.
 *
 * In the case of DTEID_UTF8 it's allowed to only provide
 * GetCharacterIndex() and let the core do the DecodeText(),
 * but that would cause a GetCharacterIndex() call per decoded
 * unicode character. So implementing both is advisable.
 *
 * If nothing is registered for DTEID_UTF8 at all, the core will
 * pass the raw unicode characters to GetGlyphInfo(), RenderGlyph() etc.
 * That's the old behaviour, fully compatible with old modules. It's
 * also a good choice if you want to avoid the character translation,
 * having an efficient font module which is based natively on unicode
 * characters.
 *
 * For registering an encoding as DTEID_OTHER both GetCharacterIndex()
 * and DecodeText() must be provided.
 */
DFBResult dfb_font_register_encoding( CoreFont                    *font,
                                      const char                  *name,
                                      const CoreFontEncodingFuncs *funcs,
                                      DFBTextEncodingID            encoding );

DFBResult dfb_font_decode_text( CoreFont          *font,
                                DFBTextEncodingID  encoding,
                                const void        *text,
                                int                length,
                                unsigned int      *ret_indices,
                                int               *ret_num );

DFBResult dfb_font_decode_character( CoreFont          *font,
                                     DFBTextEncodingID  encoding,
                                     u32                character,
                                     unsigned int      *ret_index );


#endif
