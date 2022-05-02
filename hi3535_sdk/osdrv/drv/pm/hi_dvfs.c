/*
 * hisilicon DVFS Management Routines
 *
 * Copyright (C) 2012 Hisilicon Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/plist.h>
#include <linux/slab.h>
#include "opp.h"
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/io.h>
#include <mach/platform.h>
#include <linux/platform_device.h>
#include <linux/delay.h>
#include <mach/clock.h>
#include "hi_dvfs.h"

extern struct clk mpu_ck;

#define VMAX 1315 /*mv*/
#define VMIN 900 /*mv*/
#define CORE_VMAX 1205 /*mv*/
#define CORE_VMIN 900 /*mv*/

#define PWM_STEP 5 /*mv*/
#define PWM_CLASS 2

#define AVS_STEP 5 /*mv*/
#define BEST_VRING 333 /*fixme*/

unsigned long cur_cpu_volt = 1315;
unsigned long cur_core_volt = 1200;
int sub_volt = 0;
unsigned int reg_cpu1,reg_cpu2;

DEFINE_MUTEX(hi_dvfs_lock);

/**
 * struct hi_dvfs_info - The per vdd dvfs info
 * @user_lock:	spinlock for plist operations
 *
 * This is a fundamental structure used to store all the required
 * DVFS related information for a vdd.
 */
struct hi_dvfs_info
{
    unsigned long volt;
    unsigned long new_freq;
    unsigned long old_freq;
};

int cpu_volt_scale(unsigned int volt)
{
    return 0;
}


void mpu_init_volt(void)
{
    return;
}

int core_volt_scale(unsigned int volt)
{
    return 0;
}

void core_init_volt(void)
{
    return;
}

unsigned long _get_best_vring(void)
{
    return BEST_VRING;
}

unsigned long _get_cur_vring(void)
{
    return 0;
}

void do_avs(void)
{
#if 0
    unsigned long vring, vbest;

    vbest = _get_best_vring();
    vring = _get_cur_vring();

    while (vring != vbest)
    {
        if (vring > vbest)
        {
            if ((vring - vbest) < MIN_THRESHOLD)
            {
                break;
            }

            if (!cur_cpu_volt)
            {
                cpu_volt_scale(cur_cpu_volt + AVS_STEP);
            }
        }
        else if (vring > vbest)
        {
            if ((vbest - vring) < MIN_THRESHOLD)
            {
                break;
            }

            if (!cur_cpu_volt)
            {
                cpu_volt_scale(cur_cpu_volt - AVS_STEP);
            }
        }
    }
#endif
}

/**
 * _dvfs_scale() : Scale the devices associated with a voltage domain
 *
 * Returns 0 on success else the error value.
 */
static int _dvfs_scale(struct device *target_dev, struct hi_dvfs_info *tdvfs_info)
{
    struct clk * clk;
    int ret;

    HI_TRACE("%s rate=%ld\n", __FUNCTION__, tdvfs_info->new_freq);

    clk = &mpu_ck;
    if (tdvfs_info->new_freq == tdvfs_info->old_freq)
    {
        return 0;
    }
    else if (tdvfs_info->new_freq > tdvfs_info->old_freq)
    {
        ret = cpu_volt_scale(tdvfs_info->volt);
        if (ret)
        {
            HI_TRACE("%s: scale volt to %ld falt\n",
                    __func__, tdvfs_info->volt);
            return ret;
        }

        msleep(15);
        ret = clk_set_rate(clk, tdvfs_info->new_freq);
        if (ret)
        {
            HI_TRACE("%s: scale freq to %ld falt\n",
                    __func__, tdvfs_info->new_freq);
            return ret;
        }
    }
    else
    {
        ret = clk_set_rate(clk, tdvfs_info->new_freq);
        if (ret)
        {
            HI_TRACE("%s: scale freq to %ld falt\n",
                    __func__, tdvfs_info->new_freq);
            return ret;
        }

        msleep(10);
        ret = cpu_volt_scale(tdvfs_info->volt);
        if (ret)
        {
            HI_TRACE("%s: scale volt to %ld falt\n",
                    __func__, tdvfs_info->volt);
            return ret;
        }
    }

    return ret;
}

/**
 * hi_device_scale() - Set a new rate at which the devices is to operate
 * @rate:	the rnew rate for the device.
 *
 * This API gets the device opp table associated with this device and
 * tries putting the device to the requested rate and the voltage domain
 * associated with the device to the voltage corresponding to the
 * requested rate. Since multiple devices can be assocciated with a
 * voltage domain this API finds out the possible voltage the
 * voltage domain can enter and then decides on the final device
 * rate.
 *
 * Return 0 on success else the error value
 */
int hi_device_scale(struct device *target_dev, unsigned long old_freq, unsigned long new_freq)
{
    struct opp *opp;
    unsigned long volt, freq = new_freq;
    struct hi_dvfs_info dvfs_info;

    int ret = 0;

    HI_TRACE("hi_device_scale,oldfreq = %ld,newfreq = %ld\n", old_freq, new_freq);

    /* Lock me to ensure cross domain scaling is secure */
    mutex_lock(&hi_dvfs_lock);
    rcu_read_lock();

    opp = opp_find_freq_ceil(target_dev, &freq);

    /* If we dont find a max, try a floor at least */
    if (IS_ERR(opp))
    {
        opp = opp_find_freq_floor(target_dev, &freq);
    }

    if (IS_ERR(opp))
    {
        rcu_read_unlock();
        HI_TRACE("%s: Unable to find OPP for freq%ld\n",
                  __func__, freq);
        ret = -ENODEV;
        goto out;
    }

    volt = opp_get_voltage(opp);

    rcu_read_unlock();

    dvfs_info.old_freq = old_freq;

    dvfs_info.new_freq = freq;

    dvfs_info.volt = volt;

    /* Do the actual scaling */
    ret = _dvfs_scale( target_dev, &dvfs_info);

    if (ret)
    {
        HI_TRACE("%s: scale failed %d[f=%ld, v=%ld]\n",
                  __func__, ret, freq, volt);

        /* Fall through */
    }

    /* Fall through */
out:
    mutex_unlock(&hi_dvfs_lock);
    return ret;
}
