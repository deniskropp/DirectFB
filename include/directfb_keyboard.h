/*
   (c) Copyright 2001-2009  The world wide DirectFB Open Source Community (directfb.org)
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

#ifndef __DIRECTFB_KEYBOARD_H__
#define __DIRECTFB_KEYBOARD_H__

#ifdef __cplusplus
extern "C"
{
#endif


/*
 * DirectFB key types (for advanced mapping)
 */
typedef enum {
     DIKT_UNICODE        = 0x0000,     /* Unicode 3.x character
                                           (compatible to Latin-1) */
     DIKT_SPECIAL        = 0xF000,     /* Special key (e.g. Cursor Up or Menu) */
     DIKT_FUNCTION       = 0xF100,     /* Function key (F1 - Fn) */
     DIKT_MODIFIER       = 0xF200,     /* Modifier key */
     DIKT_LOCK           = 0xF300,     /* Lock key (e.g. CapsLock) */
     DIKT_DEAD           = 0xF400,     /* Dead key (e.g. dead grave) */
     DIKT_CUSTOM         = 0xF500,     /* Custom key (vendor specific) */
     DIKT_IDENTIFIER     = 0xF600      /* DirectFB key identifier */
} DFBInputDeviceKeyType;

#define DFB_KEY(type,index)        ((DIKT_##type) | (index))

#define DFB_KEY_TYPE(symbol)       ((((symbol) & ~0xFFF) == 0xF000) ? (symbol) & 0xFF00 : DIKT_UNICODE)
#define DFB_KEY_INDEX(symbol)      ((symbol) & 0x00FF)

#define DFB_KEY_IS_ASCII(symbol)   ((symbol) < 128)

#define DFB_FUNCTION_KEY(n)        (DFB_KEY( FUNCTION, n ))
#define DFB_MODIFIER_KEY(i)        (DFB_KEY( MODIFIER, (1 << i) ))
#define DFB_CUSTOM_KEY(n)          (DFB_KEY( CUSTOM, n ))

#define DFB_LOWER_CASE(symbol)     (((symbol) >= 'A' && (symbol) <= 'Z') ?\
                                    ((symbol) | 0x20) : (symbol))
#define DFB_UPPER_CASE(symbol)     (((symbol) >= 'a' && (symbol) <= 'z') ?\
                                    ((symbol) & ~0x20) : (symbol))

/*
 * DirectFB modifier key identifiers (for advanced mapping)
 */
typedef enum {
     DIMKI_SHIFT,                       /* Shift modifier key */
     DIMKI_CONTROL,                     /* Control modifier key */
     DIMKI_ALT,                         /* Alt modifier key */
     DIMKI_ALTGR,                       /* AltGr modifier key */
     DIMKI_META,                        /* Meta modifier key */
     DIMKI_SUPER,                       /* Super modifier key */
     DIMKI_HYPER,                       /* Hyper modifier key */

     DIMKI_FIRST    = DIMKI_SHIFT,
     DIMKI_LAST     = DIMKI_HYPER
} DFBInputDeviceModifierKeyIdentifier;

/*
 * DirectFB key identifiers (for basic mapping)
 */
typedef enum {
     DIKI_UNKNOWN = DFB_KEY( IDENTIFIER, 0 ),

     DIKI_A,
     DIKI_B,
     DIKI_C,
     DIKI_D,
     DIKI_E,
     DIKI_F,
     DIKI_G,
     DIKI_H,
     DIKI_I,
     DIKI_J,
     DIKI_K,
     DIKI_L,
     DIKI_M,
     DIKI_N,
     DIKI_O,
     DIKI_P,
     DIKI_Q,
     DIKI_R,
     DIKI_S,
     DIKI_T,
     DIKI_U,
     DIKI_V,
     DIKI_W,
     DIKI_X,
     DIKI_Y,
     DIKI_Z,

     DIKI_0,
     DIKI_1,
     DIKI_2,
     DIKI_3,
     DIKI_4,
     DIKI_5,
     DIKI_6,
     DIKI_7,
     DIKI_8,
     DIKI_9,

     DIKI_F1,
     DIKI_F2,
     DIKI_F3,
     DIKI_F4,
     DIKI_F5,
     DIKI_F6,
     DIKI_F7,
     DIKI_F8,
     DIKI_F9,
     DIKI_F10,
     DIKI_F11,
     DIKI_F12,

     DIKI_SHIFT_L,
     DIKI_SHIFT_R,
     DIKI_CONTROL_L,
     DIKI_CONTROL_R,
     DIKI_ALT_L,
     DIKI_ALT_R,
     DIKI_META_L,
     DIKI_META_R,
     DIKI_SUPER_L,
     DIKI_SUPER_R,
     DIKI_HYPER_L,
     DIKI_HYPER_R,

     DIKI_CAPS_LOCK,
     DIKI_NUM_LOCK,
     DIKI_SCROLL_LOCK,

     DIKI_ESCAPE,
     DIKI_LEFT,
     DIKI_RIGHT,
     DIKI_UP,
     DIKI_DOWN,
     DIKI_TAB,
     DIKI_ENTER,
     DIKI_SPACE,
     DIKI_BACKSPACE,
     DIKI_INSERT,
     DIKI_DELETE,
     DIKI_HOME,
     DIKI_END,
     DIKI_PAGE_UP,
     DIKI_PAGE_DOWN,
     DIKI_PRINT,
     DIKI_PAUSE,

     /*  The labels on these keys depend on the type of keyboard.
      *  We've choosen the names from a US keyboard layout. The
      *  comments refer to the ISO 9995 terminology.
      */
     DIKI_QUOTE_LEFT,    /*  TLDE  */
     DIKI_MINUS_SIGN,    /*  AE11  */
     DIKI_EQUALS_SIGN,   /*  AE12  */
     DIKI_BRACKET_LEFT,  /*  AD11  */
     DIKI_BRACKET_RIGHT, /*  AD12  */
     DIKI_BACKSLASH,     /*  BKSL  */
     DIKI_SEMICOLON,     /*  AC10  */
     DIKI_QUOTE_RIGHT,   /*  AC11  */
     DIKI_COMMA,         /*  AB08  */
     DIKI_PERIOD,        /*  AB09  */
     DIKI_SLASH,         /*  AB10  */

     DIKI_LESS_SIGN,     /*  103rd  */

     DIKI_KP_DIV,
     DIKI_KP_MULT,
     DIKI_KP_MINUS,
     DIKI_KP_PLUS,
     DIKI_KP_ENTER,
     DIKI_KP_SPACE,
     DIKI_KP_TAB,
     DIKI_KP_F1,
     DIKI_KP_F2,
     DIKI_KP_F3,
     DIKI_KP_F4,
     DIKI_KP_EQUAL,
     DIKI_KP_SEPARATOR,

     DIKI_KP_DECIMAL,
     DIKI_KP_0,
     DIKI_KP_1,
     DIKI_KP_2,
     DIKI_KP_3,
     DIKI_KP_4,
     DIKI_KP_5,
     DIKI_KP_6,
     DIKI_KP_7,
     DIKI_KP_8,
     DIKI_KP_9,

     DIKI_KEYDEF_END,
     DIKI_NUMBER_OF_KEYS = DIKI_KEYDEF_END - DFB_KEY( IDENTIFIER, 0 )

} DFBInputDeviceKeyIdentifier;

/*
 * DirectFB key symbols (for advanced mapping)
 */
typedef enum {
     /*
      * Unicode excerpt - Controls and Basic Latin
      *
      * Any Unicode 3.x character can be used as a DirectFB key symbol,
      * the values of this enum are compatible with Unicode.
      */
     DIKS_NULL                     = DFB_KEY( UNICODE, 0x00 ),
     DIKS_BACKSPACE                = DFB_KEY( UNICODE, 0x08 ),
     DIKS_TAB                      = DFB_KEY( UNICODE, 0x09 ),
     DIKS_RETURN                   = DFB_KEY( UNICODE, 0x0D ),
     DIKS_CANCEL                   = DFB_KEY( UNICODE, 0x18 ),
     DIKS_ESCAPE                   = DFB_KEY( UNICODE, 0x1B ),
     DIKS_SPACE                    = DFB_KEY( UNICODE, 0x20 ),
     DIKS_EXCLAMATION_MARK         = DFB_KEY( UNICODE, 0x21 ),
     DIKS_QUOTATION                = DFB_KEY( UNICODE, 0x22 ),
     DIKS_NUMBER_SIGN              = DFB_KEY( UNICODE, 0x23 ),
     DIKS_DOLLAR_SIGN              = DFB_KEY( UNICODE, 0x24 ),
     DIKS_PERCENT_SIGN             = DFB_KEY( UNICODE, 0x25 ),
     DIKS_AMPERSAND                = DFB_KEY( UNICODE, 0x26 ),
     DIKS_APOSTROPHE               = DFB_KEY( UNICODE, 0x27 ),
     DIKS_PARENTHESIS_LEFT         = DFB_KEY( UNICODE, 0x28 ),
     DIKS_PARENTHESIS_RIGHT        = DFB_KEY( UNICODE, 0x29 ),
     DIKS_ASTERISK                 = DFB_KEY( UNICODE, 0x2A ),
     DIKS_PLUS_SIGN                = DFB_KEY( UNICODE, 0x2B ),
     DIKS_COMMA                    = DFB_KEY( UNICODE, 0x2C ),
     DIKS_MINUS_SIGN               = DFB_KEY( UNICODE, 0x2D ),
     DIKS_PERIOD                   = DFB_KEY( UNICODE, 0x2E ),
     DIKS_SLASH                    = DFB_KEY( UNICODE, 0x2F ),
     DIKS_0                        = DFB_KEY( UNICODE, 0x30 ),
     DIKS_1                        = DFB_KEY( UNICODE, 0x31 ),
     DIKS_2                        = DFB_KEY( UNICODE, 0x32 ),
     DIKS_3                        = DFB_KEY( UNICODE, 0x33 ),
     DIKS_4                        = DFB_KEY( UNICODE, 0x34 ),
     DIKS_5                        = DFB_KEY( UNICODE, 0x35 ),
     DIKS_6                        = DFB_KEY( UNICODE, 0x36 ),
     DIKS_7                        = DFB_KEY( UNICODE, 0x37 ),
     DIKS_8                        = DFB_KEY( UNICODE, 0x38 ),
     DIKS_9                        = DFB_KEY( UNICODE, 0x39 ),
     DIKS_COLON                    = DFB_KEY( UNICODE, 0x3A ),
     DIKS_SEMICOLON                = DFB_KEY( UNICODE, 0x3B ),
     DIKS_LESS_THAN_SIGN           = DFB_KEY( UNICODE, 0x3C ),
     DIKS_EQUALS_SIGN              = DFB_KEY( UNICODE, 0x3D ),
     DIKS_GREATER_THAN_SIGN        = DFB_KEY( UNICODE, 0x3E ),
     DIKS_QUESTION_MARK            = DFB_KEY( UNICODE, 0x3F ),
     DIKS_AT                       = DFB_KEY( UNICODE, 0x40 ),
     DIKS_CAPITAL_A                = DFB_KEY( UNICODE, 0x41 ),
     DIKS_CAPITAL_B                = DFB_KEY( UNICODE, 0x42 ),
     DIKS_CAPITAL_C                = DFB_KEY( UNICODE, 0x43 ),
     DIKS_CAPITAL_D                = DFB_KEY( UNICODE, 0x44 ),
     DIKS_CAPITAL_E                = DFB_KEY( UNICODE, 0x45 ),
     DIKS_CAPITAL_F                = DFB_KEY( UNICODE, 0x46 ),
     DIKS_CAPITAL_G                = DFB_KEY( UNICODE, 0x47 ),
     DIKS_CAPITAL_H                = DFB_KEY( UNICODE, 0x48 ),
     DIKS_CAPITAL_I                = DFB_KEY( UNICODE, 0x49 ),
     DIKS_CAPITAL_J                = DFB_KEY( UNICODE, 0x4A ),
     DIKS_CAPITAL_K                = DFB_KEY( UNICODE, 0x4B ),
     DIKS_CAPITAL_L                = DFB_KEY( UNICODE, 0x4C ),
     DIKS_CAPITAL_M                = DFB_KEY( UNICODE, 0x4D ),
     DIKS_CAPITAL_N                = DFB_KEY( UNICODE, 0x4E ),
     DIKS_CAPITAL_O                = DFB_KEY( UNICODE, 0x4F ),
     DIKS_CAPITAL_P                = DFB_KEY( UNICODE, 0x50 ),
     DIKS_CAPITAL_Q                = DFB_KEY( UNICODE, 0x51 ),
     DIKS_CAPITAL_R                = DFB_KEY( UNICODE, 0x52 ),
     DIKS_CAPITAL_S                = DFB_KEY( UNICODE, 0x53 ),
     DIKS_CAPITAL_T                = DFB_KEY( UNICODE, 0x54 ),
     DIKS_CAPITAL_U                = DFB_KEY( UNICODE, 0x55 ),
     DIKS_CAPITAL_V                = DFB_KEY( UNICODE, 0x56 ),
     DIKS_CAPITAL_W                = DFB_KEY( UNICODE, 0x57 ),
     DIKS_CAPITAL_X                = DFB_KEY( UNICODE, 0x58 ),
     DIKS_CAPITAL_Y                = DFB_KEY( UNICODE, 0x59 ),
     DIKS_CAPITAL_Z                = DFB_KEY( UNICODE, 0x5A ),
     DIKS_SQUARE_BRACKET_LEFT      = DFB_KEY( UNICODE, 0x5B ),
     DIKS_BACKSLASH                = DFB_KEY( UNICODE, 0x5C ),
     DIKS_SQUARE_BRACKET_RIGHT     = DFB_KEY( UNICODE, 0x5D ),
     DIKS_CIRCUMFLEX_ACCENT        = DFB_KEY( UNICODE, 0x5E ),
     DIKS_UNDERSCORE               = DFB_KEY( UNICODE, 0x5F ),
     DIKS_GRAVE_ACCENT             = DFB_KEY( UNICODE, 0x60 ),
     DIKS_SMALL_A                  = DFB_KEY( UNICODE, 0x61 ),
     DIKS_SMALL_B                  = DFB_KEY( UNICODE, 0x62 ),
     DIKS_SMALL_C                  = DFB_KEY( UNICODE, 0x63 ),
     DIKS_SMALL_D                  = DFB_KEY( UNICODE, 0x64 ),
     DIKS_SMALL_E                  = DFB_KEY( UNICODE, 0x65 ),
     DIKS_SMALL_F                  = DFB_KEY( UNICODE, 0x66 ),
     DIKS_SMALL_G                  = DFB_KEY( UNICODE, 0x67 ),
     DIKS_SMALL_H                  = DFB_KEY( UNICODE, 0x68 ),
     DIKS_SMALL_I                  = DFB_KEY( UNICODE, 0x69 ),
     DIKS_SMALL_J                  = DFB_KEY( UNICODE, 0x6A ),
     DIKS_SMALL_K                  = DFB_KEY( UNICODE, 0x6B ),
     DIKS_SMALL_L                  = DFB_KEY( UNICODE, 0x6C ),
     DIKS_SMALL_M                  = DFB_KEY( UNICODE, 0x6D ),
     DIKS_SMALL_N                  = DFB_KEY( UNICODE, 0x6E ),
     DIKS_SMALL_O                  = DFB_KEY( UNICODE, 0x6F ),
     DIKS_SMALL_P                  = DFB_KEY( UNICODE, 0x70 ),
     DIKS_SMALL_Q                  = DFB_KEY( UNICODE, 0x71 ),
     DIKS_SMALL_R                  = DFB_KEY( UNICODE, 0x72 ),
     DIKS_SMALL_S                  = DFB_KEY( UNICODE, 0x73 ),
     DIKS_SMALL_T                  = DFB_KEY( UNICODE, 0x74 ),
     DIKS_SMALL_U                  = DFB_KEY( UNICODE, 0x75 ),
     DIKS_SMALL_V                  = DFB_KEY( UNICODE, 0x76 ),
     DIKS_SMALL_W                  = DFB_KEY( UNICODE, 0x77 ),
     DIKS_SMALL_X                  = DFB_KEY( UNICODE, 0x78 ),
     DIKS_SMALL_Y                  = DFB_KEY( UNICODE, 0x79 ),
     DIKS_SMALL_Z                  = DFB_KEY( UNICODE, 0x7A ),
     DIKS_CURLY_BRACKET_LEFT       = DFB_KEY( UNICODE, 0x7B ),
     DIKS_VERTICAL_BAR             = DFB_KEY( UNICODE, 0x7C ),
     DIKS_CURLY_BRACKET_RIGHT      = DFB_KEY( UNICODE, 0x7D ),
     DIKS_TILDE                    = DFB_KEY( UNICODE, 0x7E ),
     DIKS_DELETE                   = DFB_KEY( UNICODE, 0x7F ),

     DIKS_ENTER                    = DIKS_RETURN,

     /*
      * Unicode private area - DirectFB Special keys
      */
     DIKS_CURSOR_LEFT              = DFB_KEY( SPECIAL, 0x00 ),
     DIKS_CURSOR_RIGHT             = DFB_KEY( SPECIAL, 0x01 ),
     DIKS_CURSOR_UP                = DFB_KEY( SPECIAL, 0x02 ),
     DIKS_CURSOR_DOWN              = DFB_KEY( SPECIAL, 0x03 ),
     DIKS_INSERT                   = DFB_KEY( SPECIAL, 0x04 ),
     DIKS_HOME                     = DFB_KEY( SPECIAL, 0x05 ),
     DIKS_END                      = DFB_KEY( SPECIAL, 0x06 ),
     DIKS_PAGE_UP                  = DFB_KEY( SPECIAL, 0x07 ),
     DIKS_PAGE_DOWN                = DFB_KEY( SPECIAL, 0x08 ),
     DIKS_PRINT                    = DFB_KEY( SPECIAL, 0x09 ),
     DIKS_PAUSE                    = DFB_KEY( SPECIAL, 0x0A ),
     DIKS_OK                       = DFB_KEY( SPECIAL, 0x0B ),
     DIKS_SELECT                   = DFB_KEY( SPECIAL, 0x0C ),
     DIKS_GOTO                     = DFB_KEY( SPECIAL, 0x0D ),
     DIKS_CLEAR                    = DFB_KEY( SPECIAL, 0x0E ),
     DIKS_POWER                    = DFB_KEY( SPECIAL, 0x0F ),
     DIKS_POWER2                   = DFB_KEY( SPECIAL, 0x10 ),
     DIKS_OPTION                   = DFB_KEY( SPECIAL, 0x11 ),
     DIKS_MENU                     = DFB_KEY( SPECIAL, 0x12 ),
     DIKS_HELP                     = DFB_KEY( SPECIAL, 0x13 ),
     DIKS_INFO                     = DFB_KEY( SPECIAL, 0x14 ),
     DIKS_TIME                     = DFB_KEY( SPECIAL, 0x15 ),
     DIKS_VENDOR                   = DFB_KEY( SPECIAL, 0x16 ),

     DIKS_ARCHIVE                  = DFB_KEY( SPECIAL, 0x17 ),
     DIKS_PROGRAM                  = DFB_KEY( SPECIAL, 0x18 ),
     DIKS_CHANNEL                  = DFB_KEY( SPECIAL, 0x19 ),
     DIKS_FAVORITES                = DFB_KEY( SPECIAL, 0x1A ),
     DIKS_EPG                      = DFB_KEY( SPECIAL, 0x1B ),
     DIKS_PVR                      = DFB_KEY( SPECIAL, 0x1C ),
     DIKS_MHP                      = DFB_KEY( SPECIAL, 0x1D ),
     DIKS_LANGUAGE                 = DFB_KEY( SPECIAL, 0x1E ),
     DIKS_TITLE                    = DFB_KEY( SPECIAL, 0x1F ),
     DIKS_SUBTITLE                 = DFB_KEY( SPECIAL, 0x20 ),
     DIKS_ANGLE                    = DFB_KEY( SPECIAL, 0x21 ),
     DIKS_ZOOM                     = DFB_KEY( SPECIAL, 0x22 ),
     DIKS_MODE                     = DFB_KEY( SPECIAL, 0x23 ),
     DIKS_KEYBOARD                 = DFB_KEY( SPECIAL, 0x24 ),
     DIKS_PC                       = DFB_KEY( SPECIAL, 0x25 ),
     DIKS_SCREEN                   = DFB_KEY( SPECIAL, 0x26 ),

     DIKS_TV                       = DFB_KEY( SPECIAL, 0x27 ),
     DIKS_TV2                      = DFB_KEY( SPECIAL, 0x28 ),
     DIKS_VCR                      = DFB_KEY( SPECIAL, 0x29 ),
     DIKS_VCR2                     = DFB_KEY( SPECIAL, 0x2A ),
     DIKS_SAT                      = DFB_KEY( SPECIAL, 0x2B ),
     DIKS_SAT2                     = DFB_KEY( SPECIAL, 0x2C ),
     DIKS_CD                       = DFB_KEY( SPECIAL, 0x2D ),
     DIKS_TAPE                     = DFB_KEY( SPECIAL, 0x2E ),
     DIKS_RADIO                    = DFB_KEY( SPECIAL, 0x2F ),
     DIKS_TUNER                    = DFB_KEY( SPECIAL, 0x30 ),
     DIKS_PLAYER                   = DFB_KEY( SPECIAL, 0x31 ),
     DIKS_TEXT                     = DFB_KEY( SPECIAL, 0x32 ),
     DIKS_DVD                      = DFB_KEY( SPECIAL, 0x33 ),
     DIKS_AUX                      = DFB_KEY( SPECIAL, 0x34 ),
     DIKS_MP3                      = DFB_KEY( SPECIAL, 0x35 ),
     DIKS_PHONE                    = DFB_KEY( SPECIAL, 0x36 ),
     DIKS_AUDIO                    = DFB_KEY( SPECIAL, 0x37 ),
     DIKS_VIDEO                    = DFB_KEY( SPECIAL, 0x38 ),

     DIKS_INTERNET                 = DFB_KEY( SPECIAL, 0x39 ),
     DIKS_MAIL                     = DFB_KEY( SPECIAL, 0x3A ),
     DIKS_NEWS                     = DFB_KEY( SPECIAL, 0x3B ),
     DIKS_DIRECTORY                = DFB_KEY( SPECIAL, 0x3C ),
     DIKS_LIST                     = DFB_KEY( SPECIAL, 0x3D ),
     DIKS_CALCULATOR               = DFB_KEY( SPECIAL, 0x3E ),
     DIKS_MEMO                     = DFB_KEY( SPECIAL, 0x3F ),
     DIKS_CALENDAR                 = DFB_KEY( SPECIAL, 0x40 ),
     DIKS_EDITOR                   = DFB_KEY( SPECIAL, 0x41 ),

     DIKS_RED                      = DFB_KEY( SPECIAL, 0x42 ),
     DIKS_GREEN                    = DFB_KEY( SPECIAL, 0x43 ),
     DIKS_YELLOW                   = DFB_KEY( SPECIAL, 0x44 ),
     DIKS_BLUE                     = DFB_KEY( SPECIAL, 0x45 ),

     DIKS_CHANNEL_UP               = DFB_KEY( SPECIAL, 0x46 ),
     DIKS_CHANNEL_DOWN             = DFB_KEY( SPECIAL, 0x47 ),
     DIKS_BACK                     = DFB_KEY( SPECIAL, 0x48 ),
     DIKS_FORWARD                  = DFB_KEY( SPECIAL, 0x49 ),
     DIKS_FIRST                    = DFB_KEY( SPECIAL, 0x4A ),
     DIKS_LAST                     = DFB_KEY( SPECIAL, 0x4B ),
     DIKS_VOLUME_UP                = DFB_KEY( SPECIAL, 0x4C ),
     DIKS_VOLUME_DOWN              = DFB_KEY( SPECIAL, 0x4D ),
     DIKS_MUTE                     = DFB_KEY( SPECIAL, 0x4E ),
     DIKS_AB                       = DFB_KEY( SPECIAL, 0x4F ),
     DIKS_PLAYPAUSE                = DFB_KEY( SPECIAL, 0x50 ),
     DIKS_PLAY                     = DFB_KEY( SPECIAL, 0x51 ),
     DIKS_STOP                     = DFB_KEY( SPECIAL, 0x52 ),
     DIKS_RESTART                  = DFB_KEY( SPECIAL, 0x53 ),
     DIKS_SLOW                     = DFB_KEY( SPECIAL, 0x54 ),
     DIKS_FAST                     = DFB_KEY( SPECIAL, 0x55 ),
     DIKS_RECORD                   = DFB_KEY( SPECIAL, 0x56 ),
     DIKS_EJECT                    = DFB_KEY( SPECIAL, 0x57 ),
     DIKS_SHUFFLE                  = DFB_KEY( SPECIAL, 0x58 ),
     DIKS_REWIND                   = DFB_KEY( SPECIAL, 0x59 ),
     DIKS_FASTFORWARD              = DFB_KEY( SPECIAL, 0x5A ),
     DIKS_PREVIOUS                 = DFB_KEY( SPECIAL, 0x5B ),
     DIKS_NEXT                     = DFB_KEY( SPECIAL, 0x5C ),
     DIKS_BEGIN                    = DFB_KEY( SPECIAL, 0x5D ),

     DIKS_DIGITS                   = DFB_KEY( SPECIAL, 0x5E ),
     DIKS_TEEN                     = DFB_KEY( SPECIAL, 0x5F ),
     DIKS_TWEN                     = DFB_KEY( SPECIAL, 0x60 ),

     DIKS_BREAK                    = DFB_KEY( SPECIAL, 0x61 ),
     DIKS_EXIT                     = DFB_KEY( SPECIAL, 0x62 ),
     DIKS_SETUP                    = DFB_KEY( SPECIAL, 0x63 ),

     DIKS_CURSOR_LEFT_UP           = DFB_KEY( SPECIAL, 0x64 ),
     DIKS_CURSOR_LEFT_DOWN         = DFB_KEY( SPECIAL, 0x65 ),
     DIKS_CURSOR_UP_RIGHT          = DFB_KEY( SPECIAL, 0x66 ),
     DIKS_CURSOR_DOWN_RIGHT        = DFB_KEY( SPECIAL, 0x67 ),

     DIKS_PIP                      = DFB_KEY( SPECIAL, 0x68 ),
     DIKS_SWAP                     = DFB_KEY( SPECIAL, 0x69 ),
     DIKS_FREEZE                   = DFB_KEY( SPECIAL, 0x6A ),
     DIKS_MOVE                     = DFB_KEY( SPECIAL, 0x6B ),

     DIKS_CALL                     = DFB_KEY( SPECIAL, 0x6C ),
     DIKS_SPEAKER                  = DFB_KEY( SPECIAL, 0x6D ),
     DIKS_SAVE                     = DFB_KEY( SPECIAL, 0x6E ),
     DIKS_REDIAL                   = DFB_KEY( SPECIAL, 0x6F ),
     DIKS_FLASH                    = DFB_KEY( SPECIAL, 0x70 ),
     DIKS_HOLD                     = DFB_KEY( SPECIAL, 0x71 ),

     /*
      * Unicode private area - DirectFB Function keys
      *
      * More function keys are available via DFB_FUNCTION_KEY(n).
      */
     DIKS_F1                       = DFB_FUNCTION_KEY(  1 ),
     DIKS_F2                       = DFB_FUNCTION_KEY(  2 ),
     DIKS_F3                       = DFB_FUNCTION_KEY(  3 ),
     DIKS_F4                       = DFB_FUNCTION_KEY(  4 ),
     DIKS_F5                       = DFB_FUNCTION_KEY(  5 ),
     DIKS_F6                       = DFB_FUNCTION_KEY(  6 ),
     DIKS_F7                       = DFB_FUNCTION_KEY(  7 ),
     DIKS_F8                       = DFB_FUNCTION_KEY(  8 ),
     DIKS_F9                       = DFB_FUNCTION_KEY(  9 ),
     DIKS_F10                      = DFB_FUNCTION_KEY( 10 ),
     DIKS_F11                      = DFB_FUNCTION_KEY( 11 ),
     DIKS_F12                      = DFB_FUNCTION_KEY( 12 ),

     /*
      * Unicode private area - DirectFB Modifier keys
      */
     DIKS_SHIFT                    = DFB_MODIFIER_KEY( DIMKI_SHIFT ),
     DIKS_CONTROL                  = DFB_MODIFIER_KEY( DIMKI_CONTROL ),
     DIKS_ALT                      = DFB_MODIFIER_KEY( DIMKI_ALT ),
     DIKS_ALTGR                    = DFB_MODIFIER_KEY( DIMKI_ALTGR ),
     DIKS_META                     = DFB_MODIFIER_KEY( DIMKI_META ),
     DIKS_SUPER                    = DFB_MODIFIER_KEY( DIMKI_SUPER ),
     DIKS_HYPER                    = DFB_MODIFIER_KEY( DIMKI_HYPER ),

     /*
      * Unicode private area - DirectFB Lock keys
      */
     DIKS_CAPS_LOCK                = DFB_KEY( LOCK, 0x00 ),
     DIKS_NUM_LOCK                 = DFB_KEY( LOCK, 0x01 ),
     DIKS_SCROLL_LOCK              = DFB_KEY( LOCK, 0x02 ),

     /*
      * Unicode private area - DirectFB Dead keys
      */
     DIKS_DEAD_ABOVEDOT            = DFB_KEY( DEAD, 0x00 ),
     DIKS_DEAD_ABOVERING           = DFB_KEY( DEAD, 0x01 ),
     DIKS_DEAD_ACUTE               = DFB_KEY( DEAD, 0x02 ),
     DIKS_DEAD_BREVE               = DFB_KEY( DEAD, 0x03 ),
     DIKS_DEAD_CARON               = DFB_KEY( DEAD, 0x04 ),
     DIKS_DEAD_CEDILLA             = DFB_KEY( DEAD, 0x05 ),
     DIKS_DEAD_CIRCUMFLEX          = DFB_KEY( DEAD, 0x06 ),
     DIKS_DEAD_DIAERESIS           = DFB_KEY( DEAD, 0x07 ),
     DIKS_DEAD_DOUBLEACUTE         = DFB_KEY( DEAD, 0x08 ),
     DIKS_DEAD_GRAVE               = DFB_KEY( DEAD, 0x09 ),
     DIKS_DEAD_IOTA                = DFB_KEY( DEAD, 0x0A ),
     DIKS_DEAD_MACRON              = DFB_KEY( DEAD, 0x0B ),
     DIKS_DEAD_OGONEK              = DFB_KEY( DEAD, 0x0C ),
     DIKS_DEAD_SEMIVOICED_SOUND    = DFB_KEY( DEAD, 0x0D ),
     DIKS_DEAD_TILDE               = DFB_KEY( DEAD, 0x0E ),
     DIKS_DEAD_VOICED_SOUND        = DFB_KEY( DEAD, 0x0F ),

     /*
      * Unicode private area - DirectFB Custom keys
      *
      * More custom keys are available via DFB_CUSTOM_KEY(n).
      */
     DIKS_CUSTOM0                  = DFB_CUSTOM_KEY( 0 ),
     DIKS_CUSTOM1                  = DFB_CUSTOM_KEY( 1 ),
     DIKS_CUSTOM2                  = DFB_CUSTOM_KEY( 2 ),
     DIKS_CUSTOM3                  = DFB_CUSTOM_KEY( 3 ),
     DIKS_CUSTOM4                  = DFB_CUSTOM_KEY( 4 ),
     DIKS_CUSTOM5                  = DFB_CUSTOM_KEY( 5 ),
     DIKS_CUSTOM6                  = DFB_CUSTOM_KEY( 6 ),
     DIKS_CUSTOM7                  = DFB_CUSTOM_KEY( 7 ),
     DIKS_CUSTOM8                  = DFB_CUSTOM_KEY( 8 ),
     DIKS_CUSTOM9                  = DFB_CUSTOM_KEY( 9 ),
     DIKS_CUSTOM10                 = DFB_CUSTOM_KEY( 10 ),
     DIKS_CUSTOM11                 = DFB_CUSTOM_KEY( 11 ),
     DIKS_CUSTOM12                 = DFB_CUSTOM_KEY( 12 ),
     DIKS_CUSTOM13                 = DFB_CUSTOM_KEY( 13 ),
     DIKS_CUSTOM14                 = DFB_CUSTOM_KEY( 14 ),
     DIKS_CUSTOM15                 = DFB_CUSTOM_KEY( 15 ),
     DIKS_CUSTOM16                 = DFB_CUSTOM_KEY( 16 ),
     DIKS_CUSTOM17                 = DFB_CUSTOM_KEY( 17 ),
     DIKS_CUSTOM18                 = DFB_CUSTOM_KEY( 18 ),
     DIKS_CUSTOM19                 = DFB_CUSTOM_KEY( 19 ),
     DIKS_CUSTOM20                 = DFB_CUSTOM_KEY( 20 ),
     DIKS_CUSTOM21                 = DFB_CUSTOM_KEY( 21 ),
     DIKS_CUSTOM22                 = DFB_CUSTOM_KEY( 22 ),
     DIKS_CUSTOM23                 = DFB_CUSTOM_KEY( 23 ),
     DIKS_CUSTOM24                 = DFB_CUSTOM_KEY( 24 ),
     DIKS_CUSTOM25                 = DFB_CUSTOM_KEY( 25 ),
     DIKS_CUSTOM26                 = DFB_CUSTOM_KEY( 26 ),
     DIKS_CUSTOM27                 = DFB_CUSTOM_KEY( 27 ),
     DIKS_CUSTOM28                 = DFB_CUSTOM_KEY( 28 ),
     DIKS_CUSTOM29                 = DFB_CUSTOM_KEY( 29 ),
     DIKS_CUSTOM30                 = DFB_CUSTOM_KEY( 30 ),
     DIKS_CUSTOM31                 = DFB_CUSTOM_KEY( 31 ),
     DIKS_CUSTOM32                 = DFB_CUSTOM_KEY( 32 ),
     DIKS_CUSTOM33                 = DFB_CUSTOM_KEY( 33 ),
     DIKS_CUSTOM34                 = DFB_CUSTOM_KEY( 34 ),
     DIKS_CUSTOM35                 = DFB_CUSTOM_KEY( 35 ),
     DIKS_CUSTOM36                 = DFB_CUSTOM_KEY( 36 ),
     DIKS_CUSTOM37                 = DFB_CUSTOM_KEY( 37 ),
     DIKS_CUSTOM38                 = DFB_CUSTOM_KEY( 38 ),
     DIKS_CUSTOM39                 = DFB_CUSTOM_KEY( 39 ),
     DIKS_CUSTOM40                 = DFB_CUSTOM_KEY( 40 ),
     DIKS_CUSTOM41                 = DFB_CUSTOM_KEY( 41 ),
     DIKS_CUSTOM42                 = DFB_CUSTOM_KEY( 42 ),
     DIKS_CUSTOM43                 = DFB_CUSTOM_KEY( 43 ),
     DIKS_CUSTOM44                 = DFB_CUSTOM_KEY( 44 ),
     DIKS_CUSTOM45                 = DFB_CUSTOM_KEY( 45 ),
     DIKS_CUSTOM46                 = DFB_CUSTOM_KEY( 46 ),
     DIKS_CUSTOM47                 = DFB_CUSTOM_KEY( 47 ),
     DIKS_CUSTOM48                 = DFB_CUSTOM_KEY( 48 ),
     DIKS_CUSTOM49                 = DFB_CUSTOM_KEY( 49 ),
     DIKS_CUSTOM50                 = DFB_CUSTOM_KEY( 50 ),
     DIKS_CUSTOM51                 = DFB_CUSTOM_KEY( 51 ),
     DIKS_CUSTOM52                 = DFB_CUSTOM_KEY( 52 ),
     DIKS_CUSTOM53                 = DFB_CUSTOM_KEY( 53 ),
     DIKS_CUSTOM54                 = DFB_CUSTOM_KEY( 54 ),
     DIKS_CUSTOM55                 = DFB_CUSTOM_KEY( 55 ),
     DIKS_CUSTOM56                 = DFB_CUSTOM_KEY( 56 ),
     DIKS_CUSTOM57                 = DFB_CUSTOM_KEY( 57 ),
     DIKS_CUSTOM58                 = DFB_CUSTOM_KEY( 58 ),
     DIKS_CUSTOM59                 = DFB_CUSTOM_KEY( 59 ),
     DIKS_CUSTOM60                 = DFB_CUSTOM_KEY( 60 ),
     DIKS_CUSTOM61                 = DFB_CUSTOM_KEY( 61 ),
     DIKS_CUSTOM62                 = DFB_CUSTOM_KEY( 62 ),
     DIKS_CUSTOM63                 = DFB_CUSTOM_KEY( 63 ),
     DIKS_CUSTOM64                 = DFB_CUSTOM_KEY( 64 ),
     DIKS_CUSTOM65                 = DFB_CUSTOM_KEY( 65 ),
     DIKS_CUSTOM66                 = DFB_CUSTOM_KEY( 66 ),
     DIKS_CUSTOM67                 = DFB_CUSTOM_KEY( 67 ),
     DIKS_CUSTOM68                 = DFB_CUSTOM_KEY( 68 ),
     DIKS_CUSTOM69                 = DFB_CUSTOM_KEY( 69 ),
     DIKS_CUSTOM70                 = DFB_CUSTOM_KEY( 70 ),
     DIKS_CUSTOM71                 = DFB_CUSTOM_KEY( 71 ),
     DIKS_CUSTOM72                 = DFB_CUSTOM_KEY( 72 ),
     DIKS_CUSTOM73                 = DFB_CUSTOM_KEY( 73 ),
     DIKS_CUSTOM74                 = DFB_CUSTOM_KEY( 74 ),
     DIKS_CUSTOM75                 = DFB_CUSTOM_KEY( 75 ),
     DIKS_CUSTOM76                 = DFB_CUSTOM_KEY( 76 ),
     DIKS_CUSTOM77                 = DFB_CUSTOM_KEY( 77 ),
     DIKS_CUSTOM78                 = DFB_CUSTOM_KEY( 78 ),
     DIKS_CUSTOM79                 = DFB_CUSTOM_KEY( 79 ),
     DIKS_CUSTOM80                 = DFB_CUSTOM_KEY( 80 ),
     DIKS_CUSTOM81                 = DFB_CUSTOM_KEY( 81 ),
     DIKS_CUSTOM82                 = DFB_CUSTOM_KEY( 82 ),
     DIKS_CUSTOM83                 = DFB_CUSTOM_KEY( 83 ),
     DIKS_CUSTOM84                 = DFB_CUSTOM_KEY( 84 ),
     DIKS_CUSTOM85                 = DFB_CUSTOM_KEY( 85 ),
     DIKS_CUSTOM86                 = DFB_CUSTOM_KEY( 86 ),
     DIKS_CUSTOM87                 = DFB_CUSTOM_KEY( 87 ),
     DIKS_CUSTOM88                 = DFB_CUSTOM_KEY( 88 ),
     DIKS_CUSTOM89                 = DFB_CUSTOM_KEY( 89 ),
     DIKS_CUSTOM90                 = DFB_CUSTOM_KEY( 90 ),
     DIKS_CUSTOM91                 = DFB_CUSTOM_KEY( 91 ),
     DIKS_CUSTOM92                 = DFB_CUSTOM_KEY( 92 ),
     DIKS_CUSTOM93                 = DFB_CUSTOM_KEY( 93 ),
     DIKS_CUSTOM94                 = DFB_CUSTOM_KEY( 94 ),
     DIKS_CUSTOM95                 = DFB_CUSTOM_KEY( 95 ),
     DIKS_CUSTOM96                 = DFB_CUSTOM_KEY( 96 ),
     DIKS_CUSTOM97                 = DFB_CUSTOM_KEY( 97 ),
     DIKS_CUSTOM98                 = DFB_CUSTOM_KEY( 98 ),
     DIKS_CUSTOM99                 = DFB_CUSTOM_KEY( 99 ),
     DIKS_CUSTOM100                = DFB_CUSTOM_KEY( 100 ),
     DIKS_CUSTOM101                = DFB_CUSTOM_KEY( 101 ),
     DIKS_CUSTOM102                = DFB_CUSTOM_KEY( 102 ),
     DIKS_CUSTOM103                = DFB_CUSTOM_KEY( 103 ),
     DIKS_CUSTOM104                = DFB_CUSTOM_KEY( 104 ),
     DIKS_CUSTOM105                = DFB_CUSTOM_KEY( 105 ),
     DIKS_CUSTOM106                = DFB_CUSTOM_KEY( 106 ),
     DIKS_CUSTOM107                = DFB_CUSTOM_KEY( 107 ),
     DIKS_CUSTOM108                = DFB_CUSTOM_KEY( 108 ),
     DIKS_CUSTOM109                = DFB_CUSTOM_KEY( 109 ),
     DIKS_CUSTOM110                = DFB_CUSTOM_KEY( 110 ),
     DIKS_CUSTOM111                = DFB_CUSTOM_KEY( 111 ),
     DIKS_CUSTOM112                = DFB_CUSTOM_KEY( 112 ),
     DIKS_CUSTOM113                = DFB_CUSTOM_KEY( 113 ),
     DIKS_CUSTOM114                = DFB_CUSTOM_KEY( 114 ),
     DIKS_CUSTOM115                = DFB_CUSTOM_KEY( 115 ),
     DIKS_CUSTOM116                = DFB_CUSTOM_KEY( 116 ),
     DIKS_CUSTOM117                = DFB_CUSTOM_KEY( 117 ),
     DIKS_CUSTOM118                = DFB_CUSTOM_KEY( 118 ),
     DIKS_CUSTOM119                = DFB_CUSTOM_KEY( 119 ),
     DIKS_CUSTOM120                = DFB_CUSTOM_KEY( 120 ),
     DIKS_CUSTOM121                = DFB_CUSTOM_KEY( 121 ),
     DIKS_CUSTOM122                = DFB_CUSTOM_KEY( 122 ),
     DIKS_CUSTOM123                = DFB_CUSTOM_KEY( 123 ),
     DIKS_CUSTOM124                = DFB_CUSTOM_KEY( 124 ),
     DIKS_CUSTOM125                = DFB_CUSTOM_KEY( 125 ),
     DIKS_CUSTOM126                = DFB_CUSTOM_KEY( 126 ),
     DIKS_CUSTOM127                = DFB_CUSTOM_KEY( 127 ),
     DIKS_CUSTOM128                = DFB_CUSTOM_KEY( 128 ),
     DIKS_CUSTOM129                = DFB_CUSTOM_KEY( 129 ),
     DIKS_CUSTOM130                = DFB_CUSTOM_KEY( 130 ),
     DIKS_CUSTOM131                = DFB_CUSTOM_KEY( 131 ),
     DIKS_CUSTOM132                = DFB_CUSTOM_KEY( 132 ),
     DIKS_CUSTOM133                = DFB_CUSTOM_KEY( 133 ),
     DIKS_CUSTOM134                = DFB_CUSTOM_KEY( 134 ),
     DIKS_CUSTOM135                = DFB_CUSTOM_KEY( 135 ),
     DIKS_CUSTOM136                = DFB_CUSTOM_KEY( 136 ),
     DIKS_CUSTOM137                = DFB_CUSTOM_KEY( 137 ),
     DIKS_CUSTOM138                = DFB_CUSTOM_KEY( 138 ),
     DIKS_CUSTOM139                = DFB_CUSTOM_KEY( 139 ),
     DIKS_CUSTOM140                = DFB_CUSTOM_KEY( 140 ),
     DIKS_CUSTOM141                = DFB_CUSTOM_KEY( 141 ),
     DIKS_CUSTOM142                = DFB_CUSTOM_KEY( 142 ),
     DIKS_CUSTOM143                = DFB_CUSTOM_KEY( 143 ),
     DIKS_CUSTOM144                = DFB_CUSTOM_KEY( 144 ),
     DIKS_CUSTOM145                = DFB_CUSTOM_KEY( 145 ),
     DIKS_CUSTOM146                = DFB_CUSTOM_KEY( 146 ),
     DIKS_CUSTOM147                = DFB_CUSTOM_KEY( 147 ),
     DIKS_CUSTOM148                = DFB_CUSTOM_KEY( 148 ),
     DIKS_CUSTOM149                = DFB_CUSTOM_KEY( 149 ),
     DIKS_CUSTOM150                = DFB_CUSTOM_KEY( 150 ),
     DIKS_CUSTOM151                = DFB_CUSTOM_KEY( 151 ),
     DIKS_CUSTOM152                = DFB_CUSTOM_KEY( 152 ),
     DIKS_CUSTOM153                = DFB_CUSTOM_KEY( 153 ),
     DIKS_CUSTOM154                = DFB_CUSTOM_KEY( 154 ),
     DIKS_CUSTOM155                = DFB_CUSTOM_KEY( 155 ),
     DIKS_CUSTOM156                = DFB_CUSTOM_KEY( 156 ),
     DIKS_CUSTOM157                = DFB_CUSTOM_KEY( 157 ),
     DIKS_CUSTOM158                = DFB_CUSTOM_KEY( 158 ),
     DIKS_CUSTOM159                = DFB_CUSTOM_KEY( 159 ),
     DIKS_CUSTOM160                = DFB_CUSTOM_KEY( 160 ),
     DIKS_CUSTOM161                = DFB_CUSTOM_KEY( 161 ),
     DIKS_CUSTOM162                = DFB_CUSTOM_KEY( 162 ),
     DIKS_CUSTOM163                = DFB_CUSTOM_KEY( 163 ),
     DIKS_CUSTOM164                = DFB_CUSTOM_KEY( 164 ),
     DIKS_CUSTOM165                = DFB_CUSTOM_KEY( 165 ),
     DIKS_CUSTOM166                = DFB_CUSTOM_KEY( 166 ),
     DIKS_CUSTOM167                = DFB_CUSTOM_KEY( 167 ),
     DIKS_CUSTOM168                = DFB_CUSTOM_KEY( 168 ),
     DIKS_CUSTOM169                = DFB_CUSTOM_KEY( 169 ),
     DIKS_CUSTOM170                = DFB_CUSTOM_KEY( 170 ),
     DIKS_CUSTOM171                = DFB_CUSTOM_KEY( 171 ),
     DIKS_CUSTOM172                = DFB_CUSTOM_KEY( 172 ),
     DIKS_CUSTOM173                = DFB_CUSTOM_KEY( 173 ),
     DIKS_CUSTOM174                = DFB_CUSTOM_KEY( 174 ),
     DIKS_CUSTOM175                = DFB_CUSTOM_KEY( 175 ),
     DIKS_CUSTOM176                = DFB_CUSTOM_KEY( 176 ),
     DIKS_CUSTOM177                = DFB_CUSTOM_KEY( 177 ),
     DIKS_CUSTOM178                = DFB_CUSTOM_KEY( 178 ),
     DIKS_CUSTOM179                = DFB_CUSTOM_KEY( 179 ),
     DIKS_CUSTOM180                = DFB_CUSTOM_KEY( 180 ),
     DIKS_CUSTOM181                = DFB_CUSTOM_KEY( 181 ),
     DIKS_CUSTOM182                = DFB_CUSTOM_KEY( 182 ),
     DIKS_CUSTOM183                = DFB_CUSTOM_KEY( 183 ),
     DIKS_CUSTOM184                = DFB_CUSTOM_KEY( 184 ),
     DIKS_CUSTOM185                = DFB_CUSTOM_KEY( 185 ),
     DIKS_CUSTOM186                = DFB_CUSTOM_KEY( 186 ),
     DIKS_CUSTOM187                = DFB_CUSTOM_KEY( 187 ),
     DIKS_CUSTOM188                = DFB_CUSTOM_KEY( 188 ),
     DIKS_CUSTOM189                = DFB_CUSTOM_KEY( 189 ),
     DIKS_CUSTOM190                = DFB_CUSTOM_KEY( 190 ),
     DIKS_CUSTOM191                = DFB_CUSTOM_KEY( 191 ),
     DIKS_CUSTOM192                = DFB_CUSTOM_KEY( 192 ),
     DIKS_CUSTOM193                = DFB_CUSTOM_KEY( 193 ),
     DIKS_CUSTOM194                = DFB_CUSTOM_KEY( 194 ),
     DIKS_CUSTOM195                = DFB_CUSTOM_KEY( 195 ),
     DIKS_CUSTOM196                = DFB_CUSTOM_KEY( 196 ),
     DIKS_CUSTOM197                = DFB_CUSTOM_KEY( 197 ),
     DIKS_CUSTOM198                = DFB_CUSTOM_KEY( 198 ),
     DIKS_CUSTOM199                = DFB_CUSTOM_KEY( 199 ),
     DIKS_CUSTOM200                = DFB_CUSTOM_KEY( 200 ),
     DIKS_CUSTOM201                = DFB_CUSTOM_KEY( 201 ),
     DIKS_CUSTOM202                = DFB_CUSTOM_KEY( 202 ),
     DIKS_CUSTOM203                = DFB_CUSTOM_KEY( 203 ),
     DIKS_CUSTOM204                = DFB_CUSTOM_KEY( 204 ),
     DIKS_CUSTOM205                = DFB_CUSTOM_KEY( 205 ),
     DIKS_CUSTOM206                = DFB_CUSTOM_KEY( 206 ),
     DIKS_CUSTOM207                = DFB_CUSTOM_KEY( 207 ),
     DIKS_CUSTOM208                = DFB_CUSTOM_KEY( 208 ),
     DIKS_CUSTOM209                = DFB_CUSTOM_KEY( 209 ),
     DIKS_CUSTOM210                = DFB_CUSTOM_KEY( 210 ),
     DIKS_CUSTOM211                = DFB_CUSTOM_KEY( 211 ),
     DIKS_CUSTOM212                = DFB_CUSTOM_KEY( 212 ),
     DIKS_CUSTOM213                = DFB_CUSTOM_KEY( 213 ),
     DIKS_CUSTOM214                = DFB_CUSTOM_KEY( 214 ),
     DIKS_CUSTOM215                = DFB_CUSTOM_KEY( 215 ),
     DIKS_CUSTOM216                = DFB_CUSTOM_KEY( 216 ),
     DIKS_CUSTOM217                = DFB_CUSTOM_KEY( 217 ),
     DIKS_CUSTOM218                = DFB_CUSTOM_KEY( 218 ),
     DIKS_CUSTOM219                = DFB_CUSTOM_KEY( 219 ),
     DIKS_CUSTOM220                = DFB_CUSTOM_KEY( 220 ),
     DIKS_CUSTOM221                = DFB_CUSTOM_KEY( 221 ),
     DIKS_CUSTOM222                = DFB_CUSTOM_KEY( 222 ),
     DIKS_CUSTOM223                = DFB_CUSTOM_KEY( 223 ),
     DIKS_CUSTOM224                = DFB_CUSTOM_KEY( 224 ),
     DIKS_CUSTOM225                = DFB_CUSTOM_KEY( 225 ),
     DIKS_CUSTOM226                = DFB_CUSTOM_KEY( 226 ),
     DIKS_CUSTOM227                = DFB_CUSTOM_KEY( 227 ),
     DIKS_CUSTOM228                = DFB_CUSTOM_KEY( 228 ),
     DIKS_CUSTOM229                = DFB_CUSTOM_KEY( 229 ),
     DIKS_CUSTOM230                = DFB_CUSTOM_KEY( 230 ),
     DIKS_CUSTOM231                = DFB_CUSTOM_KEY( 231 ),
     DIKS_CUSTOM232                = DFB_CUSTOM_KEY( 232 ),
     DIKS_CUSTOM233                = DFB_CUSTOM_KEY( 233 ),
     DIKS_CUSTOM234                = DFB_CUSTOM_KEY( 234 ),
     DIKS_CUSTOM235                = DFB_CUSTOM_KEY( 235 ),
     DIKS_CUSTOM236                = DFB_CUSTOM_KEY( 236 ),
     DIKS_CUSTOM237                = DFB_CUSTOM_KEY( 237 ),
     DIKS_CUSTOM238                = DFB_CUSTOM_KEY( 238 ),
     DIKS_CUSTOM239                = DFB_CUSTOM_KEY( 239 ),
     DIKS_CUSTOM240                = DFB_CUSTOM_KEY( 240 ),
     DIKS_CUSTOM241                = DFB_CUSTOM_KEY( 241 ),
     DIKS_CUSTOM242                = DFB_CUSTOM_KEY( 242 ),
     DIKS_CUSTOM243                = DFB_CUSTOM_KEY( 243 ),
     DIKS_CUSTOM244                = DFB_CUSTOM_KEY( 244 ),
     DIKS_CUSTOM245                = DFB_CUSTOM_KEY( 245 ),
     DIKS_CUSTOM246                = DFB_CUSTOM_KEY( 246 ),
     DIKS_CUSTOM247                = DFB_CUSTOM_KEY( 247 ),
     DIKS_CUSTOM248                = DFB_CUSTOM_KEY( 248 ),
     DIKS_CUSTOM249                = DFB_CUSTOM_KEY( 249 ),
     DIKS_CUSTOM250                = DFB_CUSTOM_KEY( 250 ),
     DIKS_CUSTOM251                = DFB_CUSTOM_KEY( 251 ),
     DIKS_CUSTOM252                = DFB_CUSTOM_KEY( 252 ),
     DIKS_CUSTOM253                = DFB_CUSTOM_KEY( 253 ),
     DIKS_CUSTOM254                = DFB_CUSTOM_KEY( 254 ),
     DIKS_CUSTOM255                = DFB_CUSTOM_KEY( 255 )
} DFBInputDeviceKeySymbol;

/*
 * Flags specifying the key locks that are currently active.
 */
typedef enum {
     DILS_SCROLL         = 0x00000001,  /* scroll-lock active? */
     DILS_NUM            = 0x00000002,  /* num-lock active? */
     DILS_CAPS           = 0x00000004   /* caps-lock active? */
} DFBInputDeviceLockState;

/*
 * Groups and levels as an index to the symbol array.
 */
typedef enum {
     DIKSI_BASE          = 0x00,   /* base group, base level
                                      (no modifier pressed) */
     DIKSI_BASE_SHIFT    = 0x01,   /* base group, shifted level
                                      (with Shift pressed) */
     DIKSI_ALT           = 0x02,   /* alternative group, base level
                                      (with AltGr pressed) */
     DIKSI_ALT_SHIFT     = 0x03,   /* alternative group, shifted level
                                      (with AltGr and Shift pressed) */

     DIKSI_LAST          = DIKSI_ALT_SHIFT
} DFBInputDeviceKeymapSymbolIndex;

/*
 * One entry in the keymap of an input device.
 */
typedef struct {
     int                         code;                  /* hardware
                                                           key code */
     DFBInputDeviceLockState     locks;                 /* locks activating
                                                           shifted level */
     DFBInputDeviceKeyIdentifier identifier;            /* basic mapping */
     DFBInputDeviceKeySymbol     symbols[DIKSI_LAST+1]; /* advanced key
                                                           mapping */
} DFBInputDeviceKeymapEntry;


#ifdef __cplusplus
}
#endif

#endif

