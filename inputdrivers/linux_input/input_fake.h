#ifndef _INPUT_FAKE_H
#define _INPUT_FAKE_H

#include <linux/input.h>


#if !defined(KEY_OK)

/**
 *  define some additional remote control keys in case they 
 *  were not already defined above in <linux/input.h>
 */

#define KEY_OK           0x160
#define KEY_SELECT       0x161
#define KEY_GOTO         0x162
#define KEY_CLEAR        0x163
#define KEY_POWER2       0x164
#define KEY_OPTION       0x165
#define KEY_INFO         0x166
#define KEY_TIME         0x167
#define KEY_VENDOR       0x168
#define KEY_ARCHIVE      0x169
#define KEY_PROGRAM      0x16a
#define KEY_CHANNEL      0x16b
#define KEY_FAVORITES    0x16c
#define KEY_EPG          0x16d
#define KEY_PVR          0x16e
#define KEY_MHP          0x16f
#define KEY_LANGUAGE     0x170
#define KEY_TITLE        0x171
#define KEY_SUBTITLE     0x172
#define KEY_ANGLE        0x173
#define KEY_ZOOM         0x174
#define KEY_MODE         0x175
#define KEY_KEYBOARD     0x176
#define KEY_SCREEN       0x177
#define KEY_PC           0x178
#define KEY_TV           0x179
#define KEY_TV2          0x17a
#define KEY_VCR          0x17b
#define KEY_VCR2         0x17c
#define KEY_SAT          0x17d
#define KEY_SAT2         0x17e
#define KEY_CD           0x17f
#define KEY_TAPE         0x180
#define KEY_RADIO        0x181
#define KEY_TUNER        0x182
#define KEY_PLAYER       0x183
#define KEY_TEXT         0x184
#define KEY_DVD          0x185
#define KEY_AUX          0x186
#define KEY_MP3          0x187
#define KEY_AUDIO        0x188
#define KEY_VIDEO        0x189
#define KEY_DIRECTORY    0x18a
#define KEY_LIST         0x18b
#define KEY_MEMO         0x18c
#define KEY_CALENDAR     0x18d
#define KEY_RED          0x18e
#define KEY_GREEN        0x18f
#define KEY_YELLOW       0x190
#define KEY_BLUE         0x191
#define KEY_CHANNELUP    0x192
#define KEY_CHANNELDOWN  0x193
#define KEY_FIRST        0x194
#define KEY_LAST         0x195
#define KEY_AB           0x196
#define KEY_PLAY         0x197
#define KEY_RESTART      0x198
#define KEY_SLOW         0x199
#define KEY_SHUFFLE      0x19a
#define KEY_FASTFORWARD  0x19b
#define KEY_PREVIOUS     0x19c
#define KEY_NEXT         0x19d
#define KEY_DIGITS       0x19e
#define KEY_TEEN         0x19f
#define KEY_TWEN         0x1a0
#define KEY_BREAK        0x1a1


#endif  /* !defined(KEY_OK)  */
#endif  /* _INPUT_FAKE_H */

