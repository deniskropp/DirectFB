/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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

#define DIRECT_ENABLE_DEBUG

#include <config.h>

#include <list>
#include <map>
#include <string>
#include <vector>

#include <directfb.h>

extern "C" {
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <png.h>

#include <directfb_strings.h>

#include <direct/debug.h>
#include <direct/list.h>
#include <direct/mem.h>
#include <direct/memcpy.h>
#include <direct/direct.h>

#include <gfx/convert.h>

#include <dgiff.h>
}


#define MAX_ROW_WIDTH    2047

D_DEBUG_DOMAIN( mkdgiff, "mkdgiff", "DirectFB Glyph Image File Format Tool" );

/**********************************************************************************************************************/

static DirectFBPixelFormatNames( format_names );
static DirectFBSurfaceBlittingFlagsNames( m_bflags );

static const char            *filename;
static DFBSurfacePixelFormat  m_format = DSPF_ARGB;

/**********************************************************************************************************************/

static void
print_usage (const char *prg_name)
{
     int i = 0;

     fprintf (stderr, "\nDirectFB Glyph Image File Format Tool (version %s)\n\n", DIRECTFB_VERSION);
     fprintf (stderr, "Usage: %s [options]\n\n", prg_name);
     fprintf (stderr, "Options:\n");
     fprintf (stderr, "   -f, --format   <pixelformat>   Choose the pixel format (default ARGB)\n");
     fprintf (stderr, "   -s, --sizes    <s1>[,s2...]    Choose sizes to generate glyph images for\n");
     fprintf (stderr, "   -h, --help                     Show this help message\n");
     fprintf (stderr, "   -v, --version                  Print version information\n");
     fprintf (stderr, "\n");

     fprintf (stderr, "Known pixel formats with alpha:\n");

     while (format_names[i].format != DSPF_UNKNOWN) {
          DFBSurfacePixelFormat format = format_names[i].format;

          if (DFB_PIXELFORMAT_HAS_ALPHA(format)) {
               fprintf (stderr, "   %-10s %2d bits, %d bytes",
                        format_names[i].name, DFB_BITS_PER_PIXEL(format),
                        DFB_BYTES_PER_PIXEL(format));

               if (DFB_PIXELFORMAT_IS_INDEXED(format))
                    fprintf (stderr, "   INDEXED");

               if (DFB_PLANAR_PIXELFORMAT(format)) {
                    int planes = DFB_PLANE_MULTIPLY(format, 1000);

                    fprintf (stderr, "   PLANAR (x%d.%03d)",
                             planes / 1000, planes % 1000);
               }

               fprintf (stderr, "\n");
          }

          ++i;
     }
     fprintf (stderr, "\n");
}

static DFBBoolean
parse_format( const char *arg, DFBSurfacePixelFormat *ret_format )
{
     int i = 0;

     while (format_names[i].format != DSPF_UNKNOWN) {
          if (!strcasecmp( arg, format_names[i].name )) {
               *ret_format = format_names[i].format;
               return DFB_TRUE;
          }

          ++i;
     }

     fprintf (stderr, "\nInvalid format specified!\n\n" );

     return DFB_FALSE;
}

static void
parse_flags( const char *arg, DFBSurfaceBlittingFlags *ret_flags )
{
     int i;

     *ret_flags = DSBLIT_NOFX;

     for (i=0; i<D_ARRAY_SIZE(m_bflags); i++) {
          if (strcasestr( arg, m_bflags[i].name ))
               *ret_flags = (DFBSurfaceBlittingFlags)(*ret_flags | m_bflags[i].flag);
     }
}

static DFBBoolean
parse_command_line( int argc, char *argv[] )
{
     int n;

     for (n = 1; n < argc; n++) {
          const char *arg = argv[n];

          if (strcmp (arg, "-h") == 0 || strcmp (arg, "--help") == 0) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-v") == 0 || strcmp (arg, "--version") == 0) {
               fprintf (stderr, "mkdgiff version %s\n", DIRECTFB_VERSION);
               return DFB_FALSE;
          }

          if (strcmp (arg, "-f") == 0 || strcmp (arg, "--format") == 0) {
               if (++n == argc) {
                    print_usage (argv[0]);
                    return DFB_FALSE;
               }

               if (!parse_format( argv[n], &m_format ))
                    return DFB_FALSE;

               continue;
          }

          if (filename || access( arg, R_OK )) {
               print_usage (argv[0]);
               return DFB_FALSE;
          }

          filename = arg;
     }

     if (!filename) {
          print_usage (argv[0]);
          return DFB_FALSE;
     }

     return DFB_TRUE;
}

/**********************************************************************************************************************/

class Entity
{
public:
     Entity()
     {
     }


     typedef enum {
          ENTITY_NONE,

          ENTITY_FACE,
          ENTITY_GLYPH
     } Type;

     virtual Type GetType() const = 0;


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     const char    *buf;
     size_t         length;


     typedef std::list<Entity*>   list;
     typedef std::vector<Entity*> vector;
};

class Face : public Entity
{
public:
     Face()
          :
          Entity(),
          size( 0 ),
          height( 0 ),
          ascender( 0 ),
          descender( 0 ),
          maxadvance( 0 ),
          blittingflags( DSBLIT_NOFX )
     {
     }


     virtual Type GetType() const { return ENTITY_FACE; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     unsigned int             size;
     unsigned int             height;
     int                      ascender;
     int                      descender;
     int                      maxadvance;
     DFBSurfaceBlittingFlags  blittingflags;
};

class Glyph : public Entity
{
public:
     Glyph()
          :
          Entity(),
          unicode( 0 ),
          left( 0 ),
          top( 0 ),
          advance( 0 )
     {
     }


     virtual Type GetType() const { return ENTITY_GLYPH; }


     virtual void Dump() const;

     virtual void SetProperty( const std::string &name, const std::string &value );


     unsigned int   unicode;
     int            left;
     int            top;
     int            advance;
     std::string    file;
};

/**********************************************************************************************************************/

void
Entity::Dump() const
{
     direct_log_printf( NULL, "\n" );
     direct_log_printf( NULL, "Entity (TYPE %d)\n", GetType() );
     direct_log_printf( NULL, "  Buffer at        %p [%zu]\n", buf, length );
}

void
Face::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Size             %u\n", size );
     direct_log_printf( NULL, "  Height           %u\n", height );
     direct_log_printf( NULL, "  Ascender         %d\n", ascender );
     direct_log_printf( NULL, "  Descender        %d\n", descender );
     direct_log_printf( NULL, "  MaxAdvance       %d\n", maxadvance );
     direct_log_printf( NULL, "  BlittingFlags    0x%08x\n", blittingflags );
}

void
Glyph::Dump() const
{
     Entity::Dump();

     direct_log_printf( NULL, "  Unicode          0x%04x\n", unicode );
     direct_log_printf( NULL, "  Left             %d\n", left );
     direct_log_printf( NULL, "  Top              %d\n", top );
     direct_log_printf( NULL, "  Advance          %d\n", advance );
     direct_log_printf( NULL, "  File             '%s'\n", file.c_str() );
}

/**********************************************************************************************************************/

void
Entity::SetProperty( const std::string &name,
                     const std::string &value )
{
}

void
Face::SetProperty( const std::string &name,
                   const std::string &value )
{
     if (name == "size") {
          sscanf( value.c_str(), "%u", &size );
          return;
     }

     if (name == "height") {
          sscanf( value.c_str(), "%u", &height );
          return;
     }

     if (name == "ascender") {
          sscanf( value.c_str(), "%d", &ascender );
          return;
     }

     if (name == "descender") {
          sscanf( value.c_str(), "%d", &descender );
          return;
     }

     if (name == "maxadvance") {
          sscanf( value.c_str(), "%d", &maxadvance );
          return;
     }

     if (name == "blittingflags") {
          parse_flags( value.c_str(), &blittingflags );
          return;
     }
}

void
Glyph::SetProperty( const std::string &name,
                    const std::string &value )
{
     if (name == "unicode") {
          sscanf( value.c_str(), "%x", &unicode );
          return;
     }

     if (name == "left") {
          sscanf( value.c_str(), "%d", &left );
          return;
     }

     if (name == "top") {
          sscanf( value.c_str(), "%d", &top );
          return;
     }

     if (name == "advance") {
          sscanf( value.c_str(), "%d", &advance );
          return;
     }

     if (name == "file") {
          file = value;
          return;
     }
}

/**********************************************************************************************************************/

static void
get_entities( const char     *buf,
              size_t          length,
              Entity::vector &out_vector )
{
     size_t       i;
     unsigned int level = 0;
     bool         quote = false;

     std::string                        name;
     std::map<unsigned int,std::string> names;

     Entity *entity = NULL;

     D_DEBUG_AT( mkdgiff, "%s( buf %p, length %zu )\n", __func__, buf, length );

     for (i=0; i<length; i++) {
          D_DEBUG_AT( mkdgiff, "%*s[%u]  -> '%c' <-\n", level*2, "", level, buf[i] );

          if (quote) {
               switch (buf[i]) {
                    case '"':
                         quote = false;
                         break;

                    default:
                         name += buf[i];
               }
          }
          else {
               switch (buf[i]) {
                    case '"':
                         quote = true;
                         break;

                    case '-':
                    case '_':
                    case 'a' ... 'z':
                    case 'A' ... 'Z':
                    case '0' ... '9':
                         name += buf[i];
                         break;
     
                    default:
                         if (!name.empty()) {
                              D_DEBUG_AT( mkdgiff, "%*s=-> name = '%s'\n", level*2, "", name.c_str() );
     
                              if (!names[level].empty()) {
                                   switch (level) {
                                        case 1:
                                             D_DEBUG_AT( mkdgiff, "%*s#### setting property '%s' = '%s'\n",
                                                         level*2, "", names[level].c_str(), name.c_str() );
     
                                             D_ASSERT( entity != NULL );
     
                                             entity->SetProperty( names[level], name );
                                             break;
     
                                        default:
                                             break;
                                   }
     
                                   name = "";
                              }
     
                              names[level] = name;
                              name         = "";
                         }
     
                         switch (buf[i]) {
                              case '{':
                              case '}':
                                   switch (buf[i]) {
                                        case '{':
                                             switch (level) {
                                                  case 0:
                                                       if (names[level] == "face") {
                                                            D_ASSERT( entity == NULL );
     
                                                            entity = new Face();
     
                                                            entity->buf = &buf[i + 1];
     
                                                            D_DEBUG_AT( mkdgiff, "%*s#### open entity %p (Face)\n", level*2, "", entity );
                                                       }
                                                       if (names[level] == "glyph") {
                                                            D_ASSERT( entity == NULL );
     
                                                            entity = new Glyph();
     
                                                            entity->buf = &buf[i + 1];
     
                                                            D_DEBUG_AT( mkdgiff, "%*s#### open entity %p (Glyph)\n", level*2, "", entity );
                                                       }
                                                       break;
     
                                                  default:
                                                       break;
                                             }

                                             names[level] = "";
     
                                             level++;
                                             break;
     
                                        case '}':
                                             D_ASSERT( names[level].empty() );
     
                                             level--;
     
                                             switch (level) {
                                                  case 0:
                                                       D_DEBUG_AT( mkdgiff, "%*s#### close entity %p\n", level*2, "", entity );
     
                                                       D_ASSERT( entity != NULL );
     
                                                       entity->length = &buf[i-1] - entity->buf;
     
                                                       out_vector.push_back( entity );
     
                                                       entity = NULL;
                                                       break;
     
                                                  case 1:
                                                       break;
     
                                                  default:
                                                       break;
                                             }
                                             break;
                                   }
     
                                   D_DEBUG_AT( mkdgiff, "%*s=-> level => %u\n", level*2, "", level );
                                   break;
     
                              case ' ':
                              case '\t':
                              case '\n':
                              case '\r':
                                   break;
     
                              default:
                                   break;
                         }
                         break;
               }
          }
     }
}

/**********************************************************************************************************************/

static DFBResult
load_image (const char            *filename,
            DFBSurfaceDescription *desc)
{
     DFBSurfacePixelFormat dest_format;
     DFBSurfacePixelFormat src_format;
     FILE          *fp;
     png_structp    png_ptr  = NULL;
     png_infop      info_ptr = NULL;
     png_uint_32    width, height;
     unsigned char *data     = NULL;
     int            type;
     char           header[8];
     int            bytes, pitch;

     dest_format = (desc->flags & DSDESC_PIXELFORMAT) ? desc->pixelformat : DSPF_UNKNOWN;

     desc->flags                = DSDESC_NONE;
     desc->preallocated[0].data = NULL;

     if (!(fp = fopen (filename, "rb"))) {
          fprintf (stderr, "Failed to open file '%s': %s.\n",
                   filename, strerror (errno));
          goto cleanup;
     }

     bytes = fread (header, 1, sizeof(header), fp);
     if (png_sig_cmp ((unsigned char*) header, 0, bytes)) {
          fprintf (stderr, "File '%s' doesn't seem to be a PNG image file.\n",
                   filename);
          goto cleanup;
     }

     png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING,
                                       NULL, NULL, NULL);
     if (!png_ptr)
          goto cleanup;

     if (setjmp (png_jmpbuf(png_ptr))) {
          if (desc->preallocated[0].data) {
               free (desc->preallocated[0].data);
               desc->preallocated[0].data = NULL;
          }

          /* data might have been clobbered,
             set it to NULL and leak instead of crashing */
          data = NULL;

          goto cleanup;
     }

     info_ptr = png_create_info_struct (png_ptr);
     if (!info_ptr)
          goto cleanup;

     png_init_io (png_ptr, fp);
     png_set_sig_bytes (png_ptr, bytes);

     png_read_info (png_ptr, info_ptr);

     png_get_IHDR (png_ptr, info_ptr,
                   &width, &height, &bytes, &type, NULL, NULL, NULL);

     if (bytes == 16)
          png_set_strip_16 (png_ptr);

#ifdef WORDS_BIGENDIAN
     png_set_swap_alpha (png_ptr);
#else
     png_set_bgr (png_ptr);
#endif

     src_format = (type & PNG_COLOR_MASK_ALPHA) ? DSPF_ARGB : DSPF_RGB32;
     switch (type) {
          case PNG_COLOR_TYPE_GRAY:
               if (dest_format == DSPF_A8) {
                    src_format = DSPF_A8;
                    break;
               }
               /* fallthru */
          case PNG_COLOR_TYPE_GRAY_ALPHA:
               png_set_gray_to_rgb (png_ptr);
//               if (rgbformat)
//                    dest_format = rgbformat;
               break;

          case PNG_COLOR_TYPE_PALETTE:
               png_set_palette_to_rgb (png_ptr);
               /* fallthru */
          case PNG_COLOR_TYPE_RGB:
//               if (rgbformat)
//                    dest_format = rgbformat;
          case PNG_COLOR_TYPE_RGB_ALPHA:
               if (dest_format == DSPF_RGB24) {
                    png_set_strip_alpha (png_ptr);
                    src_format = DSPF_RGB24;
               }
               break;
       }

     switch (src_format) {
          case DSPF_RGB32:
                png_set_filler (png_ptr, 0xFF,
#ifdef WORDS_BIGENDIAN
                                PNG_FILLER_BEFORE
#else
                                PNG_FILLER_AFTER
#endif
                                );
                break;
          case DSPF_ARGB:
          case DSPF_A8:
               if (png_get_valid (png_ptr, info_ptr, PNG_INFO_tRNS))
                    png_set_tRNS_to_alpha (png_ptr);
               break;
          default:
               break;
     }

     pitch = (DFB_BYTES_PER_LINE( src_format, width ) + 7) & ~7;

     data  = (unsigned char*) malloc (height * pitch);
     if (!data) {
          fprintf (stderr, "Failed to allocate %lu bytes.\n", (unsigned long)(height * pitch));
          goto cleanup;
     }

     {
          unsigned int i;
          png_bytep bptrs[height];

          for (i = 0; i < height; i++)
               bptrs[i] = data + i * pitch;

          png_read_image (png_ptr, bptrs);
     }

     if (!dest_format)
          dest_format = src_format;

     if (DFB_BYTES_PER_PIXEL(src_format) != DFB_BYTES_PER_PIXEL(dest_format)) {
          unsigned char *s, *d, *dest;
          int            d_pitch, h;

          D_ASSERT( DFB_BYTES_PER_PIXEL(src_format) == 4 );

          d_pitch = (DFB_BYTES_PER_LINE(dest_format, width) + 7) & ~7;

          dest = (unsigned char*) malloc (height * d_pitch);
          if (!dest) {
               fprintf (stderr, "Failed to allocate %lu bytes.\n", (unsigned long)(height * d_pitch));
               goto cleanup;
          }

          h = height;
          switch (dest_format) {
               case DSPF_RGB16:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_rgb16 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB8565:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb8565 ((u32 *) s, (u8 *) d, width);
                    break;
               case DSPF_ARGB1555:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb1555 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_RGBA5551:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_rgba5551 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB2554:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb2554 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_ARGB4444:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_argb4444 ((u32 *) s, (u16 *) d, width);
                    break;
               case DSPF_RGB332:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_rgb332 ((u32 *) s, (u8 *) d, width);
                    break;
               case DSPF_A8:
                    for (s = data, d = dest; h; h--, s += pitch, d += d_pitch)
                         dfb_argb_to_a8 ((u32 *) s, (u8 *) d, width);
                    break;
               default:
                    fprintf (stderr,
                             "Sorry, unsupported format conversion.\n");
                    goto cleanup;
          }

          free (data);
          data = dest;
          pitch = d_pitch;
     }

     desc->flags = (DFBSurfaceDescriptionFlags)(DSDESC_WIDTH | DSDESC_HEIGHT | DSDESC_PIXELFORMAT |
                                                DSDESC_PREALLOCATED);
     desc->width       = width;
     desc->height      = height;
     desc->pixelformat = dest_format;
     desc->preallocated[0].pitch = pitch;
     desc->preallocated[0].data  = data;

     data = NULL;

 cleanup:
     if (fp)
          fclose (fp);

     if (png_ptr)
          png_destroy_read_struct (&png_ptr, &info_ptr, NULL);

     if (data)
          free (data);

     return ((desc->flags) ? DFB_OK : DFB_FAILURE);
}

/**********************************************************************************************************************/

static void
write_glyph( DGIFFGlyphInfo *glyph, const DFBSurfaceDescription &desc, void *dst, int pitch )
{
     D_DEBUG_AT( mkdgiff, "%s( %p, %p, %d ) <- size %ux%u\n",
                 __FUNCTION__, glyph, dst, pitch, desc.width, desc.height );

     switch (m_format) {
          case DSPF_ARGB:
               dfb_convert_to_argb( desc.pixelformat, desc.preallocated[0].data, desc.preallocated[0].pitch,
                                    desc.height, (u32*) dst, pitch, desc.width, desc.height );
               break;

          default:
               D_UNIMPLEMENTED();
               break;
     }
}

static int
do_face( const Face *face )
{
     int              i, ret;
     int              align        = DFB_PIXELFORMAT_ALIGNMENT( m_format );
     int              num_glyphs   = 0;
     int              num_rows     = 1;
     int              row_index    = 0;
     int              row_offset   = 0;
     int              next_face    = sizeof(DGIFFFaceHeader);
     int              total_height = 0;

     Entity::vector   glyph_vector;
     unsigned int     glyph_count = 0;

     DGIFFFaceHeader  header;
     DGIFFGlyphInfo  *glyphs;
     DGIFFGlyphRow   *rows;
     void           **row_data;

     DFBSurfaceDescription *descs;

     D_DEBUG_AT( mkdgiff, "%s( %p )\n", __FUNCTION__, face );

     get_entities( face->buf, face->length, glyph_vector );

     glyph_count = glyph_vector.size();


     /* Clear to not leak any data into file. */
     memset( &header, 0, sizeof(header) );


     /* Allocate glyph info array. */
     glyphs   = (DGIFFGlyphInfo*)        D_CALLOC( glyph_count, sizeof(DGIFFGlyphInfo) );
     rows     = (DGIFFGlyphRow*)         D_CALLOC( glyph_count, sizeof(DGIFFGlyphRow) );            /* WORST case :) */
     row_data = (void**)                 D_CALLOC( glyph_count, sizeof(void*) );                    /* WORST case :) */
     descs    = (DFBSurfaceDescription*) D_CALLOC( glyph_count, sizeof(DFBSurfaceDescription) );    /* WORST case :) */

     for (Entity::vector::const_iterator iter = glyph_vector.begin(); iter != glyph_vector.end(); iter++) {
          const Glyph *glyph = dynamic_cast<const Glyph*>( *iter );

          glyph->Dump();


          DGIFFGlyphInfo *info = &glyphs[num_glyphs];
          DGIFFGlyphRow  *row  = &rows[num_rows - 1];

          D_DEBUG_AT( mkdgiff, "  -> code %3u\n", glyph->unicode );

          ret = load_image( glyph->file.c_str(), &descs[num_glyphs] );
          if (ret)
               continue;

          info->unicode = glyph->unicode;

          info->width   = descs[num_glyphs].width;
          info->height  = descs[num_glyphs].height;

          info->left    = glyph->left;
          info->top     = glyph->top;
          info->advance = glyph->advance;

          num_glyphs++;

          if (row->width > 0 && row->width + info->width > MAX_ROW_WIDTH) {
               num_rows++;
               row++;
          }

          row->width += (info->width + align) & ~align;

          if (row->height < info->height)
               row->height = info->height;
     }

     for (i=0; i<num_rows; i++) {
          DGIFFGlyphRow *row = &rows[i];

          D_DEBUG_AT( mkdgiff, "  ->   row %d, width %d, height %d\n", i, row->width, row->height );

          total_height += row->height;

          row->pitch = (DFB_BYTES_PER_LINE( m_format, row->width ) + 7) & ~7;

          row_data[i] = D_CALLOC( row->height, row->pitch );

          next_face += row->height * row->pitch;
     }

     D_DEBUG_AT( mkdgiff, "  -> %d glyphs, %d rows, total height %d\n", num_glyphs, num_rows, total_height );

     next_face += num_glyphs * sizeof(DGIFFGlyphInfo);
     next_face += num_rows * sizeof(DGIFFGlyphRow);

     for (i=0; i<num_glyphs; i++) {
          DGIFFGlyphInfo *glyph = &glyphs[i];

          D_DEBUG_AT( mkdgiff, "  -> writing character 0x%x (%d)\n", glyph->unicode, i );

          if (row_offset > 0 && row_offset + glyph->width > MAX_ROW_WIDTH) {
               row_index++;
               row_offset = 0;
          }


          D_DEBUG_AT( mkdgiff, "  -> row offset %d\n", row_offset );

          write_glyph( glyph, descs[i],
                       (char*) row_data[row_index] + DFB_BYTES_PER_LINE( m_format, row_offset ),
                       rows[row_index].pitch );

          glyph->row    = row_index;
          glyph->offset = row_offset;

          row_offset += (glyph->width + align) & ~align;
     }

     D_ASSERT( row_index == num_rows - 1 );

     header.next_face   = next_face;
     header.size        = face->size;

     header.ascender    = face->ascender;
     header.descender   = face->descender;
     header.height      = face->height;

     header.max_advance = face->maxadvance;

     header.pixelformat = m_format;

     header.num_glyphs  = num_glyphs;
     header.num_rows    = num_rows;

     header.blittingflags = face->blittingflags;

     D_DEBUG_AT( mkdgiff, "  -> ascender %d, descender %d\n", header.ascender, header.descender );
     D_DEBUG_AT( mkdgiff, "  -> height %d, max advance %d\n", header.height, header.max_advance );

     fwrite( &header, sizeof(header), 1, stdout );

     fwrite( glyphs, sizeof(*glyphs), num_glyphs, stdout );

     for (i=0; i<num_rows; i++) {
          DGIFFGlyphRow *row = &rows[i];

          fwrite( row, sizeof(*row), 1, stdout );

          fwrite( row_data[i], row->pitch, row->height, stdout );
     }

     for (i=0; i<num_rows; i++) {
          if (row_data[i])
               D_FREE( row_data[i] );
     }

     D_FREE( row_data );
     D_FREE( rows );
     D_FREE( glyphs );

     return 0;
}

/**********************************************************************************************************************/

int
main( int argc, char *argv[] )
{
     int             ret;
     int             fd;
     struct stat     stat;
     void           *ptr  = MAP_FAILED;
     Entity::vector  faces;
     DGIFFHeader     header = {
          magic: { 'D', 'G', 'I', 'F', 'F' },
          major: 0,
          minor: 0,
          flags: DGIFF_FLAG_LITTLE_ENDIAN,
          num_faces: 0
     };

     direct_initialize();

     direct_debug_config_domain( "mkdgiff", true );

     direct_config->debug    = true;
     direct_config->debugmem = true;

     /* Parse the command line. */
     if (!parse_command_line( argc, argv ))
          return -1;


     /* Open the file. */
     fd = open( filename, O_RDONLY );
     if (fd < 0) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during open() of '%s'!\n", filename );
          return ret;
     }

     /* Query file size etc. */
     if (fstat( fd, &stat ) < 0) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during fstat() of '%s'!\n", filename );
          goto out;
     }

     /* Memory map the file. */
     ptr = mmap( NULL, stat.st_size, PROT_READ, MAP_SHARED, fd, 0 );
     if (ptr == MAP_FAILED) {
          ret = errno2result( errno );
          D_PERROR( "Font/DGIFF: Failure during mmap() of '%s'!\n", filename );
          goto out;
     }


     get_entities( (const char*) ptr, stat.st_size, faces );

     header.num_faces = faces.size();



     fwrite( &header, sizeof(header), 1, stdout );

     for (Entity::vector::const_iterator iter = faces.begin(); iter != faces.end(); iter++) {
          const Face *face = dynamic_cast<const Face*>( *iter );

          face->Dump();

          ret = do_face( face );
          if (ret)
               goto out;
     }


out:
     if (ptr != MAP_FAILED)
          munmap( ptr, stat.st_size );

     close( fd );

     direct_print_memleaks();

     direct_shutdown();

     return ret;
}

