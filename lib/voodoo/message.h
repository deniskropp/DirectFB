/*
   (c) Copyright 2000-2002  convergence integrated media GmbH.
   (c) Copyright 2002-2004  convergence GmbH.

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Hundt <andi@fischlustig.de>,
              Sven Neumann <neo@directfb.org> and
              Ville Syrjälä <syrjala@sci.fi>.

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

#ifndef __VOODOO__MESSAGE_H__
#define __VOODOO__MESSAGE_H__

#include <voodoo/types.h>

#include <direct/debug.h>
#include <direct/memcpy.h>


#define VOODOO_MSG_ALIGN(i)     (((i) + 3) & ~3)

typedef enum {
     VMBT_NONE,
     VMBT_ID,
     VMBT_INT,
     VMBT_UINT,
     VMBT_DATA,
     VMBT_ODATA,
     VMBT_STRING
} VoodooMessageBlockType;

typedef enum {
     VREQ_NONE    = 0x00000000,
     VREQ_RESPOND = 0x00000001,
     VREQ_ASYNC   = 0x00000002,
     VREQ_QUEUE   = 0x00000004
} VoodooRequestFlags;

typedef enum {
     VMSG_SUPER,
     VMSG_REQUEST,
     VMSG_RESPONSE
} VoodooMessageType;


struct __V_VoodooMessageHeader {
     int                 size;
     VoodooMessageSerial serial;
     VoodooMessageType   type;
};


struct __V_VoodooSuperMessage {
     VoodooMessageHeader header;
};

struct __V_VoodooRequestMessage {
     VoodooMessageHeader header;

     VoodooInstanceID    instance;
     VoodooMethodID      method;

     VoodooRequestFlags  flags;
};

struct __V_VoodooResponseMessage {
     VoodooMessageHeader header;

     VoodooMessageSerial request;
     DirectResult        result;

     VoodooInstanceID    instance;
};


typedef struct {
     int         magic;

     const void *msg;
     const void *ptr;
} VoodooMessageParser;



#define __VOODOO_PARSER_PROLOG( parser, req_type )          \
     const void             *_vp_ptr;                       \
     VoodooMessageBlockType  _vp_type;                      \
     int                     _vp_length;                    \
                                                            \
     D_MAGIC_ASSERT( &(parser), VoodooMessageParser );      \
                                                            \
     _vp_ptr = (parser).ptr;                                \
                                                            \
     /* Read message block type. */                         \
     _vp_type = *(const __u32*) _vp_ptr;                    \
                                                            \
     D_ASSERT( _vp_type == (req_type) );                    \
                                                            \
     /* Read data block length. */                          \
     _vp_length = *(const __s32*) (_vp_ptr + 4)


#define __VOODOO_PARSER_EPILOG( parser )                    \
     /* Advance message data pointer. */                    \
     (parser).ptr += _vp_length + 8


#define VOODOO_PARSER_BEGIN( parser, message )                                                 \
     do {                                                                                      \
          const VoodooMessageHeader *_vp_header = (const VoodooMessageHeader *) (message);     \
                                                                                               \
          D_ASSERT( (message) != NULL );                                                       \
          D_ASSERT( _vp_header->type == VMSG_REQUEST || _vp_header->type == VMSG_RESPONSE );   \
                                                                                               \
          (parser).msg = (message);                                                            \
          (parser).ptr = (parser).msg + (_vp_header->type == VMSG_REQUEST ?                    \
                              sizeof(VoodooRequestMessage) : sizeof(VoodooResponseMessage));   \
                                                                                               \
          D_MAGIC_SET( &(parser), VoodooMessageParser );                                       \
     } while (0)


#define VOODOO_PARSER_GET_ID( parser, ret_id )                        \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_ID );                  \
                                                                      \
          D_ASSERT( _vp_length == 4 );                                \
                                                                      \
          /* Read the ID. */                                          \
          (ret_id) = *(const __u32*) (_vp_ptr + 8);                   \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_GET_INT( parser, ret_int )                      \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_INT );                 \
                                                                      \
          D_ASSERT( _vp_length == 4 );                                \
                                                                      \
          /* Read the integer. */                                     \
          (ret_int) = *(const __s32*) (_vp_ptr + 8);                  \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_GET_UINT( parser, ret_uint )                    \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_UINT );                \
                                                                      \
          D_ASSERT( _vp_length == 4 );                                \
                                                                      \
          /* Read the unsigned integer. */                            \
          (ret_uint) = *(const __u32*) (_vp_ptr + 8);                 \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_GET_DATA( parser, ret_data )                    \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_DATA );                \
                                                                      \
          D_ASSERT( _vp_length > 0 );                                 \
                                                                      \
          /* Return pointer to data. */                               \
          (ret_data) = _vp_ptr + 8;                                   \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_READ_DATA( parser, dst, max_len )               \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_DATA );                \
                                                                      \
          D_ASSERT( _vp_length > 0 );                                 \
          D_ASSERT( _vp_length <= max_len );                          \
                                                                      \
          /* Copy data block. */                                      \
          direct_memcpy( (dst), _vp_ptr + 8, _vp_length );            \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_COPY_DATA( parser, ret_data )                   \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_DATA );                \
                                                                      \
          D_ASSERT( _vp_length > 0 );                                 \
                                                                      \
          /* Allocate memory on the stack. */                         \
          (ret_data) = alloca( _vp_length );                          \
                                                                      \
          /* Copy data block. */                                      \
          direct_memcpy( (ret_data), _vp_ptr + 8, _vp_length );       \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_GET_ODATA( parser, ret_data )                   \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_ODATA );               \
                                                                      \
          D_ASSERT( _vp_length >= 0 );                                \
                                                                      \
          /* Return pointer to data or NULL. */                       \
          if (_vp_length)                                             \
               (ret_data) = _vp_ptr + 8;                              \
          else                                                        \
               (ret_data) = NULL;                                     \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)

#define VOODOO_PARSER_GET_STRING( parser, ret_string )                \
     do {                                                             \
          __VOODOO_PARSER_PROLOG( parser, VMBT_STRING );              \
                                                                      \
          D_ASSERT( _vp_length > 0 );                                 \
                                                                      \
          /* Return pointer to string. */                             \
          (ret_string) = (const char*) (_vp_ptr + 8);                 \
                                                                      \
          __VOODOO_PARSER_EPILOG( parser );                           \
     } while (0)


#define VOODOO_PARSER_END( parser )                                   \
     do {                                                             \
          D_MAGIC_ASSERT( &(parser), VoodooMessageParser );           \
                                                                      \
          D_ASSUME( *(const __u32*) ((parser).ptr) == VMBT_NONE );    \
                                                                      \
          D_MAGIC_CLEAR( &(parser) );                                 \
     } while (0)



#if 0


static inline void voodoo_parser_begin( VoodooMessageParser *parser, void *msg )
{
     VoodooMessageHeader *header = msg;

     D_ASSERT( parser != NULL );
     D_ASSERT( msg != NULL );
     D_ASSERT( header->type == VMSG_REQUEST || header->type == VMSG_RESPONSE );

     parser->msg = msg;
     parser->ptr = msg + (header->type == VMSG_REQUEST ?
                          sizeof(VoodooRequestMessage) : sizeof(VoodooResponseMessage));

     D_MAGIC_SET( parser, VoodooMessageParser );
}

static inline void voodoo_parser_read_id( VoodooMessageParser *parser, __u32 *ret_id )
{
     const void             *ptr;
     VoodooMessageBlockType  type;
     int                     length;

     D_MAGIC_ASSERT( parser, VoodooMessageParser );
     D_ASSERT( ret_id != NULL );

     ptr = parser->ptr;

     /* Read message block type. */
     type = *(__u32*) ptr;

     D_ASSERT( type == VMBT_ID );

     /* Read data block length. */
     length = *(__s32*) (ptr + 4);

     D_ASSERT( length == 4 );

     /* Read the ID. */
     *ret_id = *(__u32*) (ptr + 8);

     /* Advance message data pointer. */
     parser->ptr += length + 8;
}

static inline void voodoo_parser_read_int( VoodooMessageParser *parser, int *ret_int )
{
     const void             *ptr;
     VoodooMessageBlockType  type;
     int                     length;

     D_MAGIC_ASSERT( parser, VoodooMessageParser );
     D_ASSERT( ret_int != NULL );

     ptr = parser->ptr;

     /* Read message block type. */
     type = *(__u32*) ptr;

     D_ASSERT( type == VMBT_INT );

     /* Read data block length. */
     length = *(__s32*) (ptr + 4);

     D_ASSERT( length == 4 );

     /* Read the integer. */
     *ret_int = *(__u32*) (ptr + 8);

     /* Advance message data pointer. */
     parser->ptr += length + 8;
}

static inline void voodoo_parser_read_data( VoodooMessageParser *parser, void *dst, int size )
{
     const void             *ptr;
     VoodooMessageBlockType  type;
     int                     length;

     D_MAGIC_ASSERT( parser, VoodooMessageParser );
     D_ASSERT( dst != NULL );
     D_ASSERT( size > 0 );

     ptr = parser->ptr;

     /* Read message block type. */
     type = *(__u32*) ptr;

     D_ASSERT( type == VMBT_DATA );

     /* Read data block length. */
     length = *(__s32*) (ptr + 4);

     D_ASSERT( length > 0 );
     D_ASSERT( length <= size );

     /* Copy data block. */
     direct_memcpy( dst, ptr + 8, length );

     /* Advance message data pointer. */
     parser->ptr += length + 8;
}

static inline void voodoo_parser_read_string( VoodooMessageParser *parser, const char **ret_string )
{
     const void             *ptr;
     VoodooMessageBlockType  type;
     int                     length;

     D_MAGIC_ASSERT( parser, VoodooMessageParser );
     D_ASSERT( ret_string != NULL );

     ptr = parser->ptr;

     /* Read message block type. */
     type = *(__u32*) ptr;

     D_ASSERT( type == VMBT_STRING );

     /* Read data block length. */
     length = *(__s32*) (ptr + 4);

     D_ASSERT( length > 0 );

     /* Return pointer to the string, i.e. don't copy. */
     *ret_string = ptr + 8;

     /* Advance message data pointer. */
     parser->ptr += length + 8;
}

static inline void voodoo_parser_end( VoodooMessageParser *parser )
{
     D_MAGIC_ASSERT( parser, VoodooMessageParser );

     D_ASSUME( *(__u32*) parser->ptr == VMBT_NONE );

     D_MAGIC_CLEAR( parser );
}

#endif


#endif
