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

#include <config.h>

#include <stdlib.h>
#include <string.h>

#include <pthread.h>

#include <directfb.h>

#include <core/core.h>
#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/fonts.h>
#include <core/gfxcard.h>
#include <core/surface.h>

#include <direct/debug.h>
#include <direct/hash.h>
#include <direct/map.h>
#include <direct/mem.h>
#include <direct/messages.h>
#include <direct/utf8.h>
#include <direct/util.h>

#include <gfx/convert.h>

#include <misc/conf.h>
#include <misc/util.h>


D_DEBUG_DOMAIN( Core_Font,         "Core/Font",      "DirectFB Core Font" );
D_DEBUG_DOMAIN( Core_FontSurfaces, "Core/Font/Surf", "DirectFB Core Font Surfaces" );

D_DEBUG_DOMAIN( Font_Manager,      "Core/Font/Manager",  "DirectFB Core Font Manager" );
D_DEBUG_DOMAIN( Font_Cache,        "Core/Font/Cache",    "DirectFB Core Font Cache" );
D_DEBUG_DOMAIN( Font_CacheRow,     "Core/Font/CacheRow", "DirectFB Core Font Cache Row" );

/**********************************************************************************************************************/

static bool free_glyphs( DirectHash    *hash,
                         unsigned long  key,
                         void          *value,
                         void          *ctx );

/**********************************************************************************************************************/

struct __DFB_DFBFontManager {
     int                 magic;

     CoreDFB            *core;

     pthread_mutex_t     lock;

     DirectMap          *caches;

     unsigned int        max_rows;
     unsigned int        num_rows;
     unsigned long long  row_stamp;     // FIXME: lru calculation wrong if value overruns (non-fatal)
};

#define DFB_FONT_MANAGER_ASSERT( manager )                            \
     do {                                                             \
          D_MAGIC_ASSERT( manager, DFBFontManager );                  \
          D_ASSERT( (manager)->max_rows > 0 );                        \
          D_ASSERT( (manager)->num_rows <= (manager)->max_rows );     \
     } while (0)

/**********************************************************************************************************************/

struct __DFB_DFBFontCache {
     int                 magic;

     DFBFontManager     *manager;

     DFBFontCacheType    type;

     unsigned int        row_width;

     DirectLink         *rows;
};

#define DFB_FONT_CACHE_ASSERT( cache )                                \
     do {                                                             \
          D_MAGIC_ASSERT( cache, DFBFontCache );                      \
     } while (0)

/**********************************************************************************************************************/

struct __DFB_DFBFontCacheRow {
     DirectLink          link;

     int                 magic;

     DFBFontCache       *cache;

     unsigned long long  stamp;

     CoreSurface        *surface;
     unsigned int        next_x;

     DirectLink         *glyphs;
};

#define DFB_FONT_CACHE_ROW_ASSERT( row )                              \
     do {                                                             \
          D_MAGIC_ASSERT( row, DFBFontCacheRow );                     \
     } while (0)

/**********************************************************************************************************************/
/**********************************************************************************************************************/

static bool
font_cache_map_compare( DirectMap    *map,
                        const void   *key,
                        void         *object,
                        void         *ctx )
{
     const DFBFontCacheType *type  = key;
     DFBFontCache           *cache = object;

     return memcmp( type, &cache->type, sizeof(*type) ) == 0;
}

static unsigned int
font_cache_map_hash( DirectMap    *map,
                     const void   *key,
                     void         *ctx )
{
     const DFBFontCacheType *type = key;

     return (type->height * 131 + type->pixel_format) * 131 + type->surface_caps;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
dfb_font_manager_create( CoreDFB         *core,
                         DFBFontManager **ret_manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFBResult       ret;
     DFBFontManager *manager;

     D_ASSERT( core != NULL );
     D_ASSERT( ret_manager != NULL );

     manager = D_CALLOC( 1, sizeof(DFBFontManager) );
     if (!manager)
          return D_OOM();

     ret = dfb_font_manager_init( manager, core );
     if (ret) {
          D_FREE( manager );
          return ret;
     }

     *ret_manager = manager;

     return DFB_OK;
}

DFBResult
dfb_font_manager_destroy( DFBFontManager *manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFB_FONT_MANAGER_ASSERT( manager );

     dfb_font_manager_deinit( manager );

     D_FREE( manager );

     return DFB_OK;
}

DFBResult
dfb_font_manager_init( DFBFontManager *manager,
                       CoreDFB        *core )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFBResult ret;

     D_ASSERT( core != NULL );
     D_ASSERT( manager != NULL );

     manager->core      = core;
     manager->max_rows  = dfb_config->max_font_rows;

     ret = direct_map_create( 11, font_cache_map_compare, font_cache_map_hash, NULL, &manager->caches );
     if (ret)
          return ret;

     direct_util_recursive_pthread_mutex_init( &manager->lock );

     D_MAGIC_SET( manager, DFBFontManager );

     return DFB_OK;
}

static DirectEnumerationResult
destroy_caches( DirectMap *map,
                void      *object,
                void      *ctx )
{
     D_DEBUG_AT( Font_Manager, "%s( object %p )\n", __func__, object );

     DFBFontCache *cache = object;

     DFB_FONT_CACHE_ASSERT( cache );

     dfb_font_cache_destroy( cache );

     return DENUM_OK;
}

DFBResult
dfb_font_manager_deinit( DFBFontManager *manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFB_FONT_MANAGER_ASSERT( manager );

     direct_map_iterate( manager->caches, destroy_caches, NULL );
     direct_map_destroy( manager->caches );

     pthread_mutex_destroy( &manager->lock );

     D_MAGIC_CLEAR( manager );

     return DFB_OK;
}

DFBResult
dfb_font_manager_lock( DFBFontManager *manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFB_FONT_MANAGER_ASSERT( manager );

     pthread_mutex_lock( &manager->lock );

     // FIXME: remember row stamp here, avoiding destruction of any row used before the unlock,
     //        might happen with looong string and smaaaall maximum of rows

     return DFB_OK;
}

DFBResult
dfb_font_manager_unlock( DFBFontManager *manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFB_FONT_MANAGER_ASSERT( manager );

     // FIXME: destroy LRU row until maximum number of rows is no longer exceeded,
     //        might happen when rows are preserved due to being used after lock is called

     pthread_mutex_unlock( &manager->lock );

     return DFB_OK;
}

DFBResult
dfb_font_manager_get_cache( DFBFontManager          *manager,
                            const DFBFontCacheType  *type,
                            DFBFontCache           **ret_cache )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     DFB_FONT_MANAGER_ASSERT( manager );
     D_ASSERT( type != NULL );
     D_ASSERT( ret_cache != NULL );

     D_DEBUG_AT( Font_Manager, "  -> height %u, format 0x%x, caps 0x%x\n",
                 type->height, type->pixel_format, type->surface_caps );

     ///

     DFBResult     ret;
     DFBFontCache *cache;

     DFBFontCacheType _type = *type;

     if (_type.height < 8)
          _type.height = 8;

     cache = direct_map_lookup( manager->caches, &_type );
     if (!cache) {
          ret = dfb_font_cache_create( manager, &_type, &cache );
          if (ret)
               return ret;

          ret = direct_map_insert( manager->caches, &_type, cache );
          if (ret) {
               dfb_font_cache_destroy( cache );
               return ret;
          }
     }

     *ret_cache = cache;

     return DFB_OK;
}

typedef struct {
     unsigned int     lru_stamp;
     DFBFontCacheRow *lru_row;
} FindLruRowContext;

static DirectEnumerationResult
find_lru_row( DirectMap *map,
              void      *object,
              void      *ctx )
{
     D_DEBUG_AT( Font_Manager, "%s( object %p )\n", __func__, object );

     FindLruRowContext *context = ctx;
     DFBFontCache      *cache   = object;
     DFBFontCacheRow   *row;

     DFB_FONT_CACHE_ASSERT( cache );

     direct_list_foreach (row, cache->rows) {
          D_DEBUG_AT( Font_Manager, "  -> stamp %llu\n", row->stamp );

          if (!context->lru_row || context->lru_stamp > row->stamp) {
               context->lru_row   = row;
               context->lru_stamp = row->stamp;
          }
     }

     return DENUM_OK;
}

DFBResult
dfb_font_manager_remove_lru_row( DFBFontManager *manager )
{
     D_DEBUG_AT( Font_Manager, "%s()\n", __func__ );

     FindLruRowContext  context;
     DFBFontCache      *cache;

     DFB_FONT_MANAGER_ASSERT( manager );

     context.lru_stamp = 0;
     context.lru_row   = NULL;

     direct_map_iterate( manager->caches, find_lru_row, &context );

     if (!context.lru_row) {
          D_ERROR( "Core/Font: Could not find any row (LRU)!\n" );
          return DFB_ITEMNOTFOUND;
     }

     D_DEBUG_AT( Font_Manager, "  -> row %p (stamp %llu)\n", context.lru_row, context.lru_row->stamp );

     cache = context.lru_row->cache;
     DFB_FONT_CACHE_ASSERT( cache );

     direct_list_remove( &cache->rows, &context.lru_row->link );

     dfb_font_cache_row_destroy( context.lru_row );

     /* Decrease row counter. */
     manager->num_rows--;

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
dfb_font_cache_create( DFBFontManager          *manager,
                       const DFBFontCacheType  *type,
                       DFBFontCache           **ret_cache )
{
     DFB_FONT_MANAGER_ASSERT( manager );
     D_ASSERT( type != NULL );
     D_ASSERT( ret_cache != NULL );

     ///

     DFBResult     ret;
     DFBFontCache *cache;

     cache = D_CALLOC( 1, sizeof(DFBFontCache) );
     if (!cache)
          return D_OOM();

     ret = dfb_font_cache_init( cache, manager, type );
     if (ret) {
          D_FREE( cache );
          return ret;
     }

     *ret_cache = cache;

     return DFB_OK;
}

DFBResult
dfb_font_cache_destroy( DFBFontCache *cache )
{
     DFB_FONT_CACHE_ASSERT( cache );

     dfb_font_cache_deinit( cache );

     D_FREE( cache );

     return DFB_OK;
}

DFBResult
dfb_font_cache_init( DFBFontCache           *cache,
                     DFBFontManager         *manager,
                     const DFBFontCacheType *type )
{
     D_ASSERT( cache != NULL );
     D_ASSERT( type != NULL );
     DFB_FONT_MANAGER_ASSERT( manager );

     ///

     cache->manager = manager;
     cache->type    = *type;


     cache->row_width = 2048 * type->height / 64;

     if (cache->row_width > dfb_config->max_font_row_width)
          cache->row_width = dfb_config->max_font_row_width;

     if (cache->row_width < type->height)
          cache->row_width = type->height;

     cache->row_width = (cache->row_width + 7) & ~7;


     D_MAGIC_SET( cache, DFBFontCache );

     return DFB_OK;
}

DFBResult
dfb_font_cache_deinit( DFBFontCache *cache )
{
     DFBFontCacheRow *row;

     DFB_FONT_CACHE_ASSERT( cache );

     direct_list_foreach (row, cache->rows)
          dfb_font_cache_row_destroy( row );

     cache->rows = NULL;

     D_MAGIC_CLEAR( cache );

     return DFB_OK;
}

DFBResult
dfb_font_cache_get_row( DFBFontCache     *cache,
                        unsigned int      width,
                        DFBFontCacheRow **ret_row )
{
     DFBResult        ret;
     DFBFontManager  *manager;
     DFBFontCacheRow *row;
     DFBFontCacheRow *best_row = NULL;
     unsigned int     best_val = 0;

     DFB_FONT_CACHE_ASSERT( cache );
     D_ASSERT( ret_row != NULL );

     manager = cache->manager;
     DFB_FONT_MANAGER_ASSERT( manager );

     /* Try freshest row first. */
     row = (DFBFontCacheRow*) cache->rows;
     if (row && row->next_x + width <= cache->row_width) {
          *ret_row = row;

          return DFB_OK;
     }

     /* Check for trailing space in each row. */
     direct_list_foreach (row, cache->rows) {
          DFB_FONT_CACHE_ROW_ASSERT( row );

          /* If glyph fits... */
          if (row->next_x + width <= cache->row_width) {
               /* ...and no row yet or this row fits better... */
               if (!best_row || best_val < row->next_x) {
                    /* ...remember row */
                    best_row = row;
                    best_val = row->next_x;
               }
          }
     }

     if (best_row) {
          *ret_row = best_row;

          return DFB_OK;
     }


     /*
      * Need a new cache row
      */

     /* Maximum number of rows reached? */
     if (manager->num_rows == manager->max_rows) {
          /* Remove the least recently used row. */
          ret = dfb_font_manager_remove_lru_row( manager );
          if (ret)
               return ret;
     }

     D_ASSERT( manager->num_rows < manager->max_rows );

     /* Create another row. */
     ret = dfb_font_cache_row_create( cache, &row );
     if (ret)
          return ret;

     /* Prepend to list (freshest is first). */
     direct_list_prepend( &cache->rows, &row->link );

     /* Increase row counter in manager. */
     manager->num_rows++;

     *ret_row = row;

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
dfb_font_cache_row_create( DFBFontCache     *cache,
                           DFBFontCacheRow **ret_row )
{
     DFBResult        ret;
     DFBFontCacheRow *row;

     row = D_CALLOC( 1, sizeof(DFBFontCacheRow) );
     if (!row)
          return D_OOM();

     ret = dfb_font_cache_row_init( row, cache );
     if (ret) {
          D_FREE( row );
          return ret;
     }

     *ret_row = row;

     return DFB_OK;
}

DFBResult
dfb_font_cache_row_destroy( DFBFontCacheRow *row )
{
     DFB_FONT_CACHE_ROW_ASSERT( row );

     dfb_font_cache_row_deinit( row );

     D_FREE( row );

     return DFB_OK;
}

DFBResult
dfb_font_cache_row_init( DFBFontCacheRow *row,
                         DFBFontCache    *cache )
{
     DFBResult       ret;
     DFBFontManager *manager;

     DFB_FONT_CACHE_ASSERT( cache );

     manager = cache->manager;
     DFB_FONT_MANAGER_ASSERT( manager );

     row->cache = cache;

     /* Create a new font surface. */
     ret = dfb_surface_create_simple( manager->core,
                                      cache->row_width,
                                      cache->type.height,
                                      cache->type.pixel_format, DFB_COLORSPACE_DEFAULT(cache->type.pixel_format),
                                      cache->type.surface_caps,
                                      CSTF_FONT,
                                      0,
                                      NULL, &row->surface );
     if (ret) {
          D_DERROR( ret, "Core/Font: Could not create font surface!\n" );
          return ret;
     }

     D_DEBUG_AT( Core_FontSurfaces, "  -> new row %d - %dx%d %s\n", manager->num_rows,
                 row->surface->config.size.w, row->surface->config.size.h,
                 dfb_pixelformat_name(row->surface->config.format) );

     D_MAGIC_SET( row, DFBFontCacheRow );

     return DFB_OK;
}

DFBResult
dfb_font_cache_row_deinit( DFBFontCacheRow *row )
{
     CoreGlyphData *glyph, *next;

     DFB_FONT_CACHE_ROW_ASSERT( row );

     /* Kick out all glyphs. */
     direct_list_foreach_safe (glyph, next, row->glyphs) {
          CoreFont *font = glyph->font;

          D_MAGIC_ASSERT( glyph, CoreGlyphData );
          D_ASSERT( glyph->layer < D_ARRAY_SIZE(font->layers) );

          /*ret =*/ direct_hash_remove( font->layers[glyph->layer].glyph_hash, glyph->index );
          //FIXME: use D_ASSERT( ret == DFB_OK );

          if (glyph->index < 128)
               font->layers[glyph->layer].glyph_data[glyph->index] = NULL;

          D_MAGIC_CLEAR( glyph );
          D_FREE( glyph );
     }


     dfb_surface_unref( row->surface );

     D_MAGIC_CLEAR( row );

     return DFB_OK;
}

/**********************************************************************************************************************/
/**********************************************************************************************************************/

DFBResult
dfb_font_create( CoreDFB *core, CoreFont **ret_font )
{
     DFBResult  ret;
     int        i;
     CoreFont  *font;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_ASSERT( core != NULL );
     D_ASSERT( ret_font != NULL );

     font = D_CALLOC( 1, sizeof(CoreFont) );
     if (!font)
          return D_OOM();

     for (i=0; i<DFB_FONT_MAX_LAYERS; i++) {
          ret = direct_hash_create( 163, &font->layers[i].glyph_hash );
          if (ret) {
               while (i--)
                    direct_hash_destroy( font->layers[i].glyph_hash );

               D_FREE( font );
               return ret;
          }
     }

     font->core    = core;
     font->manager = dfb_core_font_manager( core );

     /* the proposed pixel_format, may be changed by the font provider */
     font->pixel_format = dfb_config->font_format ? : DSPF_A8;

     if ((font->pixel_format == DSPF_ARGB ||
                    font->pixel_format == DSPF_ABGR ||
                    font->pixel_format == DSPF_ARGB8565 ||
                    font->pixel_format == DSPF_ARGB4444 ||
                    font->pixel_format == DSPF_RGBA4444 ||
                    font->pixel_format == DSPF_ARGB1555 ||
                    font->pixel_format == DSPF_RGBA5551)
               && dfb_config->font_premult)
          font->surface_caps = DSCAPS_PREMULTIPLIED;

     font->blittingflags = DSBLIT_BLEND_ALPHACHANNEL | DSBLIT_COLORIZE;

     D_MAGIC_SET( font, CoreFont );

     *ret_font = font;

     return DFB_OK;
}

void
dfb_font_destroy( CoreFont *font )
{
     int i;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );

     dfb_font_dispose( font );

     for (i=0; i<DFB_FONT_MAX_LAYERS; i++)
          direct_hash_destroy( font->layers[i].glyph_hash );

     D_ASSERT( font->encodings != NULL || !font->last_encoding );

     for (i=DTEID_OTHER; i<=font->last_encoding; i++) {
          CoreFontEncoding *encoding = font->encodings[i];

          D_ASSERT( encoding != NULL );
          D_ASSERT( encoding->name != NULL );

          D_MAGIC_CLEAR( encoding );

          D_FREE( encoding->name );
          D_FREE( encoding );
     }

     if (font->encodings)
          D_FREE( font->encodings );

     D_MAGIC_CLEAR( font );

     D_FREE( font );
}

DFBResult
dfb_font_dispose( CoreFont *font )
{
     int i;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );

     dfb_font_manager_lock( font->manager );

     for (i=0; i<DFB_FONT_MAX_LAYERS; i++) {
          direct_hash_iterate( font->layers[i].glyph_hash, free_glyphs, NULL );

          memset( font->layers[i].glyph_data, 0, sizeof(font->layers[i].glyph_data) );
     }

     dfb_font_manager_unlock( font->manager );

     return DFB_OK;
}

/**********************************************************************************************************************/

DFBResult
dfb_font_get_glyph_data( CoreFont       *font,
                         unsigned int    index,
                         unsigned int    layer,
                         CoreGlyphData **ret_data )
{
     DFBResult        ret;
     CoreGlyphData   *data;
     int              align;
     DFBFontManager  *manager;
     DFBFontCache    *cache;
     DFBFontCacheRow *row = NULL;

     D_DEBUG_AT( Core_Font, "%s( index %u, layer %u )\n", __FUNCTION__, index, layer );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( ret_data != NULL );

     D_ASSERT( layer < D_ARRAY_SIZE(font->layers) );

     manager = font->manager;
     DFB_FONT_MANAGER_ASSERT( manager );

     /* Quick Lookup in array */
     if (index < 128 && font->layers[layer].glyph_data[index]) {
          *ret_data = font->layers[layer].glyph_data[index];
          return DFB_OK;
     }

     /* Standard lookup in hash */
     data = direct_hash_lookup( font->layers[layer].glyph_hash, index );
     if (data) {
          D_MAGIC_ASSERT( data, CoreGlyphData );

          D_DEBUG_AT( Core_Font, "  -> already in cache (%p)\n", data );

          row = data->row;
          if (row) {
               DFB_FONT_CACHE_ROW_ASSERT( row );

               row->stamp = manager->row_stamp++;
          }

          *ret_data = data;
          return DFB_OK;
     }


     /*
      * No glyph data available in cache, load new glyph
      */

     if (!font->GetGlyphData)
          return DFB_UNSUPPORTED;

     /* Allocate glyph data */
     data = D_CALLOC( 1, sizeof(CoreGlyphData) );
     if (!data)
          return D_OOM();

     D_MAGIC_SET( data, CoreGlyphData );

     data->font  = font;
     data->index = index;
     data->layer = layer;

     /* Get glyph data from font implementation */
     ret = font->GetGlyphData( font, index, data );
     if (ret) {
          D_DERROR( ret, "Core/Font: Could not get glyph info for index %d!\n", index );
          data->start = data->width = data->height = 0;
          goto out;
     }

     if (data->width < 1 || data->height < 1) {
          D_DEBUG_AT( Core_Font, "  -> zero size glyph bitmap!\n" );
          data->start = data->width = data->height = 0;
          goto out;
     }


     /* Get the proper cache based on size... */
     DFBFontCacheType type;

     type.height       = MAX( data->height, data->width );
     type.pixel_format = font->pixel_format;
     type.surface_caps = font->surface_caps;

     /* Avoid too many surface switches during one string rendering */
     type.height       = MAX( font->height, type.height );

     ret = dfb_font_manager_get_cache( font->manager, &type, &cache );
     if (ret) {
          D_DEBUG_AT( Core_Font, "  -> could not get cache from manager!\n" );
          goto error;
     }

     /* Check for a cache row (surface) to use */
     ret = dfb_font_cache_get_row( cache, data->width, &row );
     if (ret) {
          D_DEBUG_AT( Core_Font, "  -> could not get row from cache!\n" );
          goto error;
     }

     /*
      * Add the glyph to the cache row
      */

     D_DEBUG_AT( Core_FontSurfaces, "  -> render %2d - %2dx%2d at %03d font <%p>\n",
                 index, data->width, data->height, row->next_x, font );

     data->row     = row;
     data->start   = row->next_x;
     data->surface = row->surface;

     align = (8 / (DFB_BYTES_PER_PIXEL( font->pixel_format ) ? : 1)) *
                  (DFB_PIXELFORMAT_ALIGNMENT( font->pixel_format ) + 1) - 1;

     row->next_x  += (data->width + align) & ~align;

     row->stamp = manager->row_stamp++;

     /* Render the glyph data into the surface. */
     ret = font->RenderGlyph( font, index, data );
     if (ret) {
          D_DEBUG_AT( Core_Font, "  -> rendering glyph failed!\n" );
          data->start = data->width = data->height = 0;
          goto out;
     }

     dfb_gfxcard_flush_texture_cache();

     CORE_GLYPH_DATA_DEBUG_AT( Core_Font, data );


out:
     if (row)
          direct_list_append( &row->glyphs, &data->link );

     direct_hash_insert( font->layers[layer].glyph_hash, index, data );

     if (index < 128)
          font->layers[layer].glyph_data[index] = data;

     *ret_data = data;

     return DFB_OK;


error:
     D_MAGIC_CLEAR( data );
     D_FREE( data );

     return ret;
}

/**********************************************************************************************************************/

DFBResult
dfb_font_register_encoding( CoreFont                    *font,
                            const char                  *name,
                            const CoreFontEncodingFuncs *funcs,
                            DFBTextEncodingID            encoding_id )
{
     CoreFontEncoding  *encoding;
     CoreFontEncoding **encodings;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( encoding_id == DTEID_UTF8 || name != NULL );
     D_ASSERT( funcs != NULL );

     if (!funcs->GetCharacterIndex)
          return DFB_INVARG;

     /* Special case for default, native format. */
     if (encoding_id == DTEID_UTF8) {
          font->utf8 = funcs;

          return DFB_OK;
     }

     if (!funcs->DecodeText)
          return DFB_INVARG;

     /* Setup new encoding information. */
     encoding = D_CALLOC( 1, sizeof(CoreFontEncoding) );
     if (!encoding)
          return D_OOM();

     encoding->encoding = font->last_encoding + 1;
     encoding->funcs    = funcs;
     encoding->name     = D_STRDUP( name );

     if (!encoding->name) {
          D_FREE( encoding );
          return D_OOM();
     }

     /* Add to array. */
     encodings = D_REALLOC( font->encodings,
                            (encoding->encoding + 1) * sizeof(CoreFontEncoding*) );
     if (!encodings) {
          D_FREE( encoding->name );
          D_FREE( encoding );
          return D_OOM();
     }

     font->encodings = encodings;

     font->last_encoding++;

     D_ASSERT( font->last_encoding == encoding->encoding );

     encodings[encoding->encoding] = encoding;

     D_MAGIC_SET( encoding, CoreFontEncoding );

     return DFB_OK;
}

DFBResult
dfb_font_decode_text( CoreFont          *font,
                      DFBTextEncodingID  encoding,
                      const void        *text,
                      int                length,
                      unsigned int      *ret_indices,
                      int               *ret_num )
{
     int pos = 0, num = 0;
     const u8 *bytes = text;

     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( text != NULL );
     D_ASSERT( length >= 0 );  /* TODO: handle -1 here? */
     D_ASSERT( ret_indices != NULL );
     D_ASSERT( ret_num != NULL );

     if (encoding != DTEID_UTF8) {
          if (encoding > font->last_encoding)
               return DFB_IDNOTFOUND;

          D_ASSERT( font->encodings[encoding] != NULL );
          D_ASSERT( font->encodings[encoding]->funcs != NULL );
          D_ASSERT( font->encodings[encoding]->funcs->DecodeText != NULL );

          return font->encodings[encoding]->funcs->DecodeText( font, text, length,
                                                               ret_indices, ret_num );
     }
     else if (font->utf8) {
          const CoreFontEncodingFuncs *funcs = font->utf8;

          if (funcs->DecodeText)
               return funcs->DecodeText( font, text, length, ret_indices, ret_num );

          D_ASSERT( funcs->GetCharacterIndex != NULL );

          while (pos < length) {
               unsigned int c;

               if (bytes[pos] < 128)
                    c = bytes[pos++];
               else {
                    c = DIRECT_UTF8_GET_CHAR( &bytes[pos] );
                    pos += DIRECT_UTF8_SKIP(bytes[pos]);
               }

               if (funcs->GetCharacterIndex( font, c, &ret_indices[num] ) == DFB_OK)
                    num++;
          }

     }
     else {
          while (pos < length) {
               if (bytes[pos] < 128)
                    ret_indices[num++] = bytes[pos++];
               else {
                    ret_indices[num++] = DIRECT_UTF8_GET_CHAR( &bytes[pos] );
                    pos += DIRECT_UTF8_SKIP(bytes[pos]);
               }
          }
     }

     *ret_num = num;

     return DFB_OK;
}

DFBResult
dfb_font_decode_character( CoreFont          *font,
                           DFBTextEncodingID  encoding,
                           u32                character,
                           unsigned int      *ret_index )
{
     D_DEBUG_AT( Core_Font, "%s()\n", __FUNCTION__ );

     D_MAGIC_ASSERT( font, CoreFont );
     D_ASSERT( ret_index != NULL );

     if (encoding > font->last_encoding)
          return DFB_IDNOTFOUND;

     if (encoding != DTEID_UTF8) {
          D_ASSERT( font->encodings[encoding] != NULL );
          D_ASSERT( font->encodings[encoding]->funcs != NULL );
          D_ASSERT( font->encodings[encoding]->funcs->GetCharacterIndex != NULL );

          return font->encodings[encoding]->funcs->GetCharacterIndex( font, character, ret_index );
     }
     else if (font->utf8) {
          const CoreFontEncodingFuncs *funcs = font->utf8;

          D_ASSERT( funcs->GetCharacterIndex != NULL );

          return funcs->GetCharacterIndex( font, character, ret_index );
     }
     else
          *ret_index = character;

     return DFB_OK;
}

/**********************************************************************************************************************/

static bool
free_glyphs( DirectHash    *hash,
             unsigned long  key,
             void          *value,
             void          *ctx )
{
     D_DEBUG_AT( Core_Font, "%s( %lu )\n", __FUNCTION__, key );

     CoreGlyphData   *data = value;
     DFBFontCacheRow *row;

     D_MAGIC_ASSERT( data, CoreGlyphData );

     CORE_GLYPH_DATA_DEBUG_AT( Core_Font, data );

     /* Remove glyph from font. */
     direct_hash_remove( hash, key );


     row = data->row;
     if (row) {
          DFB_FONT_CACHE_ROW_ASSERT( row );

          /* Remove glyph from cache row. */
          direct_list_remove( &row->glyphs, &data->link );

          /* If cache row got empty, destroy it. */
          if (!row->glyphs) {
               DFBFontManager *manager;
               DFBFontCache   *cache = row->cache;

               DFB_FONT_CACHE_ASSERT( cache );

               manager = cache->manager;
               DFB_FONT_MANAGER_ASSERT( manager );

               /* Remove row from cache. */
               direct_list_remove( &cache->rows, &row->link );

               /* Destroy row. */
               dfb_font_cache_row_destroy( row );

               /* Decrease row counter in manager. */
               manager->num_rows--;
          }
     }


     D_MAGIC_CLEAR( data );

     D_FREE( data );

     return true;
}

