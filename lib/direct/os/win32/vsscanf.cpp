/*
   (c) Copyright 2012-2013  DirectFB integrated media GmbH
   (c) Copyright 2001-2013  The world wide DirectFB Open Source Community (directfb.org)
   (c) Copyright 2000-2004  Convergence (integrated media) GmbH

   All rights reserved.

   Written by Denis Oliver Kropp <dok@directfb.org>,
              Andreas Shimokawa <andi@directfb.org>,
              Marek Pikarski <mass@directfb.org>,
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


extern "C" {
#include <direct/debug.h>
#include <direct/util.h>
}

#define MAX_ARGS    10

#define ADD_ARGS_1  argPointer[0]
#define ADD_ARGS_2  argPointer[0], argPointer[1]
#define ADD_ARGS_3  argPointer[0], argPointer[1], argPointer[2]
#define ADD_ARGS_4  argPointer[0], argPointer[1], argPointer[2], argPointer[3]
#define ADD_ARGS_5  argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4]
#define ADD_ARGS_6  argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4], argPointer[5]
#define ADD_ARGS_7  argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4], argPointer[5], argPointer[6]
#define ADD_ARGS_8  argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4], argPointer[5], argPointer[6], argPointer[7]
#define ADD_ARGS_9  argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4], argPointer[5], argPointer[6], argPointer[7], argPointer[8]
#define ADD_ARGS_10 argPointer[0], argPointer[1], argPointer[2], argPointer[3], argPointer[4], argPointer[5], argPointer[6], argPointer[7], argPointer[8], argPointer[9]

int
direct_vsscanf( const char *str, LPCTSTR format, va_list arglist)
{
     int         numArgs;
     int         numScanned;
     void       *argPointer[MAX_ARGS];
     const char *currBuff;
     char        currChar;

     numArgs  = 0;
     currBuff = strchr( format, '%' );

     if ( currBuff == NULL ) {
          // No valid format specifier!
          D_ASSERT( !TRUE );
          return 0;
     }

     do {
          // Move pointer to next character
          currBuff++;
          currChar = *currBuff;

          if (currChar == NULL) {
               // End of string
               //      -> processing will stop!
          }
          else if (currChar == '*') {
               // "%*" suppresses argument assignment
               //      -> do not get argument from stack!
          }
          else if (currChar == '%') {
               // "%%" substitutes "%" character!
               //      -> do not get argument from stack!
               //      -> Increment to next character

               currBuff++;
          }
          else {
               if (numArgs >= MAX_ARGS) {
                    // This function can only handle
                    // <CSTRINGEX_SCANF_MAX_ARGS> (10) arguments!
                    D_ASSERT( !TRUE );
                    return 0;
               }

               argPointer[numArgs++] = va_arg( arglist, void * );
          }

          currBuff = strchr( currBuff, '%' );
     } while (currBuff != NULL);

     va_end( arglist );

     // Call sscanf with correct no. of arguments
     switch (numArgs) {
          case  0: return 0;
          case  1: return sscanf( str, format, ADD_ARGS_1  );
          case  2: return sscanf( str, format, ADD_ARGS_2  );
          case  3: return sscanf( str, format, ADD_ARGS_3  );
          case  4: return sscanf( str, format, ADD_ARGS_4  );
          case  5: return sscanf( str, format, ADD_ARGS_5  );
          case  6: return sscanf( str, format, ADD_ARGS_6  );
          case  7: return sscanf( str, format, ADD_ARGS_7  );
          case  8: return sscanf( str, format, ADD_ARGS_8  );
          case  9: return sscanf( str, format, ADD_ARGS_9  );
          case 10: return sscanf( str, format, ADD_ARGS_10 );
     }

     return 0;
}

