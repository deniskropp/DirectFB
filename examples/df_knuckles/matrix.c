/*
   Written by Mark Vojkovich <markv@valinux.com>
*/

#include <math.h>
#include <string.h>
#include <stdio.h>
#include "matrix.h"

float CTM[9];

static float Scratch[9];
static float IdentityMatrix[9] = { 
   1.0, 0.0, 0.0,
   0.0, 1.0, 0.0,
   0.0, 0.0, 1.0
};

static float Cosine[3600];
static float Sine[3600];

#define M_CLEAR(m) bzero(m, MATRIX_SIZE)
#define M_IDENTITY(m) memcpy(m, IdentityMatrix, MATRIX_SIZE)

static void MultiplyMatrix(float *A, float *B)
{
    float tmp[3];
    int row, column;

    /* make sure to compile with loop unrolling */
    for(row = 0; row < 3; row++) {
	memcpy(tmp, A + (row * 3), sizeof(float) * 3);
	for(column = 0; column < 3; column++) {
	    A[(row * 3) + column] = 
		(tmp[0] * B[column + 0]) +
		(tmp[1] * B[column + 3]) +
		(tmp[2] * B[column + 6]);
	}
    }

}

void RotateLight(Vertex *light, int dx, int dy)
{
    float matrix[9];
    float X, Y, Z;

    M_IDENTITY(matrix);

    if(dx) {
	while(dx >= 3600)
	    dx -= 3600;
	while(dx < 0)
	    dx += 3600;

	M_CLEAR(Scratch);
    
	Scratch[4] = Cosine[dx];
	Scratch[8] = Cosine[dx];
	Scratch[5] = -Sine[dx];
	Scratch[7] = Sine[dx];
	Scratch[0] = 1.0;

	MultiplyMatrix(matrix, Scratch);
    }

    if(dy) {
	while(dy >= 3600)
	    dy -= 3600;
	while(dy < 0)
	    dy += 3600;

	M_CLEAR(Scratch);
    
	Scratch[0] = Cosine[dy];
	Scratch[2] = Sine[dy];
	Scratch[6] = -Sine[dy];
	Scratch[8] = Cosine[dy];
	Scratch[4] = 1.0;

	MultiplyMatrix(matrix, Scratch);
    }

    X = light->x;
    Y = light->y;
    Z = light->z;

    light->x =  (X * matrix[0]) + (Y * matrix[1]) + (Z * matrix[2]); 
    light->y =  (X * matrix[3]) + (Y * matrix[4]) + (Z * matrix[5]);
    light->z =  (X * matrix[6]) + (Y * matrix[7]) + (Z * matrix[8]);
}

void MultiplyVector(Vertex *V, Vertex *R) 
{
   float divisor;

   R->x =  (V->x * CTM[0]) + (V->y * CTM[1]) + (V->z * CTM[2]); 
   R->y =  (V->x * CTM[3]) + (V->y * CTM[4]) + (V->z * CTM[5]);
   R->z =  (V->x * CTM[6]) + (V->y * CTM[7]) + (V->z * CTM[8]); 

   divisor = (R->z + 350.0) / 250.0;

   if(divisor < 0)
	divisor = -divisor;

    R->x *= divisor;
    R->y *= divisor;
}

void InitMatrix(void)
{
    int i;

    for(i = 0; i < 3600; i++) {
	Cosine[i] = (float)cos(2.0 * PI * (double)i / 3600.0);
	Sine[i]   = (float)sin(2.0 * PI * (double)i / 3600.0);
    }
}




void Scale(float x, float y, float z)
{
    M_CLEAR(Scratch);

    Scratch[0] = x;
    Scratch[4] = y;
    Scratch[8] = z;

    MultiplyMatrix(CTM, Scratch); 
}


void SetupMatrix(float scale)
{

    M_IDENTITY(CTM);

    Scale(scale, -scale, scale);
    Rotate(1800, 'y');
}


void Rotate(int degree_tenths, char axis)
{
     while(degree_tenths >= 3600)
	degree_tenths -= 3600;
     while(degree_tenths < 0)
	degree_tenths += 3600;

     M_CLEAR(Scratch);

     switch(axis){
	case 'x':
		Scratch[4] = Cosine[degree_tenths];
		Scratch[8] = Cosine[degree_tenths];
		Scratch[5] = -Sine[degree_tenths];
		Scratch[7] = Sine[degree_tenths];
		Scratch[0] = 1.0;
		break;
	case 'y':
		Scratch[0] = Cosine[degree_tenths];
		Scratch[2] = Sine[degree_tenths];
		Scratch[6] = -Sine[degree_tenths];
		Scratch[8] = Cosine[degree_tenths];
		Scratch[4] = 1.0;
		break;
	case 'z':
		Scratch[0] = Cosine[degree_tenths];
		Scratch[1] = -Sine[degree_tenths];
		Scratch[3] = Sine[degree_tenths];
		Scratch[4] = Cosine[degree_tenths];
		Scratch[8] = 1.0;
		break;
    }

    MultiplyMatrix(CTM, Scratch); 
}
