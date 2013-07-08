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



#include <config.h>

#include <core/layers.h>
#include <core/screens.h>

#include <misc/conf.h>

#include <directfb_keyboard.h>


#include "android_keycodes.h"
#include "android_system.h"

D_DEBUG_DOMAIN( ANDROID_MAIN, "ANDROID/Main", "Android Main" );

// FIXME
AndroidNativeData native_data;
extern AndroidData *m_data;

/**********************************************************************************************************************/

extern int main( int argc, char **argv );


static inline void crashme()
{
     char* a = 0;
     *a = 0;
}


static void *
dfb_main_thread( DirectThread *thread,
                 void         *arg )
{
     int   ret;
//   char *argv[] = { "android-native-dfb-app", "--dfb:debug=ANDROID,debug=direct/interface" };
//     char *argv[] = { "android-native-dfb-app", "--dfb:no-debug", "-a" };
   char *argv[] = { "android-native-dfb-app", "--dfb:no-cursor-updates,wm-fullscreen-updates,no-sighandler,layer-buffer-mode=backvideo,wm=sawman"};
     //char *argv[] = { "android-native-dfb-app", "--dfb:no-cursor-updates,no-sighandler,layer-buffer-mode=backvideo,wm-fullscreen-updates,debug,no-debug=IDFBEventBuffer,no-debug=Core/GraphicsOps,no-debug=Core/GfxState,no-debug=Fusion/Skirmish,no-debug=Direct/Serial,no-debug=Core/SurfAllocation,no-debug=Core/WM,no-debug=GLES2/2D,no-debug=Core/SurfBuffer,no-debug=Core/Layers,no-debug=Core/SurfPoolLock,no-debug=Core/Input,no-debug=Core/LayerContext,no-debug=Core/WindowStack" };

     LOGI( "Running main()..." );

     ret = main( 2, argv );

     LOGI( "main() has returned %d!", ret );

     return NULL;
}

static DFBInputDeviceKeyIdentifier
translate_keycode( const int key_code )
{
     switch (key_code) {
          case AKEYCODE_UNKNOWN:             return DIKI_UNKNOWN;
          case AKEYCODE_SOFT_LEFT:           break;
          case AKEYCODE_SOFT_RIGHT:          break;
          case AKEYCODE_HOME:                break;
          case AKEYCODE_BACK:                return DIKI_ESCAPE;
          case AKEYCODE_CALL:                break;
          case AKEYCODE_ENDCALL:             break;
          case AKEYCODE_0:                   return DIKI_0;
          case AKEYCODE_1:                   return DIKI_1;
          case AKEYCODE_2:                   return DIKI_2;
          case AKEYCODE_3:                   return DIKI_3;
          case AKEYCODE_4:                   return DIKI_4;
          case AKEYCODE_5:                   return DIKI_5;
          case AKEYCODE_6:                   return DIKI_6;
          case AKEYCODE_7:                   return DIKI_7;
          case AKEYCODE_8:                   return DIKI_8;
          case AKEYCODE_9:                   return DIKI_9;
          case AKEYCODE_STAR:                break;
          case AKEYCODE_POUND:               break;
          case AKEYCODE_DPAD_UP:             return DIKI_UP;
          case AKEYCODE_DPAD_DOWN:           return DIKI_DOWN;
          case AKEYCODE_DPAD_LEFT:           return DIKI_LEFT;
          case AKEYCODE_DPAD_RIGHT:          return DIKI_RIGHT;
          case AKEYCODE_DPAD_CENTER:         break;
          case AKEYCODE_VOLUME_UP:           break;
          case AKEYCODE_VOLUME_DOWN:         break;
          case AKEYCODE_POWER:               break;
          case AKEYCODE_CAMERA:              break;
          case AKEYCODE_CLEAR:               break;
          case AKEYCODE_A:                   return DIKI_A;
          case AKEYCODE_B:                   return DIKI_B;
          case AKEYCODE_C:                   return DIKI_C;
          case AKEYCODE_D:                   return DIKI_D;
          case AKEYCODE_E:                   return DIKI_E;
          case AKEYCODE_F:                   return DIKI_F;
          case AKEYCODE_G:                   return DIKI_G;
          case AKEYCODE_H:                   return DIKI_H;
          case AKEYCODE_I:                   return DIKI_I;
          case AKEYCODE_J:                   return DIKI_J;
          case AKEYCODE_K:                   return DIKI_K;
          case AKEYCODE_L:                   return DIKI_L;
          case AKEYCODE_M:                   return DIKI_M;
          case AKEYCODE_N:                   return DIKI_N;
          case AKEYCODE_O:                   return DIKI_O;
          case AKEYCODE_P:                   return DIKI_P;
          case AKEYCODE_Q:                   return DIKI_Q;
          case AKEYCODE_R:                   return DIKI_R;
          case AKEYCODE_S:                   return DIKI_S;
          case AKEYCODE_T:                   return DIKI_T;
          case AKEYCODE_U:                   return DIKI_U;
          case AKEYCODE_V:                   return DIKI_V;
          case AKEYCODE_W:                   return DIKI_W;
          case AKEYCODE_X:                   return DIKI_X;
          case AKEYCODE_Y:                   return DIKI_Y;
          case AKEYCODE_Z:                   return DIKI_Z;
          case AKEYCODE_COMMA:               return DIKI_COMMA;
          case AKEYCODE_PERIOD:              break;
          case AKEYCODE_ALT_LEFT:            return DIKI_ALT_L;
          case AKEYCODE_ALT_RIGHT:           return DIKI_ALT_R;
          case AKEYCODE_SHIFT_LEFT:          return DIKI_SHIFT_L;
          case AKEYCODE_SHIFT_RIGHT:         return DIKI_SHIFT_R;
          case AKEYCODE_TAB:                 return DIKI_TAB;
          case AKEYCODE_SPACE:               return DIKI_SPACE;
          case AKEYCODE_SYM:                 break;
          case AKEYCODE_EXPLORER:            break;
          case AKEYCODE_ENVELOPE:            break;
          case AKEYCODE_ENTER:               return DIKI_ENTER;
          case AKEYCODE_DEL:                 return DIKI_DELETE;
          case AKEYCODE_GRAVE:               break;
          case AKEYCODE_MINUS:               break;
          case AKEYCODE_EQUALS:              break;
          case AKEYCODE_LEFT_BRACKET:        return DIKI_BRACKET_LEFT;
          case AKEYCODE_RIGHT_BRACKET:       return DIKI_BRACKET_RIGHT;
          case AKEYCODE_BACKSLASH:           return DIKI_BACKSLASH;
          case AKEYCODE_SEMICOLON:           return DIKI_SEMICOLON;
          case AKEYCODE_APOSTROPHE:          break;
          case AKEYCODE_SLASH:               return DIKI_SLASH;
          case AKEYCODE_AT:                  break;
          case AKEYCODE_NUM:                 break;
          case AKEYCODE_HEADSETHOOK:         break;
          case AKEYCODE_FOCUS:               break;
          case AKEYCODE_PLUS:                break;
          case AKEYCODE_MENU:                break;
          case AKEYCODE_NOTIFICATION:        break;
          case AKEYCODE_SEARCH:              break;
          case AKEYCODE_MEDIA_PLAY_PAUSE:    break;
          case AKEYCODE_MEDIA_STOP:          break;
          case AKEYCODE_MEDIA_NEXT:          break;
          case AKEYCODE_MEDIA_PREVIOUS:      break;
          case AKEYCODE_MEDIA_REWIND:        break;
          case AKEYCODE_MEDIA_FAST_FORWARD:  break;
          case AKEYCODE_MUTE:                break;
          case AKEYCODE_PAGE_UP:             return DIKI_PAGE_UP;
          case AKEYCODE_PAGE_DOWN:           return DIKI_PAGE_DOWN;
          case AKEYCODE_PICTSYMBOLS:         break;
          case AKEYCODE_SWITCH_CHARSET:      break;
          case AKEYCODE_BUTTON_A:            break;
          case AKEYCODE_BUTTON_B:            break;
          case AKEYCODE_BUTTON_C:            break;
          case AKEYCODE_BUTTON_X:            break;
          case AKEYCODE_BUTTON_Y:            break;
          case AKEYCODE_BUTTON_Z:            break;
          case AKEYCODE_BUTTON_L1:           break;
          case AKEYCODE_BUTTON_R1:           break;
          case AKEYCODE_BUTTON_L2:           break;
          case AKEYCODE_BUTTON_R2:           break;
          case AKEYCODE_BUTTON_THUMBL:       break;
          case AKEYCODE_BUTTON_THUMBR:       break;
          case AKEYCODE_BUTTON_START:        break;
          case AKEYCODE_BUTTON_SELECT:       break;
          case AKEYCODE_BUTTON_MODE:         break;
          case AKEYCODE_ESCAPE:              return DIKI_ESCAPE;
          case AKEYCODE_FORWARD_DEL:         break;
          case AKEYCODE_CTRL_LEFT:           return DIKI_CONTROL_L;
          case AKEYCODE_CTRL_RIGHT:          return DIKI_CONTROL_R;
          case AKEYCODE_CAPS_LOCK:           return DIKI_CAPS_LOCK;
          case AKEYCODE_SCROLL_LOCK:         return DIKI_SCROLL_LOCK;
          case AKEYCODE_META_LEFT:           return DIKI_META_L;
          case AKEYCODE_META_RIGHT:          return DIKI_META_R;
          case AKEYCODE_FUNCTION:            break;
          case AKEYCODE_SYSRQ:               break;
          case AKEYCODE_BREAK:               break;
          case AKEYCODE_MOVE_HOME:           break;
          case AKEYCODE_MOVE_END:            break;
          case AKEYCODE_INSERT:              return DIKI_INSERT;
          case AKEYCODE_FORWARD:             break;
          case AKEYCODE_MEDIA_PLAY:          break;
          case AKEYCODE_MEDIA_PAUSE:         break;
          case AKEYCODE_MEDIA_CLOSE:         break;
          case AKEYCODE_MEDIA_EJECT:         break;
          case AKEYCODE_MEDIA_RECORD:        break;
          case AKEYCODE_F1:                  return DIKI_F1;
          case AKEYCODE_F2:                  return DIKI_F2;
          case AKEYCODE_F3:                  return DIKI_F3;
          case AKEYCODE_F4:                  return DIKI_F4;
          case AKEYCODE_F5:                  return DIKI_F5;
          case AKEYCODE_F6:                  return DIKI_F6;
          case AKEYCODE_F7:                  return DIKI_F7;
          case AKEYCODE_F8:                  return DIKI_F8;
          case AKEYCODE_F9:                  return DIKI_F9;
          case AKEYCODE_F10:                 return DIKI_F10;
          case AKEYCODE_F11:                 return DIKI_F11;
          case AKEYCODE_F12:                 return DIKI_F12;
          case AKEYCODE_NUM_LOCK:            return DIKI_NUM_LOCK;
          case AKEYCODE_NUMPAD_0:            return DIKI_KP_0;
          case AKEYCODE_NUMPAD_1:            return DIKI_KP_1;
          case AKEYCODE_NUMPAD_2:            return DIKI_KP_2;
          case AKEYCODE_NUMPAD_3:            return DIKI_KP_3;
          case AKEYCODE_NUMPAD_4:            return DIKI_KP_4;
          case AKEYCODE_NUMPAD_5:            return DIKI_KP_5;
          case AKEYCODE_NUMPAD_6:            return DIKI_KP_6;
          case AKEYCODE_NUMPAD_7:            return DIKI_KP_7;
          case AKEYCODE_NUMPAD_8:            return DIKI_KP_8;
          case AKEYCODE_NUMPAD_9:            return DIKI_KP_9;
          case AKEYCODE_NUMPAD_DIVIDE:       return DIKI_KP_DIV;
          case AKEYCODE_NUMPAD_MULTIPLY:     return DIKI_KP_MULT;
          case AKEYCODE_NUMPAD_SUBTRACT:     return DIKI_KP_MINUS;
          case AKEYCODE_NUMPAD_ADD:          return DIKI_KP_PLUS;
          case AKEYCODE_NUMPAD_DOT:          break;
          case AKEYCODE_NUMPAD_COMMA:        return DIKI_COMMA;
          case AKEYCODE_NUMPAD_ENTER:        return DIKI_KP_ENTER;
          case AKEYCODE_NUMPAD_EQUALS:       return DIKI_KP_EQUAL;
          case AKEYCODE_NUMPAD_LEFT_PAREN:   break;
          case AKEYCODE_NUMPAD_RIGHT_PAREN:  break;
          case AKEYCODE_VOLUME_MUTE:         break;
          case AKEYCODE_INFO:                break;
          case AKEYCODE_CHANNEL_UP:          break;
          case AKEYCODE_CHANNEL_DOWN:        break;
          case AKEYCODE_ZOOM_IN:             break;
          case AKEYCODE_ZOOM_OUT:            break;
          case AKEYCODE_TV:                  break;
          case AKEYCODE_WINDOW:              break;
          case AKEYCODE_GUIDE:               break;
          case AKEYCODE_DVR:                 break;
          case AKEYCODE_BOOKMARK:            break;
          case AKEYCODE_CAPTIONS:            break;
          case AKEYCODE_SETTINGS:            break;
          case AKEYCODE_TV_POWER:            break;
          case AKEYCODE_TV_INPUT:            break;
          case AKEYCODE_STB_POWER:           break;
          case AKEYCODE_STB_INPUT:           break;
          case AKEYCODE_AVR_POWER:           break;
          case AKEYCODE_AVR_INPUT:           break;
          case AKEYCODE_PROG_RED:            break;
          case AKEYCODE_PROG_GREEN:          break;
          case AKEYCODE_PROG_YELLOW:         break;
          case AKEYCODE_PROG_BLUE:           break;
          case AKEYCODE_APP_SWITCH:          break;
          case AKEYCODE_BUTTON_1:            break;
          case AKEYCODE_BUTTON_2:            break;
          case AKEYCODE_BUTTON_3:            break;
          case AKEYCODE_BUTTON_4:            break;
          case AKEYCODE_BUTTON_5:            break;
          case AKEYCODE_BUTTON_6:            break;
          case AKEYCODE_BUTTON_7:            break;
          case AKEYCODE_BUTTON_8:            break;
          case AKEYCODE_BUTTON_9:            break;
          case AKEYCODE_BUTTON_10:           break;
          case AKEYCODE_BUTTON_11:           break;
          case AKEYCODE_BUTTON_12:           break;
          case AKEYCODE_BUTTON_13:           break;
          case AKEYCODE_BUTTON_14:           break;
          case AKEYCODE_BUTTON_15:           break;
          case AKEYCODE_BUTTON_16:           break;
          case AKEYCODE_LANGUAGE_SWITCH:     break;
          case AKEYCODE_MANNER_MODE:         break;
          case AKEYCODE_3D_MODE:             break;
          case AKEYCODE_CONTACTS:            break;
          case AKEYCODE_CALENDAR:            break;
          case AKEYCODE_MUSIC:               break;
          case AKEYCODE_CALCULATOR:          break;
          default:                           break;
     }
     return DIKI_UNKNOWN;
}

/**
 * Process the next input event.
 */
static int32_t
native_handle_input( struct android_app *app, AInputEvent *event )
{
     int           type = AInputEvent_getType( event );
     DFBInputEvent evt;

     evt.clazz     = DFEC_INPUT;
     evt.device_id = dfb_input_device_id( m_data->input );

     if (type == AINPUT_EVENT_TYPE_MOTION) {
          int action = AMotionEvent_getAction( event ) & AMOTION_EVENT_ACTION_MASK;
          int meta   = AMotionEvent_getMetaState (event );
          int proc   = 0;
          int pos_x  = AMotionEvent_getX( event, 0 );
          int pos_y  = AMotionEvent_getY( event, 0 );

          if ((meta & AMETA_SHIFT_ON) || (meta & AMETA_SHIFT_LEFT_ON) || (meta & AMETA_SHIFT_RIGHT_ON))
               evt.modifiers |= DIMM_SHIFT;

          if ((meta & AMETA_ALT_ON) || (meta & AMETA_ALT_LEFT_ON) || (meta & AMETA_ALT_RIGHT_ON))
               evt.modifiers |= DIMM_ALT;

          if (meta & AMETA_SYM_ON)
               evt.modifiers |= DIMM_HYPER;

          switch (action) {
               case AMOTION_EVENT_ACTION_UP:
                    evt.type    = DIET_AXISMOTION;
                    evt.button  = DIBI_LEFT;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.w - 1;
                    evt.axisabs = pos_x;

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.h - 1;
                    evt.axisabs = pos_y;

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.type    = DIET_BUTTONRELEASE;
                    evt.flags   = DIEF_NONE;

                    dfb_input_dispatch( m_data->input, &evt );

                    D_DEBUG_AT( ANDROID_MAIN, "dispatched motion event UP (%d,%d)\n", pos_x, pos_y );

                    break;

               case AMOTION_EVENT_ACTION_DOWN:
                    evt.type    = DIET_AXISMOTION;
                    evt.button  = DIBI_LEFT;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.w - 1;
                    evt.axisabs = pos_x;

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.h - 1;
                    evt.axisabs = pos_y;

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.type    = DIET_BUTTONPRESS;
                    evt.flags   = DIEF_NONE;

                    dfb_input_dispatch( m_data->input, &evt );

                    D_DEBUG_AT( ANDROID_MAIN, "dispatched motion event DOWN (%d,%d)\n", pos_x, pos_y );

                    break;

               case AMOTION_EVENT_ACTION_MOVE:
               case 7: //FIXME!!!
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.w - 1;
                    evt.axisabs = pos_x;

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.min     = 0;
                    evt.max     = m_data->shared->screen_size.h - 1;
                    evt.axisabs = pos_y;
                    evt.flags  &= ~DIEF_FOLLOW;

                    dfb_input_dispatch( m_data->input, &evt );

                    D_DEBUG_AT( ANDROID_MAIN, "dispatched motion event MOVE (%d,%d)\n", pos_x, pos_y );

                    break;

               default:
                    D_DEBUG_AT( ANDROID_MAIN, "unhandled motion event action %d at (%d,%d)\n", action, pos_x, pos_y );

                    return 0;
          }
     }
     else if (type == AINPUT_EVENT_TYPE_KEY) {
          int action = AKeyEvent_getAction( event );
          int meta   = AKeyEvent_getMetaState( event );
          int flags  = AKeyEvent_getFlags( event );

          if (flags & AKEY_EVENT_FLAG_CANCELED) {
               D_DEBUG_AT( ANDROID_MAIN, "unhandled key event action %d (cancel)", action );
               return 0;
          }

          if ((meta & AMETA_SHIFT_ON) || (meta & AMETA_SHIFT_LEFT_ON) || (meta & AMETA_SHIFT_RIGHT_ON))        
               evt.modifiers |= DIMM_SHIFT;   

          if ((meta & AMETA_ALT_ON) || (meta & AMETA_ALT_LEFT_ON) || (meta & AMETA_ALT_RIGHT_ON))                   
               evt.modifiers |= DIMM_ALT;          

          if (meta & AMETA_SYM_ON)  
               evt.modifiers |= DIMM_HYPER;

          switch (action) {
               case AKEY_EVENT_ACTION_DOWN:
                    evt.type = DIET_KEYPRESS;
                    break;
               case AKEY_EVENT_ACTION_UP:
                    evt.type = DIET_KEYRELEASE;
                    break;
               default:
                    D_DEBUG_AT( ANDROID_MAIN, "unhandled key event action %d", action );
                    return 0;
          }

          evt.flags    = DIEF_KEYCODE | DIEF_KEYID;
          evt.key_code = AKeyEvent_getKeyCode( event );
          evt.key_id   = translate_keycode( evt.key_code );

          dfb_input_dispatch( m_data->input, &evt );

          return 1;
     }

     return 0;
}

/**
 * Process the next main command.
 */
static void
native_handle_cmd( struct android_app* app, int32_t cmd )
{
     AndroidNativeData* native_data = (AndroidNativeData*)app->userData;

     switch (cmd) {
          case APP_CMD_SAVE_STATE:
               // The system has asked us to save our current state.  Do so.
//               native_data->app->savedState = malloc(sizeof(struct saved_state));
//               *((struct saved_state*)native_data->app->savedState) = native_data->state;
//               native_data->app->savedStateSize = sizeof(struct saved_state);
               break;
          case APP_CMD_INIT_WINDOW:
               // The window is being shown, get it ready.
               //if (native_data->app->window != NULL) {
//                    native_init_display(native_data);
//                    native_draw_frame(native_data);

                    native_data->main_thread = direct_thread_create( DTT_DEFAULT, dfb_main_thread, native_data, "dfb-main" );
               //}
               break;
          case APP_CMD_TERM_WINDOW:
               // The window is being hidden or closed, clean it up.
//               native_term_display(native_data);

               crashme();

               direct_thread_join( native_data->main_thread );
               direct_thread_destroy( native_data->main_thread );
               break;
          case APP_CMD_GAINED_FOCUS:
               // When our app gains focus, we start monitoring the accelerometer.
               if (native_data->accelerometerSensor != NULL) {
                    ASensorEventQueue_enableSensor(native_data->sensorEventQueue,
                                                   native_data->accelerometerSensor);
                    // We'd like to get 60 events per second (in us).
                    ASensorEventQueue_setEventRate(native_data->sensorEventQueue,
                                                   native_data->accelerometerSensor, (1000L/60)*1000);
               }
               break;
          case APP_CMD_LOST_FOCUS:
               // When our app loses focus, we stop monitoring the accelerometer.
               // This is to avoid consuming battery while not being used.

               crashme();

               if (native_data->accelerometerSensor != NULL) {
                    ASensorEventQueue_disableSensor(native_data->sensorEventQueue,
                                                    native_data->accelerometerSensor);
               }
//               native_draw_frame(native_data);
               break;
     }
}

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
void
android_main( struct android_app* state )
{
     // Make sure glue isn't stripped.
     app_dummy();

     memset(&native_data, 0, sizeof(native_data));
     state->userData = &native_data;
     state->onAppCmd = native_handle_cmd;
     state->onInputEvent = native_handle_input;
     native_data.app = state;

     // Prepare to monitor accelerometer
     native_data.sensorManager = ASensorManager_getInstance();
     native_data.accelerometerSensor = ASensorManager_getDefaultSensor(native_data.sensorManager,
                                                                       ASENSOR_TYPE_ACCELEROMETER);
     native_data.sensorEventQueue = ASensorManager_createEventQueue(native_data.sensorManager,
                                                                    state->looper, LOOPER_ID_USER, NULL, NULL);

     if (state->savedState != NULL) {
          // We are starting with a previous saved state; restore from it.
//          native_data.state = *(struct saved_state*)state->savedState;
     }

     // loop waiting for stuff to do.

     while (1) {
          // Read all pending events.
          int ident;
          int events;
          struct android_poll_source* source;

          // If not animating, we will block forever waiting for events.
          // If animating, we loop until all events are read, then continue
          // to draw the next frame of animation.
          while ((ident=ALooper_pollAll(-1, NULL, &events, (void**)&source)) >= 0) {

               // Process this event.
               if (source != NULL) {
                    source->process(state, source);
               }

               // If a sensor has data, process it now.
               if (ident == LOOPER_ID_USER) {
                    if (native_data.accelerometerSensor != NULL) {
                         ASensorEvent event;
                         while (ASensorEventQueue_getEvents(native_data.sensorEventQueue, &event, 1) > 0) {
                              //LOGI("accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y, event.acceleration.z);
                         }
                    }
               }

               // Check if we are exiting.
               if (state->destroyRequested != 0) {
                    //native_term_display(&native_data);
                    return;
               }
          }
     }
}

