/* 
   Ported to DirectFB by Denis Oliver Kropp <dok@convergence.de>

   Written by Mark Vojkovich <markv@valinux.com>
*/
   
#include <stdio.h>
#include <math.h>

#include <directfb.h>

#include "matrix.h"
#include "skull.h"


IDirectFB            *dfb;
IDirectFBSurface     *primary;
IDirectFBInputDevice *keyboard;
IDirectFBInputDevice *mouse;
IDirectFBInputBuffer *mouse_buffer;


Bool	BackfaceCull = True;
Bool	DoubleBuffer = True;
Bool	Lighting = True;
int	PrimitiveType = FLAT_SHADED;
float	ScaleFactor = 1.0;

unsigned int Width, Height;

Vertex Light = {0.0, 0.0, 1.0};


static Tri3D Triangles[SKULL_TRIANGLES];
static Vertex TransformedVerticies[SKULL_VERTICIES];


static void ClearBuffer (void)
{
  primary->SetColor (primary, 0, 0, 0, 0);
  primary->FillRectangle (primary, 0, 0, Width, Height);
}

static void DrawTriangle (float color, Tri3D* tri)
{
  __u8 gray = (__u8)(color * 255.0);
  int X, Y;

  X = Width >> 1;
  Y = Height >> 1;

  primary->SetColor (primary, gray, (gray*gray)/255, gray/4, 0xff);
    
  switch (PrimitiveType)
    {
    case FLAT_SHADED:
      primary->FillTriangle (primary,
			     tri->a->x + X, tri->a->y + Y,
			     tri->b->x + X, tri->b->y + Y,
			     tri->c->x + X, tri->c->y + Y);
      break;
      /*    case WIRE_FRAME:
	    Points[3].x = Points[0].x;
	    Points[3].y = Points[0].y;
	    
	    XDrawLines(TheDisplay, TheDrawable, SolidContext, Points, 4,
	    CoordModeOrigin);
	    break;
	    case AS_POINTS:
	    XDrawPoints(TheDisplay, TheDrawable, SolidContext, Points, 3,
	    CoordModeOrigin);
	    break;*/
    }
}

static void DrawIt (void)
{
  int count, NumUsed = 0;
  Tri3D *current = Triangles;
  Tri3D *first = Triangles;
  Tri3D *pntr, *prev;
  Triangle *points = SkullTriangles;
  Vertex *transPoints = TransformedVerticies;
  Vertex *untransPoints = SkullVerticies;
  Vertex A, B;
  float length;

  ClearBuffer();

  count = SKULL_VERTICIES;
  while(count--)
    MultiplyVector(untransPoints++, transPoints++);   

  first->next = NULL;

  count = SKULL_TRIANGLES;
  while(count--)
    {
      current->a = TransformedVerticies + points->a;
      current->b = TransformedVerticies + points->b;
      current->c = TransformedVerticies + points->c;

      A.x = current->b->x - current->a->x;
      A.y = current->b->y - current->a->y;
      A.z = current->b->z - current->a->z;

      B.x = current->c->x - current->b->x;
      B.y = current->c->y - current->b->y;
      B.z = current->c->z - current->b->z;

      current->normal.z = (A.x * B.y) - (A.y * B.x);

      if(BackfaceCull && (current->normal.z >= 0.0))
	{
	  points++;
	  continue;
	}

      current->normal.y = (A.z * B.x) - (A.x * B.z);
      current->normal.x = (A.y * B.z) - (A.z * B.y);

      current->depth = current->a->z + current->b->z + current->c->z;	

      /* Not the smartest sorting algorithm */
      if(NumUsed)
	{
	  prev = NULL;
	  pntr = first;
	  while(pntr)
	    {
	      if(current->depth > pntr->depth)
		{
		  if(pntr->next)
		    {
		      prev = pntr;
		      pntr = pntr->next;
		    }
		  else
		    {
		      pntr->next = current;
		      current->next = NULL;
		      break;
		    }
		}
	      else
		{
		  if(prev)
		    {
		      prev->next = current;
		      current->next = pntr;
		    }
		  else
		    {
		      current->next = pntr;
		      first = current;
		    }
		  break;
		}
	    }
	}
		
      NumUsed++;	
      current++;
      points++;
    }
   

  while(first)
    {
      if(Lighting)
	{
	  length = ((first->normal.x * first->normal.x) +	
		    (first->normal.y * first->normal.y) +	
		    (first->normal.z * first->normal.z));

	  length = (float)sqrt((double)length);

	  length =  -((first->normal.x * Light.x) +
		      (first->normal.y * Light.y) +
		      (first->normal.z * Light.z)) / length;

	  if(length < 0.0)
	    length = 0.0;
	  else if(length > 1.0)
	    length = 1.0;
	}
      else
	length = 1.0;

      
      DrawTriangle(length, first);

      first = first->next;
    }
}

static int SetupDirectFB (int argc, char *argv[])
{
  DFBResult ret;
  DFBSurfaceDescription dsc;

  ret = DirectFBInit (&argc, &argv);
  if (ret)
    {
      DirectFBError ("DirectFBInit failed", ret);
      return -1;
    }

  ret = DirectFBCreate (&dfb);
  if (ret)
    {
      DirectFBError ("DirectFBCreate failed", ret);
      return -2;
    }

  dfb->SetCooperativeLevel (dfb, DFSCL_FULLSCREEN);

  ret = dfb->GetInputDevice (dfb, DIDID_KEYBOARD, &keyboard);
  if (ret)
    {
      DirectFBError ("GetInputDevice for keyboard failed", ret);
      dfb->Release (dfb);
      return -3;
    }

  ret = dfb->GetInputDevice (dfb, DIDID_MOUSE, &mouse);
  if (ret)
    {
      DirectFBError ("GetInputDevice for mouse failed", ret);
      keyboard->Release (keyboard);
      dfb->Release (dfb);
      return -4;
    }

  ret = mouse->CreateInputBuffer (mouse, &mouse_buffer);
  if (ret)
    {
      DirectFBError ("CreateInputBuffer for mouse failed", ret);
      mouse->Release (mouse);
      keyboard->Release (keyboard);
      dfb->Release (dfb);
      return -5;
    }

  dsc.flags = DSDESC_CAPS;
  dsc.caps  = DSCAPS_PRIMARY | (DoubleBuffer ? DSCAPS_FLIPPING : 0);

  ret = dfb->CreateSurface (dfb, &dsc, &primary);
  if (ret)
    {
      DirectFBError ("CreateSurface for primary failed", ret);
      mouse_buffer->Release (mouse_buffer);
      mouse->Release (mouse);
      keyboard->Release (keyboard);
      dfb->Release (dfb);
      return -5;
    }

  primary->GetSize (primary, &Width, &Height);

  return 0;
}

static void ClosedownDirectFB (void)
{
  primary->Release (primary);
  mouse_buffer->Release (mouse_buffer);
  mouse->Release (mouse);
  keyboard->Release (keyboard);
  dfb->Release (dfb);
}

int main (int argc, char *argv[])
{
  DFBInputDeviceKeyState Quit = DIKS_UP;
//  int dxL, dyL;

  if(SetupDirectFB (argc, argv))
    return -1;

  ScaleFactor = Height / 900.0f;

  InitMatrix();
  SetupMatrix(ScaleFactor);

  DrawIt();
  if(DoubleBuffer)
    primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC);

  while(Quit == DIKS_UP)
    {
      DFBInputEvent evt;

      keyboard->GetKeyState (keyboard, DIKC_ESCAPE, &Quit);

      while (mouse_buffer->GetEvent (mouse_buffer, &evt) == DFB_OK)
	{
	  if (evt.type == DIET_AXISMOTION && evt.flags & DIEF_AXISREL)
	    {
	      if (evt.axis == DIAI_X)
		Rotate (evt.axisrel * 2, 'y');
	      else if (evt.axis == DIAI_Y)
		Rotate (-evt.axisrel * 2, 'x');
	    }
	}
      
      //      Rotate (30, 'x');
      //      Rotate (70, 'z');

      //	if(dxL | dyL)
      //	   RotateLight(&Light, dxL, dyL);
      
      DrawIt();
      if(DoubleBuffer)
	primary->Flip (primary, NULL, DSFLIP_WAITFORSYNC);
    }
  
  ClosedownDirectFB();

  return 0;
}
