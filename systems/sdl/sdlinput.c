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

#include <config.h>

#include <string.h>
#include <unistd.h>
#include <errno.h>

#include <directfb.h>

#include <SDL.h>

#include <core/coredefs.h>
#include <core/coretypes.h>
#include <core/input.h>
#include <core/system.h>

#include <direct/mem.h>
#include <direct/thread.h>

#include "sdl.h"

#include <core/input_driver.h>


DFB_INPUT_DRIVER( sdlinput )

/*
 * declaration of private data
 */
typedef struct {
     CoreInputDevice *device;
     DirectThread    *thread;
     DFBSDL          *dfb_sdl;
     int              stop;
} SDLInputData;


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
motion_realize( SDLInputData *data )
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
translate_key( SDLKey key, DFBInputEvent *evt )
{
     /* Numeric keypad */
     if (key >= SDLK_KP0  &&  key <= SDLK_KP9) {
          evt->flags = DIEF_KEYID;
          evt->key_id = DIKI_KP_0 + key - SDLK_KP0;
          return true;
     }

     /* Function keys */
     if (key >= SDLK_F1  &&  key <= SDLK_F15) {
          evt->flags = DIEF_KEYSYMBOL;
          evt->key_symbol = DIKS_F1 + key - SDLK_F1;
          return true;
     }

     switch (key) {
          /* Numeric keypad */
          case SDLK_KP_PERIOD:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_DECIMAL;
               break;

          case SDLK_KP_DIVIDE:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_DIV;
               break;

          case SDLK_KP_MULTIPLY:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_MULT;
               break;

          case SDLK_KP_MINUS:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_MINUS;
               break;

          case SDLK_KP_PLUS:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_PLUS;
               break;

          case SDLK_KP_ENTER:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_ENTER;
               break;

          case SDLK_KP_EQUALS:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_KP_EQUAL;
               break;


          /* Arrows + Home/End pad */
          case SDLK_UP:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_CURSOR_UP;
               break;

          case SDLK_DOWN:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_CURSOR_DOWN;
               break;

          case SDLK_RIGHT:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_CURSOR_RIGHT;
               break;

          case SDLK_LEFT:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_CURSOR_LEFT;
               break;

          case SDLK_INSERT:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_INSERT;
               break;

          case SDLK_HOME:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_HOME;
               break;

          case SDLK_END:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_END;
               break;

          case SDLK_PAGEUP:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_PAGE_UP;
               break;

          case SDLK_PAGEDOWN:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_PAGE_DOWN;
               break;


          /* Key state modifier keys */
          case SDLK_NUMLOCK:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_NUM_LOCK;
               break;

          case SDLK_CAPSLOCK:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_CAPS_LOCK;
               break;

          case SDLK_SCROLLOCK:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_SCROLL_LOCK;
               break;

          case SDLK_RSHIFT:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SHIFT_R;
               break;

          case SDLK_LSHIFT:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SHIFT_L;
               break;

          case SDLK_RCTRL:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_CONTROL_R;
               break;

          case SDLK_LCTRL:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_CONTROL_L;
               break;

          case SDLK_RALT:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ALT_R;
               break;

          case SDLK_LALT:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ALT_L;
               break;

          case SDLK_RMETA:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_META_R;
               break;

          case SDLK_LMETA:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_META_L;
               break;

          case SDLK_LSUPER:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SUPER_L;
               break;

          case SDLK_RSUPER:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_SUPER_R;
               break;

          case SDLK_MODE:
               evt->flags = DIEF_KEYID;
               evt->key_id = DIKI_ALTGR;
               break;


          /* Miscellaneous function keys */
          case SDLK_HELP:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_HELP;
               break;

          case SDLK_PRINT:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_PRINT;
               break;

          case SDLK_BREAK:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_BREAK;
               break;

          case SDLK_MENU:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_MENU;
               break;

          case SDLK_POWER:
               evt->flags = DIEF_KEYSYMBOL;
               evt->key_symbol = DIKS_POWER;
               break;


          default:
               return false;
     }

     return true;
}

/*
 * Input thread reading from device.
 * Generates events on incoming data.
 */
static void*
sdlEventThread( DirectThread *thread, void *driver_data )
{
     SDLInputData *data    = (SDLInputData*) driver_data;
     DFBSDL       *dfb_sdl = data->dfb_sdl;

     while (!data->stop) {
          DFBInputEvent evt;
          SDL_Event     event;

          fusion_skirmish_prevail( &dfb_sdl->lock );

          /* Check for events */
          while ( SDL_PollEvent(&event) ) {
               fusion_skirmish_dismiss( &dfb_sdl->lock );

               switch (event.type) {
                    case SDL_MOUSEMOTION:
                         motion_compress( event.motion.x, event.motion.y );
                         break;

                    case SDL_MOUSEBUTTONUP:
                    case SDL_MOUSEBUTTONDOWN:
                         motion_realize( data );

                         if (event.type == SDL_MOUSEBUTTONDOWN)
                              evt.type = DIET_BUTTONPRESS;
                         else
                              evt.type = DIET_BUTTONRELEASE;

                         evt.flags = DIEF_NONE;

                         switch (event.button.button) {
                              case SDL_BUTTON_LEFT:
                                   evt.button = DIBI_LEFT;
                                   break;
                              case SDL_BUTTON_MIDDLE:
                                   evt.button = DIBI_MIDDLE;
                                   break;
                              case SDL_BUTTON_RIGHT:
                                   evt.button = DIBI_RIGHT;
                                   break;
                              case SDL_BUTTON_WHEELUP:
                              case SDL_BUTTON_WHEELDOWN:
                                   if (event.type != SDL_MOUSEBUTTONDOWN) {
                                        fusion_skirmish_prevail( &dfb_sdl->lock );
                                        continue;
                                   }
                                   evt.type  = DIET_AXISMOTION;
                                   evt.flags = DIEF_AXISREL;
                                   evt.axis  = DIAI_Z;
                                   if (event.button.button == SDL_BUTTON_WHEELUP)
                                        evt.axisrel = -1;
                                   else
                                        evt.axisrel = 1;
                                   break;
                              default:
                                   fusion_skirmish_prevail( &dfb_sdl->lock );
                                   continue;
                         }

                         dfb_input_dispatch( data->device, &evt );
                         break;

                    case SDL_KEYUP:
                    case SDL_KEYDOWN:
			//XXX sdl does not give us the keymap
			 evt.key_code=event.key.keysym.scancode;
                         if (event.type == SDL_KEYDOWN)
                              evt.type = DIET_KEYPRESS;
                         else
                              evt.type = DIET_KEYRELEASE;
                         if (translate_key( event.key.keysym.sym, &evt )) {
			      evt.key_code=evt.key_id;
                              dfb_input_dispatch( data->device, &evt );
                         }
                         else if (event.key.keysym.unicode) {
                              evt.flags      = DIEF_KEYSYMBOL;
                              evt.key_symbol = event.key.keysym.unicode;
			      evt.key_code=evt.key_id;
                              dfb_input_dispatch( data->device, &evt );
                         }

                         break;

                    case SDL_QUIT:
                         evt.type       = DIET_KEYPRESS;
                         evt.flags      = DIEF_KEYSYMBOL;
                         evt.key_symbol = DIKS_ESCAPE;

                         dfb_input_dispatch( data->device, &evt );

                         evt.type       = DIET_KEYRELEASE;
                         evt.flags      = DIEF_KEYSYMBOL;
                         evt.key_symbol = DIKS_ESCAPE;

                         dfb_input_dispatch( data->device, &evt );
                         break;

                    default:
                         break;
               }

               fusion_skirmish_prevail( &dfb_sdl->lock );
          }

          fusion_skirmish_dismiss( &dfb_sdl->lock );

          motion_realize( data );

          usleep(10000);

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
     if (dfb_system_type() == CORE_SDL)
          return 1;

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
                DFB_INPUT_DRIVER_INFO_NAME_LENGTH, "SDL Input Driver" );
     snprintf ( info->vendor,
                DFB_INPUT_DRIVER_INFO_VENDOR_LENGTH, "Denis Oliver Kropp" );

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
     SDLInputData *data;
     DFBSDL       *dfb_sdl = dfb_system_data();

     fusion_skirmish_prevail( &dfb_sdl->lock );

     SDL_EnableUNICODE( true );

     SDL_EnableKeyRepeat( 250, 40 );

     fusion_skirmish_dismiss( &dfb_sdl->lock );

     /* set device name */
     snprintf( info->desc.name,
               DFB_INPUT_DEVICE_DESC_NAME_LENGTH, "SDL Input" );

     /* set device vendor */
     snprintf( info->desc.vendor,
               DFB_INPUT_DEVICE_DESC_VENDOR_LENGTH, "SDL" );

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
     data = D_CALLOC( 1, sizeof(SDLInputData) );

     data->device  = device;
     data->dfb_sdl = dfb_sdl;

     /* start input thread */
     data->thread = direct_thread_create( DTT_INPUT, sdlEventThread, data, "SDL Input" );

     /* set private data pointer */
     *driver_data = data;

     return DFB_OK;
}


static DFBInputDeviceKeySymbol
id_to_symbol( DFBInputDeviceKeyIdentifier id,
              DFBInputDeviceModifierMask  modifiers)
{
     bool shift = (modifiers & DIMM_SHIFT);

     if (id >= DIKI_A && id <= DIKI_Z)
          return (shift ? DIKS_CAPITAL_A : DIKS_SMALL_A) + id - DIKI_A;

     if (id >= DIKI_0 && id <= DIKI_9)
          return DIKS_0 + id - DIKI_0;

     if (id >= DIKI_KP_0 && id <= DIKI_KP_9)
          return DIKS_0 + id - DIKI_KP_0;

     if (id >= DIKI_F1 && id <= DIKI_F12)
          return DIKS_F1 + id - DIKI_F1;

     switch (id) {
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

          case DIKI_ALTGR:
               return DIKS_ALTGR;

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
               return DIKS_BACKSLASH;

          case DIKI_EQUALS_SIGN:
               return DIKS_EQUALS_SIGN;

          case DIKI_LESS_SIGN:
               return DIKS_LESS_THAN_SIGN;

          case DIKI_MINUS_SIGN:
               return DIKS_MINUS_SIGN;

          case DIKI_PERIOD:
               return DIKS_PERIOD;

          case DIKI_QUOTE_LEFT:
          case DIKI_QUOTE_RIGHT:
               return DIKS_QUOTATION;

          case DIKI_SEMICOLON:
               return DIKS_SEMICOLON;

          case DIKI_SLASH:
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
     int                         code = entry->code;
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
     SDLInputData *data = (SDLInputData*) driver_data;

     /* stop input thread */
     data->stop = 1;

     direct_thread_join( data->thread );
     direct_thread_destroy( data->thread );

     /* free private data */
     D_FREE ( data );
}

