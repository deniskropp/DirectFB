/*
   (c) 1998-2001 Petr Vandrovec <vandrove@vc.cvut.cz>

   This code originally comes from matroxfb.
   Relicensed under the LGPL with the authors permission.
   Adapted for CRTC2 ITU-R 656 mode by Ville Syrjala <syrjala@sci.fi>

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

#ifndef __MATROX_MAVEN_H__
#define __MATROX_MAVEN_H__

struct maven_data {
     int        fd;
     int        mode;
};

struct mavenregs {
     __u8       regs[256];
     int        mode;
};

int maven_open( struct maven_data *md );
void maven_close( struct maven_data *md );

/*
 * Set TV standard
 * Must be called before maven_compute()
 */
#define MODE_PAL	1
#define MODE_NTSC	2
void maven_set_mode( struct maven_data *md,
                     int mode );
/*
 * Compute initial register values
 * Must be called before maven_program()
 */
void maven_compute( struct maven_data *md,
                    struct mavenregs *m );

/*
 * Program registers
 */
void maven_program( struct maven_data *md,
                    const struct mavenregs *m );

/* The following functions can be called at any time */

/* Set deflicker mode (0=disabled,1,2) */
void maven_set_deflicker( struct maven_data *md,
                          int mode );
/* Set hue (0-255) */
void maven_set_hue( struct maven_data *md,
                    int hue );
/* Set saturation (0-255) */
void maven_set_saturation( struct maven_data *md,
                           int saturation );
/* Set brightness and contrast (0-255) */
void maven_set_bwlevel( struct maven_data *md,
                        int brightness,
                        int contrast );

#endif
