/*
   (c) Copyright 2001-2012  The world wide DirectFB Open Source Community (directfb.org)
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
   //char *argv[] = { "android-native-dfb-app", "--dfb:no-cursor-updates,no-sighandler,layer-buffer-mode=backvideo,debug=ANDROID/Main,debug=core/input,debug=core/inputevt" };
     char *argv[] = { "android-native-dfb-app", "--dfb:no-cursor-updates,no-sighandler,layer-buffer-mode=backvideo,wm-fullscreen-updates,debug=GL,debug=EGL,debug=idirectfbsurface,debug=idirectfbsurfacew" };

     LOGI( "Running main()..." );

     ret = main( 2, argv );

     LOGI( "main() has returned %d!", ret );

     return NULL;
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

          switch (evt.key_code) {
               case AKEYCODE_DPAD_UP:
                    evt.key_id = DIKI_UP;
                    break;
               case AKEYCODE_DPAD_DOWN:
                    evt.key_id = DIKI_DOWN;
                    break;
               case AKEYCODE_DPAD_LEFT:
                    evt.key_id = DIKI_LEFT;
                    break;
               case AKEYCODE_DPAD_RIGHT:
                    evt.key_id = DIKI_RIGHT;
                    break;
               case AKEYCODE_BACK:
                    evt.key_id = DIKI_ESCAPE;
                    break;
               case AKEYCODE_1:
                    evt.key_id = DIKI_1;
                    break;
               case AKEYCODE_2:
                    evt.key_id = DIKI_2;
                    break;
               case AKEYCODE_3:
                    evt.key_id = DIKI_3;
                    break;
               case AKEYCODE_4:
                    evt.key_id = DIKI_4;
                    break;
               case AKEYCODE_5:
                    evt.key_id = DIKI_5;
                    break;
               case AKEYCODE_6:
                    evt.key_id = DIKI_6;
                    break;
               case AKEYCODE_7:
                    evt.key_id = DIKI_7;
                    break;
               case AKEYCODE_8:
                    evt.key_id = DIKI_8;
                    break;
               case AKEYCODE_9:
                    evt.key_id = DIKI_9;
                    break;
               case AKEYCODE_0:
                    evt.key_id = DIKI_0;
                    break;
               case AKEYCODE_MINUS:
                    evt.key_id = DIKI_MINUS_SIGN;
                    break;
               case AKEYCODE_TAB:
                    evt.key_id = DIKI_TAB;
                    break;
               case AKEYCODE_Q:
                    evt.key_id = DIKI_Q;
                    break;
               case AKEYCODE_W:
                    evt.key_id = DIKI_W;
                    break;
               case AKEYCODE_E:
                    evt.key_id = DIKI_E;
                    break;
               case AKEYCODE_R:
                    evt.key_id = DIKI_R;
                    break;
               case AKEYCODE_T:
                    evt.key_id = DIKI_T;
                    break;
               case AKEYCODE_Y:
                    evt.key_id = DIKI_Y;
                    break;
               case AKEYCODE_U:
                    evt.key_id = DIKI_U;
                    break;
               case AKEYCODE_I:
                    evt.key_id = DIKI_I;
                    break;
               case AKEYCODE_O:
                    evt.key_id = DIKI_O;
                    break;
               case AKEYCODE_P:
                    evt.key_id = DIKI_P;
                    break;
               case AKEYCODE_LEFT_BRACKET:
                    evt.key_id = DIKI_BRACKET_LEFT;
                    break;
               case AKEYCODE_RIGHT_BRACKET:
                    evt.key_id = DIKI_BRACKET_RIGHT;
                    break;
               case AKEYCODE_ENTER:
                    evt.key_id = DIKI_ENTER;
                    break;
               case AKEYCODE_A:
                    evt.key_id = DIKI_A;
                    break;
               case AKEYCODE_S:
                    evt.key_id = DIKI_S;
                    break;
               case AKEYCODE_D:
                    evt.key_id = DIKI_D;
                    break;
               case AKEYCODE_F:
                    evt.key_id = DIKI_F;
                    break;
               case AKEYCODE_G:
                    evt.key_id = DIKI_G;
                    break;
               case AKEYCODE_H:
                    evt.key_id = DIKI_H;
                    break;
               case AKEYCODE_J:
                    evt.key_id = DIKI_J;
                    break;
               case AKEYCODE_K:
                    evt.key_id = DIKI_K;
                    break;
               case AKEYCODE_L:
                    evt.key_id = DIKI_L;
                    break;
               case AKEYCODE_SEMICOLON:
                    evt.key_id = DIKI_SEMICOLON;
                    break;
               case AKEYCODE_APOSTROPHE:
                    evt.key_id = 0;
                    break;
               case AKEYCODE_GRAVE:
                    evt.key_id = 0;
                    break;
               case AKEYCODE_SHIFT_LEFT:
                    evt.key_id = DIKI_SHIFT_L;
                    break;
               case AKEYCODE_SHIFT_RIGHT:
                    evt.key_id = DIKI_SHIFT_R;
                    break;
               case AKEYCODE_ALT_LEFT:
                    evt.key_id = DIKI_ALT_L;
                    break;
               case AKEYCODE_ALT_RIGHT:
                    evt.key_id = DIKI_ALT_R;
                    break;
               case AKEYCODE_CTRL_LEFT:
                    evt.key_id = DIKI_CONTROL_L;
                    break;
               case AKEYCODE_CTRL_RIGHT:
                    evt.key_id = DIKI_CONTROL_R;
                    break;
               case AKEYCODE_BACKSLASH:
                    evt.key_id = DIKI_BACKSLASH;
                    break;
               case AKEYCODE_Z:
                    evt.key_id = DIKI_Z;
                    break;
               case AKEYCODE_X:
                    evt.key_id = DIKI_X;
                    break;
               case AKEYCODE_C:
                    evt.key_id = DIKI_C;
                    break;
               case AKEYCODE_V:
                    evt.key_id = DIKI_V;
                    break;
               case AKEYCODE_B:
                    evt.key_id = DIKI_B;
                    break;
               case AKEYCODE_N:
                    evt.key_id = DIKI_N;
                    break;
               case AKEYCODE_M:
                    evt.key_id = DIKI_M;
                    break;
               case AKEYCODE_COMMA:
                    evt.key_id = DIKI_COMMA;
                    break;
               case AKEYCODE_SLASH:
                    evt.key_id = DIKI_SLASH;
                    break;
               case AKEYCODE_SPACE:
                    evt.key_id = DIKI_SPACE;
                    break;
               case AKEYCODE_F1:
                    evt.key_id = DIKI_F1;
                    break;
               case AKEYCODE_F2:
                    evt.key_id = DIKI_F2;
                    break;
               case AKEYCODE_F3:
                    evt.key_id = DIKI_F3;
                    break;
               case AKEYCODE_F4:
                    evt.key_id = DIKI_F4;
                    break;
               case AKEYCODE_F5:
                    evt.key_id = DIKI_F5;
                    break;
               case AKEYCODE_F6:
                    evt.key_id = DIKI_F6;
                    break;
               case AKEYCODE_F7:
                    evt.key_id = DIKI_F7;
                    break;
               case AKEYCODE_F8:
                    evt.key_id = DIKI_F8;
                    break;
               case AKEYCODE_F9:
                    evt.key_id = DIKI_F9;
                    break;
               case AKEYCODE_F10:
                    evt.key_id = DIKI_F10;
                    break;
               case AKEYCODE_F11:
                    evt.key_id = DIKI_F11;
                    break;
               case AKEYCODE_F12:
                    evt.key_id = DIKI_F12;
                    break;

               default:
                    D_DEBUG_AT( ANDROID_MAIN, "unhandled key event action %d key_code %d", action, evt.key_code );
                    return 0;
          }

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

