/*
 * HI OPP table definitions.
 *
 * Copyright (C) 2010 Texas Instruments Incorporated - http://www.ti.com/
 *	Nishanth Menon
 *	Kevin Hilman
 *	Thara Gopinath
 * Copyright (C) 2010-2011 Nokia Corporation.
 *      Eduardo Valentin
 *      Paul Walmsley
 * Copyright (C) 2012-2013 Hisilicon Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/module.h>
#include <linux/device.h>
#include "hi_opp_data.h"
#include "hi_dvfs.h"
//#include "hi_drv_pmoc.h"

/*
 * Structures containing HI voltage supported and various
 * voltage dependent data for each VDD.
 */
#define HI_VDD_MPU_OPP0_UV 1100 
#define HI_VDD_MPU_OPP1_UV 1200
#define HI_VDD_MPU_OPP2_UV 1200
#define HI_VDD_MPU_OPP3_UV 1250
#define HI_VDD_MPU_OPP4_UV 1315
#define HI_VDD_MPU_OPP5_UV 1315

/*
profile0    1.2GHz  1.35V    BPLL 1.2GHz��
profile1    1GHz    1.30V   WPLL��
profile2    750MHz  1.20V   APLL 750MHz��
profile3    600MHz  1.15V    BPLL 1.2GHz DIV2��;
profile4    400MHz  1.10V   400MHz��֧��
*/

static struct hi_opp_def __initdata hi_opp_def_list[] = {
    OPP_INITIALIZER(true,  24000, HI_VDD_MPU_OPP0_UV),
    OPP_INITIALIZER(true,  400000, HI_VDD_MPU_OPP1_UV),
    OPP_INITIALIZER(true,  600000, HI_VDD_MPU_OPP2_UV),
    OPP_INITIALIZER(true,  800000, HI_VDD_MPU_OPP3_UV),
    OPP_INITIALIZER(true, 1000000, HI_VDD_MPU_OPP4_UV),
    OPP_INITIALIZER(true, 1200000, HI_VDD_MPU_OPP5_UV),
};

/**
 * hi_opp_init() - initialize hi opp table
 */
int __init hi_opp_init(void)
{
    int r = -ENODEV;

    HI_TRACE("enter %s\n", __FUNCTION__);

    r = hi_init_opp_table(hi_opp_def_list,
                          ARRAY_SIZE(hi_opp_def_list));

    return r;
}

#if 0
#ifndef MODULE
device_initcall(hi_opp_init);
#endif
#endif
