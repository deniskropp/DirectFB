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


#include <math.h>
#include <malloc.h>

#include "math3d.h"

static const Matrix identity = { { 1, 0, 0, 0,
                                   0, 1, 0, 0,
                                   0, 0, 1, 0,
                                   0, 0, 0, 1 } };

float vector_length( Vector *vector )
{
     return sqrt( vector->v[X] * vector->v[X] +
                  vector->v[Y] * vector->v[Y] +
                  vector->v[Z] * vector->v[Z] );
}

void vector_scale( Vector *vector, float factor )
{
     vector->v[X] *= factor;
     vector->v[Y] *= factor;
     vector->v[Z] *= factor;
}

void matrix_transform( Matrix *matrix, Vector *source, Vector *destination )
{
/*     printf( "source: %f, %f, %f, %f\n",
             source->v[X], source->v[Y], source->v[Z], source->v[W] );*/

     destination->v[X] = matrix->v[X1] * source->v[X] +
                         matrix->v[Y1] * source->v[Y] +
                         matrix->v[Z1] * source->v[Z] +
                         matrix->v[W1] * source->v[W];

     destination->v[Y] = matrix->v[X2] * source->v[X] +
                         matrix->v[Y2] * source->v[Y] +
                         matrix->v[Z2] * source->v[Z] +
                         matrix->v[W2] * source->v[W];

     destination->v[Z] = matrix->v[X3] * source->v[X] +
                         matrix->v[Y3] * source->v[Y] +
                         matrix->v[Z3] * source->v[Z] +
                         matrix->v[W3] * source->v[W];

     destination->v[W] = matrix->v[X4] * source->v[X] +
                         matrix->v[Y4] * source->v[Y] +
                         matrix->v[Z4] * source->v[Z] +
                         matrix->v[W4] * source->v[W];

/*     printf( "destination: %f, %f, %f, %f\n",
             destination->v[X], destination->v[Y], destination->v[Z], destination->v[W] );*/
}

Matrix *matrix_new_identity()
{
     Matrix *m = malloc( sizeof(Matrix) );

     memcpy( m, &identity, sizeof(Matrix) );

     return m;
}

Matrix *matrix_new_perspective( float d )
{
     Matrix *m = matrix_new_identity();

     m->v[Z4] = 1.0f / d;

     return m;
}

void matrix_multiply( Matrix *destination, Matrix *source )
{
     float tmp[4];

     tmp[0] = source->v[X1] * destination->v[X1] +
              source->v[Y1] * destination->v[X2] +
              source->v[Z1] * destination->v[X3] +
              source->v[W1] * destination->v[X4];
     tmp[1] = source->v[X2] * destination->v[X1] +
              source->v[Y2] * destination->v[X2] +
              source->v[Z2] * destination->v[X3] +
              source->v[W2] * destination->v[X4];
     tmp[2] = source->v[X3] * destination->v[X1] +
              source->v[Y3] * destination->v[X2] +
              source->v[Z3] * destination->v[X3] +
              source->v[W3] * destination->v[X4];
     tmp[3] = source->v[X4] * destination->v[X1] +
              source->v[Y4] * destination->v[X2] +
              source->v[Z4] * destination->v[X3] +
              source->v[W4] * destination->v[X4];

     destination->v[X1] = tmp[0];
     destination->v[X2] = tmp[1];
     destination->v[X3] = tmp[2];
     destination->v[X4] = tmp[3];


     tmp[0] = source->v[X1] * destination->v[Y1] +
              source->v[Y1] * destination->v[Y2] +
              source->v[Z1] * destination->v[Y3] +
              source->v[W1] * destination->v[Y4];
     tmp[1] = source->v[X2] * destination->v[Y1] +
              source->v[Y2] * destination->v[Y2] +
              source->v[Z2] * destination->v[Y3] +
              source->v[W2] * destination->v[Y4];
     tmp[2] = source->v[X3] * destination->v[Y1] +
              source->v[Y3] * destination->v[Y2] +
              source->v[Z3] * destination->v[Y3] +
              source->v[W3] * destination->v[Y4];
     tmp[3] = source->v[X4] * destination->v[Y1] +
              source->v[Y4] * destination->v[Y2] +
              source->v[Z4] * destination->v[Y3] +
              source->v[W4] * destination->v[Y4];

     destination->v[Y1] = tmp[0];
     destination->v[Y2] = tmp[1];
     destination->v[Y3] = tmp[2];
     destination->v[Y4] = tmp[3];


     tmp[0] = source->v[X1] * destination->v[Z1] +
              source->v[Y1] * destination->v[Z2] +
              source->v[Z1] * destination->v[Z3] +
              source->v[W1] * destination->v[Z4];
     tmp[1] = source->v[X2] * destination->v[Z1] +
              source->v[Y2] * destination->v[Z2] +
              source->v[Z2] * destination->v[Z3] +
              source->v[W2] * destination->v[Z4];
     tmp[2] = source->v[X3] * destination->v[Z1] +
              source->v[Y3] * destination->v[Z2] +
              source->v[Z3] * destination->v[Z3] +
              source->v[W3] * destination->v[Z4];
     tmp[3] = source->v[X4] * destination->v[Z1] +
              source->v[Y4] * destination->v[Z2] +
              source->v[Z4] * destination->v[Z3] +
              source->v[W4] * destination->v[Z4];

     destination->v[Z1] = tmp[0];
     destination->v[Z2] = tmp[1];
     destination->v[Z3] = tmp[2];
     destination->v[Z4] = tmp[3];


     tmp[0] = source->v[X1] * destination->v[W1] +
              source->v[Y1] * destination->v[W2] +
              source->v[Z1] * destination->v[W3] +
              source->v[W1] * destination->v[W4];
     tmp[1] = source->v[X2] * destination->v[W1] +
              source->v[Y2] * destination->v[W2] +
              source->v[Z2] * destination->v[W3] +
              source->v[W2] * destination->v[W4];
     tmp[2] = source->v[X3] * destination->v[W1] +
              source->v[Y3] * destination->v[W2] +
              source->v[Z3] * destination->v[W3] +
              source->v[W3] * destination->v[W4];
     tmp[3] = source->v[X4] * destination->v[W1] +
              source->v[Y4] * destination->v[W2] +
              source->v[Z4] * destination->v[W3] +
              source->v[W4] * destination->v[W4];

     destination->v[W1] = tmp[0];
     destination->v[W2] = tmp[1];
     destination->v[W3] = tmp[2];
     destination->v[W4] = tmp[3];
}

void matrix_translate( Matrix *matrix, float x, float y, float z )
{
     Matrix *tmp = alloca( sizeof(Matrix) );

     memcpy( tmp, &identity, sizeof(Matrix) );

     tmp->v[W1] = x;
     tmp->v[W2] = y;
     tmp->v[W3] = z;

     matrix_multiply( matrix, tmp );
}

void matrix_scale( Matrix *matrix, float x, float y, float z )
{
     Matrix *tmp = alloca( sizeof(Matrix) );

     memcpy( tmp, &identity, sizeof(Matrix) );

     tmp->v[X1] = x;
     tmp->v[Y2] = y;
     tmp->v[Z3] = z;

     matrix_multiply( matrix, tmp );
}

void matrix_rotate( Matrix *matrix, Vector_Elements axis, float angle )
{
     float  _cos = (float) cos( angle );
     float  _sin = (float) sin( angle );
     Matrix *tmp = alloca( sizeof(Matrix) );

     memcpy( tmp, &identity, sizeof(Matrix) );

     switch (axis) {
          case X:
               tmp->v[Y2] =   _cos;
               tmp->v[Z2] = - _sin;
               tmp->v[Y3] =   _sin;
               tmp->v[Z3] =   _cos;
               break;
          case Y:
               tmp->v[X1] =   _cos;
               tmp->v[Z1] =   _sin;
               tmp->v[X3] = - _sin;
               tmp->v[Z3] =   _cos;
               break;
          case Z:
               tmp->v[X1] =   _cos;
               tmp->v[Y1] = - _sin;
               tmp->v[X2] =   _sin;
               tmp->v[X2] =   _cos;
               break;
          default:
               break;
     }

     matrix_multiply( matrix, tmp );
}


