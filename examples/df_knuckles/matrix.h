/*
   Written by Mark Vojkovich <markv@valinux.com>
*/

#ifndef _MATRIX_H_
#define _MATRIX_H_

#ifndef Bool
# define Bool int
#endif

#ifndef True
# define True 1
#endif

#ifndef False
# define False 0
#endif

#ifndef PI
# define PI 3.1415927
#endif

#define MATRIX_SIZE (sizeof(float) * 9)

typedef struct {
   float x, y, z;
} Vertex;

typedef struct {
   int a, b, c;
} Triangle;

typedef struct _Tri3D {
   Vertex *a, *b, *c;
   float depth;
   Vertex normal;
   struct _Tri3D *next;
} Tri3D;

#define FLAT_SHADED	0
#define WIRE_FRAME	1
#define AS_POINTS	2

void InitMatrix(void);
void SetupMatrix(float Scale);
void Rotate(int degree_tenths, char axis);
void RotateLight(Vertex *light, int dx, int dy);
void MultiplyVector(Vertex *V, Vertex *R);

extern Bool BackfaceCull;
extern Bool DoubleBuffer;
extern Bool Lighting;
extern int  PrimitiveType;
extern float ScaleFactor;


#endif /* _MATRIX_H_ */
