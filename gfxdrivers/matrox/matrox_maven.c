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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include "matrox_maven.h"

#ifndef min
#define min(a,b) (a < b ? a : b)
#endif
#ifndef max
#define max(a,b) (a > b ? a : b)
#endif

#define MAVEN_I2CID	(0x1B)

static int maven_set_reg( int fd, __u8 reg, __u8 val )
{
     __s32 err;

     err = i2c_smbus_write_byte_data( fd, reg, val );
     if (err)
          fprintf( stderr, "maven_set_reg(0x%X) failed\n", reg );
     return err;
}

static int maven_set_reg_pair( int fd, __u8 reg, __u16 val )
{
     __s32 err;

     err = i2c_smbus_write_word_data( fd, reg, val );
     if (err)
          fprintf( stderr, "maven_set_reg_pair(0x%X) failed\n", reg );
     return err;
}

#define LR(x) maven_set_reg( fd, (x), m->regs[(x)] )
#define LRP(x) maven_set_reg_pair( fd, (x), m->regs[(x)] | (m->regs[(x)+1] << 8) )

void maven_set_mode( struct maven_data *md,
                     int mode )
{
     md->mode = mode;
}

void maven_compute( struct maven_data *md,
                    struct mavenregs *data )
{
     static struct mavenregs palregs = { {
          0x2A, 0x09, 0x8A, 0xCB,	/* 00: chroma subcarrier */
          0x00,
          0x00,	/* ? not written */
          0x00,	/* modified by code (09 written...) */
          0x00,	/* ? not written */
          0x7E,	/* 08 */
          0x3C,	/* 09 */
          0x82,	/* 0A */
          0x2E,	/* 0B */
          0x21,	/* 0C */
          0x00,	/* ? not written */
          0x3F, 0x03, /* 0E-0F */
          0x3F, 0x03, /* 10-11 */
          0x1A,	/* 12 */
          0x2A,	/* 13 */
          0x1C, 0x3D, 0x14, /* 14-16 */
          0x9C, 0x01, /* 17-18 */
          0x00,	/* 19 */
          0xFE,	/* 1A */
          0x7E,	/* 1B */
          0x60,	/* 1C */
          0x05,	/* 1D */
          0xC4, 0x01, /* 1E-1F */
          0x95,	/* 20 */
          0x07,	/* 21 */
          0x95,	/* 22 */
          0x00,	/* 23 */
          0x00,	/* 24 */
          0x00,	/* 25 */
          0x08,	/* 26 */
          0x04,	/* 27 */
          0x00,	/* 28 */
          0x1A,	/* 29 */
          0x55, 0x01, /* 2A-2B */
          0x20,	/* 2C */
          0x07, 0x7E, /* 2D-2E */
          0x02, 0x54, /* 2F-30 */
          0xB4, 0x00, /* 31-32 */
          0x14,	/* 33 */
          0x49,	/* 34 */
          0x00,	/* 35 written multiple times */
          0x00,	/* 36 not written */
          0xA3,	/* 37 */
          0xC8,	/* 38 */
          0x22,	/* 39 */
          0x02,	/* 3A */
          0x22,	/* 3B */
          0x3F, 0x03, /* 3C-3D */
          0x00,	/* 3E written multiple times */
          0x00,	/* 3F not written */
     }, MODE_PAL };
     static struct mavenregs ntscregs = { {
          0x21, 0xF0, 0x7C, 0x1F,	/* 00: chroma subcarrier */
          0x00,
          0x00,	/* ? not written */
          0x00,	/* modified by code (09 written...) */
          0x00,	/* ? not written */
          0x7E,	/* 08 */
          0x43,	/* 09 */
          0x7E,	/* 0A */
          0x3D,	/* 0B */
          0x00,	/* 0C */
          0x00,	/* ? not written */
          0x46, 0x03, /* 0E-0F */
          0x3C, 0x02, /* 10-11 */
          0x17,	/* 12 */
          0x21,	/* 13 */
          0x1B, 0x1B, 0x24, /* 14-16 */
          0x83, 0x01, /* 17-18 */
          0x00,	/* 19 */
          0x0F,	/* 1A */
          0x0F,	/* 1B */
          0x60,	/* 1C */
          0x05,	/* 1D */
          0xC4, 0x02, /* 1E-1F */
          0x8E,	/* 20 */
          0x04,	/* 21 */
          0x8E,	/* 22 */
          0x01,	/* 23 */
          0x02,	/* 24 */
          0x00,	/* 25 */
          0x0A,	/* 26 */
          0x05,	/* 27 */
          0x00,	/* 28 */
          0x10,	/* 29 */
          0xFF, 0x03, /* 2A-2B */
          0x18,	/* 2C */
          0x0F, 0x78, /* 2D-2E */
          0x00, 0x00, /* 2F-30 */
          0xB4, 0x00, /* 31-32 */
          0x14,	/* 33 */
          0x02,	/* 34 */
          0x00,	/* 35 written multiple times */
          0x00,	/* 36 not written */
          0xA3,	/* 37 */
          0xC8,	/* 38 */
          0x15,	/* 39 */
          0x05,	/* 3A */
          0x15,	/* 3B */
          0x3C, 0x00, /* 3C-3D */
          0x00,	/* 3E written multiple times */
          0x00,	/* never written */
     }, MODE_NTSC };

     if (md->mode & MODE_PAL)
          *data = palregs;
     else
          *data = ntscregs;

     /* gamma correction registers */
     data->regs[0x83] = 0x00;
     data->regs[0x84] = 0x00;
     data->regs[0x85] = 0x00;
     data->regs[0x86] = 0x1F;
     data->regs[0x87] = 0x10;
     data->regs[0x88] = 0x10;
     data->regs[0x89] = 0x10;
     data->regs[0x8A] = 0x64;	/* 100 */
     data->regs[0x8B] = 0xC8;	/* 200 */
}

void maven_program( struct maven_data* md,
                    const struct mavenregs* m )
{
     int fd = md->fd;

     maven_set_reg( fd, 0x3E, 0x01 );

     maven_set_reg( fd, 0x8C, 0x00 );
     maven_set_reg( fd, 0x94, 0xA2 );

     maven_set_reg_pair( fd, 0x8E, 0x1EFF );
     maven_set_reg( fd, 0xC6, 0x01 );

     maven_set_reg( fd, 0x06, 0x09 );

     /* chroma subcarrier */
     LR(0x00); LR(0x01); LR(0x02); LR(0x03);

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
     if (m->mode & MODE_PAL)
          maven_set_reg( fd, 0x35, 0x10 );
     else
          maven_set_reg( fd, 0x35, 0x0F );
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
     if (m->mode & MODE_PAL)
          maven_set_reg( fd, 0x35, 0x1D );
     else
          maven_set_reg( fd, 0x35, 0x1C );

     LRP(0x3C);
     LR(0x37);
     LR(0x38);

     maven_set_reg( fd, 0xB3, 0x01 );

     maven_set_reg( fd, 0x82, 0xA0 );

     maven_set_reg( fd, 0xD3, 0x01 );
     maven_set_reg( fd, 0x8C, 0x10 );

     maven_set_reg( fd, 0x94, 0xA2 );

     LR(0x33);
     maven_set_reg(fd, 0x8D, 0x03);
     maven_set_reg(fd, 0xB9, 0x78);
     maven_set_reg(fd, 0xBF, 0x02);

     maven_set_deflicker( md, 0 );
     if (m->mode & MODE_PAL)
          maven_set_saturation( md, 149 );
     else
          maven_set_saturation( md, 142 );
     maven_set_hue( md, 0 );
     LRP(0x10);
     maven_set_bwlevel( md, 255, 255 );

     /* load gamma correction stuff */
     LR(0x83);
     LR(0x84);
     LR(0x85);
     LR(0x86);
     LR(0x87);
     LR(0x88);
     LR(0x89);
     LR(0x8A);
     LR(0x8B);

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
     if (m->mode & MODE_PAL)
          maven_set_reg( fd, 0x35, 0x1D );
     else
          maven_set_reg( fd, 0x35, 0x1C );
     LRP(0x3C);
     LR(0x37);
     LR(0x38);

     maven_set_reg( fd, 0xB0, 0x80 );

     maven_set_reg( fd, 0x82, 0x20 );
     maven_set_reg( fd, 0x3E, 0x00 );

     maven_set_reg( md->fd, 0xD4, 0x01 );
     maven_set_reg( md->fd, 0xD4, 0x00 );
}

void maven_set_deflicker( struct maven_data *md,
                          int mode )
{
     __u8 val;

     switch (mode) {
     case 1:
          val = 0xB1;
          break;
     case 2:
          val = 0xA2;
          break;
     default:
          val = 0x00;
     }
     maven_set_reg( md->fd, 0x93, val );
}

void maven_set_hue( struct maven_data *md,
                    int hue )
{
     maven_set_reg( md->fd, 0x25, hue );
}

void maven_set_saturation( struct maven_data *md,
                           int saturation )
{
     maven_set_reg( md->fd, 0x20, saturation );
     maven_set_reg( md->fd, 0x22, saturation );
}

void maven_set_bwlevel( struct maven_data *md,
                        int brightness,
                        int contrast )
{
     int bl, wl;
     int wlmax = 786;
     int blmin = (md->mode == MODE_NTSC) ? 242 : 255;
     int range = (wlmax - blmin) / 2;

     /* convert values to proper range */
     brightness = brightness * range / 255 + blmin;
     contrast = contrast * range / 255;

     /* calculate levels */
     bl = max( brightness - contrast, blmin );
     wl = min( brightness + contrast, wlmax );

     /* Convert to maven 8+2 bits format */
     bl = ((bl << 8) & 0x0300) | ((bl >> 2) & 0x00FF);
     wl = ((wl << 8) & 0x0300) | ((wl >> 2) & 0x00FF);

     /* Write regs */
     maven_set_reg_pair( md->fd, 0x0E, bl );
     maven_set_reg_pair( md->fd, 0x1E, wl );
}

int maven_open( struct maven_data *md )
{
     char dev[16] = "/dev/";
     char line[256];
     FILE *file;
     int fd;

     /* Locate maven /dev/i2c entry */
     file = fopen( "/proc/bus/i2c", "r" );
     if (!file)
          return -1;
     while (fgets( line, 256, file )) {
          if (strstr( line, "MAVEN" )) {
               strtok( line, " \t\n" );
               strncat( dev, line, 10 );
               break;
          }
     }
     fclose( file );

     if ((fd = open( dev, O_RDWR )) < 0) {
          return -1;
     }

     if (ioctl( fd, I2C_SLAVE, MAVEN_I2CID ) < 0) {
          close(fd);
          return -1;
     }

     md->fd = fd;
     md->mode = MODE_PAL;

     return 0;
}

void maven_close( struct maven_data *md )
{
     close( md->fd );
}
