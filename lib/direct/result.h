/*
   (c) Copyright 2001-2008  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECT__RESULT_H__
#define __DIRECT__RESULT_H__

#include <direct/types.h>

/**********************************************************************************************************************/

#define D_RESULT_TYPE_CHAR_MASK         ((unsigned int) 0x2F)
#define D_RESULT_TYPE_CHAR_MIN          ((unsigned int) 0x30)
#define D_RESULT_TYPE_CHAR_MAX          (D_RESULT_TYPE_CHAR_MIN + D_RESULT_TYPE_CHAR_MASK)
#define D_RESULT_TYPE_CHAR_MUL_0        ((unsigned int) 1)
#define D_RESULT_TYPE_CHAR_MUL_1        ((unsigned int)(D_RESULT_TYPE_CHAR_MASK + 1))
#define D_RESULT_TYPE_CHAR_MUL_2        (D_RESULT_TYPE_CHAR_MUL_1 * D_RESULT_TYPE_CHAR_MUL_1)
#define D_RESULT_TYPE_CHAR_MUL_3        (D_RESULT_TYPE_CHAR_MUL_1 * D_RESULT_TYPE_CHAR_MUL_2)
#define D_RESULT_TYPE_CHAR( C )         (((unsigned int)(C) - D_RESULT_TYPE_CHAR_MIN) & D_RESULT_TYPE_CHAR_MASK)
#define D_RESULT_TYPE_SPACE             ((unsigned int)(0xFFFFFFFF / (D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_3 +   \
                                                                      D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_2 +   \
                                                                      D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_1 +   \
                                                                      D_RESULT_TYPE_CHAR_MASK * D_RESULT_TYPE_CHAR_MUL_0) - 1))

/**********************************************************************************************************************/

#ifndef DIRECT_DISABLE_DEPRECATED

// @deprecated
#define D_RESULT_TYPE_BASE( a,b,c )                         \
     D_RESULT_TYPE_CODE_BASE( '@', a,b,c )

// @deprecated
#define D_RESULT_TYPE_MAX( a,b,c )                          \
     (D_RESULT_TYPE_BASE( a,b,c ) + D_RESULT_TYPE_SPACE - 1)

// @deprecated
#define D_RESULT_TYPE_IS( code, a,b,c )                     \
     ((code) >= D_RESULT_TYPE_BASE(a,b,c) && (code) <= D_RESULT_TYPE_MAX(a,b,c))

#endif


/*
 * Generate result code base for API ('A','B','C','D'), e.g. ('C','O','R','E')
 *
 * Allowed are ASCII values between (inclusive)
 *   D_RESULT_TYPE_CHAR_MIN   (0x30)    and
 *   D_RESULT_TYPE_CHAR_MAX   (0x5F)
 *
 *
 *  0  1  2  3  4  5  6  7  8  9  :  ;  <  =  >  ?
 *
 *  @  A  B  C  D  E  F  G  H  I  J  K  L  M  N  O
 *
 *  P  Q  R  S  T  U  V  W  X  Y  Z  [  \  ]  ^  _
 *
 */
#define D_RESULT_TYPE_CODE_BASE(a,b,c,d)     ((D_RESULT_TYPE_CHAR( a ) * (D_RESULT_TYPE_CHAR_MUL_3) +  \
                                               D_RESULT_TYPE_CHAR( b ) * (D_RESULT_TYPE_CHAR_MUL_2) +  \
                                               D_RESULT_TYPE_CHAR( c ) * (D_RESULT_TYPE_CHAR_MUL_1) +  \
                                               D_RESULT_TYPE_CHAR( d ) * (D_RESULT_TYPE_CHAR_MUL_0)) * \
                                              D_RESULT_TYPE_SPACE)

#define D_RESULT_TYPE(code)                  ((code) - ((code) % D_RESULT_TYPE_SPACE))
#define D_RESULT_INDEX(code)                 ((code) % D_RESULT_TYPE_SPACE)

/**********************************************************************************************************************/

typedef enum {
     DR_OK,              /* No error occured */


     DR__RESULT_BASE = D_RESULT_TYPE_CODE_BASE( 'D','R','_', '1' ),

     DR_FAILURE,         /* A general or unknown error occured */
     DR_INIT,            /* A general initialization error occured */
     DR_BUG,             /* Internal bug or inconsistency has been detected */
     DR_DEAD,            /* Interface has a zero reference counter (available in debug mode) */
     DR_UNSUPPORTED,     /* The requested operation or an argument is (currently) not supported */
     DR_UNIMPLEMENTED,   /* The requested operation is not implemented, yet */
     DR_ACCESSDENIED,    /* Access to the resource is denied */
     DR_INVAREA,         /* An invalid area has been specified or detected */
     DR_INVARG,          /* An invalid argument has been specified */
     DR_NOLOCALMEMORY,   /* There's not enough local system memory */
     DR_NOSHAREDMEMORY,  /* There's not enough shared system memory */
     DR_LOCKED,          /* The resource is (already) locked */
     DR_BUFFEREMPTY,     /* The buffer is empty */
     DR_FILENOTFOUND,    /* The specified file has not been found */
     DR_IO,              /* A general I/O error occured */
     DR_BUSY,            /* The resource or device is busy */
     DR_NOIMPL,          /* No implementation for this interface or content type has been found */
     DR_TIMEOUT,         /* The operation timed out */
     DR_THIZNULL,        /* 'thiz' pointer is NULL */
     DR_IDNOTFOUND,      /* No resource has been found by the specified id */
     DR_DESTROYED,       /* The requested object has been destroyed */
     DR_FUSION,          /* Internal fusion error detected, most likely related to IPC resources */
     DR_BUFFERTOOLARGE,  /* Buffer is too large */
     DR_INTERRUPTED,     /* The operation has been interrupted */
     DR_NOCONTEXT,       /* No context available */
     DR_TEMPUNAVAIL,     /* Temporarily unavailable */
     DR_LIMITEXCEEDED,   /* Attempted to exceed limit, i.e. any kind of maximum size, count etc */
     DR_NOSUCHMETHOD,    /* Requested method is not known */
     DR_NOSUCHINSTANCE,  /* Requested instance is not known */
     DR_ITEMNOTFOUND,    /* No such item found */
     DR_VERSIONMISMATCH, /* Some versions didn't match */
     DR_EOF,             /* Reached end of file */
     DR_SUSPENDED,       /* The requested object is suspended */
     DR_INCOMPLETE,      /* The operation has been executed, but not completely */
     DR_NOCORE,          /* Core part not available */
     DR_SIGNALLED,       /* Received a signal, e.g. while waiting */
     DR_TASK_NOT_FOUND,  /* The corresponding task has not been found */

     DR__RESULT_END
} DirectResult;

/**********************************************************************************************************************/


typedef struct {
     int            magic;
     int            refs;

     unsigned int   base;

     const char   **result_strings;
     unsigned int   result_count;
} DirectResultType;


const char   DIRECT_API *DirectResultString( DirectResult result );

DirectResult DIRECT_API  DirectResultTypeRegister  ( DirectResultType *type );
DirectResult DIRECT_API  DirectResultTypeUnregister( DirectResultType *type );


/*
 * Example usage:
 *
 *   static const char *FooResult__strings[] = {
 *        "FooResult",        // result type name
 *
 *        "General foobar",   // FOO_GENERAL_FOOBAR
 *        "Too bar",          // FOO_TOO_BAR
 *   };
 *
 *
 * Corresponding example enumeration:
 *
 *   typedef enum {
 *        FOO__RESULT_BASE = D_RESULT_TYPE_BASE( 'F','o','o' ),
 *
 *        FOO_GENERAL_FOOBAR, // General foobar
 *        FOO_TOO_BAR,        // Too bar
 * 
 *        FOO__RESULT_END
 *   } FooResult;
 */


void __D_result_init( void );
void __D_result_deinit( void );

#endif

