/*
   (c) Copyright 2000  convergence integrated media GmbH.
   All rights reserved.

   Written by Denis Oliver Kropp <dok@convergence.de> and
              Andreas Hundt <andi@convergence.de>.

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


#ifndef __DFB_MATH_H__
#define __DFB_MATH_H__

#include <asm/types.h>


typedef struct {
     float v[4];
} Vector;

typedef struct {
     float v[16];
} Matrix;


typedef enum {
     X = 0,
     Y = 1,
     Z = 2,
     W = 3
} Vector_Elements;

typedef enum {
     X1 = 0,
     Y1 = 1,
     Z1 = 2,
     W1 = 3,
     X2 = 4,
     Y2 = 5,
     Z2 = 6,
     W2 = 7,
     X3 = 8,
     Y3 = 9,
     Z3 = 10,
     W3 = 11,
     X4 = 12,
     Y4 = 13,
     Z4 = 14,
     W4 = 15
} Matrix_Elements;


float vector_length( Vector *vector );
void  vector_scale( Vector *vector, float factor );


void matrix_transform( Matrix *matrix, Vector *source, Vector *destination );

Matrix *matrix_new_identity();
Matrix *matrix_new_perspective( float d );
void    matrix_multiply( Matrix *destination, Matrix *source );
void    matrix_translate( Matrix *matrix, float x, float y, float z );
void    matrix_scale( Matrix *matrix, float x, float y, float z );
void    matrix_rotate( Matrix *matrix, Vector_Elements axis, float angle );


#endif

