/*
 * $Workfile: nsc_galfns.c $
 * $Revision: 1.5 $
 * $Author: andi $
 *
 * File Contents: This file contains the main functions of the Geode 
 *                frame buffer device drivers GAL function definitions.
 *
 * Project:       Geode Frame buffer device driver
 *
 */

/* NSC_LIC_ALTERNATIVE_PREAMBLE
 *
 * Revision 1.0
 *
 * National Semiconductor Alternative GPL-BSD License
 *
 * National Semiconductor Corporation licenses this software 
 * ("Software"):
 *
 * National Xfree frame buffer driver
 *
 * under one of the two following licenses, depending on how the 
 * Software is received by the Licensee.
 * 
 * If this Software is received as part of the Linux Framebuffer or
 * other GPL licensed software, then the GPL license designated 
 * NSC_LIC_GPL applies to this Software; in all other circumstances 
 * then the BSD-style license designated NSC_LIC_BSD shall apply.
 *
 * END_NSC_LIC_ALTERNATIVE_PREAMBLE */

/* NSC_LIC_BSD
 *
 * National Semiconductor Corporation Open Source License for 
 *
 * National Xfree frame buffer driver
 *
 * (BSD License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 *   * Redistributions of source code must retain the above copyright 
 *     notice, this list of conditions and the following disclaimer. 
 *
 *   * Redistributions in binary form must reproduce the above 
 *     copyright notice, this list of conditions and the following 
 *     disclaimer in the documentation and/or other materials provided 
 *     with the distribution. 
 *
 *   * Neither the name of the National Semiconductor Corporation nor 
 *     the names of its contributors may be used to endorse or promote 
 *     products derived from this software without specific prior 
 *     written permission. 
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE,
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE.
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * END_NSC_LIC_BSD */

/* NSC_LIC_GPL
 *
 * National Semiconductor Corporation Gnu General Public License for 
 *
 * National Xfree frame buffer driver
 *
 * (GPL License with Export Notice)
 *
 * Copyright (c) 1999-2001
 * National Semiconductor Corporation.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted under the terms of the GNU General 
 * Public License as published by the Free Software Foundation; either 
 * version 2 of the License, or (at your option) any later version  
 *
 * In addition to the terms of the GNU General Public License, neither 
 * the name of the National Semiconductor Corporation nor the names of 
 * its contributors may be used to endorse or promote products derived 
 * from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS 
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT 
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS 
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE 
 * NATIONAL SEMICONDUCTOR CORPORATION OR CONTRIBUTORS BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE 
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS 
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER 
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE, 
 * INTELLECTUAL PROPERTY INFRINGEMENT, OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY 
 * OF SUCH DAMAGE. See the GNU General Public License for more details. 
 *
 * EXPORT LAWS: THIS LICENSE ADDS NO RESTRICTIONS TO THE EXPORT LAWS OF 
 * YOUR JURISDICTION. It is licensee's responsibility to comply with 
 * any export regulations applicable in licensee's jurisdiction. Under 
 * CURRENT (2001) U.S. export regulations this software 
 * is eligible for export from the U.S. and can be downloaded by or 
 * otherwise exported or reexported worldwide EXCEPT to U.S. embargoed 
 * destinations which include Cuba, Iraq, Libya, North Korea, Iran, 
 * Syria, Sudan, Afghanistan and any other country to which the U.S. 
 * has embargoed goods and services. 
 *
 * You should have received a copy of the GNU General Public License 
 * along with this file; if not, write to the Free Software Foundation, 
 * Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA 
 *
 * END_NSC_LIC_GPL */

#ifndef XFree86LOADER
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>
#endif

#include <core/fbdev/fbdev.h>

#include "nsc_galproto.h"

/*
 * Compile time constants
 */
#define FBDEV_NAME  "/dev/nscgal"

/*
 * Cool Macros to access the structures
 */
#define INIT_GAL(x) \
		(x)->dwSignature = FBGAL_SIGNATURE;\
		(x)->dwSize = sizeof(*x);\
		(x)->dwVersion = FBGAL_VERSION;

/*------------------------------------------------------------------------
 * create_devicenode
 *
 * Description:	This function creates nscgal device node in the device
 *					directory.
 * parameters :	none
 *	
 *	return:	'0' was return on creating the galdevice node.
 *----------------------------------------------------------------------*/
int
create_devicenode()
{

#if 1
   FILE *pfdevices;
   char line[200], devname[200];
   int majdev;

   /* remove fails if device is open */
   remove("/dev/nscgal");

   if ((pfdevices = fopen("/proc/devices", "r"))) {
      while (fgets(line, sizeof(line), pfdevices)) {
	 if (sscanf(line, "%d%*[ \t]%s", &majdev, devname) == 2) {
	    if (strstr(devname, "nscgal"))
	       mknod("/dev/nscgal", S_IFCHR | S_IRUSR | S_IWUSR,
		     makedev(majdev, 0));
	 }
      }
      fclose(pfdevices);
   }
   return 1;
#endif

}

/*------------------------------------------------------------------------
 * Gal_initialize_interface 
 *
 * Description:	This function intializes the  nscgal device .
 * parameters :	none
 *	
 *	return:	 '1' was returned on intialization of the galdevice 
 *					otherwise '0' was returned on failure.
 *----------------------------------------------------------------------*/
BOOLEAN
Gal_initialize_interface()
{
/*	create_devicenode();  */

   return 1;
}

/*------------------------------------------------------------------------
 * Gal_cleanup_interface
 *
 * Description:	This function closes the  nscgal device .
 * parameters :	none
 *	
 *	return:	 '1' was returned on closing the galdevice.
 *----------------------------------------------------------------------*/
BOOLEAN
Gal_cleanup_interface()
{
   return 1;
}

/*---------------------------------------------------------------------------
 * Gal_write_register
 *
 * Description:	This function writes the data to the hardware register
 *					of the nscgal device .
 *  parameters:	
 *        type:	It specifies the hardware access type.
 *      offset: It specifies the offset address the register to be accessed.
 *       value:	It specifies the data value to be written into the register.
 *        size: It specifies the size of the data to be written. 
 *	
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_write_register(int type, unsigned long offset, unsigned long value,
		   int size)
{
   GAL_HWACCESS hwAccess;

   INIT_GAL(&hwAccess);
   hwAccess.dwSubfunction = GALFN_WRITEREG;
   hwAccess.dwType = type;
   hwAccess.dwOffset = offset;
   hwAccess.dwValue = value;
   hwAccess.dwByteCount = size;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &hwAccess))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_read_register
 *
 * Description:	This function reads the data from the hardware register
 *					of the nscgal device .
 *  parameters:	
 *        type:	It specifies the hardware access type.
 *      offset: It specifies the offset address of the register to be accessed.
 *       value:	It specifies the pointer to hold the data  to be read from
 *					the gal hardware register.
 *        size: It specifies the size of the data to be read
 *	
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_read_register(int type, unsigned long offset, unsigned long *value,
		  int size)
{
   GAL_HWACCESS hwAccess;

   INIT_GAL(&hwAccess);
   hwAccess.dwSubfunction = GALFN_READREG;
   hwAccess.dwType = type;
   hwAccess.dwOffset = offset;
   hwAccess.dwByteCount = size;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &hwAccess))
      return 0;
   else {
      *value = hwAccess.dwValue;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_get_adapter_info
 *
 * Description:	This function gets the adapter information of the
 *					nscgal device .
 *  parameters:	
 *pAdapterInfo: It specifies the adapter information structure.     
 *	
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_adapter_info(PGAL_ADAPTERINFO pAdapterInfo)
{
   INIT_GAL(pAdapterInfo);

   pAdapterInfo->dwSubfunction = GALFN_GETADAPTERINFO;

   if (!dfb_fbdev || ioctl(dfb_fbdev->fd, FBIOGAL_API, pAdapterInfo))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_softvga_state 
 *
 * Description:	This function sets the softvga state of the platform device .
 *  parameters:	
 *     bEnable:	It specifies the softvga state enable state.    
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_softvga_state(BOOLEAN bEnable)
{
   GAL_SOFTVGASTATE sSoftVgaState;

   INIT_GAL(&sSoftVgaState);
   sSoftVgaState.dwSubfunction = GALFN_SETSOFTVGASTATE;
   sSoftVgaState.bSoftVgaEnable = bEnable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSoftVgaState))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_softvga_state 
 *
 * Description:	This function gets the softvga state of the platform device .
 *  parameters:	
 *     bEnable:	get the softvga state.    
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_softvga_state(int *bState)
{
   GAL_SOFTVGASTATE sSoftVgaState;

   INIT_GAL(&sSoftVgaState);
   sSoftVgaState.dwSubfunction = GALFN_GETSOFTVGASTATE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSoftVgaState))
      return 0;
   else {
      *bState = sSoftVgaState.bSoftVgaEnable;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_vga_test_pci
 *
 * Description:	This function tests the vga pci.
 *  parameters:	
 *     softvga:	It is pointer to the softvga state.    
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_test_pci(int *softvga)
{
   GAL_VGATESTPCI sVgatestpci;

   INIT_GAL(&sVgatestpci);
   sVgatestpci.dwSubfunction = GALFN_GETSOFTVGASTATE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgatestpci))
      return 0;
   else {
      *softvga = sVgatestpci.softvga;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_vga_get_pci_command
 *
 * Description:	This function gets the vga pci command.
 *  parameters:	
 *       value:	It is pointer to pci command value.    
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_get_pci_command(unsigned char *value)
{
   GAL_VGAGETPCICOMMAND sVgagetpcicommand;

   INIT_GAL(&sVgagetpcicommand);
   sVgagetpcicommand.dwSubfunction = GALFN_VGAGETPCICOMMAND;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgagetpcicommand))
      return 0;
   else {
      *value = sVgagetpcicommand.value;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_vga_seq_reset
 *
 * Description:	This function resets the vga seq.
 *  parameters:	
 *       reset:	It gives the reset value.    
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_seq_reset(int reset)
{
   GAL_VGASEQRESET sVgaseqreset;

   INIT_GAL(&sVgaseqreset);
   sVgaseqreset.dwSubfunction = GALFN_VGASEQRESET;
   sVgaseqreset.reset = reset;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgaseqreset))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_vga_set_graphics_bits
 *
 * Description:	This function resets the vga seq.
 *  parameters:	None.
 *        
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_set_graphics_bits(void)
{
   GAL_VGASETGRAPHICSBITS sVgasetgraphics;

   INIT_GAL(&sVgasetgraphics);
   sVgasetgraphics.dwSubfunction = GALFN_VGASETGRAPHICSBITS;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgasetgraphics))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_crt_enable
 *
 * Description:	This function sets the crt state of the device .
 *  parameters:	
 *    crtState:	It specifies the crt state of the galdevice.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_crt_enable(int crtEnable)
{
   GAL_CRTENABLE sCrtEnable;

   INIT_GAL(&sCrtEnable);
   sCrtEnable.dwSubfunction = GALFN_SETCRTENABLE;
   sCrtEnable.wCrtEnable = crtEnable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCrtEnable))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_is_display_mode_supported 
 *
 * Description:	This function checks the display mode is supported or not.
 *  parameters:	
 *        xres:	It specifies x co-ordinate resolution.
 *        Yres:	It specifies y co-ordinate resolution.  
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	    hz:	It specifies the frequency of the display mode.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_is_display_mode_supported(int xres, int yres, int bpp, int hz,
			      int *supported)
{
   GAL_DISPLAYMODE sDisplayMode;

   *supported = 0;
   INIT_GAL(&sDisplayMode);
   sDisplayMode.dwSubfunction = GALFN_ISDISPLAYMODESUPPORTED;
   sDisplayMode.wXres = xres;
   sDisplayMode.wYres = yres;
   sDisplayMode.wBpp = bpp;
   sDisplayMode.wRefresh = hz;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayMode))
      return 0;
   else {
      *supported = sDisplayMode.dwSupported;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_mode
 *
 * Description:	This function sets the display mode of the galdevice.
 *  parameters:	
 *        xres:	It specifies x co-ordinate resolution.
 *        Yres:	It specifies y co-ordinate resolution.  
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	    hz:	It specifies the frequency of the display mode.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_mode(int xres, int yres, int bpp, int hz)
{
   GAL_DISPLAYMODE sDisplayMode;

   INIT_GAL(&sDisplayMode);
   sDisplayMode.dwSubfunction = GALFN_SETDISPLAYMODE;
   sDisplayMode.wXres = xres;
   sDisplayMode.wYres = yres;
   sDisplayMode.wBpp = bpp;
   sDisplayMode.wRefresh = hz;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayMode))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_mode
 *
 * Description:	This function gets the display mode of the galdevice.
 *  parameters:	
 *        xres:	It specifies x co-ordinate resolution.
 *        Yres:	It specifies y co-ordinate resolution.  
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	    hz:	It specifies the frequency of the display mode.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_mode(int *xres, int *yres, int *bpp, int *hz)
{
   GAL_DISPLAYMODE sDisplayMode;

   INIT_GAL(&sDisplayMode);
   sDisplayMode.dwSubfunction = GALFN_GETDISPLAYMODE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayMode))
      return 0;
   else {
      *xres = sDisplayMode.wXres;
      *yres = sDisplayMode.wYres;
      *bpp = sDisplayMode.wBpp;
      *hz = sDisplayMode.wRefresh;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_bpp
 *
 * Description:	This function sets the number bits per pixel in the display
 *					mode of the galdevice.
 *  parameters:	
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_bpp(unsigned short bpp)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_SETDISPLAYBPP;
   sDisplayParams.wBpp = bpp;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_set_bpp
 *
 * Description:	This function sets the number bits per pixel in the display
 *					mode of the galdevice.
 *  parameters:	
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_bpp(unsigned short bpp)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_SETBPP;
   sDisplayParams.wBpp = bpp;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_bpp
 *
 * Description:	This function gets the number bits per pixel in the display
 *					mode of the galdevice.
 *  parameters:	
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_bpp(unsigned short *bpp)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_GETDISPLAYBPP;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else {
      *bpp = sDisplayParams.wBpp;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_pitch
 *
 * Description:	This function sets the display pitch of the galdevice.
 *  parameters:	
 *       pitch:	It specifies pitch of the display mode.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_pitch(unsigned short pitch)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_SETDISPLAYPITCH;
   sDisplayParams.wPitch = pitch;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_pitch
 *
 * Description:	This function gets the display pitch of the galdevice.
 *  parameters:	
 *       pitch:	It specifies pitch of the display mode.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_pitch(unsigned short *pitch)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_GETDISPLAYPITCH;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else {
      *pitch = sDisplayParams.wPitch;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_offset
 *
 * Description:	This function sets the offset of display parameters.
 *  parameters:	
 *      offset:	It specifies the offset address of display parameters.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_offset(unsigned long offset)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_SETDISPLAYOFFSET;
   sDisplayParams.dwOffset = offset;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_offset
 *
 * Description:	This function gets the offset of display parameters.
 *  parameters:	
 *      offset:	It specifies the offset address of display parameters.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_offset(unsigned long *offset)
{
   GAL_DISPLAYPARAMS sDisplayParams;

   INIT_GAL(&sDisplayParams);
   sDisplayParams.dwSubfunction = GALFN_GETDISPLAYOFFSET;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDisplayParams))
      return 0;
   else {
      *offset = sDisplayParams.dwOffset;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_get_refreshrate_from_dotclock
 *
 * Description:	This function gets the refreshrate from dotclock.
 *  parameters:	
 *        xres:	It specifies x co-ordinate resolution.
 *        Yres:	It specifies y co-ordinate resolution.  
 *	   bpp:	It specifies the bits per pixel (8/16 bits).
 *	    hz:	It is a pointer which holds the refresh rate of the display.
 *   frequency:	It spcifies the frequency of the dotclock.
 *	return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_refreshrate_from_dotclock(int xres, int yres, int bpp, int *hz,
				  unsigned long frequency)
{
   GAL_DOTCLKTOREFRESH sDclkToRefresh;

   INIT_GAL(&sDclkToRefresh);
   sDclkToRefresh.dwSubfunction = GALFN_DOTCLKTOREFRESH;
   sDclkToRefresh.wXres = xres;
   sDclkToRefresh.wYres = yres;
   sDclkToRefresh.wBpp = bpp;
   sDclkToRefresh.dwDotClock = frequency;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sDclkToRefresh))
      return 0;
   else {
      *hz = sDclkToRefresh.wRefreshRate;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_get_display_timing
 *
 *    Description:	This function gets the display timing from galdevice.
 *     parameters:	
 * pDisplayTiming:	It specifies the display timing of the galdevice.
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_timing(PGAL_DISPLAYTIMING pDisplayTiming)
{
   INIT_GAL(pDisplayTiming);
   pDisplayTiming->dwSubfunction = GALFN_GETDISPLAYTIMINGS;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pDisplayTiming))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_timing
 *
 *    Description:	This function sets the display timing of the galdevice. 
 *     parameters:	
 * pDisplayTiming:	It specifies the display timing of the galdevice.
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_timing(PGAL_DISPLAYTIMING pDisplayTiming)
{
   INIT_GAL(pDisplayTiming);
   pDisplayTiming->dwSubfunction = GALFN_SETDISPLAYTIMINGS;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pDisplayTiming))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_set_fixed_timings
 *
 *    Description:	This function sets the fixed display timings of the 
 *						galdevice. 
 *     parameters:
 *	  pnlXres:	It specifies the panel X resolution.
 *	  pnlYres:	It specifies the panel Y resolution.
 *        totXres:  It specifies the total X resolution.  
 *        totYres:  It specifies the total Y resolution.
 *	      bpp:	It specifies the bits per pixel (8/16 bits).
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_fixed_timings(int pnlXres, int pnlYres, int totXres,
		      int totYres, int bpp)
{
   GAL_DISPLAYTIMING DisplayTiming;

   INIT_GAL(&DisplayTiming);
   DisplayTiming.dwSubfunction = GALFN_SETFIXEDTIMINGS;
   DisplayTiming.wHActive = pnlXres;	/* panel Xres */
   DisplayTiming.wVActive = pnlYres;	/* panel Yres */
   DisplayTiming.wHTotal = totXres;	/* Total Xres */
   DisplayTiming.wVTotal = totYres;	/* Total Yres */
   DisplayTiming.wBpp = bpp;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &DisplayTiming))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_set_display_palette_entry
 *
 *    Description:	This function sets the display palette entry of the 
 *						galdevice. 
 *     parameters:
 *	    index:	It specifies the palette index,
 *        palette:	It specifies the palette of the galdevice. 
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_palette_entry(unsigned long index, unsigned long palette)
{
   GAL_PALETTE_ENTRY sPalette;

   INIT_GAL(&sPalette);
   sPalette.dwSubfunction = GALFN_SETPALETTE_ENTRY;
   sPalette.dwIndex = index;
   sPalette.dwPalette = palette;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sPalette))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_palette_entry
 *
 *    Description:	This function gets the display palette entry of the 
 *						galdevice. 
 *     parameters:
 *          index:	It specifies the palette index,
 *        palette:	It is a pointer to the palette of the galdevice. 
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_palette_entry(unsigned long index, unsigned long *palette)
{
   GAL_PALETTE_ENTRY sPalette;

   INIT_GAL(&sPalette);
   sPalette.dwSubfunction = GALFN_GETPALETTE_ENTRY;
   sPalette.dwIndex = index;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sPalette))
      return 0;
   else {
      *palette = sPalette.dwPalette;
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_set_display_palette_entry
 *
 *    Description:	This function sets the display palette entry of the 
 *						galdevice. 
 *     parameters:
 *       pPalette:	It specifies the palette structure of the galdevice. 
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_display_palette(PGAL_PALETTE pPalette)
{
   INIT_GAL(pPalette);
   pPalette->dwSubfunction = GALFN_SETPALETTE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pPalette))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_get_display_palette_entry
 *
 *    Description:	This function gets the display palette entry of the 
 *						galdevice. 
 *     parameters:
 *       pPalette:	It specifies the palette structure of the galdevice. 
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_display_palette(PGAL_PALETTE pPalette)
{
   INIT_GAL(pPalette);
   pPalette->dwSubfunction = GALFN_GETPALETTE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pPalette))
      return 0;
   else {
      return 1;
   }
}

/*---------------------------------------------------------------------------
 * Gal_wait_until_idle
 *
 *    Description:	This function waits until the graphics engine is idle. 
 *     parameters:	none.
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_wait_until_idle(void)
{
   GAL_WAITUNTILIDLE sWaitIdle;

   INIT_GAL(&sWaitIdle);
   sWaitIdle.dwSubfunction = GALFN_WAITUNTILIDLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sWaitIdle))
      return 0;
   else
      return 1;
}

/*---------------------------------------------------------------------------
 * Gal_wait_vertical_blank
 *
 *    Description:	This function wait until start of vertical blank.
 *     parameters:	none.
 *	   return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_wait_vertical_blank(void)
{
   GAL_WAITVERTICALBLANK sWaitVerticalBlank;

   INIT_GAL(&sWaitVerticalBlank);
   sWaitVerticalBlank.dwSubfunction = GALFN_WAITVERTICALBLANK;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sWaitVerticalBlank))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_cursor_enable
 *
 * Description:	This function enable or disable the hardware cursor.
 *  parameters:
 *      enable:	This specifies the enable value of the cursor.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_cursor_enable(int enable)
{
   GAL_CURSORENABLE sCursorEnable;

   INIT_GAL(&sCursorEnable);
   sCursorEnable.dwSubfunction = GALFN_SETCURSORENABLE;
   sCursorEnable.bCursorEnable = enable ? 1 : 0;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorEnable))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_cursor_position
 *
 * Description:	This function sets the position of the cursor.
 *  parameters:
 *   memoffset:	It specifies the memory offset of the cursor position.
 *        xpos: It specifies the X co-ordinate position of the cursor.
 *        ypos: It specifies the Y co-ordinate position of the cursor.
 *    xhotspot: It specifies the X hotspot location for current cursor shape.
 *    yhotspot: It specifies the Y hotspot location for current cursor shape. 
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_cursor_position(unsigned long memoffset,
			unsigned short xpos, unsigned short ypos,
			unsigned short xhotspot, unsigned short yhotspot)
{
   GAL_CURSORPOSITION sCursorPos;

   INIT_GAL(&sCursorPos);
   sCursorPos.dwSubfunction = GALFN_SETCURSORPOSITION;
   sCursorPos.dwMemOffset = memoffset;
   sCursorPos.wXPos = xpos;
   sCursorPos.wYPos = ypos;
   sCursorPos.wXHot = xhotspot;
   sCursorPos.wYHot = yhotspot;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorPos))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_cursor_position
 *
 * Description:	This function gets the cursor position.
 *  parameters:
 *   memoffset:	It points the memory offset of the cursor position.
 *        xpos: It points the X co-ordinate position of the cursor.
 *        ypos: It points  the Y co-ordinate position of the cursor.
 *    xhotspot: It points the X hotspot location for current cursor shape.
 *    yhotspot: It points  the Y hotspot location for current cursor shape. 
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_cursor_position(unsigned long *memoffset,
			unsigned short *xpos, unsigned short *ypos,
			unsigned short *xhotspot, unsigned short *yhotspot)
{
   GAL_CURSORPOSITION sCursorPos;

   INIT_GAL(&sCursorPos);
   sCursorPos.dwSubfunction = GALFN_GETCURSORPOSITION;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorPos))
      return 0;
   else {
      *memoffset = sCursorPos.dwMemOffset;
      *xpos = sCursorPos.wXPos;
      *ypos = sCursorPos.wYPos;
      *xhotspot = sCursorPos.wXHot;
      *yhotspot = sCursorPos.wYHot;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_cursor_shape32
 *
 * Description:	This function loads 32x32 cursor pattern.
 *  parameters:
 *   memoffset:	It specifies the graphics memory offset for cursor shape.
 *     andmask:	It is a pointer to 32 DWORD of AND data.
 *     xormask:	It is a pointer to 32 DWORD of XOR data.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_cursor_shape32(unsigned long memoffset,
		       unsigned long *andmask, unsigned long *xormask)
{
   GAL_SETCURSORSHAPE sCursorShape;

   INIT_GAL(&sCursorShape);
   sCursorShape.dwSubfunction = GALFN_SETCURSORSHAPE;
   sCursorShape.dwMemOffset = memoffset;

   memcpy(sCursorShape.dwAndMask, andmask, sizeof(sCursorShape.dwAndMask));

   memcpy(sCursorShape.dwXorMask, xormask, sizeof(sCursorShape.dwXorMask));

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorShape))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_cursor_shape64
 *
 * Description:	This function loads 64x64 cursor pattern.
 *  parameters:
 *   memoffset:	It specifies the graphics memory offset for cursor shape.
 *     andmask:	It is a pointer to 64 DWORD of AND data.
 *     xormask:	It is a pointer to 64 DWORD of XOR data.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/ BOOLEAN
Gal_set_cursor_shape64(unsigned long memoffset,
		       unsigned long *andmask, unsigned long *xormask)
{
   GAL_SETCURSORSHAPE sCursorShape;

   INIT_GAL(&sCursorShape);
   sCursorShape.dwSubfunction = GALFN_SETCURSORSHAPE_RCLD;
   sCursorShape.dwMemOffset = memoffset;

   memcpy(sCursorShape.dwAndMask, andmask, sizeof(sCursorShape.dwAndMask));

   memcpy(sCursorShape.dwXorMask, xormask, sizeof(sCursorShape.dwXorMask));

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorShape))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_cursor_colors
 *
 * Description:	This function sets the colors of the hardware cursor.
 *  parameters:
 *     bkcolor:It specifies the RGB value for the background color.
 *     fgcolor:It specifies the RGB value for the foreground color.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_cursor_colors(unsigned long bkcolor, unsigned long fgcolor)
{
   GAL_CURSORCOLORS sCursorColor;

   INIT_GAL(&sCursorColor);
   sCursorColor.dwSubfunction = GALFN_SETCURSORCOLORS;
   sCursorColor.dwBgColor = bkcolor;
   sCursorColor.dwFgColor = fgcolor;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorColor))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_cursor_colors
 *
 * Description:	This function gets the colors of the hardware cursor.
 *  parameters:
 *     bkcolor:It points the RGB value for the background color.
 *     fgcolor:It points the RGB value for the foreground color.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_get_cursor_colors(unsigned long *bkcolor, unsigned long *fgcolor)
{
   GAL_CURSORCOLORS sCursorColor;

   INIT_GAL(&sCursorColor);
   sCursorColor.dwSubfunction = GALFN_GETCURSORCOLORS;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCursorColor))
      return 0;
   else {
      *bkcolor = sCursorColor.dwBgColor;
      *fgcolor = sCursorColor.dwFgColor;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_solid_pattern
 *
 * Description:	This function sets a solid pattern color for future rendering.
 *  parameters:
 *       color:	It specifies the pattern color in proper format for current 
 *					display mode.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_solid_pattern(unsigned long color)
{
   GAL_SETSOLIDPATTERN sSetSoildPat;

   INIT_GAL(&sSetSoildPat);
   sSetSoildPat.dwSubfunction = GALFN_SETSOLIDPATTERN;
   sSetSoildPat.dwColor = color;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetSoildPat))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_solid_source
 *
 * Description:	This function specifies a constant source data value for 
 *				raster operations that use both pattern 
 *				and source data.
 *  parameters:
 *       color:	It specifies the source color.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *-------------------------------------------------------------------------*/
BOOLEAN
Gal_set_solid_source(unsigned long color)
{
   GAL_SETSOLIDSOURCE sSetSolidSrc;

   INIT_GAL(&sSetSolidSrc);
   sSetSolidSrc.dwSubfunction = GALFN_SETSOLIDSOURCE;
   sSetSolidSrc.dwColor = color;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetSolidSrc))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_mono_source
 *
 * Description:	
 *  parameters:
 *     bkcolor: It specifies the background color.
 *     fgcolor: It specifies the foreground color.
 *transparency:	It specifies the transparency flag.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_mono_source(unsigned long bgcolor, unsigned long fgcolor,
		    unsigned char transparency)
{
   GAL_SETMONOSOURCE sSetMonoSrc;

   INIT_GAL(&sSetMonoSrc);
   sSetMonoSrc.dwSubfunction = GALFN_SETMONOSOURCE;
   sSetMonoSrc.dwFgColor = fgcolor;
   sSetMonoSrc.dwBgColor = bgcolor;
   sSetMonoSrc.cTransparency = transparency;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetMonoSrc))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_mono_pattern
 *
 * Description:	This function specifies an 8x8 monochrome pattern 
 *				used in future rendering operations.
 *  parameters:
 *     bkcolor: It specifies the background color.
 *     fgcolor: It specifies the foreground color.
 *		 data0: It specifies the bits of 8x8 monochrome pattern.
 *       data1: It specifies the bits of 8x8 monochrome pattern.
 *transparency:	It specifies the transparency flag.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_mono_pattern(unsigned long bgcolor, unsigned long fgcolor,
		     unsigned long data0, unsigned long data1,
		     unsigned char transparency)
{
   GAL_SETMONOPATTERN sSetMonoPat;

   INIT_GAL(&sSetMonoPat);
   sSetMonoPat.dwSubfunction = GALFN_SETMONOPATTERN;
   sSetMonoPat.dwFgColor = fgcolor;
   sSetMonoPat.dwBgColor = bgcolor;
   sSetMonoPat.dwData0 = data0;
   sSetMonoPat.dwData1 = data1;
   sSetMonoPat.cTransparency = transparency;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetMonoPat))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_raster_operation
 *
 * Description:	This function specifies the raster operation for 
 *					future rendering.  
 *  parameters:
 *         rop: It specifies the ternary raster operation
 *					(pattern/source/destination).
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_raster_operation(unsigned char rop)
{
   GAL_RASTEROPERATION sSetRop;

   INIT_GAL(&sSetRop);
   sSetRop.dwSubfunction = GALFN_SETRASTEROPERATION;
   sSetRop.cRop = rop;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetRop))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_pattern_fill
 *
 * Description:	This function renders pattern data to a rectangular 
 *					region. 
 *  parameters:
 *           x:	It specifies the screen X position, in pixels.
 *           y:	It specifies the screen Y position, in pixels.
 *       width:	It specifies the width of rectangle, in pixels.
 *      height:	It specifies the height of rectangle, in pixels.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pattern_fill(unsigned short x, unsigned short y,
		 unsigned short width, unsigned short height)
{
   GAL_PATTERNFILL sPatternFill;

   INIT_GAL(&sPatternFill);
   sPatternFill.dwSubfunction = GALFN_PATTERNFILL;
   sPatternFill.wXPos = x;
   sPatternFill.wYPos = y;
   sPatternFill.wWidth = width;
   sPatternFill.wHeight = height;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sPatternFill))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_screen_to_screen_blt
 *
 * Description:	This function is used to perform a screen to screen 
 *				BLT operation. 
 *  parameters:
 *        srcx:	It specifies the source X position.
 *        srcy:	It specifies the source Y position.
 *        dstx:	It specifies the destination X position.
 *        dsty:	It specifies the destination Y position.
 *       width: It specifies the width of BLT, in pixels.
 *      height: It specifies the height of BLT, in pixels.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_screen_to_screen_blt(unsigned short srcx, unsigned short srcy,
			 unsigned short dstx, unsigned short dsty,
			 unsigned short width, unsigned short height)
{
   GAL_SCREENTOSCREENBLT sScreenBlt;

   INIT_GAL(&sScreenBlt);
   sScreenBlt.dwSubfunction = GALFN_SCREENTOSCREENBLT;
   sScreenBlt.wXStart = srcx;
   sScreenBlt.wYStart = srcy;
   sScreenBlt.wXEnd = dstx;
   sScreenBlt.wYEnd = dsty;
   sScreenBlt.wWidth = width;
   sScreenBlt.wHeight = height;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sScreenBlt))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_screen_to_screen_xblt
 *
 * Description:	This function is used to perform a screen to screen 
 *				BLT operation using a transparency color. 
 *  parameters:
 *        srcx:	It specifies the source X position.
 *        srcy:	It specifies the source Y position.
 *        dstx:	It specifies the destination X position.
 *        dsty:	It specifies the destination Y position.
 *       width: It specifies the width of BLT, in pixels.
 *      height: It specifies the height of BLT, in pixels.
 *       color: It specifies the transparency color.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_screen_to_screen_xblt(unsigned short srcx, unsigned short srcy,
			  unsigned short dstx, unsigned short dsty,
			  unsigned short width, unsigned short height,
			  unsigned long color)
{
   GAL_SCREENTOSCREENXBLT sScreenXBlt;

   INIT_GAL(&sScreenXBlt);
   sScreenXBlt.dwSubfunction = GALFN_SCREENTOSCREENXBLT;
   sScreenXBlt.wXStart = srcx;
   sScreenXBlt.wYStart = srcy;
   sScreenXBlt.wXEnd = dstx;
   sScreenXBlt.wYEnd = dsty;
   sScreenXBlt.wWidth = width;
   sScreenXBlt.wHeight = height;
   sScreenXBlt.dwColor = color;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sScreenXBlt))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_bresenham_line
 *
 * Description:	This function is used to draw a single pixel line
 *					using the specified Bresenham parameters. 
 *  parameters:
 *           x:	It specifies the starting X position.
 *           y:	It specifies the starting Y position.
 *      length:	It specifies the length of the vector, in pixels.
 *     initerr:	It specifies the Bresenham initial error term.
 *    axialerr:	It specifies the Bresenham axial error term
 *					(moving in major direction only).
 *     diagerr: It specifies Bresenham diagonal error term
 *					(moving in major and minor direction.
 *       flags: It specifies the flag.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_bresenham_line(unsigned short x, unsigned short y,
		   unsigned short length, unsigned short initerr,
		   unsigned short axialerr, unsigned short diagerr,
		   unsigned short flags)
{
   GAL_BRESENHAMLINE sBresenhamLine;

   INIT_GAL(&sBresenhamLine);
   sBresenhamLine.dwSubfunction = GALFN_BRESENHAMLINE;
   sBresenhamLine.wX1 = x;
   sBresenhamLine.wY1 = y;
   sBresenhamLine.wLength = length;
   sBresenhamLine.wErr = initerr;
   sBresenhamLine.wE1 = axialerr;
   sBresenhamLine.wE2 = diagerr;
   sBresenhamLine.wFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sBresenhamLine))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal_color_pattern_fill(unsigned short x, unsigned short y,
		       unsigned short width, unsigned short height,
		       unsigned long pattern)
{
   GAL_COLOR_PATTERNFILL sColorPat;

   INIT_GAL(&sColorPat);
   sColorPat.dwSubfunction = GALFN_COLOR_PATTERNFILL;
   sColorPat.wDstx = x;
   sColorPat.wDsty = y;
   sColorPat.wWidth = width;
   sColorPat.wHeight = height;
   sColorPat.dwPattern = pattern;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sColorPat))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal_color_bitmap_to_screen_blt(unsigned short srcx, unsigned short srcy,
			       unsigned short dstx, unsigned short dsty,
			       unsigned short width, unsigned short height,
			       unsigned long data, long pitch)
{
   GAL_COLOR_BITMAP_TO_SCREEN_BLT sBmp2Scr;

   INIT_GAL(&sBmp2Scr);
   sBmp2Scr.dwSubfunction = GALFN_COLOR_BITMAP_TO_SCREEN_BLT;
   sBmp2Scr.wSrcx = srcx;
   sBmp2Scr.wSrcy = srcy;
   sBmp2Scr.wDstx = dstx;
   sBmp2Scr.wDsty = dsty;
   sBmp2Scr.wWidth = width;
   sBmp2Scr.wHeight = height;
   sBmp2Scr.dwData = data;
   sBmp2Scr.wPitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sBmp2Scr))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal_color_bitmap_to_screen_xblt(unsigned short srcx, unsigned short srcy,
				unsigned short dstx, unsigned short dsty,
				unsigned short width, unsigned short height,
				unsigned long data, long pitch,
				unsigned long color)
{
   GAL_COLOR_BITMAP_TO_SCREEN_XBLT sBmp2Scr;

   INIT_GAL(&sBmp2Scr);
   sBmp2Scr.dwSubfunction = GALFN_COLOR_BITMAP_TO_SCREEN_XBLT;
   sBmp2Scr.wSrcx = srcx;
   sBmp2Scr.wSrcy = srcy;
   sBmp2Scr.wDstx = dstx;
   sBmp2Scr.wDsty = dsty;
   sBmp2Scr.wWidth = width;
   sBmp2Scr.wHeight = height;
   sBmp2Scr.dwData = data;
   sBmp2Scr.wPitch = pitch;
   sBmp2Scr.dwColor = color;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sBmp2Scr))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal_mono_bitmap_to_screen_blt(unsigned short srcx, unsigned short srcy,
			      unsigned short dstx, unsigned short dsty,
			      unsigned short width, unsigned short height,
			      unsigned long data, short pitch)
{
   GAL_MONO_BITMAP_TO_SCREEN_BLT sBmp2Scr;

   INIT_GAL(&sBmp2Scr);
   sBmp2Scr.dwSubfunction = GALFN_MONO_BITMAP_TO_SCREEN_BLT;
   sBmp2Scr.wSrcx = srcx;
   sBmp2Scr.wSrcy = srcy;
   sBmp2Scr.wDstx = dstx;
   sBmp2Scr.wDsty = dsty;
   sBmp2Scr.wWidth = width;
   sBmp2Scr.wHeight = height;
   sBmp2Scr.dwData = data;
   sBmp2Scr.wPitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sBmp2Scr))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal_text_blt(unsigned short dstx, unsigned short dsty, unsigned short width,
	     unsigned short height, unsigned long data)
{
   GAL_TEXT_BLT sTextBlt;

   INIT_GAL(&sTextBlt);
   sTextBlt.dwSubfunction = GALFN_TEXT_BLT;
   sTextBlt.wDstx = dstx;
   sTextBlt.wDsty = dsty;
   sTextBlt.wWidth = width;
   sTextBlt.wHeight = height;
   sTextBlt.dwData = data;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sTextBlt))
      return 0;
   else
      return 1;
}

/*------------------------------------------------------------------------
 * Gal_set_compression_enable
 *
 *       Description:	This function enables or disables display 
 * 			compression.
 *	  parameters:
 * bCompressionState:	It specifies the display compression state.  
 *  	      return:	'1' was returned on success otherwise 
 *			'0' was returned.
 *----------------------------------------------------------------------*/
BOOLEAN
Gal_set_compression_enable(BOOLEAN bCompressionState)
{
   GAL_COMPRESSIONSTATE sCompState;

   INIT_GAL(&sCompState);
   sCompState.dwSubfunction = GALFN_SETCOMPRESSIONSTATE;
   sCompState.bCompressionState = bCompressionState;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCompState))
      return 0;
   else
      return 1;
}

/*------------------------------------------------------------------------
 * Gal_get_compression_enable
 *
 * 	Description:	This function gets the compression state.
 *
 *	  parameters:
 * bCompressionState:	gets the display compression state.  
 *	      return:	'1' was returned on success otherwise 
 *			'0' was returned.
 *----------------------------------------------------------------------*/
BOOLEAN
Gal_get_compression_enable(int *bCompressionState)
{
   GAL_COMPRESSIONSTATE sCompState;

   INIT_GAL(&sCompState);
   sCompState.dwSubfunction = GALFN_GETCOMPRESSIONSTATE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCompState))
      return 0;
   else {
      *bCompressionState = sCompState.bCompressionState;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_compression_parameters
 *
 * Description:	This function sets the compression parameters of the
 * 			frame buffer device.
 *  parameters:
 *       flags:	It specifies the flag.
 *      offset:	It specifies the base offset in graphics memory for the 
 *			compression buffer.
 *	 pitch:	It specifies the pitch of compression buffer, in bytes.
 *        size:	It specifies the maximum line size of the compression buffer
 *			in bytes.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_compression_parameters(unsigned long flags,
			       unsigned long offset, unsigned short pitch,
			       unsigned short size)
{
   GAL_COMPRESSIONPARAMS sCompParams;

   INIT_GAL(&sCompParams);
   sCompParams.dwSubfunction = GALFN_SETCOMPRESSIONPARAMS;
   sCompParams.dwFlags = flags;
   sCompParams.dwCompOffset = offset;
   sCompParams.dwCompPitch = pitch;
   sCompParams.dwCompSize = size;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCompParams))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_compression_parameters
 *
 * Description:	This function gets the compression parameters of the
 *				frame buffer device.
 *  parameters:
 *       flags:	It specifies the flag.
 *      offset:	gets the base offset in graphics memory for the 
 *					compression buffer.
 *		 pitch:	gets the pitch of compression buffer, in bytes.
 *        size:	gets the maximum line size of the compression buffer
 *					in bytes.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_compression_parameters(unsigned long flags,
			       unsigned long *offset,
			       unsigned short *pitch, unsigned short *size)
{
   GAL_COMPRESSIONPARAMS sCompParams;

   INIT_GAL(&sCompParams);
   sCompParams.dwSubfunction = GALFN_GETCOMPRESSIONPARAMS;
   sCompParams.dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sCompParams))
      return 0;
   else {
      *offset = sCompParams.dwCompOffset;
      *pitch = sCompParams.dwCompPitch;
      *size = sCompParams.dwCompSize;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_vga_mode_switch
 *
 * Description:This function signals the beginning or end of a 
 *				mode switch.  
 *  parameters:
 *      active:	It specifies the mode switch.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_mode_switch(int active)
{
   GAL_VGAMODEDATA sVgaData;

   INIT_GAL(&sVgaData);
   sVgaData.dwSubfunction = GALFN_VGAMODESWITCH;
   sVgaData.dwFlags = active;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgaData))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_vga_clear_extended
 *
 * Description:	This will clear the Svga data.
 *  parameters:	none.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_clear_extended(void)
{
   GAL_VGAMODEDATA sVgaData;

   INIT_GAL(&sVgaData);
   sVgaData.dwSubfunction = GALFN_VGACLEARCRTEXT;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sVgaData))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_vga_pitch
 *
 * Description:	This function sets VGA register values in VGA 
 *					structure for specified pitch. 
 *  parameters:
 *    pVgaData: It specifies the vga structure.
 *		 pitch:	It specifies the number of bytes between scanlines.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_pitch(PGAL_VGAMODEDATA pVgaData, unsigned short pitch)
{
   INIT_GAL(pVgaData);
   pVgaData->dwSubfunction = GALFN_VGASETPITCH;
   pVgaData->dwFlags = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pVgaData))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_vga_restore
 *
 * Description:	This function sets the VGA state to the values in the
 *				VGA structure.  
 *  parameters:
 *    pVgaData: It specifies the vga structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_restore(PGAL_VGAMODEDATA pVgaData)
{
   INIT_GAL(pVgaData);
   pVgaData->dwSubfunction = GALFN_VGARESTORE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pVgaData))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_vga_save
 *
 * Description:	This function saves the current VGA state in the
 *					VGA structure.  
 *  parameters:
 *    pVgaData: It specifies the vga structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_save(PGAL_VGAMODEDATA pVgaData)
{
   INIT_GAL(pVgaData);
   pVgaData->dwSubfunction = GALFN_VGASAVE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pVgaData))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_vga_mode
 *
 * Description:	This function sets VGA register values in VGA 
 *					structure for specified mode. 
 *  parameters:
 *    pVgaData: It specifies the vga structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_vga_mode(PGAL_VGAMODEDATA pVgaData)
{
   INIT_GAL(pVgaData);
   pVgaData->dwSubfunction = GALFN_VGASETMODE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pVgaData))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_enabled_in_bios
 *
 * Description:	This function gets the status of the FP in BIOS.
 *  parameters:
 *       status: returns the state of FP in Bios.
 *	pParam:	It specifies the panel parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_enabled_in_bios(int *state)
{
   GAL_PNLBIOS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLBIOSENABLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      *state = pStat.state;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_info_from_bios
 *
 * Description:	This function gets the parameters of the FP in BIOS.
 *  parameters:
 *       status: returns the state of FP in Bios.
 *	pParam:	It specifies the panel parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_info_from_bios(int *xres, int *yres, int *bpp, int *hz)
{
   GAL_PNLBIOS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLBIOSINFO;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      *xres = pStat.XRes;
      *yres = pStat.YRes;
      *bpp = pStat.Bpp;
      *hz = pStat.Freq;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_set_params
 *
 * Description:	This function sets the panel parameters. 
 *  parameters:
 *       flags: 
 *	pParam:	It specifies the panel parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_set_params(unsigned long flags, PPnl_PanelParams pParam)
{
   GAL_PNLPARAMS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLSETPARAMS;
   pParam->Flags = flags;
   memcpy(&(pStat.PanelParams), pParam, sizeof(Pnl_PanelParams));

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_get_params
 *
 * Description:	This function gets the panel parameters. 
 *  parameters:
 *       flags:
 *	pParam:	It specifies the panel parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_get_params(unsigned long flags, PPnl_PanelParams pParam)
{
   GAL_PNLPARAMS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLGETPARAMS;
   memcpy(&(pStat.PanelParams), pParam, sizeof(Pnl_PanelParams));
   pStat.PanelParams.Flags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      memcpy(pParam, &(pStat.PanelParams), sizeof(Pnl_PanelParams));
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_init
 *
 * Description:	This function initializes the panel parameters. 
 *  parameters:
 *	pParam:	It specifies the panel parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_init(PPnl_PanelParams pParam)
{
   GAL_PNLPARAMS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLINITPANEL;
   memcpy(&(pStat.PanelParams), pParam, sizeof(Pnl_PanelParams));

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else
      return 1;

}

/*--------------------------------------------------------------------------
 * Gal_pnl_save
 *
 * Description:	This function saves the current panel parameters. 
 *  parameters:	none.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_save(void)
{
   GAL_PNLPARAMS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLSAVESTATE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_restore
 *
 * Description:	This function restores the current panel parameters. 
 *  parameters:	none.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_restore(void)
{
   GAL_PNLPARAMS pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLRESTORESTATE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_powerup
 *
 * Description:	This function powers up the panel. 
 *  parameters:	none.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_powerup(void)
{
   GAL_BASE pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLPOWERUP;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_pnl_powerdown
 *
 * Description:	This function powers down the panel. 
 *  parameters:	none.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_pnl_powerdown(void)
{
   GAL_BASE pStat;

   INIT_GAL(&pStat);
   pStat.dwSubfunction = GALFN_PNLPOWERDOWN;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pStat))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_enable_panning
 *
 * Description: This routine  enables the panning when the Mode
 *              is bigger than the panel size.
 *  parameters:
 *           x: x-positon of the screen.
 *           y: y-positon of the screen.
 *
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_enable_panning(int x, int y)
{
   GAL_ENABLEPANNING pEnablePanning;

   INIT_GAL(&pEnablePanning);
   pEnablePanning.dwSubfunction = GALFN_ENABLEPANNING;
   pEnablePanning.x = x;
   pEnablePanning.y = y;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pEnablePanning))
      return 0;
   else {
      return 1;
   }
}


/*--------------------------------------------------------------------------
 * Gal_tv_set_params
 *
 * Description:	This function sets the tv parameters of
 *					tvparameters structure. 
 *  parameters:
 *       flags:
 *	   pTV:	It specifies the tv parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_tv_set_params(unsigned long flags, PGAL_TVPARAMS pTV)
{
   INIT_GAL(pTV);
   pTV->dwSubfunction = GALFN_SETTVPARAMS;
   pTV->dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pTV))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_tv_get_params
 *
 * Description:	This function gets the tv parameters of
 *					tvparameters structure. 
 *  parameters:
 *       flags: Dummy flag
 *	   pTV:	It specifies the tv parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_tv_get_params(unsigned long flags, PGAL_TVPARAMS pTV)
{
   INIT_GAL(pTV);
   pTV->dwSubfunction = GALFN_GETTVPARAMS;
   pTV->dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pTV))
      return 0;
   else
      return 1;

}

/*--------------------------------------------------------------------------
 * Gal_tv_set_timings
 *
 * Description:	This function sets the tv timing registers.
 *  parameters:
 *       flags:	Dummy flag.
 *	   pTV:	It specifies the tv parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_tv_set_timings(unsigned long flags, PGAL_TVTIMING pTV)
{
   INIT_GAL(pTV);
   pTV->dwSubfunction = GALFN_SETTVTIMING;
   pTV->dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pTV))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_tv_get_timings
 *
 * Description:	This function gets the tv timing registers.
 *  parameters:
 *       flags:	Dummy flag.
 *	   pTV:	It specifies the tv parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_tv_get_timings(unsigned long flags, PGAL_TVTIMING pTV)
{
   INIT_GAL(pTV);
   pTV->dwSubfunction = GALFN_GETTVTIMING;
   pTV->dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pTV))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_tv_enable
 *
 * Description:	This function sets the tv state of the device .
 *  parameters: 
 *     bState : set the tv state.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_tv_enable(int bState)
{
   GAL_TVPARAMS pTV;

   INIT_GAL(&pTV);
   pTV.dwSubfunction = GALFN_SETENABLE;
   pTV.bState = bState;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pTV))
      return 0;
   else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_tv_enable
 *
 * Description:	This function gets the tv state of the device .
 *  parameters: 
 *     bState : get the tv state.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_tv_enable(unsigned int *bState)
{
   GAL_TVPARAMS pTV;

   INIT_GAL(&pTV);
   pTV.dwSubfunction = GALFN_GETENABLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &pTV)) {
      *bState = 0;
      return 0;
   } else {
      *bState = pTV.bState;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_is_tv_mode_supported
 *
 * Description:	This function checks the tv mode is supported or not.
 *  parameters:
 *       flags: Dummy flag
 *	   pTV:	It specifies the tv parameters structure.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_is_tv_mode_supported(unsigned long flags, PGAL_TVPARAMS pTV, int *bState)
{
   INIT_GAL(pTV);
   pTV->dwSubfunction = GALFN_ISTVMODESUPPORTED;
   pTV->dwFlags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, pTV)) {
      return 0;
   } else {
      *bState = pTV->bState;
      return 1;
   }
}

/** Video **********************************************************/

/*--------------------------------------------------------------------------
 * Gal_set_video_enable
 *
 * Description:	This function sets the video enable state.
 *  parameters:
 *      enable: Its value is '1' to enable video and '0' to disable video.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_enable(int enable)
{
   GAL_VIDEOENABLE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOENABLE;
   sSetVideo.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_format
 *
 * Description:	This function sets the video format.
 *  parameters:
 *      format: Its video format value.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_format(int format)
{
   GAL_VIDEOFORMAT sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOFORMAT;
   sSetVideo.format = format;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_size
 *
 * Description:	This function sets the video size.
 *  parameters:
 *       width: Width of the video.
 *      height: Height of the video.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_size(unsigned short width, unsigned short height)
{
   GAL_VIDEOSIZE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOSIZE;
   sSetVideo.width = width;
   sSetVideo.height = height;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_offset
 *
 * Description:	This function sets the video size.
 *  parameters:
 *      offset: Offset of the video.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_offset(unsigned long offset)
{
   GAL_VIDEOOFFSET sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOOFFSET;
   sSetVideo.offset = offset;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_window
 *
 * Description:	This function sets the video window.
 *  parameters:
 *			 x: X co-ordinate of the Video screen.
 *			 y: Y co-ordinate of the Video screen.
 *			 w: Width of the Video screen.
 *           h: Height of the Video screen.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_window(short x, short y, short w, short h)
{
   GAL_VIDEOWINDOW sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOWINDOW;
   sSetVideo.x = x;
   sSetVideo.y = y;
   sSetVideo.w = w;
   sSetVideo.h = h;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_scale
 *
 * Description:	This function sets the video scale.
 *  parameters:
 *		  srcw: Source width.
 *		  srch: Source height.
 *		  dstw: Destination width.
 *        dsth: Destination height.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_scale(unsigned short srcw, unsigned short srch,
		    unsigned short dstw, unsigned short dsth)
{
   GAL_VIDEOSCALE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOSCALE;
   sSetVideo.srcw = srcw;
   sSetVideo.srch = srch;
   sSetVideo.dstw = dstw;
   sSetVideo.dsth = dsth;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_filter.
 *
 * Description:	This function sets the video filter.
 *  parameters:
 *	   xfilter: X-co-ordinate filter.
 *	   yfilter: Y-co-ordinate filter.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_filter(int xfilter, int yfilter)
{
   GAL_VIDEOFILTER sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOFILTER;
   sSetVideo.xfilter = xfilter;
   sSetVideo.yfilter = yfilter;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_color_key.
 *
 * Description:	This function sets the video color key.
 *  parameters:
 *	       key: Color key.
 *	      mask: Color mask.
 *  bluescreen: Value for bluescreen.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_color_key(unsigned long key, unsigned long mask, int bluescreen)
{
   GAL_VIDEOCOLORKEY sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOCOLORKEY;
   sSetVideo.key = key;
   sSetVideo.mask = mask;
   sSetVideo.bluescreen = bluescreen;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_downscale_enable.
 *
 * Description:	This function sets the video downscale enable state.
 *  parameters:
 *	    enable: Value for enable or disable the video downscale.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_downscale_enable(int enable)
{
   GAL_VIDEODOWNSCALEENABLE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEODOWNSCALEENABLE;
   sSetVideo.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_downscale_config.
 *
 * Description:	This function sets the video downscale configuration.
 *  parameters:
 *	      type: Video down scale type.
 *			 m: Factor for the Video overlay window.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_downscale_config(unsigned short type, unsigned short m)
{
   GAL_VIDEODOWNSCALECONFIG sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEODOWNSCALECONFIG;
   sSetVideo.type = type;
   sSetVideo.m = m;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_downscale_coefficients.
 *
 * Description:	This function sets the video downscale coefficients.
 *  parameters:
 *	     coef1: Video downscale filter coefficient.
 *	     coef2: Video downscale filter coefficient.
 *	     coef3: Video downscale filter coefficient.
 *	     coef4: Video downscale filter coefficient.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_downscale_coefficients(unsigned short coef1,
				     unsigned short coef2,
				     unsigned short coef3,
				     unsigned short coef4)
{
   GAL_VIDEODOWNSCALECOEFF sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEODOWNSCALECOEFF;
   sSetVideo.coef1 = coef1;
   sSetVideo.coef2 = coef2;
   sSetVideo.coef3 = coef3;
   sSetVideo.coef4 = coef4;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_source.
 *
 * Description:	This function sets the video source to either memory or Direct
 *				VIP  
 *  parameters:
 *	    source: Video source.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_source(int source)
{
   GAL_VIDEOSOURCE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOSOURCE;
   sSetVideo.source = source;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_interlaced
 *
 * Description:	This function configures the Video processor video overlay mode 
 *				to be interlaced YUV.
 *  parameters:
 *	    enable: Value used to enable or disalbe the Video interlaced.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/

BOOLEAN
Gal_set_video_interlaced(int enable)
{
   GAL_SETVIDEOINTERLACED sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOINTERLACED;
   sSetVideo.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_color_space
 *
 * Description:	This function configures the Video processor to prcoess 
 *				graphics and video in either YUV or RGB color space. 
 *				
 *  parameters:
 *	    enable: Value used to enable or disalbe the Video color space.
 *      return:	'1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_color_space_YUV(int colorspace)
{
   GAL_COLORSPACEYUV sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOCOLORSPACE;
   sSetVideo.colorspace = colorspace;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_cursor.
 *
 *  Description: This function configures the Video Hardware cursor.
 *	     		 
 *				
 *   parameters:
 *          key: color key.
 *		   mask: color mask.
 *select_color2: selected for color2.
 *		 color1: color1 value.
 *		 color2: color2 value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_cursor(unsigned long key,
		     unsigned long mask,
		     unsigned short select_color2,
		     unsigned long color1, unsigned long color2)
{
   GAL_VIDEOCURSOR sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOCURSOR;
   sSetVideo.key = key;
   sSetVideo.mask = mask;
   sSetVideo.select_color2 = select_color2;
   sSetVideo.color1 = color1;
   sSetVideo.color2 = color2;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_request.
 *
 *  Description: This function sets the horizontal(pixel) and vertical(line) 
 *				 video request values.
 *	     		 
 *   parameters:
 *            x: X video request value.
 *		      y: Y video request value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_request(short x, short y)
{
   GAL_VIDEOREQUEST sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOREQUEST;
   sSetVideo.x = x;
   sSetVideo.y = y;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_enable.
 *
 *  Description: This function enables or disables the currently selected 
 *				 alpha region.
 *	     		 
 *   parameters:
 *       enable: Value to enalbe or disable alha region.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_alpha_enable(int enable)
{
   GAL_ALPHAENABLE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHAENABLE;
   sSetVideo.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_alpha_enable.
 *
 *  Description: This function gets the alpha enable state.
 *	     		 
 *   parameters:
 *       enable: Pointer to get the enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_alpha_enable(int *enable)
{
   GAL_ALPHAENABLE sGetalphaenable;

   INIT_GAL(&sGetalphaenable);
   sGetalphaenable.dwSubfunction = GALFN_GETALPHAENABLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetalphaenable))
      return 0;
   else

      *enable = sGetalphaenable.enable;
   return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_window
 *
 *  Description: This function sets the size of the currently selected 
 *				 alpha region.  		 
 *   parameters:
 *			  x: X co-ordinate of the alpha region.
 *			  y: Y co-ordinate of the alpha region.
 *	      width: Width of the alpha region.
 *		 height: Height of the alpha region.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_alpha_window(short x, short y,
		     unsigned short width, unsigned short height)
{
   GAL_ALPHAWINDOW sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHAWINDOW;
   sSetVideo.x = x;
   sSetVideo.y = y;
   sSetVideo.width = width;
   sSetVideo.height = height;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_alpha_size
 *
 *  Description: This function gets the size of the currently selected 
 *				 alpha region.  		 
 *   parameters:
 *			  x: X co-ordinate of the alpha region.
 *			  y: Y co-ordinate of the alpha region.
 *	      width: Width of the alpha region.
 *		 height: Height of the alpha region.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_alpha_size(unsigned short *x, unsigned short *y,
		   unsigned short *width, unsigned short *height)
{
   GAL_ALPHASIZE sGetalphasize;

   INIT_GAL(&sGetalphasize);
   sGetalphasize.dwSubfunction = GALFN_GETALPHASIZE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetalphasize))
      return 0;
   else {
      *x = *(sGetalphasize.x);
      *y = *(sGetalphasize.y);
      *width = *(sGetalphasize.width);
      *height = *(sGetalphasize.height);
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_value
 *
 *  Description: This function sets the alpha value for the selected alpha
 *				 region. It also specifies an increment/decrement value for
 *				 fading.
 *   parameters:
 *		  alpha: Alpha value for the currently selected alpha region.
 *		  delta: Gives the increment/decrement fading value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_alpha_value(unsigned char alpha, char delta)
{
   GAL_ALPHAVALUE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHAVALUE;
   sSetVideo.alpha = alpha;
   sSetVideo.delta = delta;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_alpha_value
 *
 *  Description: This function gets the alpha value for the selected alpha
 *				 region. It also gets increment/decrement value for
 *				 fading.
 *   parameters:
 *		  alpha: Alpha value for the currently selected alpha region.
 *		  delta: Gives the increment/decrement fading value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_alpha_value(unsigned char *alpha, char *delta)
{
   GAL_ALPHAVALUE sGetalphavalue;

   INIT_GAL(&sGetalphavalue);
   sGetalphavalue.dwSubfunction = GALFN_GETALPHAVALUE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetalphavalue))
      return 0;
   else {
      *alpha = sGetalphavalue.alpha;
      *delta = sGetalphavalue.delta;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_priority
 *
 *  Description: This function sets the priority of the selected alpha
 *				 region. 
 *   parameters:
 *	   priority: Gives the priority value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_alpha_priority(int priority)
{
   GAL_ALPHAPRIORITY sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHAPRIORITY;
   sSetVideo.priority = priority;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_alpha_priority
 *
 *  Description: This function gets the priority of the selected alpha
 *				 region. 
 *   parameters:
 *	   priority: Gives the priority value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_alpha_priority(int *priority)
{
   GAL_ALPHAPRIORITY sGetalphapriority;

   INIT_GAL(&sGetalphapriority);
   sGetalphapriority.dwSubfunction = GALFN_GETALPHAPRIORITY;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetalphapriority))
      return 0;
   else {
      *priority = sGetalphapriority.priority;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_color
 *
 *  Description: This function sets the color to be displayed inside the 
 *				 currently of the selected alpha window. 
 *   parameters:
 *	      color: Gives the color value to be displayed.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_alpha_color(unsigned long color)
{
   GAL_ALPHACOLOR sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHACOLOR;
   sSetVideo.color = color;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_get_alpha_color
 *
 *  Description: This function gets the color to be displayed inside the 
 *				 currently of the selected alpha window. 
 *   parameters:
 *	      color: Gives the color value to be displayed.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_alpha_color(unsigned long *color)
{
   GAL_ALPHACOLOR sGetalphacolor;

   INIT_GAL(&sGetalphacolor);
   sGetalphacolor.dwSubfunction = GALFN_GETALPHACOLOR;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetalphacolor))
      return 0;
   else {
      *color = sGetalphacolor.color;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_select_alpha_region
 *
 *  Description: This function selects the alpha region should be used for 
 *				 future updates. 
 *   parameters:
 *	     region: Gives the alpha window number.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_select_alpha_region(int region)
{
   GAL_ALPHAREGION sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETALPHAREGION;
   sSetVideo.region = region;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_outside_alpha
 *
 *  Description: This function enable/disable the video outside alpha region. 
 *   parameters:
 *	     enable: Gives the value for enable/disable.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_outside_alpha(int enable)
{
   GAL_VIDEOOUTSIDEALPHA sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOOUTSIDEALPHA;
   sSetVideo.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_video_palette
 *
 *  Description: This function loads the video hardware palette. 
 *   parameters:
 *	     palette: Gives value for hardware palette.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_palette(unsigned long *palette)
{
   GAL_VIDEOPALETTE sSetVideo;

   INIT_GAL(&sSetVideo);
   sSetVideo.dwSubfunction = GALFN_SETVIDEOPALETTE;

   if (palette == NULL) {
      sSetVideo.identity = 1;
   } else {
      sSetVideo.identity = 0;
      memcpy(sSetVideo.palette, palette, 256 * sizeof(*palette));
   }

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideo))
      return 0;
   else
      return 1;
}

/** Video **********************************************************/

/*--------------------------------------------------------------------------
 * Gal_set_icon_enable
 *
 *  Description: This function enable/disables the hardware icon. The icon 
 *				 position and colors should be programmed prior to calling 
 *				 this routine. 
 *   parameters:
 *	     enable: Gives value for enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_icon_enable(int enable)
{
   GAL_ICONENABLE sSetIconenable;

   INIT_GAL(&sSetIconenable);
   sSetIconenable.dwSubfunction = GALFN_SETICONENABLE;
   sSetIconenable.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetIconenable)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_icon_colors
 *
 *  Description: This function sets the three hardware icon colors.
 *   parameters:
 *	     color0: Gives first color value.
 *	     color1: Gives second color value.
 *	     color2: Gives third color value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_icon_colors(unsigned long color0, unsigned long color1,
		    unsigned long color2)
{
   GAL_ICONCOLORS sSetIconcolors;

   INIT_GAL(&sSetIconcolors);
   sSetIconcolors.dwSubfunction = GALFN_SETICONCOLORS;
   sSetIconcolors.color0 = color0;
   sSetIconcolors.color1 = color1;
   sSetIconcolors.color2 = color2;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetIconcolors)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_icon_position.
 *
 *  Description: This function sets the hardware icon position.
 *   parameters:
 *	  memoffset: Memory offset of the icon buffer.
 *	       xpos: Starting X co-ordinate for the hardware icon.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_icon_position(unsigned long memoffset, unsigned short xpos)
{
   GAL_ICONPOSITION sSetIconposi;

   INIT_GAL(&sSetIconposi);
   sSetIconposi.dwSubfunction = GALFN_SETICONPOSITION;
   sSetIconposi.memoffset = memoffset;
   sSetIconposi.xpos = xpos;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetIconposi)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_icon_shape64.
 *
 *  Description: This function initializes the icon buffer according to 
 *				  the current mode.
 *	 parameters:
 *	  memoffset: Memory offset of the icon buffer.
 *	    andmask: Andmask of the icon buffer.
 *	    xormask: Xormask of the icon buffer.
 *		  lines: Lines of the icon buffer.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_icon_shape64(unsigned long memoffset, unsigned long *andmask,
		     unsigned long *xormask, unsigned int lines)
{
   GAL_ICONSHAPE64 sSetIconshape64;

   INIT_GAL(&sSetIconshape64);
   sSetIconshape64.dwSubfunction = GALFN_SETICONSHAPE64;
   sSetIconshape64.memoffset = memoffset;
   *(sSetIconshape64.andmask) = *andmask;
   *(sSetIconshape64.xormask) = *xormask;
   sSetIconshape64.lines = lines;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetIconshape64)) {
      return 0;
   } else {
      return 1;
   }
}

/*  VIP Functions */

/*--------------------------------------------------------------------------
 * Gal_set_vip_enable
 *
 *  Description: This function enable/disables the writes to memory from the 
 *				 video port. 
 *				 position and colors should be programmed prior to calling 
 *				 this routine. 
 *   parameters:
 *	     enable: Gives value for enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_enable(int enable)
{
   GAL_VIPENABLE sSetVipenable;

   INIT_GAL(&sSetVipenable);
   sSetVipenable.dwSubfunction = GALFN_SETVIPENABLE;
   sSetVipenable.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipenable)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_enable
 *
 *  Description: This function gets the enable state of the 
 *				 video port. 
 *   parameters:
 *	     enable: Gives value for enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_enable(int *enable)
{
   GAL_VIPENABLE sGetVipenable;

   INIT_GAL(&sGetVipenable);
   sGetVipenable.dwSubfunction = GALFN_GETVIPENABLE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipenable)) {
      return 0;
   } else {

      *enable = sGetVipenable.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_capture_run_mode
 *
 *  Description: This function selects the VIP capture run mode.
 *
 *   parameters:
 *	       mode: VIP capture run mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_capture_run_mode(int mode)
{
   GAL_VIPCAPTURERUNMODE sSetVipcapturerunmode;

   INIT_GAL(&sSetVipcapturerunmode);
   sSetVipcapturerunmode.dwSubfunction = GALFN_SETVIPCAPTURERUNMODE;
   sSetVipcapturerunmode.mode = mode;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipcapturerunmode)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_base
 *
 *  Description: This routine sets the odd and even base address values for 
 *				 the VIP memory buffer.
 *   parameters:
 *	       even: Even base address.
 *	        odd: odd base address.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_base(unsigned long even, unsigned long odd)
{
   GAL_VIPBASE sSetVipBase;

   INIT_GAL(&sSetVipBase);
   sSetVipBase.dwSubfunction = GALFN_SETVIPBASE;
   sSetVipBase.even = even;
   sSetVipBase.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipBase)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_base
 *
 *  Description: This routine gets the  base address value for 
 *				 the VIP memory buffer.
 *   parameters:
 *	    address: VIP base address.
 *	        odd: odd base address.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_base(unsigned long *address, int odd)
{
   GAL_VIPBASE sGetVipBase;

   INIT_GAL(&sGetVipBase);
   sGetVipBase.dwSubfunction = GALFN_GETVIPBASE;
   sGetVipBase.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipBase)) {
      return 0;
   } else {
      *address = sGetVipBase.address;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_pitch
 *
 *  Description: This routine sets the  number of bytes between scanlines 
 *				 for the VIP data.
 *   parameters:
 *	      pitch: VIP pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_pitch(unsigned long pitch)
{
   GAL_VIPPITCH sSetVipPitch;

   INIT_GAL(&sSetVipPitch);
   sSetVipPitch.dwSubfunction = GALFN_SETVIPPITCH;
   sSetVipPitch.pitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipPitch)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_pitch
 *
 *  Description: This routine gets the  number of bytes between scanlines 
 *				 for the VIP data.
 *   parameters:
 *	      pitch: VIP pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_pitch(unsigned long *pitch)
{
   GAL_VIPPITCH sGetVipPitch;

   INIT_GAL(&sGetVipPitch);
   sGetVipPitch.dwSubfunction = GALFN_GETVIPPITCH;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipPitch)) {
      return 0;
   } else {
      *pitch = sGetVipPitch.pitch;
      return 1;

   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_mode
 *
 *  Description: This routine sets the  VIP operating mode. 
 *   parameters:
 *	       mode: VIP operating mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_mode(int mode)
{
   GAL_VIPMODE sSetVipMode;

   INIT_GAL(&sSetVipMode);
   sSetVipMode.dwSubfunction = GALFN_SETVIPMODE;
   sSetVipMode.mode = mode;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipMode)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_mode
 *
 *  Description: This routine gets the  VIP operating mode. 
 *   parameters:
 *	       mode: VIP operating mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_mode(int *mode)
{
   GAL_VIPMODE sGetVipMode;

   INIT_GAL(&sGetVipMode);
   sGetVipMode.dwSubfunction = GALFN_GETVIPMODE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipMode)) {
      return 0;
   } else {

      *mode = sGetVipMode.mode;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_bus_request_threshold_high
 *
 *  Description: This function sets the VIP FIFO bus request threshold.
 *	     		 
 *   parameters:
 *       enable: Enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_bus_request_threshold_high(int enable)
{
   GAL_VIPBUS_RTH sSetVipBRTH;

   INIT_GAL(&sSetVipBRTH);
   sSetVipBRTH.dwSubfunction = GALFN_SETVIPBRTH;
   sSetVipBRTH.enable = enable;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVipBRTH)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_bus_request_threshold_high
 *
 *  Description: This function gets the VIP FIFO bus request threshold.
 *	     		 
 *   parameters:
 *       enable: Enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_bus_request_threshold_high(int *enable)
{
   GAL_VIPBUS_RTH sGetVipBRTH;

   INIT_GAL(&sGetVipBRTH);
   sGetVipBRTH.dwSubfunction = GALFN_GETVIPBRTH;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipBRTH)) {
      return 0;
   } else {

      *enable = sGetVipBRTH.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vip_last_line
 *
 *  Description: This function sets the maximum number of lines captured 
 *				 in each field.
 *	     		 
 *   parameters:
 *    last_line: Maximum number of lines captured in each field.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vip_last_line(int last_line)
{
   GAL_VIPLASTLINE sSetViplastline;

   INIT_GAL(&sSetViplastline);
   sSetViplastline.dwSubfunction = GALFN_SETVIPLASTLINE;
   sSetViplastline.last_line = last_line;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetViplastline)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vip_line
 *
 *  Description: This function gets the number of the current video line being
 *				 recieved by the VIP interface.
 *	     		 
 *   parameters:
 *     vip_line: Number of the current video line.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vip_line(int *vip_line)
{
   GAL_VIPLINE sGetVipline;

   INIT_GAL(&sGetVipline);
   sGetVipline.dwSubfunction = GALFN_GETVIPLINE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVipline)) {
      return 0;
   } else {
      *vip_line = sGetVipline.status;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_test_vip_odd_field
 *
 *  Description: This function tests the VIP odd field.
 *	     		 
 *   parameters:
 *       status: Status of the odd field.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_test_vip_odd_field(int *status)
{
   GAL_TESTVIPODDFIELD sTestVipoddfield;

   INIT_GAL(&sTestVipoddfield);
   sTestVipoddfield.dwSubfunction = GALFN_TESTVIPODDFIELD;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sTestVipoddfield)) {
      return 0;
   } else {
      *status = sTestVipoddfield.status;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_test_vip_bases_updated
 *
 *  Description: This function tests the VIP bases updated.
 *	     		 
 *   parameters:
 *       status: Status of the VIP bases updated.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_test_vip_bases_updated(int *status)
{
   GAL_TESTVIPBASESUPDATED sTestVipbasesupdated;

   INIT_GAL(&sTestVipbasesupdated);
   sTestVipbasesupdated.dwSubfunction = GALFN_TESTVIPBASESUPDATED;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sTestVipbasesupdated)) {
      return 0;
   } else {
      *status = sTestVipbasesupdated.status;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_test_vip_fifo_overflow
 *
 *  Description: This function tests the VIP FIFO overflow.
 *	     		 
 *   parameters:
 *       status: Status of the VIP FIFO overflow.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_test_vip_fifo_overflow(int *status)
{
   GAL_TESTVIPOVERFLOW sTestVipoverflow;

   INIT_GAL(&sTestVipoverflow);
   sTestVipoverflow.dwSubfunction = GALFN_TESTVIPFIFOOVERFLOW;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sTestVipoverflow)) {
      return 0;
   } else {
      *status = sTestVipoverflow.status;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_enable
 *
 *  Description: This function enable/disables the VBI data capture.
 *	     		 
 *   parameters:
 *       enable: VBI enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_enable(int enable)
{
   GAL_VBIENABLE sSetVbienable;

   INIT_GAL(&sSetVbienable);
   sSetVbienable.dwSubfunction = GALFN_SETVBIENABLE;
   sSetVbienable.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbienable)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_enable
 *
 *  Description: This function gets the enable state of the VBI data capture.
 *	     		 
 *   parameters:
 *       enable: VBI enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_enable(int *enable)
{
   GAL_VBIENABLE sGetVbienable;

   INIT_GAL(&sGetVbienable);
   sGetVbienable.dwSubfunction = GALFN_GETVBIENABLE;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbienable)) {
      return 0;
   } else {

      *enable = sGetVbienable.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_base
 *
 *  Description: This function sets the VBI base addresses.
 *	     		 
 *   parameters:
 *         even: Even base address.
 *          odd: Odd base address.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_base(unsigned long even, unsigned long odd)
{
   GAL_VBIBASE sSetVbiBase;

   INIT_GAL(&sSetVbiBase);
   sSetVbiBase.dwSubfunction = GALFN_SETVBIBASE;
   sSetVbiBase.even = even;
   sSetVbiBase.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbiBase)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_base
 *
 *  Description: This function gets the VBI base address.
 *	     		 
 *   parameters:
 *      address: VBI base address.
 *          odd: Odd base address.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_base(unsigned long *address, int odd)
{
   GAL_VBIBASE sGetVbiBase;

   INIT_GAL(&sGetVbiBase);
   sGetVbiBase.dwSubfunction = GALFN_GETVBIBASE;
   sGetVbiBase.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbiBase)) {
      return 0;
   } else {
      *address = sGetVbiBase.address;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_pitch
 *
 *  Description: This function sets the number of bytes between scanlines for 
 *				 VBI capture.
 *	     		 
 *   parameters:
 *        pitch: VBI pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_pitch(unsigned long pitch)
{
   GAL_VBIPITCH sSetVbiPitch;

   INIT_GAL(&sSetVbiPitch);
   sSetVbiPitch.dwSubfunction = GALFN_SETVBIPITCH;
   sSetVbiPitch.pitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbiPitch)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_pitch
 *
 *  Description: This function gets the number of bytes between scanlines for 
 *				 VBI capture.
 *	     		 
 *   parameters:
 *        pitch: VBI pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_pitch(unsigned long *pitch)
{
   GAL_VBIPITCH sGetVbiPitch;

   INIT_GAL(&sGetVbiPitch);
   sGetVbiPitch.dwSubfunction = GALFN_GETVBIPITCH;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbiPitch)) {
      return 0;
   } else {
      *pitch = sGetVbiPitch.pitch;
      return 1;

   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_mode
 *
 *  Description: This function sets the VBI data types captured to memory. 
 *	     		 
 *   parameters:
 *         mode: VBI mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_mode(int mode)
{
   GAL_VBIMODE sSetVbiMode;

   INIT_GAL(&sSetVbiMode);
   sSetVbiMode.dwSubfunction = GALFN_SETVBIMODE;
   sSetVbiMode.mode = mode;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbiMode)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_mode
 *
 *  Description: This function gets the VBI data types captured to memory. 
 *	     		 
 *   parameters:
 *         mode: VBI mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_mode(int *mode)
{
   GAL_VBIMODE sGetVbiMode;

   INIT_GAL(&sGetVbiMode);
   sGetVbiMode.dwSubfunction = GALFN_GETVBIMODE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbiMode)) {
      return 0;
   } else {

      *mode = sGetVbiMode.mode;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_direct
 *
 *  Description: This function sets the VBI lines to be passed to the 
 *				 Direct VIP. 
 *	     		 
 *   parameters:
 *   even_lines: VBI even lines.
 *    odd_lines: VBI odd lines.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_direct(unsigned long even_lines, unsigned long odd_lines)
{
   GAL_SETVBIDIRECT sSetVbidirect;

   INIT_GAL(&sSetVbidirect);
   sSetVbidirect.dwSubfunction = GALFN_SETVBIDIRECT;
   sSetVbidirect.even_lines = even_lines;
   sSetVbidirect.odd_lines = odd_lines;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbidirect)) {
      return 0;
   } else {
      return 1;
   }
}
BOOLEAN
Gal2_set_destination_stride(unsigned short stride)
{
   GAL_STRIDE sSetStride;

   INIT_GAL(&sSetStride);
   sSetStride.dwSubfunction = GALFN_SETDESTINATIONSTRIDE;

   sSetStride.stride = stride;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetStride))
      return 0;
   else
      return 1;
}

BOOLEAN
Gal2_set_pattern_origin(int x, int y)
{
   GAL_PATTERNORIGIN sSetPatOrigin;

   INIT_GAL(&sSetPatOrigin);
   sSetPatOrigin.dwSubfunction = GALFN_SETPATTERNORIGIN;

   sSetPatOrigin.x = x;
   sSetPatOrigin.y = y;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetPatOrigin))
      return 0;
   else
      return 1;
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_direct
 *
 *  Description: This function gets the VBI lines to be passed to the 
 *				 Direct VIP. 
 *	     		 
 *   parameters:
 *          odd: VBI odd lines.
 * direct_lines: VBI direct lines.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_direct(int odd, unsigned long *direct_lines)
{
   GAL_GETVBIDIRECT sGetVbidirect;

   INIT_GAL(&sGetVbidirect);
   sGetVbidirect.dwSubfunction = GALFN_GETVBIDIRECT;
   sGetVbidirect.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbidirect)) {
      return 0;
   } else {
      *direct_lines = sGetVbidirect.direct_lines;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_interrupt
 *
 *  Description: This function enable/disables the VBI field interrupt.
 *	     		 
 *   parameters:
 *       enable: Value to enable/disable VBI interrupt.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_interrupt(int enable)
{
   GAL_VBIINTERRUPT sSetVbiinterrupt;

   INIT_GAL(&sSetVbiinterrupt);
   sSetVbiinterrupt.dwSubfunction = GALFN_SETVBIINTERRUPT;
   sSetVbiinterrupt.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbiinterrupt)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_interrupt
 *
 *  Description: This function gets the VBI field interrupt.
 *	     		 
 *   parameters:
 *       enable: Value of enable/disable VBI interrupt.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_interrupt(int *enable)
{
   GAL_VBIINTERRUPT sGetVbiinterrupt;

   INIT_GAL(&sGetVbiinterrupt);
   sGetVbiinterrupt.dwSubfunction = GALFN_GETVBIINTERRUPT;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbiinterrupt)) {
      return 0;
   } else {
      *enable = sGetVbiinterrupt.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_source_stride
 *
 *  Description: This function sets the stride to be used in successive screen
 *				 to screen BLTs.
 *	     		 
 *   parameters:
 *       enable: Value of enable/disable VBI interrupt.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_set_source_stride(unsigned short stride)
{
   GAL_STRIDE sSetsourcestride;

   INIT_GAL(&sSetsourcestride);
   sSetsourcestride.dwSubfunction = GALFN_SETSOURCESTRIDE;

   sSetsourcestride.stride = stride;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetsourcestride)) {
      return 0;
   } else {

      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_source_transparency
 *
 *  Description: This function sets the source transparency color and 
 *				 mask to be used in future rendering routines.
 *				 to screen BLTs.
 *	     		 
 *   parameters:
 *        color: Source color.
 *		   mask: Source mask.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_set_source_transparency(unsigned long color, unsigned long mask)
{
   GAL_SOURCETRANSPARENCY sSetsourcetransparency;

   INIT_GAL(&sSetsourcetransparency);
   sSetsourcetransparency.dwSubfunction = GALFN_SETSOURCETRANSPARENCY;

   sSetsourcetransparency.color = color;
   sSetsourcetransparency.mask = mask;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetsourcetransparency)) {
      return 0;
   } else {

      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_alpha_mode
 *
 *  Description: This function sets the alpha blending mode. 
 *   parameters:
 *         mode: Alpha blending mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_set_alpha_mode(int mode)
{
   GAL_GFX2ALPHAMODE sSetalphamode;

   INIT_GAL(&sSetalphamode);
   sSetalphamode.dwSubfunction = GALFN_GFX2SETALPHAMODE;

   sSetalphamode.mode = mode;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetalphamode)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_gfx2_set_alpha_value
 *
 *  Description: This function sets the alpha value to be used with certain 
 *				 alpha blending modes. 
 *   parameters:
 *        value: Alpha blending value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_set_alpha_value(unsigned char value)
{
   GAL_GFX2ALPHAVALUE sSetalphavalue;

   INIT_GAL(&sSetalphavalue);
   sSetalphavalue.dwSubfunction = GALFN_GFX2SETALPHAVALUE;

   sSetalphavalue.value = value;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetalphavalue)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_gfx2_pattern_fill
 *
 *  Description: This function used to fill the pattern of GX2. 
 *				 It allows the arbitary destination stride. The rendering 
 *				 position is also specified as an offset instead of (x,y) 
 *				 position.
 *   parameters:
 *    dstoffset: Rendering offset.
 *		  width: Width of the pattern.
 *       height: Height of the pattern.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_gfx2_pattern_fill(unsigned long dstoffset, unsigned short width,
		      unsigned short height)
{
   GAL_GFX2PATTERNFILL sPatternfill;

   INIT_GAL(&sPatternfill);
   sPatternfill.dwSubfunction = GALFN_GFX2PATTERNFILL;

   sPatternfill.dstoffset = dstoffset;
   sPatternfill.width = width;
   sPatternfill.height = height;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sPatternfill)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_gfx2_screen_to_screen_blt
 *
 *  Description: This function used for screen to screen BLTs of GX2. 
 *				 It allows the arbitary source and destination strides and 
 *				 alpha blending. 
 *   parameters:
 *    srcoffset: Source Rendering offset.
 *    dstoffset: Destination Rendering offset.
 *		  width: Width of the screen.
 *       height: Height of the screen.
 *		  flags: Flags of the screen to screen BLT.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_screen_to_screen_blt(unsigned long srcoffset,
			  unsigned long dstoffset, unsigned short width,
			  unsigned short height, int flags)
{
   GAL_GFX2SCREENTOSCREENBLT sScreentoScreenblt;

   INIT_GAL(&sScreentoScreenblt);
   sScreentoScreenblt.dwSubfunction = GALFN_GFX2SCREENTOSCREENBLT;

   sScreentoScreenblt.srcoffset = srcoffset;
   sScreentoScreenblt.dstoffset = dstoffset;
   sScreentoScreenblt.width = width;
   sScreentoScreenblt.height = height;
   sScreentoScreenblt.flags = flags;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sScreentoScreenblt)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal2_mono_expand_blt
 *
 *  Description: This function used to expand monochrome data stored in 
 *				 graphics memory for screen to screen BLTs. 
 *   parameters:
 *      srcbase: Source Rendering base address.
 *         srcx: Source X offset.
 *         srcy: Source Y offset.
 *    dstoffset: Destination Rendering offset.
 *		  width: Width of the screen.
 *       height: Height of the screen.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_mono_expand_blt(unsigned long srcbase, unsigned short srcx,
		     unsigned short srcy, unsigned long dstoffset,
		     unsigned short width, unsigned short height,
		     int byte_packed)
{
   GAL_GFX2MONOEXPANDBLT sMonoexpandblt;

   INIT_GAL(&sMonoexpandblt);
   sMonoexpandblt.dwSubfunction = GALFN_GFX2MONOEXPANDBLT;
   sMonoexpandblt.srcbase = srcbase;
   sMonoexpandblt.srcx = srcx;
   sMonoexpandblt.srcy = srcy;
   sMonoexpandblt.dstoffset = dstoffset;
   sMonoexpandblt.width = width;
   sMonoexpandblt.height = height;
   sMonoexpandblt.byte_packed = byte_packed;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sMonoexpandblt)) {
      return 0;
   } else {
      return 1;

   }
}

/*--------------------------------------------------------------------------
 * Gal2_color_bitmap_to_screen_blt
 *
 *  Description: This function used for color bmp to screen BLTs. 
 *   parameters:
 *         srcx: Source X offset.
 *         srcy: Source Y offset.
 *    dstoffset: Destination Rendering offset.
 *		  width: Width of the screen.
 *       height: Height of the screen.
 *		  *data: Color bmp data.
 *	      pitch: Pitch of the dispaly mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_color_bitmap_to_screen_blt(unsigned short srcx,
				unsigned short srcy,
				unsigned long dstoffset,
				unsigned short width,
				unsigned short height,
				unsigned char *data, unsigned short pitch)
{
   GAL_GFX2COLORBMPTOSCRBLT sColorbmptoscrblt;

   INIT_GAL(&sColorbmptoscrblt);
   sColorbmptoscrblt.dwSubfunction = GALFN_GFX2COLORBMPTOSCRBLT;
   sColorbmptoscrblt.srcx = srcx;
   sColorbmptoscrblt.srcy = srcy;
   sColorbmptoscrblt.dstoffset = dstoffset;
   sColorbmptoscrblt.width = width;
   sColorbmptoscrblt.height = height;
   sColorbmptoscrblt.data = *data;
   sColorbmptoscrblt.pitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sColorbmptoscrblt)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal2_mono_bitmap_to_screen_blt
 *
 *  Description: This function used for mono bmp to screen BLTs. 
 *   parameters:
 *         srcx: Source X offset.
 *         srcy: Source Y offset.
 *    dstoffset: Destination Rendering offset.
 *		  width: Width of the screen.
 *       height: Height of the screen.
 *		  *data: mono bmp data.
 *	      pitch: Pitch of the display mode.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_mono_bitmap_to_screen_blt(unsigned short srcx,
			       unsigned short srcy,
			       unsigned long dstoffset,
			       unsigned short width,
			       unsigned short height,
			       unsigned char *data, unsigned short pitch)
{
   GAL_GFX2MONOBMPTOSCRBLT sMonobmptoscrblt;

   INIT_GAL(&sMonobmptoscrblt);
   sMonobmptoscrblt.dwSubfunction = GALFN_GFX2MONOBMPTOSCRBLT;
   sMonobmptoscrblt.srcx = srcx;
   sMonobmptoscrblt.srcy = srcy;
   sMonobmptoscrblt.dstoffset = dstoffset;
   sMonobmptoscrblt.width = width;
   sMonobmptoscrblt.height = height;
   sMonobmptoscrblt.data = *data;
   sMonobmptoscrblt.pitch = pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sMonobmptoscrblt)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal2_bresenham_line
 *
 *  Description: This function used to draw bresenham line. It allows the 
 *				 arbitary destination stride. 
 *   parameters:
 *    dstoffset: Destination  offset.
 *		 length: Length of the line.
 *      initerr: Intial error.
 *	   axialerr: 
 *	    diagerr: 
 *		  flags:
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_bresenham_line(unsigned long dstoffset, unsigned short length,
		    unsigned short initerr, unsigned short axialerr,
		    unsigned short diagerr, unsigned short flags)
{
   GAL_GFX2BRESENHAMLINE sBresenhamline;

   INIT_GAL(&sBresenhamline);
   sBresenhamline.dwSubfunction = GALFN_GFX2BRESENHAMLINE;
   sBresenhamline.dstoffset = dstoffset;
   sBresenhamline.length = length;
   sBresenhamline.initerr = initerr;
   sBresenhamline.axialerr = axialerr;
   sBresenhamline.diagerr = diagerr;
   sBresenhamline.flags = flags;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sBresenhamline)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal2_sync_to_vblank
 *
 *  Description: This function sets the a flag to synchronize the next 
 *				 rendering routine to VBLANK. 
 *   parameters: none.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal2_sync_to_vblank(void)
{
   GAL_GFX2SYNCTOVBLANK sSynctovblank;

   INIT_GAL(&sSynctovblank);
   sSynctovblank.dwSubfunction = GALFN_GFX2SYNCTOVBLANK;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSynctovblank)) {
      return 0;
   } else {
      return 1;
   }
}

/* Video routines */

/*--------------------------------------------------------------------------
 * Gal_set_video_yuv_pitch
 *
 *  Description: This function sets the Video YUV pitch. 
 *				 
 *   parameters:
 *       y_pitch: Y pitch.
 *	    uv_pitch: UV pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_yuv_pitch(unsigned long y_pitch, unsigned long uv_pitch)
{
   GAL_VIDEOYUVPITCH sSetVideoyuvpitch;

   INIT_GAL(&sSetVideoyuvpitch);
   sSetVideoyuvpitch.dwSubfunction = GALFN_SETVIDEOYUVPITCH;
   sSetVideoyuvpitch.y_pitch = y_pitch;
   sSetVideoyuvpitch.uv_pitch = uv_pitch;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideoyuvpitch)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_video_yuv_pitch
 *
 *  Description: This function gets the Video YUV pitch. 
 *				 
 *   parameters:
 *       y_pitch: Y pitch.
 *	    uv_pitch: UV pitch.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_video_yuv_pitch(unsigned long *y_pitch, unsigned long *uv_pitch)
{
   GAL_VIDEOYUVPITCH sGetVideoyuvpitch;

   INIT_GAL(&sGetVideoyuvpitch);
   sGetVideoyuvpitch.dwSubfunction = GALFN_GETVIDEOYUVPITCH;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVideoyuvpitch)) {
      return 0;
   } else {
      *y_pitch = sGetVideoyuvpitch.y_pitch;
      *uv_pitch = sGetVideoyuvpitch.uv_pitch;

      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_video_yuv_offsets
 *
 *  Description: This function sets the Video YUV offsets. 
 *				 
 *   parameters:
 *      y_offset: Y offset.
 *		u_offset: U offset.
 *		v_offset: V offset.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_video_yuv_offsets(unsigned long y_offset, unsigned long u_offset,
			  unsigned long v_offset)
{
   GAL_VIDEOYUVOFFSETS sSetVideoyuvoffsets;

   INIT_GAL(&sSetVideoyuvoffsets);
   sSetVideoyuvoffsets.dwSubfunction = GALFN_SETVIDEOYUVOFFSETS;
   sSetVideoyuvoffsets.dwYoffset = y_offset;
   sSetVideoyuvoffsets.dwUoffset = u_offset;
   sSetVideoyuvoffsets.dwVoffset = v_offset;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideoyuvoffsets)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_video_yuv_offsets
 *
 *  Description: This function gets the Video YUV offsets. 
 *				 
 *   parameters:
 *      y_offset: Y offset.
 *		u_offset: U offset.
 *		v_offset: V offset.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_get_video_yuv_offsets(unsigned long *y_offset,
			  unsigned long *u_offset, unsigned long *v_offset)
{
   GAL_VIDEOYUVOFFSETS sGetVideoyuvoffsets;

   INIT_GAL(&sGetVideoyuvoffsets);
   sGetVideoyuvoffsets.dwSubfunction = GALFN_GETVIDEOYUVOFFSETS;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVideoyuvoffsets)) {
      return 0;
   } else {
      *y_offset = sGetVideoyuvoffsets.dwYoffset;
      *u_offset = sGetVideoyuvoffsets.dwUoffset;
      *v_offset = sGetVideoyuvoffsets.dwVoffset;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_video_left_crop
 *
 *  Description: This function sets the number of pixels which will be cropped
 *				 from the beginning of each video line. 
 *				 
 *   parameters:
 *			  x: 
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_set_video_left_crop(unsigned short x)
{
   GAL_VIDEOLEFTCROP sSetVideoleftcrop;;

   INIT_GAL(&sSetVideoleftcrop);
   sSetVideoleftcrop.dwSubfunction = GALFN_SETVIDEOLEFTCROP;
   sSetVideoleftcrop.x = x;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideoleftcrop)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_video_vertical_downscale
 *
 *  Description: This function sets the vertical downscale factor for the video 
 *				 overlay window.	
 *				 
 *   parameters:
 *		   srch: Height of the source.
 *		   dsth: Height of the destination.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_set_video_vertical_downscale(unsigned short srch, unsigned short dsth)
{
   GAL_VIDEOVERTICALDOWNSCALE sSetVideoverticaldownscale;

   INIT_GAL(&sSetVideoverticaldownscale);
   sSetVideoverticaldownscale.dwSubfunction = GALFN_SETVIDEOVERTICALDOWNSCALE;
   sSetVideoverticaldownscale.srch = srch;
   sSetVideoverticaldownscale.dsth = dsth;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVideoverticaldownscale)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_source
 *
 *  Description: This function sets the VBI source.	
 *				 
 *   parameters:
 *		 source: VBI Source type.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_set_vbi_source(VbiSourceType source)
{
   GAL_VBISOURCE sSetVbisource;

   INIT_GAL(&sSetVbisource);
   sSetVbisource.dwSubfunction = GALFN_SETVBISOURCE;
   sSetVbisource.source = source;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbisource)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_source
 *
 *  Description: This function gets the VBI source.	
 *				 
 *   parameters:
 *		 source: VBI Source type.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_vbi_source(VbiSourceType * source)
{
   GAL_VBISOURCE sGetVbisource;

   INIT_GAL(&sGetVbisource);
   sGetVbisource.dwSubfunction = GALFN_GETVBISOURCE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbisource)) {
      return 0;
   } else {

      *source = sGetVbisource.source;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_lines
 *
 *  Description: This function sets the VBI lines.	
 *				 
 *   parameters:
 *		   even: VBI even lines.
 *			odd: VBI odd lines.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_lines(unsigned long even, unsigned long odd)
{
   GAL_VBILINES sSetVbilines;

   INIT_GAL(&sSetVbilines);
   sSetVbilines.dwSubfunction = GALFN_SETVBILINES;
   sSetVbilines.even = even;
   sSetVbilines.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbilines)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vbi_lines
 *
 *  Description: This function gets the VBI lines.	
 *				 
 *   parameters:
 *	      lines: VBI lines.
 *			odd: VBI odd lines.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_get_vbi_lines(int odd, unsigned long *lines)
{
   GAL_VBILINES sGetVbilines;

   INIT_GAL(&sGetVbilines);
   sGetVbilines.dwSubfunction = GALFN_GETVBILINES;
   sGetVbilines.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbilines)) {
      return 0;
   } else {
      *lines = sGetVbilines.lines;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_total
 *
 *  Description: This function sets the total number of VBI bytes for each 
 *				 field.	
 *				 
 *   parameters:
 *	       even:
 *			odd: 
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_vbi_total(unsigned long even, unsigned long odd)
{
   GAL_VBITOTAL sSetVbitotal;

   INIT_GAL(&sSetVbitotal);
   sSetVbitotal.dwSubfunction = GALFN_SETVBITOTAL;
   sSetVbitotal.even = even;
   sSetVbitotal.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVbitotal)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vbi_total
 *
 *  Description: This function gets the total number of VBI bytes in the
 *				 field.	
 *				 
 *   parameters:
 *	       even:
 *			odd: 
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_get_vbi_total(int odd, unsigned long *total)
{
   GAL_VBITOTAL sGetVbitotal;

   INIT_GAL(&sGetVbitotal);
   sGetVbitotal.dwSubfunction = GALFN_GETVBITOTAL;
   sGetVbitotal.odd = odd;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVbitotal)) {
      return 0;
   } else {
      *total = sGetVbitotal.total;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_vertical_scaler_offset
 *
 *  Description: This function sets the Video vertical scaler offset.	
 *				 
 *   parameters:
 *	     offset: Vertical Scaler offset.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_set_vertical_scaler_offset(char offset)
{
   GAL_VSCALEROFFSET sSetVscaleroffset;

   INIT_GAL(&sSetVscaleroffset);
   sSetVscaleroffset.dwSubfunction = GALFN_SETVSCALEROFFSET;
   sSetVscaleroffset.offset = offset;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetVscaleroffset)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_vertical_scaler_offset
 *
 *  Description: This function gets the Video vertical scaler offset.	
 *				 
 *   parameters:
 *	     offset: Vertical Scaler offset.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/ BOOLEAN
Gal_get_vertical_scaler_offset(char *offset)
{
   GAL_VSCALEROFFSET sGetVscaleroffset;

   INIT_GAL(&sGetVscaleroffset);
   sGetVscaleroffset.dwSubfunction = GALFN_GETVSCALEROFFSET;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetVscaleroffset)) {
      return 0;
   } else {

      *offset = sGetVscaleroffset.offset;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_video_interlaced
 *
 *  Description: This function gets the video interlaced mode.
 *   parameters:
 *   interlaced: ptr to the interlaced status.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_video_interlaced(int *interlaced)
{
   GAL_GETVIDEOINTERLACED sGetvideointerlaced;

   INIT_GAL(&sGetvideointerlaced);
   sGetvideointerlaced.dwSubfunction = GALFN_GETVIDEOINTERLACED;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetvideointerlaced)) {
      return 0;
   } else {
      *interlaced = sGetvideointerlaced.interlaced;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_color_space_YUV
 *
 *  Description: This function gets the video color space YUV.
 *   parameters:
 *   colorspace: ptr to the color space.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_color_space_YUV(int *colorspace)
{
   GAL_COLORSPACEYUV sGetcolorspaceyuv;

   INIT_GAL(&sGetcolorspaceyuv);
   sGetcolorspaceyuv.dwSubfunction = GALFN_GETCOLORSPACEYUV;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetcolorspaceyuv)) {
      return 0;
   } else {
      *colorspace = sGetcolorspaceyuv.colorspace;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_genlock_enable
 *
 *  Description: This function gets the enable state of the genlock.
 *   parameters:
 *       enable: ptr to the enable state of the genlock.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_genlock_enable(int *enable)
{
   GAL_GENLOCKENABLE sGetgenlockenable;

   INIT_GAL(&sGetgenlockenable);
   sGetgenlockenable.dwSubfunction = GALFN_GETGENLOCKENABLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetgenlockenable)) {
      return 0;
   } else {
      *enable = sGetgenlockenable.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_genlock_enable
 *
 *  Description: This function enable/disables and configure the genlock
 *				 according to the parameters.
 *   parameters:
 *       enable:  enable state of the genlock.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_genlock_enable(int enable)
{
   GAL_GENLOCKENABLE sSetgenlockenable;

   INIT_GAL(&sSetgenlockenable);
   sSetgenlockenable.dwSubfunction = GALFN_SETGENLOCKENABLE;

   sSetgenlockenable.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetgenlockenable)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_genlock_delay
 *
 *  Description: This function gets the genlock delay. 
 *   parameters:
 *        delay:  Ptr to the genlock delay.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_genlock_delay(unsigned long *delay)
{
   GAL_GENLOCKDELAY sGetgenlockdelay;

   INIT_GAL(&sGetgenlockdelay);
   sGetgenlockdelay.dwSubfunction = GALFN_GETGENLOCKDELAY;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetgenlockdelay)) {
      return 0;
   } else {
      *delay = sGetgenlockdelay.delay;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_genlock_delay
 *
 *  Description: This function sets the genlock delay. 
 *   parameters:
 *        delay:  genlock delay.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_genlock_delay(unsigned long delay)
{
   GAL_GENLOCKDELAY sSetgenlockdelay;

   INIT_GAL(&sSetgenlockdelay);
   sSetgenlockdelay.dwSubfunction = GALFN_SETGENLOCKDELAY;

   sSetgenlockdelay.delay = delay;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetgenlockdelay)) {
      return 0;
   } else {
      return 1;
   }
}

BOOLEAN
Gal_set_top_line_in_odd(int enable)
{
   GAL_TOPLINEINODD sSettoplineinodd;

   INIT_GAL(&sSettoplineinodd);
   sSettoplineinodd.dwSubfunction = GALFN_SETTOPLINEINODD;

   sSettoplineinodd.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSettoplineinodd)) {
      return 0;
   } else {
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_video_cursor.
 *
 *  Description: This function gets configuration of the Video Hardware
 *				 cursor.
 *   parameters:
 *          key: color key.
 *		   mask: color mask.
 *select_color2: selected for color2.
 *		 color1: color1 value.
 *		 color2: color2 value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_video_cursor(unsigned long *key,
		     unsigned long *mask,
		     unsigned short *select_color2,
		     unsigned long *color1, unsigned long *color2)
{
   GAL_VIDEOCURSOR sGetvideocursor;

   INIT_GAL(&sGetvideocursor);
   sGetvideocursor.dwSubfunction = GALFN_GETVIDEOCURSOR;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetvideocursor)) {
      return 0;
   } else {
      *key = sGetvideocursor.key;
      *mask = sGetvideocursor.mask;
      *select_color2 = sGetvideocursor.select_color2;
      *color1 = sGetvideocursor.color1;
      *color2 = sGetvideocursor.color2;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_read_crc.
 *
 *  Description: This function reads the hardware CRC value, which is used for
 *				 automated testing.
 *   parameters:
 *          crc: Holds the crc value.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_read_crc(unsigned long *crc)
{
   GAL_READCRC sReadcrc;

   INIT_GAL(&sReadcrc);
   sReadcrc.dwSubfunction = GALFN_READCRC;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sReadcrc)) {
      return 0;
   } else {
      *crc = sReadcrc.crc;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_read_window_crc.
 *
 *  Description: This function reads the hardware CRC value for a subsection
 *				 of the display.
 *				 
 *   parameters:
 *       source: 
 *			  x:
 *			  y:
 *	      width:
 *		 height:
 *			crc:
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_read_window_crc(int source, unsigned short x, unsigned short y,
		    unsigned short width, unsigned short height,
		    int crc32, unsigned long *crc)
{
   GAL_READWINDOWCRC sReadwindowcrc;

   INIT_GAL(&sReadwindowcrc);
   sReadwindowcrc.dwSubfunction = GALFN_READWINDOWCRC;
   sReadwindowcrc.source = source;
   sReadwindowcrc.x = x;
   sReadwindowcrc.y = y;
   sReadwindowcrc.width = width;
   sReadwindowcrc.height = height;
   sReadwindowcrc.crc32 = crc32;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sReadwindowcrc)) {
      return 0;
   } else {
      *crc = sReadwindowcrc.crc;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_get_macrovision_enable.
 *
 *  Description: This function gets the enable state of the macrovision.
 *				 
 *   parameters:
 *       enable: ptr holds the macrovision enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_get_macrovision_enable(int *enable)
{
   GAL_MACROVISIONENABLE sGetmacrovisionenable;

   INIT_GAL(&sGetmacrovisionenable);
   sGetmacrovisionenable.dwSubfunction = GALFN_GETMACROVISIONENABLE;

   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sGetmacrovisionenable)) {
      return 0;
   } else {
      *enable = sGetmacrovisionenable.enable;
      return 1;
   }
}

/*--------------------------------------------------------------------------
 * Gal_set_macrovision_enable.
 *
 *  Description: This function gets the enable state of the macrovision.
 *				 
 *   parameters:
 *       enable: macrovision enable state.
 *       return: '1' was returned on success otherwise '0' was returned.
 *------------------------------------------------------------------------*/
BOOLEAN
Gal_set_macrovision_enable(int enable)
{
   GAL_MACROVISIONENABLE sSetmacrovisionenable;

   INIT_GAL(&sSetmacrovisionenable);
   sSetmacrovisionenable.dwSubfunction = GALFN_SETMACROVISIONENABLE;

   sSetmacrovisionenable.enable = enable;
   if (ioctl(dfb_fbdev->fd, FBIOGAL_API, &sSetmacrovisionenable)) {
      return 0;
   } else {
      return 1;
   }
}
