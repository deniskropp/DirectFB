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

typedef struct {
     char dev[256];
     __u8 regs[256];
} MatroxMavenData;

DFBResult maven_init( MatroxMavenData  *mav,
                      MatroxDriverData *mdrv );

DFBResult maven_open( MatroxMavenData  *mav,
                      MatroxDriverData *mdrv );
void maven_close( MatroxMavenData  *mav,
                  MatroxDriverData *mdrv );

void maven_enable( MatroxMavenData  *mav,
                   MatroxDriverData *mdrv );
void maven_disable( MatroxMavenData  *mav,
                    MatroxDriverData *mdrv );
void maven_sync( MatroxMavenData  *mav,
                 MatroxDriverData *mdrv );

void maven_set_regs( MatroxMavenData       *mav,
                     MatroxDriverData      *mdrv,
                     CoreLayerRegionConfig *config,
                     DFBColorAdjustment    *adj );

void maven_set_hue( MatroxMavenData  *mav,
                    MatroxDriverData *mdrv,
                    __u8              hue );
void maven_set_saturation( MatroxMavenData  *mav,
                           MatroxDriverData *mdrv,
                           __u8              saturation );
void maven_set_bwlevel( MatroxMavenData  *mav,
                        MatroxDriverData *mdrv,
                        __u8              brightness,
                        __u8              contrast );

#endif
