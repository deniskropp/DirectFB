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

#include <config.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>

#ifdef USE_SYSFS
#include <sysfs/libsysfs.h>
#endif

#include "i2c-dev.h"

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <misc/conf.h>
#include <misc/util.h>

#include "matrox.h"
#include "regs.h"
#include "mmio.h"
#include "matrox_maven.h"

#ifndef min
#define min(a,b) (a < b ? a : b)
#endif
#ifndef max
#define max(a,b) (a > b ? a : b)
#endif

#define MAVEN_I2CID	(0x1B)

static void
maven_write_byte( MatroxMavenData  *mav,
                  MatroxDriverData *mdrv,
                  __u8              reg,
                  __u8              val )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox) {
          volatile __u8 *mmio = mdrv->mmio_base;

          mga_out_dac( mmio, 0x87, reg );
          mga_out_dac( mmio, 0x88, val );
     } else
          i2c_smbus_write_byte_data( mdrv->maven_fd, reg, val );
}

static void
maven_write_word( MatroxMavenData  *mav,
                  MatroxDriverData *mdrv,
                  __u8              reg,
                  __u16             val )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox) {
          volatile __u8 *mmio = mdrv->mmio_base;

          mga_out_dac( mmio, 0x87, reg );
          mga_out_dac( mmio, 0x88, val );
          mga_out_dac( mmio, 0x87, reg + 1 );
          mga_out_dac( mmio, 0x88, val >> 8);
     } else
          i2c_smbus_write_word_data( mdrv->maven_fd, reg, val );
}

void
maven_disable( MatroxMavenData  *mav,
               MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     maven_write_byte( mav, mdrv, 0x3E, 0x01 );

     if (mdev->g450_matrox) {
          maven_write_byte( mav, mdrv, 0x80, 0x00 );
          return;
     }

     maven_write_byte( mav, mdrv, 0x82, 0x80 );
     maven_write_byte( mav, mdrv, 0x8C, 0x00 );
     maven_write_byte( mav, mdrv, 0x94, 0xA2 );
     maven_write_word( mav, mdrv, 0x8E, 0x1EFF );
     maven_write_byte( mav, mdrv, 0xC6, 0x01 );
}

void
maven_enable( MatroxMavenData  *mav,
              MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox) {
          if (dfb_config->matrox_cable == 1)
               /* SCART RGB */
               maven_write_byte( mav, mdrv, 0x80,
                                 dfb_config->matrox_ntsc ? 0x43 : 0x41 );
          else
               /* Composite / S-Video */
               maven_write_byte( mav, mdrv, 0x80,
                                 dfb_config->matrox_ntsc ? 0x03 : 0x01 );
     }
     else
          maven_write_byte( mav, mdrv, 0x82, 0x20 );

     maven_write_byte( mav, mdrv, 0x3E, 0x00 );
}

void
maven_sync( MatroxMavenData  *mav,
            MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox)
          return;

     maven_write_byte( mav, mdrv, 0xD4, 0x01 );
     maven_write_byte( mav, mdrv, 0xD4, 0x00 );
}

#define LR(x)  maven_write_byte( mav, mdrv, (x), mav->regs[(x)] )
#define LRP(x) maven_write_word( mav, mdrv, (x), mav->regs[(x)] | (mav->regs[(x)+1] << 8) )

void
maven_set_regs( MatroxMavenData       *mav,
                MatroxDriverData      *mdrv,
                CoreLayerRegionConfig *config,
                DFBColorAdjustment    *adj )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     LR(0x00);
     LR(0x01);
     LR(0x02);
     LR(0x03);
     LR(0x04);
     LR(0x2C);
     LR(0x08);
     LR(0x0A);
     LR(0x09);
     LR(0x29);
     LRP(0x31);
     LRP(0x17);
     LR(0x0B);
     LR(0x0C);
     LR(0x35);
     LRP(0x10);
     LRP(0x0E);
     LRP(0x1E);
     LR(0x20);
     LR(0x22);
     LR(0x25);
     LR(0x34);

     LR(0x33);
     LR(0x19);
     LR(0x12);
     LR(0x3B);
     LR(0x13);
     LR(0x39);
     LR(0x1D);
     LR(0x3A);
     LR(0x24);
     LR(0x14);
     LR(0x15);
     LR(0x16);
     LRP(0x2D);
     LRP(0x2F);
     LR(0x1A);
     LR(0x1B);
     LR(0x1C);
     LR(0x23);
     LR(0x26);
     LR(0x28);
     LR(0x27);
     LR(0x21);
     LRP(0x2A);
     LR(0x35);
     LRP(0x3C);
     LR(0x37);
     LR(0x38);

     if (mdev->g450_matrox) {
          maven_write_word( mav, mdrv, 0x82,
                            dfb_config->matrox_ntsc ? 0x0014 : 0x0017 );
          maven_write_word( mav, mdrv, 0x84, 0x0001 );
     } else {
          maven_write_byte( mav, mdrv, 0xB3, 0x01 );
          maven_write_byte( mav, mdrv, 0x82, 0xA0 );
          maven_write_byte( mav, mdrv, 0xD3, 0x01 );
          maven_write_byte( mav, mdrv, 0x8C, 0x10 );
          maven_write_byte( mav, mdrv, 0x94, 0xA2 );
          maven_write_byte( mav, mdrv, 0x8D, 0x03);
          maven_write_byte( mav, mdrv, 0xB9, 0x78);
          maven_write_byte( mav, mdrv, 0xBF, 0x02);

          /*  deflicker: 0x00, 0xB1, 0xA2 */
          maven_write_byte( mav, mdrv, 0x93,
                            config->options & DLOP_FLICKER_FILTERING ? 0xA2 : 0x00 );
     }

     maven_set_saturation( mav, mdrv, adj->saturation >> 8 );
     maven_set_hue( mav, mdrv, adj->hue >> 8 );
     maven_set_bwlevel( mav, mdrv, adj->brightness >> 8,
                        adj->contrast >> 8 );

     if (!mdev->g450_matrox) {
          LR(0x83);
          LR(0x84);
          LR(0x85);
          LR(0x86);
          LR(0x87);
          LR(0x88);
          LR(0x89);
          LR(0x8A);
          LR(0x8B);

          switch (dfb_config->matrox_cable) {
               case 1:
                    /* SCART RGB */
                    maven_write_byte( mav, mdrv, 0xB0, 0x85 );
                    break;
               case 2:
                    /* SCART Composite */
                    maven_write_byte( mav, mdrv, 0xB0, 0x81 );
                    break;
               default:
                    /* Composite / S-Video */
                    maven_write_byte( mav, mdrv, 0xB0, 0x80 );
                    break;
          }
     }
}

void
maven_set_hue( MatroxMavenData  *mav,
               MatroxDriverData *mdrv,
               __u8              hue )
{
     maven_write_byte( mav, mdrv, 0x25, hue );
}

void
maven_set_saturation( MatroxMavenData  *mav,
                      MatroxDriverData *mdrv,
                      __u8              saturation )
{
     maven_write_byte( mav, mdrv, 0x20, saturation );
     maven_write_byte( mav, mdrv, 0x22, saturation );
}

void
maven_set_bwlevel( MatroxMavenData  *mav,
                   MatroxDriverData *mdrv,
                   __u8              brightness,
                   __u8              contrast )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     int b, c, bl, wl, wlmax, blmin, range;
     bool ntsc = dfb_config->matrox_ntsc;

     if (mdev->g450_matrox) {
          wlmax = ntsc ? 936 : 938;
          blmin = ntsc ? 267 : 281;
     } else {
          wlmax = 786;
          blmin = ntsc ? 242 : 255;
     }
     range = wlmax - blmin - 128;

     b = brightness * range / 255 + blmin;
     c = contrast * range / 2 / 255 + 64;

     bl = max( b - c, blmin );
     wl = min( b + c, wlmax );

     blmin = ((blmin << 8) & 0x0300) | ((blmin >> 2) & 0x00FF);
     bl    = ((bl    << 8) & 0x0300) | ((bl    >> 2) & 0x00FF);
     wl    = ((wl    << 8) & 0x0300) | ((wl    >> 2) & 0x00FF);

     maven_write_word( mav, mdrv, 0x10, blmin );
     maven_write_word( mav, mdrv, 0x0E, bl );
     maven_write_word( mav, mdrv, 0x1E, wl );
}

DFBResult
maven_open( MatroxMavenData  *mav,
            MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox)
          return DFB_OK;

     if (mdrv->maven_fd != -1)
          D_BUG( "DirectFB/Matrox/Maven: Device already open!\n" );

     if ((mdrv->maven_fd = open( mav->dev, O_RDWR )) < 0) {
          D_PERROR( "DirectFB/Matrox/Maven: Error opening `%s'!\n",
                     mav->dev );
          mdrv->maven_fd = -1;
          return errno2result( errno );
     }

     if (ioctl( mdrv->maven_fd, I2C_SLAVE, MAVEN_I2CID ) < 0) {
          D_PERROR( "DirectFB/Matrox/Maven: Error controlling `%s'!\n",
                     mav->dev );
          close( mdrv->maven_fd );
          mdrv->maven_fd = -1;
          return errno2result( errno );
     }

     return DFB_OK;
}

void
maven_close( MatroxMavenData  *mav,
             MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox)
          return;

     if (mdrv->maven_fd == -1)
          D_BUG( "DirectFB/Matrox/Maven: Device not open!\n" );

     close( mdrv->maven_fd );
     mdrv->maven_fd = -1;
}

DFBResult maven_init( MatroxMavenData  *mav,
                      MatroxDriverData *mdrv )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     char  line[512];
     int   fd;
     FILE *file;
     bool  found = false;

     /* Locate G400 maven /dev/i2c file */
#ifdef USE_SYSFS
     /* Try sysfs */
     if (!mdev->g450_matrox && !sysfs_get_mnt_path( line, 512 )) {
          struct sysfs_class *class;
          struct dlist *class_devices;
          struct sysfs_class_device *class_device;
          struct sysfs_device *device;
          struct sysfs_attribute *attr;

          class = sysfs_open_class( "i2c-dev" );
          if (!class) {
               D_PERROR( "DirectFB/Matrox/Maven: "
                          "Error opening sysfs class `i2c-dev'!\n" );
               return errno2result( errno );
          }

          class_devices = sysfs_get_class_devices( class );
          if (!class_devices) {
               D_PERROR( "DirectFB/Matrox/Maven: "
                          "Error reading sysfs class devices!\n" );
               sysfs_close_class( class );
               return errno2result( errno );
          }
          dlist_for_each_data( class_devices, class_device,
                               struct sysfs_class_device ) {
               device = sysfs_get_classdev_device( class_device );
               if (device) {
                    attr = sysfs_get_device_attr( device, "name" );
                    if (attr) {
                         if (strstr( attr->value, "MAVEN" )) {
                              strncpy( mav->dev, "/dev/", 6 );
                              strncat( mav->dev, class_device->name, 250 );
                              found = true;
                              break;
                         }
                    }
               }
          }
          sysfs_close_class( class );
     }
#endif

     /* Try /proc/bus/i2c */
     if (!mdev->g450_matrox && !found) {
          file = fopen( "/proc/bus/i2c", "r" );
          if (!file) {
               D_PERROR( "DirectFB/Matrox/Maven: "
                          "Error opening `/proc/bus/i2c'!\n" );
               return errno2result( errno );
          }
          while (fgets( line, 512, file )) {
               if (strstr( line, "MAVEN" )) {
                    char *p = line;
                    while (!isspace( *p ))
                         p++;
                    *p = '\0';
                    strncpy( mav->dev, "/dev/", 6 );
                    strncat( mav->dev, line, 250 );
                    found = true;
                    break;
               }
          }
          fclose( file );
     }

     if (!mdev->g450_matrox) {
          if (!found) {
               D_ERROR( "DirectFB/Matrox/Maven: "
                         "Can't find MAVEN i2c device file!\n" );
               return DFB_UNSUPPORTED;
          }

          /* Try to use it */
          if ((fd = open( mav->dev, O_RDWR )) < 0) {
               D_PERROR( "DirectFB/Matrox/Maven: Error opening `%s'!\n",
                          mav->dev );
               return errno2result( errno );
          }

          if (ioctl( fd, I2C_SLAVE, MAVEN_I2CID ) < 0) {
               D_PERROR( "DirectFB/Matrox/Maven: Error controlling `%s'!\n",
                          mav->dev );
               close( fd );
               return errno2result( errno );
          }
          close( fd );
     }

     /* Maven registers */
     {
          const __u8 ntscregs[64] = {
               0x21, 0xF0, 0x7C, 0x1F, /* 00-03 */
               0x00, /* 04 */
               0x00,
               0x00,
               0x00,
               0x7E, /* 08 */
               0x43, /* 09 */
               0x7E, /* 0A */
               0x3D, /* 0B */
               0x00, /* 0C */
               0x00,
               0x46, 0x03, /* 0E-0F */
               0x3C, 0x02, /* 10-11 */
               0x17, /* 12 */
               0x21, /* 13 */
               0x1B, /* 14 */
               0x1B, /* 15 */
               0x24, /* 16 */
               0x83, 0x01, /* 17-18 */
               0x00, /* 19 */
               0x0F, /* 1A */
               0x0F, /* 1B */
               0x60, /* 1C */
               0x05, /* 1D */
               0xC4, 0x02, /* 1E-1F */
               0x8E, /* 20 */
               0x04, /* 21 */
               0x8E, /* 22 */
               0x01, /* 23 */
               0x02, /* 24 */
               0x00, /* 25 */
               0x0A, /* 26 */
               0x05, /* 27 */
               0x00, /* 28 */
               0x10, /* 29 */
               0xFF, 0x03, /* 2A-2B */
               0x18, /* 2C */
               0x0F, 0x78, /* 2D-2E */
               0x00, 0x00, /* 2F-30 */
               0xB4, 0x00, /* 31-32 */
               0x14, /* 33 */
               0x02, /* 34 */
               0x1C, /* 35 */
               0x00,
               0xA3, /* 37 */
               0xC8, /* 38 */
               0x15, /* 39 */
               0x05, /* 3A */
               0x15, /* 3B */
               0x3C, 0x00, /* 3C-3D */
               0x00, /* 3E */
               0x00
          };
          const __u8 palregs[64] = {
               0x2A, 0x09, 0x8A, 0xCB, /* 00-03 */
               0x00, /* 04 */
               0x00,
               0x00,
               0x00,
               0x7E, /* 08 */
               0x3C, /* 09 */
               0x82, /* 0A */
               0x2E, /* 0B */
               0x21, /* 0C */
               0x00,
               0x3F, 0x03, /* 0E-0F */
               0x3F, 0x03, /* 10-11 */
               0x1A, /* 12 */
               0x2A, /* 13 */
               0x1C, /* 14 */
               0x3D, /* 15 */
               0x14, /* 16 */
               0x9C, 0x01, /* 17-18 */
               0x00, /* 19 */
               0xFE, /* 1A */
               0x7E, /* 1B */
               0x60, /* 1C */
               0x05, /* 1D */
               0xC4, 0x01, /* 1E-1F */
               0x95, /* 20 */
               0x07, /* 21 */
               0x95, /* 22 */
               0x00, /* 23 */
               0x00, /* 24 */
               0x00, /* 25 */
               0x08, /* 26 */
               0x04, /* 27 */
               0x00, /* 28 */
               0x1A, /* 29 */
               0x55, 0x01, /* 2A-2B */
               0x20, /* 2C */
               0x07, 0x7E, /* 2D-2E */
               0x02, 0x54, /* 2F-30 */
               0xB4, 0x00, /* 31-32 */
               0x14, /* 33 */
               0x49, /* 34 */
               0x1D, /* 35 */
               0x00,
               0xA3, /* 37 */
               0xC8, /* 38 */
               0x22, /* 39 */
               0x02, /* 3A */
               0x22, /* 3B */
               0x3F, 0x03, /* 3C-3D */
               0x00, /* 3E */
               0x00,
          };

          if (dfb_config->matrox_ntsc)
               direct_memcpy( mav->regs, ntscregs, 64 );
          else
               direct_memcpy( mav->regs, palregs, 64 );

          if (mdev->g450_matrox) {
               if (dfb_config->matrox_ntsc) {
                    mav->regs[0x09] = 0x44;
                    mav->regs[0x0A] = 0x76;
                    mav->regs[0x0B] = 0x49;
                    mav->regs[0x0C] = 0x00;
                    mav->regs[0x0E] = 0x4E;
                    mav->regs[0x0F] = 0x03;
                    mav->regs[0x10] = 0x42;
                    mav->regs[0x11] = 0x03;
                    mav->regs[0x1E] = 0xEA;
                    mav->regs[0x1F] = 0x00;
                    mav->regs[0x20] = 0xAE;
                    mav->regs[0x22] = 0xAE;
                    mav->regs[0x29] = 0x11;
                    mav->regs[0x2C] = 0x20;
                    mav->regs[0x33] = 0x14;
                    mav->regs[0x35] = 0x00;
                    mav->regs[0x37] = 0xBD;
                    mav->regs[0x38] = 0xDA;
                    mav->regs[0x3C] = 0x42;
                    mav->regs[0x3D] = 0x03;
               } else {
                    mav->regs[0x09] = 0x3A;
                    mav->regs[0x0A] = 0x8A;
                    mav->regs[0x0B] = 0x38;
                    mav->regs[0x0C] = 0x28;
                    mav->regs[0x0E] = 0x46;
                    mav->regs[0x0F] = 0x01;
                    mav->regs[0x10] = 0x46;
                    mav->regs[0x11] = 0x01;
                    mav->regs[0x1E] = 0xEA;
                    mav->regs[0x1F] = 0x00;
                    mav->regs[0x20] = 0xBB;
                    mav->regs[0x22] = 0xBB;
                    mav->regs[0x29] = 0x1A;
                    mav->regs[0x2C] = 0x18;
                    mav->regs[0x33] = 0x16;
                    mav->regs[0x35] = 0x00;
                    mav->regs[0x37] = 0xB9;
                    mav->regs[0x38] = 0xDD;
                    mav->regs[0x3C] = 0x46;
                    mav->regs[0x3D] = 0x00;
               }
          } else {
               /* gamma */
               mav->regs[0x83] = 0x00;
               mav->regs[0x84] = 0x00;
               mav->regs[0x85] = 0x00;
               mav->regs[0x86] = 0x1F;
               mav->regs[0x87] = 0x10;
               mav->regs[0x88] = 0x10;
               mav->regs[0x89] = 0x10;
               mav->regs[0x8A] = 0x64;
               mav->regs[0x8B] = 0xC8;
          }
     }

     return DFB_OK;
}
