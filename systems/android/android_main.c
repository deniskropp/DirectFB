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

#include <linux/input.h>

#include "android_system.h"

// FIXME
AndroidNativeData native_data;
extern AndroidData *m_data;

/**********************************************************************************************************************/

extern int main( int argc, char **argv );

static void *
dfb_main_thread( DirectThread *thread,
                 void         *arg )
{
     int   ret;
     char *argv[] = { "android-native-dfb-app", "--dfb:debug" };

     LOGI( "Running main()..." );

     ret = main( 1, argv );

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

          if ((meta & AMETA_SHIFT_ON) || (meta & AMETA_SHIFT_LEFT_ON) || (meta & AMETA_SHIFT_RIGHT_ON))
               evt.modifiers |= DIMM_SHIFT;

          if ((meta & AMETA_ALT_ON) || (meta & AMETA_ALT_LEFT_ON) || (meta & AMETA_ALT_RIGHT_ON))
               evt.modifiers |= DIMM_ALT;

          if (meta & AMETA_SYM_ON)
               evt.modifiers |= DIMM_HYPER;

          switch (action) {
               case AMOTION_EVENT_ACTION_DOWN:
                    evt.type    = DIET_BUTTONPRESS;
                    evt.button  = DIBI_LEFT;
                    evt.buttons = DIBM_LEFT;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = AMotionEvent_getX( event, 0 );

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.axisabs = AMotionEvent_getY( event, 0 );
                    evt.flags  &= ~DIEF_FOLLOW;

                    dfb_input_dispatch( m_data->input, &evt );
LOGW("dispatched motion event DOWN\n");
                    return 1;

               case AMOTION_EVENT_ACTION_UP:
                    evt.type    = DIET_BUTTONRELEASE;
                    evt.button  = DIBI_LEFT;
                    evt.buttons = DIBM_LEFT;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = AMotionEvent_getX( event, 0 );

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.axisabs = AMotionEvent_getY( event, 0 );
                    evt.flags  &= ~DIEF_FOLLOW;

                    dfb_input_dispatch( m_data->input, &evt );
LOGW("dispatched motion event UP\n");
                    return 1;

               case AMOTION_EVENT_ACTION_MOVE:
                    evt.type    = DIET_AXISMOTION;
                    evt.flags   = DIEF_FOLLOW | DIEF_AXISABS;
                    evt.axis    = DIAI_X;
                    evt.axisabs = AMotionEvent_getX( event, 0 );

                    dfb_input_dispatch( m_data->input, &evt );

                    evt.axis    = DIAI_Y;
                    evt.axisabs = AMotionEvent_getY( event, 0 );
                    evt.flags  &= ~DIEF_FOLLOW;

                    dfb_input_dispatch( m_data->input, &evt );
LOGW("dispatched motion event MOVE\n");
                    return 1;

               default:
                    LOGW( "unhandled motion event action %d at %d:%d", action, AMotionEvent_getX( event, 0 ), AMotionEvent_getY( event, 0 ) );
                    return 0;
          }
     }
     else if (type == AINPUT_EVENT_TYPE_KEY) {
          int action = AKeyEvent_getAction( event );
          int meta   = AKeyEvent_getMetaState( event );
          int flags  = AKeyEvent_getFlags( event );
          
          if (!(flags & AKEY_EVENT_FLAG_FROM_SYSTEM)) {
               LOGW( "unhandled key event action %d (non-system)", action );
               return 0;
          }

          if (flags & AKEY_EVENT_FLAG_CANCELED) {
               LOGW( "unhandled key event action %d (canceled)", action );
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
                    LOGW( "unhandled key event action %d", action );
                    return 0;
          }

          evt.flags    = DIEF_KEYCODE | DIEF_KEYID;
          evt.key_code = AKeyEvent_getKeyCode( event );

          switch (evt.key_code) {
               case KEY_ESC:
                    evt.key_id = DIKI_ESCAPE;
                    break;
               case KEY_1:
                    evt.key_id = DIKI_1;
                    break;
               case KEY_2:
                    evt.key_id = DIKI_2;
                    break;
               case KEY_3:
                    evt.key_id = DIKI_3;
                    break;
               case KEY_4:
                    evt.key_id = DIKI_4;
                    break;
               case KEY_5:
                    evt.key_id = DIKI_5;
                    break;
               case KEY_6:
                    evt.key_id = DIKI_6;
                    break;
               case KEY_7:
                    evt.key_id = DIKI_7;
                    break;
               case KEY_8:
                    evt.key_id = DIKI_8;
                    break;
               case KEY_9:
                    evt.key_id = DIKI_9;
                    break;
               case KEY_0:
                    evt.key_id = DIKI_0;
                    break;
               case KEY_MINUS:
                    evt.key_id = DIKI_MINUS_SIGN;
                    break;
               case KEY_EQUAL:
                    evt.key_id = DIKI_EQUALS_SIGN;
                    break;
               case KEY_BACKSPACE:
                    evt.key_id = DIKI_BACKSPACE;
                    break;
               case KEY_TAB:
                    evt.key_id = DIKI_TAB;
                    break;
               case KEY_Q:
                    evt.key_id = DIKI_Q;
                    break;
               case KEY_W:
                    evt.key_id = DIKI_W;
                    break;
               case KEY_E:
                    evt.key_id = DIKI_E;
                    break;
               case KEY_R:
                    evt.key_id = DIKI_R;
                    break;
               case KEY_T:
                    evt.key_id = DIKI_T;
                    break;
               case KEY_Y:
                    evt.key_id = DIKI_Y;
                    break;
               case KEY_U:
                    evt.key_id = DIKI_U;
                    break;
               case KEY_I:
                    evt.key_id = DIKI_I;
                    break;
               case KEY_O:
                    evt.key_id = DIKI_O;
                    break;
               case KEY_P:
                    evt.key_id = DIKI_P;
                    break;
               case KEY_LEFTBRACE:
                    evt.key_id = DIKI_BRACKET_LEFT;
                    break;
               case KEY_RIGHTBRACE:
                    evt.key_id = DIKI_BRACKET_RIGHT;
                    break;
               case KEY_ENTER:
                    evt.key_id = DIKI_ENTER;
                    break;
               case KEY_LEFTCTRL:
                    evt.key_id = DIKI_CONTROL_L;
                    break;
               case KEY_A:
                    evt.key_id = DIKI_A;
                    break;
               case KEY_S:
                    evt.key_id = DIKI_S;
                    break;
               case KEY_D:
                    evt.key_id = DIKI_D;
                    break;
               case KEY_F:
                    evt.key_id = DIKI_F;
                    break;
               case KEY_G:
                    evt.key_id = DIKI_G;
                    break;
               case KEY_H:
                    evt.key_id = DIKI_H;
                    break;
               case KEY_J:
                    evt.key_id = DIKI_J;
                    break;
               case KEY_K:
                    evt.key_id = DIKI_K;
                    break;
               case KEY_L:
                    evt.key_id = DIKI_L;
                    break;
               case KEY_SEMICOLON:
                    evt.key_id = DIKI_SEMICOLON;
                    break;
               case KEY_APOSTROPHE:
                    evt.key_id = 0;
                    break;
               case KEY_GRAVE:
                    evt.key_id = 0;
                    break;
               case KEY_LEFTSHIFT:
                    evt.key_id = DIKI_SHIFT_L;
                    break;
               case KEY_BACKSLASH:
                    evt.key_id = DIKI_BACKSLASH;
                    break;
               case KEY_Z:
                    evt.key_id = DIKI_Z;
                    break;
               case KEY_X:
                    evt.key_id = DIKI_X;
                    break;
               case KEY_C:
                    evt.key_id = DIKI_C;
                    break;
               case KEY_V:
                    evt.key_id = DIKI_V;
                    break;
               case KEY_B:
                    evt.key_id = DIKI_B;
                    break;
               case KEY_N:
                    evt.key_id = DIKI_N;
                    break;
               case KEY_M:
                    evt.key_id = DIKI_M;
                    break;
               case KEY_COMMA:
                    evt.key_id = DIKI_COMMA;
                    break;
               case KEY_DOT:
                    evt.key_id = 0;
                    break;
               case KEY_SLASH:
                    evt.key_id = DIKI_SLASH;
                    break;
               case KEY_RIGHTSHIFT:
                    evt.key_id = DIKI_SHIFT_R;
                    break;
               case KEY_KPASTERISK:
                    evt.key_id = 0;
                    break;
               case KEY_LEFTALT:
                    evt.key_id = DIKI_ALT_L;
                    break;
               case KEY_SPACE:
                    evt.key_id = DIKI_SPACE;
                    break;
               case KEY_CAPSLOCK:
                    evt.key_id = DIKI_CAPS_LOCK;
                    break;
               case KEY_F1:
                    evt.key_id = DIKI_F1;
                    break;
               case KEY_F2:
                    evt.key_id = DIKI_F2;
                    break;
               case KEY_F3:
                    evt.key_id = DIKI_F3;
                    break;
               case KEY_F4:
                    evt.key_id = DIKI_F4;
                    break;
               case KEY_F5:
                    evt.key_id = DIKI_F5;
                    break;
               case KEY_F6:
                    evt.key_id = DIKI_F6;
                    break;
               case KEY_F7:
                    evt.key_id = DIKI_F7;
                    break;
               case KEY_F8:
                    evt.key_id = DIKI_F8;
                    break;
               case KEY_F9:
                    evt.key_id = DIKI_F9;
                    break;
               case KEY_F10:
                    evt.key_id = DIKI_F10;
                    break;
               case KEY_NUMLOCK:
                    evt.key_id = DIKI_NUM_LOCK;
                    break;
               case KEY_SCROLLLOCK:
                    evt.key_id = DIKI_SCROLL_LOCK;
                    break;
               case KEY_KP7:
                    evt.key_id = DIKI_KP_7;
                    break;
               case KEY_KP8:
                    evt.key_id = DIKI_KP_8;
                    break;
               case KEY_KP9:
                    evt.key_id = DIKI_KP_9;
                    break;
               case KEY_KPMINUS:
                    evt.key_id = DIKI_KP_MINUS;
                    break;
               case KEY_KP4:
                    evt.key_id = DIKI_KP_4;
                    break;
               case KEY_KP5:
                    evt.key_id = DIKI_KP_5;
                    break;
               case KEY_KP6:
                    evt.key_id = DIKI_KP_6;
                    break;
               case KEY_KPPLUS:
                    evt.key_id = DIKI_KP_PLUS;
                    break;
               case KEY_KP1:
                    evt.key_id = DIKI_KP_1;
                    break;
               case KEY_KP2:
                    evt.key_id = DIKI_KP_2;
                    break;
               case KEY_KP3:
                    evt.key_id = DIKI_KP_3;
                    break;
               case KEY_KP0:
                    evt.key_id = DIKI_KP_0;
                    break;
               case KEY_KPDOT:
                    evt.key_id = 0;
                    break;
               default:
                    LOGW( "unhandled key event action %d key_code %d", action, evt.key_code );
                    return 0;
          }

          dfb_input_dispatch( m_data->input, &evt );
LOGW("dispatched key event\n");
          return 1;
     }

     return 1;
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
               if (native_data->app->window != NULL) {
//                    native_init_display(native_data);
//                    native_draw_frame(native_data);

                    native_data->main_thread = direct_thread_create( DTT_DEFAULT, dfb_main_thread, native_data, "dfb-main" );
               }
               break;
          case APP_CMD_TERM_WINDOW:
               // The window is being hidden or closed, clean it up.
//               native_term_display(native_data);

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
                              LOGI("accelerometer: x=%f y=%f z=%f",
                                   event.acceleration.x, event.acceleration.y,
                                   event.acceleration.z);
                         }
                    }
               }

               // Check if we are exiting.
               if (state->destroyRequested != 0) {
//                    native_term_display(&native_data);
                    return;
               }
          }
     }
}

