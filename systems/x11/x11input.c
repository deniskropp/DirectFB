/*
   (c) Copyright 2001-2007  The DirectFB Organization (directfb.org)
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

#include <config.h>

#include <fusion/types.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <directfb.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>


#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/system.h>

#include <direct/mem.h>
#include <direct/thread.h>

#include "xwindow.h"

#include "x11.h"

#include <core/input_driver.h>

extern DFBX11  *dfb_x11;
extern CoreDFB *dfb_x11_core;

DFB_INPUT_DRIVER( x11input )


/*
 * declaration of private data
 */
typedef struct {
     CoreInputDevice*	device;
     DirectThread*		thread;
     DFBX11*			dfb_x11;
     int              	stop;
} X11InputData;


static DFBInputEvent motionX = {
     type:     DIET_UNKNOWN,
     axisabs:  0
};

static DFBInputEvent motionY = {
     type:     DIET_UNKNOWN,
     axisabs:  0
};

static void
motion_compress( int x, int y )
{
	if (motionX.axisabs != x) {
          motionX.type    = DIET_AXISMOTION;
          motionX.flags   = DIEF_AXISABS;
          motionX.axis    = DIAI_X;
          motionX.axisabs = x;
     }

     if (motionY.axisabs != y) {
          motionY.type    = DIET_AXISMOTION;
          motionY.flags   = DIEF_AXISABS;
          motionY.axis    = DIAI_Y;
          motionY.axisabs = y;
     }
}

static void
motion_realize( X11InputData *data )
{
	if (motionX.type != DIET_UNKNOWN) {
		dfb_input_dispatch( data->device, &motionX );

		motionX.type = DIET_UNKNOWN;
    }

	if (motionY.type != DIET_UNKNOWN) {
		
		dfb_input_dispatch( data->device, &motionY );
		motionY.type = DIET_UNKNOWN;
	}
}

static bool
translate_key( KeySym xKeySymbol, DFBInputEvent* pDFBEvent)
{
	pDFBEvent->flags  			= DIEF_KEYID;
	switch (xKeySymbol)
	{
		case XK_a 			: pDFBEvent->key_id	= DIKI_A; 		return true; break;
		case XK_b 			: pDFBEvent->key_id	= DIKI_B; 		return true; break;
		case XK_c 			: pDFBEvent->key_id	= DIKI_C; 		return true; break;
		case XK_d 			: pDFBEvent->key_id	= DIKI_D; 		return true; break;
		case XK_e 			: pDFBEvent->key_id	= DIKI_E; 		return true; break;
		case XK_f 			: pDFBEvent->key_id	= DIKI_F; 		return true; break;
		case XK_g 			: pDFBEvent->key_id	= DIKI_G; 		return true; break;
		case XK_h 			: pDFBEvent->key_id	= DIKI_H; 		return true; break;
		case XK_i 			: pDFBEvent->key_id	= DIKI_I; 		return true; break;
		case XK_j 			: pDFBEvent->key_id	= DIKI_J; 		return true; break;
		case XK_k 			: pDFBEvent->key_id	= DIKI_K; 		return true; break;
		case XK_l 			: pDFBEvent->key_id	= DIKI_L; 		return true; break;
		case XK_m 			: pDFBEvent->key_id	= DIKI_M; 		return true; break;
		case XK_n 			: pDFBEvent->key_id	= DIKI_N; 		return true; break;
		case XK_o 			: pDFBEvent->key_id	= DIKI_O; 		return true; break;
		case XK_p 			: pDFBEvent->key_id	= DIKI_P; 		return true; break;
		case XK_q 			: pDFBEvent->key_id	= DIKI_Q; 		return true; break;
		case XK_r 			: pDFBEvent->key_id	= DIKI_R; 		return true; break;
		case XK_s 			: pDFBEvent->key_id	= DIKI_S; 		return true; break;
		case XK_t 			: pDFBEvent->key_id	= DIKI_T; 		return true; break;
		case XK_u 			: pDFBEvent->key_id	= DIKI_U; 		return true; break;
		case XK_v 			: pDFBEvent->key_id	= DIKI_V; 		return true; break;
		case XK_w 			: pDFBEvent->key_id	= DIKI_W; 		return true; break;
		case XK_x 			: pDFBEvent->key_id	= DIKI_X; 		return true; break;
		case XK_y 			: pDFBEvent->key_id	= DIKI_Y; 		return true; break;
		case XK_z 			: pDFBEvent->key_id	= DIKI_Z; 		return true; break;
		case XK_0 			: pDFBEvent->key_id	= DIKI_0; 		return true; break;
		case XK_1 			: pDFBEvent->key_id	= DIKI_1; 		return true; break;
		case XK_2 			: pDFBEvent->key_id	= DIKI_2; 		return true; break;
		case XK_3 			: pDFBEvent->key_id	= DIKI_3; 		return true; break;
		case XK_4 			: pDFBEvent->key_id	= DIKI_4; 		return true; break;
		case XK_5 			: pDFBEvent->key_id	= DIKI_5; 		return true; break;
		case XK_6 			: pDFBEvent->key_id	= DIKI_6; 		return true; break;
		case XK_7 			: pDFBEvent->key_id	= DIKI_7; 		return true; break;
		case XK_8 			: pDFBEvent->key_id	= DIKI_8; 		return true; break;
		case XK_9 			: pDFBEvent->key_id	= DIKI_9; 		return true; break;
		case XK_F1 			: pDFBEvent->key_id	= DIKI_F1; 		return true; break;
		case XK_F2 			: pDFBEvent->key_id	= DIKI_F2; 		return true; break;
		case XK_F3 			: pDFBEvent->key_id	= DIKI_F3; 		return true; break;
		case XK_F4 			: pDFBEvent->key_id	= DIKI_F4; 		return true; break;
		case XK_F5 			: pDFBEvent->key_id	= DIKI_F5; 		return true; break;
		case XK_F6 			: pDFBEvent->key_id	= DIKI_F6; 		return true; break;
		case XK_F7 			: pDFBEvent->key_id	= DIKI_F7; 		return true; break;
		case XK_F8 			: pDFBEvent->key_id	= DIKI_F8; 		return true; break;
		case XK_F9 			: pDFBEvent->key_id	= DIKI_F9; 		return true; break;
		case XK_F10			: pDFBEvent->key_id	= DIKI_F10; 	return true; break;
		case XK_F11			: pDFBEvent->key_id	= DIKI_F11; 	return true; break;
		case XK_F12			: pDFBEvent->key_id	= DIKI_F12; 	return true; break;
		

		case XK_Shift_L		: pDFBEvent->key_id	= DIKI_SHIFT_L; 	return true; break;
		case XK_Shift_R		: pDFBEvent->key_id	= DIKI_SHIFT_R; 	return true; break;
		case XK_Control_L	: pDFBEvent->key_id	= DIKI_CONTROL_L; 	return true; break;
		case XK_Control_R	: pDFBEvent->key_id	= DIKI_CONTROL_R; 	return true; break;
		case XK_Alt_L		: pDFBEvent->key_id	= DIKI_ALT_L; 		return true; break;
		case XK_Alt_R		: pDFBEvent->key_id	= DIKI_ALT_R; 		return true; break;
//		case XK_Alt_??		: pDFBEvent->key_id	= DIKI_ALTGR; 	return true; break;
		case XK_Meta_L		: pDFBEvent->key_id	= DIKI_META_L; 		return true; break;
		case XK_Meta_R		: pDFBEvent->key_id	= DIKI_META_R; 		return true; break;
		case XK_Super_L		: pDFBEvent->key_id	= DIKI_SUPER_L; 	return true; break;
		case XK_Super_R		: pDFBEvent->key_id	= DIKI_SUPER_R; 	return true; break;
		case XK_Hyper_L		: pDFBEvent->key_id	= DIKI_HYPER_L; 	return true; break;
		case XK_Hyper_R		: pDFBEvent->key_id	= DIKI_HYPER_R; 	return true; break;

		case XK_Caps_Lock	: pDFBEvent->key_id	= DIKI_CAPS_LOCK; 	return true; break;
		case XK_Num_Lock	: pDFBEvent->key_id	= DIKI_NUM_LOCK; 	return true; break;
		case XK_Scroll_Lock	: pDFBEvent->key_id	= DIKI_SCROLL_LOCK; return true; break;

		case XK_Escape 		: pDFBEvent->key_id	= DIKI_ESCAPE; 		return true; break;
		case XK_Left 		: pDFBEvent->key_id	= DIKI_LEFT; 		return true; break;
		case XK_Right 		: pDFBEvent->key_id	= DIKI_RIGHT; 		return true; break;
		case XK_Up 			: pDFBEvent->key_id	= DIKI_UP; 			return true; break;
		case XK_Down 		: pDFBEvent->key_id	= DIKI_DOWN; 		return true; break;
		
		case XK_Tab			: pDFBEvent->key_id	= DIKI_TAB; 		return true; break;
		case XK_Return		: pDFBEvent->key_id	= DIKI_ENTER; 		return true; break;
		case XK_space		: pDFBEvent->key_id	= DIKI_SPACE; 		return true; break;
		case XK_BackSpace	: pDFBEvent->key_id	= DIKI_BACKSPACE; 	return true; break;
		case XK_Insert		: pDFBEvent->key_id	= DIKI_INSERT; 		return true; break;
		case XK_Delete		: pDFBEvent->key_id	= DIKI_DELETE; 		return true; break;
		case XK_Home		: pDFBEvent->key_id	= DIKI_HOME; 		return true; break;
		case XK_End			: pDFBEvent->key_id	= DIKI_END; 		return true; break;
		case XK_Page_Up		: pDFBEvent->key_id	= DIKI_PAGE_UP; 	return true; break;
		case XK_Page_Down	: pDFBEvent->key_id	= DIKI_PAGE_DOWN; 	return true; break;
// ML: Not working		case XK_Print		: pDFBEvent->key_id	= DIKI_PRINT; 		return true; break;
		case XK_Pause		: pDFBEvent->key_id	= DIKI_PAUSE; 		return true; break;

		/*  The labels on these keys depend on the type of keyboard.
			*  We've choosen the names from a US keyboard layout. The
			*  comments refer to the ISO 9995 terminology.
		*/
		case XK_quoteleft	: pDFBEvent->key_id	= DIKI_QUOTE_LEFT; 	return true; break; /*  TLDE  */
		case XK_minus		: pDFBEvent->key_id	= DIKI_MINUS_SIGN; 	return true; break; /*  AE11  */
		case XK_equal		: pDFBEvent->key_id	= DIKI_EQUALS_SIGN; return true; break; /*  AE12  */
		case XK_bracketleft	: pDFBEvent->key_id	= DIKI_BRACKET_LEFT;return true; break; /*  AD11  */
		case XK_bracketright: pDFBEvent->key_id	= DIKI_BRACKET_RIGHT;return true; break;/*  AD12  */
		case XK_backslash	: pDFBEvent->key_id	= DIKI_BACKSLASH; 	return true; break; /*  BKSL  */
		case XK_semicolon	: pDFBEvent->key_id	= DIKI_SEMICOLON; 	return true; break; /*  AC10  */
		case XK_quoteright	: pDFBEvent->key_id	= DIKI_QUOTE_RIGHT; return true; break; /*  AC11  */
		case XK_comma		: pDFBEvent->key_id	= DIKI_COMMA; 		return true; break; /*  AB08  */
		case XK_period		: pDFBEvent->key_id	= DIKI_PERIOD; 		return true; break; /*  AB09  */
		case XK_slash		: pDFBEvent->key_id	= DIKI_SLASH; 		return true; break; /*  AB10  */
		case XK_less		: pDFBEvent->key_id	= DIKI_LESS_SIGN; 	return true; break; /*  103rd  */

		case XK_KP_Divide	: pDFBEvent->key_id	= DIKI_KP_DIV; 		return true; break;
		case XK_KP_Multiply	: pDFBEvent->key_id	= DIKI_KP_MULT; 	return true; break;
		case XK_KP_Subtract	: pDFBEvent->key_id	= DIKI_KP_MINUS; 	return true; break;
		case XK_KP_Add		: pDFBEvent->key_id	= DIKI_KP_PLUS; 	return true; break;
		case XK_KP_Enter	: pDFBEvent->key_id	= DIKI_KP_ENTER; 	return true; break;
		case XK_KP_Space	: pDFBEvent->key_id	= DIKI_KP_SPACE; 	return true; break;
		case XK_KP_Tab		: pDFBEvent->key_id	= DIKI_KP_TAB; 		return true; break;
		case XK_KP_F1		: pDFBEvent->key_id	= DIKI_KP_F1; 		return true; break;
		case XK_KP_F2		: pDFBEvent->key_id	= DIKI_KP_F2; 		return true; break;
		case XK_KP_F3		: pDFBEvent->key_id	= DIKI_KP_F3; 		return true; break;
		case XK_KP_F4		: pDFBEvent->key_id	= DIKI_KP_F4; 		return true; break;
		case XK_KP_Equal	: pDFBEvent->key_id	= DIKI_KP_EQUAL; 	return true; break;
		case XK_KP_Separator: pDFBEvent->key_id	= DIKI_KP_SEPARATOR;return true; break;

		// \note ML: I'm not quite sure about this, but it works for me. But the real problem I guess
		// is that the codes I get from X might be symbols and not IDs. Someone please help out :-)
		// Actually this is true for this whole switch ... I need some verification from someone more
		// fluent in X ...
		case XK_KP_Delete	: pDFBEvent->key_id	= DIKI_KP_DECIMAL; 	return true; break;
		case XK_KP_Insert	: pDFBEvent->key_id	= DIKI_KP_0; 		return true; break;
		case XK_KP_End		: pDFBEvent->key_id	= DIKI_KP_1; 		return true; break;
		case XK_KP_Down		: pDFBEvent->key_id	= DIKI_KP_2; 		return true; break;
		case XK_KP_Page_Down: pDFBEvent->key_id	= DIKI_KP_3; 		return true; break;
		case XK_KP_Left		: pDFBEvent->key_id	= DIKI_KP_4; 		return true; break;
		case XK_KP_Begin	: pDFBEvent->key_id	= DIKI_KP_5; 		return true; break;
		case XK_KP_Right	: pDFBEvent->key_id	= DIKI_KP_6; 		return true; break;
		case XK_KP_Home		: pDFBEvent->key_id	= DIKI_KP_7; 		return true; break;
		case XK_KP_Up		: pDFBEvent->key_id	= DIKI_KP_8; 		return true; break;
		case XK_KP_Page_Up	: pDFBEvent->key_id	= DIKI_KP_9; 		return true; break;
		default:
			printf("X11: Unknown key pressed\n");
	}	
	
	pDFBEvent->flags    = DIEF_NONE;
	return false;
}



static void handleMouseEvent(XEvent* pXEvent, X11InputData*	pData)
{
	static int 		iMouseEventCount = 0;
	DFBInputEvent 	dfbEvent;
	if (pXEvent->type == MotionNotify) 
	{
		motion_compress( pXEvent->xmotion.x, pXEvent->xmotion.y );
		++iMouseEventCount;
	}
	
	if ( pXEvent->type == ButtonPress || pXEvent->type == ButtonRelease )
	{
        motion_realize( pData );

        if ( pXEvent->type == ButtonPress ) 	
		dfbEvent.type = DIET_BUTTONPRESS;
	else
		dfbEvent.type = DIET_BUTTONRELEASE;

		dfbEvent.flags = DIEF_NONE;
				
		/* Get pressed button */
		switch( pXEvent->xbutton.button ) {
			case 1:
				dfbEvent.button = DIBI_LEFT;
			break;
			case 2:
				dfbEvent.button = DIBI_MIDDLE;
			break;
			case 3:
				dfbEvent.button = DIBI_RIGHT;
			break;
			//Wheel events
			case 4: /*up*/
			case 5: /*down*/
			case 6: /*left*/
			case 7: /*right*/
			{
				dfbEvent.type = DIET_AXISMOTION;
				dfbEvent.flags = DIEF_AXISREL;
				dfbEvent.axis = DIAI_Z;
				/*SCROLL UP*/
				if( pXEvent->xbutton.button == 4 ) {
					dfbEvent.axisrel = -1;
				}
				/*SCROLL DOWN */
				else if (pXEvent->xbutton.button == 5) {
					dfbEvent.axisrel = 1;
				}
				/*SCROLL LEFT*/
          			else if (pXEvent->xbutton.button == 6) {
					dfbEvent.axis = DIAI_X;
					dfbEvent.axisrel = -1;
				}
				/*SCROLL RIGHT*/
          			else if (pXEvent->xbutton.button == 7 ){
					dfbEvent.axis = DIAI_X;
					dfbEvent.axisrel = 1;
				}
					
			}
			break;
			default:
			break;
		}

		dfb_input_dispatch( pData->device, &dfbEvent );
		++iMouseEventCount;
	} 
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
x11EventThread( DirectThread *thread, void *driver_data )
{
	X11InputData*	data    = (X11InputData*) driver_data;
	DFBX11*			dfb_x11 = data->dfb_x11;
	/* X11 event masks for mouse and keyboard */
	const long iKeyPressMask 	= KeyPressMask;
	const long iKeyReleaseMask 	= KeyReleaseMask;
	const long iMouseEventMask 	= ButtonPressMask | ButtonReleaseMask | PointerMotionMask;	// ExposureMask

	while (!data->stop) 
	{
		XEvent xEvent; 
		DFBInputEvent dfbEvent;
        bool hasEvent;
		
        usleep(10000);

//        fusion_skirmish_prevail( &dfb_x11->lock );

		// --- Mouse events ---
        do {
             hasEvent = false;

             if (XCheckMaskEvent(dfb_x11->display, iMouseEventMask, &xEvent)) 
             {
                 hasEvent = true;
                 handleMouseEvent(&xEvent, data); // crash ???
             }


             if (XCheckMaskEvent(dfb_x11->display, iKeyPressMask, &xEvent)) 
             {
                 KeySym xKeySymbol = XKeycodeToKeysym(dfb_x11->display, xEvent.xkey.keycode, 0);
                 hasEvent = true;
                 if (translate_key( xKeySymbol, &dfbEvent )) {
                     dfbEvent.type		= DIET_KEYPRESS;
                     dfbEvent.key_code	= dfbEvent.key_id;
                     motion_realize( data );
                     dfb_input_dispatch( data->device, &dfbEvent );
                 }
             }
             else if (XCheckMaskEvent(dfb_x11->display, iKeyReleaseMask, &xEvent)) 
             {
                 KeySym xKeySymbol = XKeycodeToKeysym(dfb_x11->display, xEvent.xkey.keycode, 0);
                 hasEvent = true;
                 if (translate_key( xKeySymbol, &dfbEvent )) {
                     dfbEvent.type		= DIET_KEYRELEASE;
                     dfbEvent.key_code	= dfbEvent.key_id;
                     motion_realize( data );
                     dfb_input_dispatch( data->device, &dfbEvent );
                 }
             }
        } while ( hasEvent );
						

//          fusion_skirmish_dismiss( &dfb_x11->lock );

          motion_realize( data );

          direct_thread_testcancel( thread );
    }

     return NULL;
}

/* exported symbols */

/*
 * Return the number of available devices.
 * Called once during initialization of DirectFB.
 */
static int
driver_get_available()
{
	if (dfb_system_type() == CORE_X11) {
		return 1;
	}
    return 0;
}

/*
 * Fill out general information about this driver.
 * Called once during initialization of DirectFB.
 */
static void
driver_get_info( InputDriverInfo *info )
{
	/* fill driver info structure */
     snprintf ( info->name,
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "X11 Input Driver" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "Martin Lutken" );

     info->version.major = 0;
     info->version.minor = 1;
}

/*
 * Open the device, fill out information about it,
 * allocate and fill private data, start input thread.
 * Called during initialization, resuming or taking over mastership.
 */
static DFBResult
driver_open_device( CoreInputDevice  *device,
                    unsigned int      number,
                    InputDeviceInfo  *info,
                    void            **driver_data )
{
     X11InputData *data;
     DFBX11       *dfb_x11 = dfb_system_data();

     fusion_skirmish_prevail( &dfb_x11->lock );

     fusion_skirmish_dismiss( &dfb_x11->lock );

     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "X11 Input" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "X11" );

     /* set one of the primary input device IDs */
     info->prefered_id = DIDID_KEYBOARD;

     /* set type flags */
     info->desc.type   = DIDTF_JOYSTICK | DIDTF_KEYBOARD | DIDTF_MOUSE;

     /* set capabilities */
     info->desc.caps   = DICAPS_ALL;
     /* enable translation of fake raw hardware keycodes */
     info->desc.min_keycode = DIKI_A;
     info->desc.max_keycode = DIKI_KP_9;
 


     /* allocate and fill private data */
     data = D_CALLOC( 1, sizeof(X11InputData) );

     data->device  = device;
     data->dfb_x11 = dfb_x11;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, x11EventThread, data, "X11 Input" );

     /* set private data pointer */
     *driver_data = data;

     
     XInitThreads();
     
     return DFB_OK;
}


static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers)
{
	bool shift = (modifiers & DIMM_SHIFT);

     if (id >= DIKI_A && id <= DIKI_Z)
          return (shift ? DIKS_CAPITAL_A : DIKS_SMALL_A) + id - DIKI_A;

     if (id >= DIKI_KP_0 && id <= DIKI_KP_9)
          return DIKS_0 + id - DIKI_KP_0;

     if (id >= DIKI_F1 && id <= DIKI_F12)
          return DIKS_F1 + id - DIKI_F1;

     switch (id) {
          case DIKI_0:
               return shift ? DIKS_PARENTHESIS_RIGHT : DIKS_0;
          case DIKI_1:
               return shift ? DIKS_EXCLAMATION_MARK : DIKS_1;
          case DIKI_2:
               return shift ? DIKS_AT : DIKS_2;
          case DIKI_3:
               return shift ? DIKS_NUMBER_SIGN : DIKS_3;
          case DIKI_4:
               return shift ? DIKS_DOLLAR_SIGN : DIKS_4;
          case DIKI_5:
               return shift ? DIKS_PERCENT_SIGN : DIKS_5;
          case DIKI_6:
               return shift ? DIKS_CIRCUMFLEX_ACCENT : DIKS_6;
          case DIKI_7:
               return shift ? DIKS_AMPERSAND : DIKS_7;
          case DIKI_8:
               return shift ? DIKS_ASTERISK : DIKS_8;
          case DIKI_9:
               return shift ? DIKS_PARENTHESIS_LEFT : DIKS_9;

          case DIKI_ESCAPE:
               return DIKS_ESCAPE;

          case DIKI_LEFT:
               return DIKS_CURSOR_LEFT;

          case DIKI_RIGHT:
               return DIKS_CURSOR_RIGHT;

          case DIKI_UP:
               return DIKS_CURSOR_UP;

          case DIKI_DOWN:
               return DIKS_CURSOR_DOWN;

          case DIKI_CONTROL_L:
          case DIKI_CONTROL_R:
               return DIKS_CONTROL;

          case DIKI_SHIFT_L:
          case DIKI_SHIFT_R:
               return DIKS_SHIFT;

          case DIKI_ALT_L:
          case DIKI_ALT_R:
               return DIKS_ALT;

          case DIKI_META_L:
          case DIKI_META_R:
               return DIKS_META;

          case DIKI_SUPER_L:
          case DIKI_SUPER_R:
               return DIKS_SUPER;

          case DIKI_HYPER_L:
          case DIKI_HYPER_R:
               return DIKS_HYPER;

          case DIKI_TAB:
               return DIKS_TAB;

          case DIKI_ENTER:
               return DIKS_ENTER;

          case DIKI_SPACE:
               return DIKS_SPACE;

          case DIKI_BACKSPACE:
               return DIKS_BACKSPACE;

          case DIKI_INSERT:
               return DIKS_INSERT;

          case DIKI_DELETE:
               return DIKS_DELETE;

          case DIKI_HOME:
               return DIKS_HOME;

          case DIKI_END:
               return DIKS_END;

          case DIKI_PAGE_UP:
               return DIKS_PAGE_UP;

          case DIKI_PAGE_DOWN:
               return DIKS_PAGE_DOWN;

          case DIKI_CAPS_LOCK:
               return DIKS_CAPS_LOCK;

          case DIKI_NUM_LOCK:
               return DIKS_NUM_LOCK;

          case DIKI_SCROLL_LOCK:
               return DIKS_SCROLL_LOCK;

          case DIKI_PRINT:
               return DIKS_PRINT;

          case DIKI_PAUSE:
               return DIKS_PAUSE;

          case DIKI_KP_DIV:
               return DIKS_SLASH;

          case DIKI_KP_MULT:
               return DIKS_ASTERISK;

          case DIKI_KP_MINUS:
               return DIKS_MINUS_SIGN;

          case DIKI_KP_PLUS:
               return DIKS_PLUS_SIGN;

          case DIKI_KP_ENTER:
               return DIKS_ENTER;

          case DIKI_KP_SPACE:
               return DIKS_SPACE;

          case DIKI_KP_TAB:
               return DIKS_TAB;

          case DIKI_KP_EQUAL:
               return DIKS_EQUALS_SIGN;

          case DIKI_KP_DECIMAL:
               return DIKS_PERIOD;

          case DIKI_KP_SEPARATOR:
                  return DIKS_COMMA;

          case DIKI_BACKSLASH:
                if( shift )
                  return DIKS_VERTICAL_BAR;
                else
                  return DIKS_BACKSLASH;

          case DIKI_EQUALS_SIGN:
                if( shift )
                  return DIKS_PLUS_SIGN;
                else
                  return DIKS_EQUALS_SIGN;

          case DIKI_LESS_SIGN:
               return shift ? DIKS_GREATER_THAN_SIGN : DIKS_LESS_THAN_SIGN;

          case DIKI_MINUS_SIGN:
                if( shift )
                  return DIKS_UNDERSCORE;
                else
                  return DIKS_MINUS_SIGN;
           case DIKI_COMMA:
                if( shift )
                  return DIKS_LESS_THAN_SIGN;
                else
                  return DIKS_COMMA;

          case DIKI_PERIOD:
                if( shift )
                  return DIKS_GREATER_THAN_SIGN;
                else
                  return DIKS_PERIOD;

           case DIKI_BRACKET_LEFT:
                if( shift )
                  return DIKS_CURLY_BRACKET_LEFT;
                else
                  return DIKS_SQUARE_BRACKET_LEFT;

           case DIKI_BRACKET_RIGHT:
                if( shift )
                  return DIKS_CURLY_BRACKET_RIGHT;
                else
                  return DIKS_SQUARE_BRACKET_RIGHT;
          case DIKI_QUOTE_LEFT:
                if( shift )
                  return DIKS_TILDE;
                else
                  return DIKS_GRAVE_ACCENT;
          case DIKI_QUOTE_RIGHT:
                if( shift )
                  return DIKS_QUOTATION;
                else
                  return DIKS_APOSTROPHE;
          case DIKI_SEMICOLON:
                if( shift )
                  return DIKS_COLON;
                else
                  return DIKS_SEMICOLON;
          case DIKI_SLASH:
                if( shift )
                  return DIKS_QUESTION_MARK;
                else
                  return DIKS_SLASH;

          default:
               ;
     }

     return DIKS_NULL;
}
/*
 * Fetch one entry from the device's keymap if supported.
 * this does a fake mapping based on the orginal DFB code
 */
static DFBResult
driver_get_keymap_entry( CoreInputDevice           *device,
                         void                      *driver_data,
                         DFBInputDeviceKeymapEntry *entry )
{
	int  code = entry->code;
    entry->identifier=code;

     /* is CapsLock effective? */
     if (code >= DIKI_A && code <= DIKI_Z)
		entry->locks |= DILS_CAPS;

     /* is NumLock effective? */
     if (entry->identifier >= DIKI_KP_DECIMAL && entry->identifier <= DIKI_KP_9)
          entry->locks |= DILS_NUM;

     entry->symbols[DIKSI_BASE]=id_to_symbol(entry->identifier,0);
     entry->symbols[DIKSI_BASE_SHIFT]=id_to_symbol(entry->identifier,DIMM_SHIFT);
     entry->symbols[DIKSI_ALT]=entry->symbols[DIKSI_BASE];
     entry->symbols[DIKSI_ALT_SHIFT]=entry->symbols[DIKSI_BASE_SHIFT];
     return DFB_OK;
    
}

/*
 * End thread, close device and free private data.
 */
static void
driver_close_device( void *driver_data )
{
	X11InputData *data = (X11InputData*) driver_data;

     /* stop input thread */
     data->stop = 1;

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* free private data */
     D_FREE ( data );
}

