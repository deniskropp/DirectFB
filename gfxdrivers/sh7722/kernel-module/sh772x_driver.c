/*
 * SH7722/SH7723 Graphics Device
 *
 * Copyright (C) 2006-2008  IGEL Co.,Ltd
 *
 * Written by Denis Oliver Kropp <dok@directfb.org>
 *
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License v2
 * as published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/version.h>

#include <asm/io.h>
#include <asm/processor.h>

#include "sh7722.h"
#include "sh7723.h"


/**********************************************************************************************************************/

static int sh772x_init = 0;

/**********************************************************************************************************************/

static int __init
sh772x_driver_init( void )
{
     int ret = -ENODEV;

     if ((ctrl_inl(CCN_PVR) & 0xffff00) == 0x300800) {
          switch (ctrl_inl(CCN_PRR) & 0xff0) {
               case 0xa00:
                    ret = sh7722_init();
                    if (ret)
                         return ret;

                    sh772x_init = 7722;
                    break;

               case 0x500:
                    ret = sh7723_init();
                    if (ret)
                         return ret;

                    sh772x_init = 7723;
                    break;
          }
     }

     return ret;
}

module_init( sh772x_driver_init );

/**********************************************************************************************************************/

static void __exit
sh772x_driver_exit( void )
{
     switch (sh772x_init) {
          case 7722:
               sh7722_exit();
               break;

          case 7723:
               sh7723_exit();
               break;
     }
}

module_exit( sh772x_driver_exit );

/**********************************************************************************************************************/

MODULE_AUTHOR( "Denis Oliver Kropp <dok@directfb.org> & Janine Kropp <nin@directfb.org>" );
MODULE_LICENSE( "GPL v2" );

