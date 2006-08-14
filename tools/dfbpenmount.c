/*
   (c) Copyright 2000-2006  convergence integrated media GmbH.
   All rights reserved.
   
   Written by Nikita Egorov <nikego@gmail.com>
   
   Calibration utility for PenMount's touchscreen panel. Run the program 
   and touch to center of left/top cross ( active cross is blinked ). 
   Then touch to right/bottom cross. The program will create four values for 
   penmout's driver. The values will be printed to the console.

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

#include <directfb.h>

#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <string.h>

static IDirectFB *dfb;
static IDirectFBSurface *primary;
static IDirectFBEventBuffer *buffer;
static int sx,sy;

int
main( int argc, char *argv[] )
{
     int quit = 0;
     DFBResult err;
     DFBGraphicsDeviceDescription gdesc;
     
     char init_str[64];
     const char* dev = "/dev/ttyS0";
     char buf[4096]={0};
     
     char *home = getenv( "HOME" );
     int   file;

     int leftx, topy, rightx, bottomy, ofs;
     int mouse_x, mouse_y, tx1, ty1;
     int touch, count, color; 
	struct timespec rqtp,rmtp;
     
     if(home)
         sprintf(init_str,"%s/.directfbrc",home);
	 else
	     strcpy(init_str,"root/.directfbrc");
	 		   
	 file = open ( init_str, O_RDONLY );
	 if ( file != -1 ){
	 	 char *pos, *pos2;
           read(file, buf, sizeof(buf));
		 close(file);
	 
		 pos = strstr( buf, "penmount-device" );
		 if(pos){
	 		 pos = strchr(pos,'=');
		 	 if(pos){
		 	 	*pos++ = '\0';
	 		 	if( (pos2=strchr(pos,':'))||(pos2=strchr(pos,'\n')) )
	 	 			*pos2 = '\0';
		 	 	dev = pos;	
		 	 }
		 } 
	 }
	 printf( "penmount device '%s'\n", dev );
	 
	 sprintf( init_str,"--dfb:penmount-device=%s:raw", dev);
	 argv[argc++] = init_str;
	 	
     if (DirectFBInit( &argc, &argv ) != DFB_OK)
          return 1;

     if (DirectFBCreate( &dfb ) != DFB_OK)
          return 1;

     dfb->GetDeviceDescription( dfb, &gdesc );

	 err = dfb->SetCooperativeLevel( dfb, DFSCL_FULLSCREEN );
     if (err != DFB_OK)
          DirectFBError( "Failed requesting exclusive access", err );

     err = dfb->CreateInputEventBuffer( dfb, DICAPS_ALL, DFB_FALSE, &buffer );
     if (err != DFB_OK) {
          DirectFBError( "CreateInputEventBuffer failed", err );
          dfb->Release( dfb );
          return 1;
     }

     {
          DFBSurfaceDescription dsc;

          dsc.flags = DSDESC_CAPS;
          dsc.caps = (gdesc.drawing_flags & DSDRAW_BLEND) ?
                         DSCAPS_PRIMARY | DSCAPS_FLIPPING :
                         DSCAPS_PRIMARY | DSCAPS_FLIPPING | DSCAPS_SYSTEMONLY;

          err = dfb->CreateSurface( dfb, &dsc, &primary );
          if (err != DFB_OK) {
               DirectFBError( "Failed creating primary surface", err );
               buffer->Release( buffer );
               dfb->Release( dfb );
               return 1;
          }

          primary->GetSize( primary, &sx, &sy );
     }

     primary->Clear( primary, 0x0, 0x0, 0x0, 0xFF );
     
     leftx = sx/10;
     topy = sy/10;
     rightx = sx*9/10;
     bottomy = sy*9/10;
     ofs = 10;

     primary->SetColor( primary,0xFF,0,0,0xFF );
   	 primary->DrawLine( primary,rightx-ofs,bottomy,rightx+ofs,bottomy );
     primary->DrawLine( primary,rightx,bottomy-ofs,rightx,bottomy+ofs );
	      
     err = primary->Flip( primary, NULL, 0 );
     if (err != DFB_OK) {
          DirectFBError( "Failed flipping the primary surface", err );
          primary->Release( primary );
          buffer->Release( buffer );
          dfb->Release( dfb );
          return 1;
     }

	mouse_x=0,mouse_y=0,tx1=0,ty1=0;
	touch=0,count=0,color=0;
      
     while (!quit) {
          DFBInputEvent evt;
          rqtp.tv_nsec = 10000;
          rqtp.tv_sec = 0;
          nanosleep( &rqtp,&rmtp );
          if (count++ >= 30){
          	count = 0;
          	color = !color;
          	if (color)
          		primary->SetColor( primary,0x00,0xFF,0,0xFF );
          	else
          		primary->SetColor( primary,0xFF,0x00,0,0xFF );
          	
          	switch(touch){
          		case 0:
          			primary->DrawLine( primary,leftx-ofs,topy,leftx+ofs,topy );
     				primary->DrawLine( primary,leftx,topy-ofs,leftx,topy+ofs );
     				break;
     			case 1:
				    primary->DrawLine( primary,rightx-ofs,bottomy,rightx+ofs,bottomy );
				    primary->DrawLine( primary,rightx,bottomy-ofs,rightx,bottomy+ofs );  			
     				break;
          	}
          	primary->Flip( primary, NULL, 0 );
          }		 

          while (buffer->GetEvent( buffer, DFB_EVENT(&evt) ) == DFB_OK) {
          	if ( evt.type == DIET_AXISMOTION){
          		if (evt.flags & DIEF_AXISABS) {
	               	switch (evt.axis) {
	               	case DIAI_X:
	                    mouse_x = evt.axisabs;
	               		break;
	               	case DIAI_Y:
	                    mouse_y = evt.axisabs;
	                    break;
	               default:
	                    break;
	               }
          		}
          	}
          	if ( evt.type == DIET_BUTTONPRESS ){
          		switch(++touch){
          			case 1: //save first touchscreen position
          				tx1=mouse_x;
          				ty1=mouse_y;
          				break;
          			case 2://build new calibration values and quit
          			{	
          				float dx = ((float)mouse_x-tx1)/(rightx-leftx);
          				float dy = ((float)mouse_y-ty1)/(bottomy-topy);
          				printf( "Insert followed values into source code of penmount's driver\n'inputdrivers/penmount/penmount.c:96,99' and rebuild:\n" );
          				printf( "min_x=%d min_y=%d\n",(int)(tx1-leftx*dx+.5),(int)(ty1-topy*dy+.5));
           				printf( "max_x=%d max_y=%d\n",(int)(mouse_x+leftx*dx+.5),(int)(mouse_y+topy*dy+.5));
 	                 	quit = 1;
 	                 	break;
          			}
           		}
           	}
            if (evt.type == DIET_KEYPRESS  &&  evt.key_id == DIKI_ESCAPE)
                 quit = 1;
          }
     }
     primary->Release( primary );
     buffer->Release( buffer );
     dfb->Release( dfb );

     return 0;
}

