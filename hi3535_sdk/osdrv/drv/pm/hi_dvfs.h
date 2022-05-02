/*
 * hisilicon Management Routines
 *
 * Copyright (C) 2012 Hisilicon Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __ARCH_ARM_MACH_HI_DVFS_H
#define __ARCH_ARM_MACH_HI_DVFS_H

int hi_device_scale(struct device *target_dev,unsigned long old_freq,unsigned long new_freq);
void do_avs(void);

#define HI_TRACE(s, params...)   do{ \
	printk("[%s, %d]: " s "\n", __func__, __LINE__, ##params);      \
}while (0)
#endif
