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
#include <errno.h>
#include <dirent.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include <directfb.h>

#include <direct/memcpy.h>
#include <direct/messages.h>
#include <direct/util.h>

#include <misc/conf.h>

#include "matrox.h"
#include "regs.h"
#include "mmio.h"
#include "matrox_maven.h"

#define SYS_CLASS_I2C_DEV "/sys/class/i2c-dev"

static void
maven_write_byte( MatroxMavenData  *mav,
                  MatroxDriverData *mdrv,
                  u8                reg,
                  u8                val )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox) {
          volatile u8 *mmio = mdrv->mmio_base;

          mga_out_dac( mmio, 0x87, reg );
          mga_out_dac( mmio, 0x88, val );
     } else {
          union  i2c_smbus_data       data;
          struct i2c_smbus_ioctl_data args;

          data.byte = val;

          args.read_write = I2C_SMBUS_WRITE;
          args.command    = reg;
          args.size       = I2C_SMBUS_BYTE_DATA;
          args.data       = &data;

          ioctl( mdrv->maven_fd, I2C_SMBUS, &args );
     }
}

static void
maven_write_word( MatroxMavenData  *mav,
                  MatroxDriverData *mdrv,
                  u8                reg,
                  u16               val )
{
     MatroxDeviceData *mdev = mdrv->device_data;

     if (mdev->g450_matrox) {
          volatile u8 *mmio = mdrv->mmio_base;

          mga_out_dac( mmio, 0x87, reg );
          mga_out_dac( mmio, 0x88, val );
          mga_out_dac( mmio, 0x87, reg + 1 );
          mga_out_dac( mmio, 0x88, val >> 8 );
     } else {
          union  i2c_smbus_data       data;
          struct i2c_smbus_ioctl_data args;

          data.word = val;

          args.read_write = I2C_SMBUS_WRITE;
          args.command    = reg;
          args.size       = I2C_SMBUS_WORD_DATA;
          args.data       = &data;

          ioctl( mdrv->maven_fd, I2C_SMBUS, &args );
     }
}

#if 0
/* i2c_smbus_read_byte_data() doesn't work with maven. */
static int
i2c_read_byte( int fd, u8 addr, u8 reg )
{
     int ret;
     u8 val;
     struct i2c_msg msgs[] = {
          { addr, I2C_M_REV_DIR_ADDR, sizeof(reg), &reg },
          { addr, I2C_M_RD | I2C_M_NOSTART, sizeof(val), &val }
     };
     struct i2c_rdwr_ioctl_data data = {
          msgs, 2
     };

     ret = ioctl( fd, I2C_RDWR, &data );
     if (ret < 0)
          return ret;

     return val;
}
#endif

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
     bool              ntsc = dfb_config->matrox_tv_std != DSETV_PAL;

     if (mdev->g450_matrox) {
          if (dfb_config->matrox_cable == 1)
               /* SCART RGB */
               maven_write_byte( mav, mdrv, 0x80, ntsc ? 0x43 : 0x41 );
          else
               /* Composite / S-Video */
               maven_write_byte( mav, mdrv, 0x80, ntsc ? 0x03 : 0x01 );
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
     bool              ntsc = dfb_config->matrox_tv_std != DSETV_PAL;

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
          maven_write_word( mav, mdrv, 0x82, ntsc ? 0x0014 : 0x0017 );
          maven_write_word( mav, mdrv, 0x84, 0x0001 );
     } else {
          maven_write_byte( mav, mdrv, 0xB3, 0x01 );
          maven_write_byte( mav, mdrv, 0x82, 0xA0 );
          maven_write_byte( mav, mdrv, 0xD3, 0x01 );
          maven_write_byte( mav, mdrv, 0x8C, 0x10 );
          maven_write_byte( mav, mdrv, 0x94, 0xA2 );
          maven_write_byte( mav, mdrv, 0x8D, 0x03 );
          maven_write_byte( mav, mdrv, 0xB9, 0x78 );
          maven_write_byte( mav, mdrv, 0xBF, 0x02 );

          /*
           * Deflicker: 0x00, 0xB1, 0xA2
           * Doesn't work due to:
           * - ITU-R BT.656 mode?
           * - scaler is not used?
           * - something else?
           */
          maven_write_byte( mav, mdrv, 0x93, 0x00 );
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
               u8                hue )
{
     maven_write_byte( mav, mdrv, 0x25, hue );
}

void
maven_set_saturation( MatroxMavenData  *mav,
                      MatroxDriverData *mdrv,
                      u8                saturation )
{
     maven_write_byte( mav, mdrv, 0x20, saturation );
     maven_write_byte( mav, mdrv, 0x22, saturation );
}

void
maven_set_bwlevel( MatroxMavenData  *mav,
                   MatroxDriverData *mdrv,
                   u8                brightness,
                   u8                contrast )
{
     MatroxDeviceData *mdev = mdrv->device_data;
     int b, c, bl, wl, wlmax, blmin, range;
     bool ntsc = dfb_config->matrox_tv_std == DSETV_NTSC;

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

     bl = MAX( b - c, blmin );
     wl = MIN( b + c, wlmax );

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

     if (ioctl( mdrv->maven_fd, I2C_SLAVE, mav->address ) < 0) {
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
     DIR  *dir;

     /* Locate G400 maven /dev/i2c file */

     /* Try sysfs */
     if (!mdev->g450_matrox && (dir = opendir( SYS_CLASS_I2C_DEV )) != NULL) {
          char path[PATH_MAX];
          struct dirent *ent;

          while ((ent = readdir( dir )) != NULL) {
               if (!strcmp( ent->d_name, "." ))
                    continue;
               if (!strcmp( ent->d_name, ".." ))
                    continue;

               snprintf( path, sizeof(path), "%s/%s/name", SYS_CLASS_I2C_DEV, ent->d_name );

               FILE *fp = fopen( path, "r" );
               if (!fp) {
                    D_PERROR( "DirectFB/Matrox/Maven: Error opening `%s'!\n", path );
                    continue;
               }

               memset( line, 0, 6 );

               fread( line, 1, 5, fp );
               if (ferror( fp )) {
                    D_PERROR( "DirectFB/Matrox/Maven: Error reading `%s'!\n", path );
                    fclose( fp );
                    continue;
               }

               fclose( fp );

               if (strcmp( line, "MAVEN" ))
                    continue;

               snprintf( mav->dev, sizeof(mav->dev), "/dev/%s", ent->d_name );
               found = true;
               break;
          }
          if (!ent && errno)
               D_PERROR( "DirectFB/Matrox/Maven: Error reading `%s'!\n", SYS_CLASS_I2C_DEV );

          closedir( dir );
     }

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
                    direct_snputs( mav->dev, "/dev/", 6 );
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

#if 0
          /* FIXME: This fails for some people */
          /* Check if maven is at address 0x1B (DH board) or 0x1A (DH add-on) */
          if (i2c_read_byte( fd, 0x1B, 0xB2 ) < 0) {
               if (i2c_read_byte( fd, 0x1A, 0xB2 ) < 0) {
                    D_ERROR( "DirectFB/Matrox/Maven: Error reading from maven chip!\n" );
                    close( fd );
                    return errno2result( errno );
               } else
                    mav->address = 0x1A;
          } else
               mav->address = 0x1B;
#else
          mav->address = 0x1B;
#endif

          close( fd );
     }

     /* Maven registers */
     {
          static const u8 ntscregs[2][0x40] = {
          /* G400 */
          {
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
          },
          /* G450 / G550 */
          {
               0x21, 0xF0, 0x7C, 0x1F, /* 00-03 */
               0x00, /* 04 */
               0x00,
               0x00,
               0x00,
               0x7E, /* 08 */
               0x44, /* 09 */
               0x76, /* 0A */
               0x49, /* 0B */
               0x00, /* 0C */
               0x00,
               0x4E, 0x03, /* 0E-0F */
               0x42, 0x03, /* 10-11 */
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
               0xEA, 0x00, /* 1E-1F */
               0xAE, /* 20 */
               0x04, /* 21 */
               0xAE, /* 22 */
               0x01, /* 23 */
               0x02, /* 24 */
               0x00, /* 25 */
               0x0A, /* 26 */
               0x05, /* 27 */
               0x00, /* 28 */
               0x11, /* 29 */
               0xFF, 0x03, /* 2A-2B */
               0x20, /* 2C */
               0x0F, 0x78, /* 2D-2E */
               0x00, 0x00, /* 2F-30 */
               0xB4, 0x00, /* 31-32 */
               0x14, /* 33 */
               0x02, /* 34 */
               0x00, /* 35 */
               0x00,
               0xBD, /* 37 */
               0xDA, /* 38 */
               0x15, /* 39 */
               0x05, /* 3A */
               0x15, /* 3B */
               0x42, 0x03, /* 3C-3D */
               0x00, /* 3E */
               0x00
          }
          };
          static const u8 palregs[2][0x40] = {
          /* G400 */
          {
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
          },
          /* G450 / G550 */
          {
               0x2A, 0x09, 0x8A, 0xCB, /* 00-03 */
               0x00, /* 04 */
               0x00,
               0x00,
               0x00,
               0x7E, /* 08 */
               0x3A, /* 09 */
               0x8A, /* 0A */
               0x38, /* 0B */
               0x28, /* 0C */
               0x00,
               0x46, 0x01, /* 0E-0F */
               0x46, 0x01, /* 10-11 */
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
               0xEA, 0x00, /* 1E-1F */
               0xBB, /* 20 */
               0x07, /* 21 */
               0xBB, /* 22 */
               0x00, /* 23 */
               0x00, /* 24 */
               0x00, /* 25 */
               0x08, /* 26 */
               0x04, /* 27 */
               0x00, /* 28 */
               0x1A, /* 29 */
               0x55, 0x01, /* 2A-2B */
               0x18, /* 2C */
               0x07, 0x7E, /* 2D-2E */
               0x02, 0x54, /* 2F-30 */
               0xB4, 0x00, /* 31-32 */
               0x16, /* 33 */
               0x49, /* 34 */
               0x00, /* 35 */
               0x00,
               0xB9, /* 37 */
               0xDD, /* 38 */
               0x22, /* 39 */
               0x02, /* 3A */
               0x22, /* 3B */
               0x46, 0x00, /* 3C-3D */
               0x00, /* 3E */
               0x00,
          }
          };

          if (dfb_config->matrox_tv_std != DSETV_PAL)
               direct_memcpy( mav->regs, ntscregs[mdev->g450_matrox], 64 );
          else
               direct_memcpy( mav->regs, palregs[mdev->g450_matrox], 64 );

          if (dfb_config->matrox_tv_std == DSETV_PAL_60) {
               mav->regs[0x00] = palregs[mdev->g450_matrox][0x00];
               mav->regs[0x01] = palregs[mdev->g450_matrox][0x01];
               mav->regs[0x02] = palregs[mdev->g450_matrox][0x02];
               mav->regs[0x03] = palregs[mdev->g450_matrox][0x03];
               mav->regs[0x0B] = palregs[mdev->g450_matrox][0x0B];
               mav->regs[0x0C] = palregs[mdev->g450_matrox][0x0C];
               mav->regs[0x0E] = palregs[mdev->g450_matrox][0x0E];
               mav->regs[0x0F] = palregs[mdev->g450_matrox][0x0F];
               mav->regs[0x10] = palregs[mdev->g450_matrox][0x10];
               mav->regs[0x11] = palregs[mdev->g450_matrox][0x11];
               mav->regs[0x1E] = palregs[mdev->g450_matrox][0x1E];
               mav->regs[0x1F] = palregs[mdev->g450_matrox][0x1F];
               mav->regs[0x20] = palregs[mdev->g450_matrox][0x20];
               mav->regs[0x22] = palregs[mdev->g450_matrox][0x22];
               mav->regs[0x25] = palregs[mdev->g450_matrox][0x25];
               mav->regs[0x34] = palregs[mdev->g450_matrox][0x34];
          }

          if (!mdev->g450_matrox) {
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
