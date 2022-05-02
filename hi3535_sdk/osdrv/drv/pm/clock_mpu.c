/*
 * hisilicon mpu clock management Routines
 *
 * Copyright (C) 2012 Hisilicon Instruments, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/err.h>
#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/clk.h>
#include <mach/platform.h>
#include <linux/clkdev.h>
#include <asm/clkdev.h>
#include <mach/clock.h>
#include <mach/early-debug.h>
#include <linux/device.h>
#include <mach/io.h>

#include "hi_dvfs.h"

struct device *mpu_dev;

#define PERI_CRG0_OFFSET 0x0
#define PERI_CRG1_OFFSET 0x4
#define PERI_CRG10_OFFSET 0x28
#define PERI_CRG12_OFFSET 0x30
#define PERI_CRG58_OFFSET 0xe8

static DEFINE_SPINLOCK(mpu_clock_lock);

#define DEFAULT_INIT_FREQ 1000000 
#define MAX_FREQ 1200000
#define MIN_FREQ 600000

struct clk mpu_ck = {
    .name   = "mpu_ck",
    .parent = NULL,
};

#ifdef HPM_DEBUG
static int cpu_init_hpm(unsigned long rate)
{
    return 0;
}


void core_init_hpm(void)
{
    return ;
}

#endif


/*
 * mpu_clksel_set_rate() - program clock rate in hardware
 * @clk: struct clk * to program rate
 * @rate: target rate to program
 *profile	freq		volt		sel
 *profile0	1.2GHz	1.3V		BPLL 1.2GHz
 *profile1	1GHz	1.25V	WPLL
 *profile2	750MHz	1.15V	APLL 750MHz
 *profile3	600MHz	1.1V		BPLL 1.2GHz DIV2
 *profile4	400MHz	1.05V	400MHz
 */
int mpu_clk_set_rate(struct clk *clk, unsigned rate)
{
	unsigned long ret;
	unsigned long apll_postdiv1,apll_postdiv2,apll_refdiv;
	unsigned long fbdiv;
	int time_out = 1000;

	unsigned long crg_base = IO_ADDRESS(0x20030000);

//	HI_TRACE("mpu_clk_set_rate %dk\n", rate); 
	
	if (rate < 600000 || rate > 2400000) {
		HI_TRACE("error,please input a valid frequency!\n");
		return -1;
	}

	
	/* select cpu_clock 750M */
	ret = readl(crg_base + PERI_CRG12_OFFSET);
	ret |= (1<<8);
	writel(ret, crg_base + PERI_CRG12_OFFSET);

	ret = readl(crg_base + PERI_CRG0_OFFSET);
	apll_postdiv1 = (ret >> 24) & 0x7;
	apll_postdiv2 = (ret >> 30) & 0x7;

	ret = readl(crg_base + PERI_CRG1_OFFSET);
	apll_refdiv = (ret >> 12) & 0x3f;

	/* coculate the fbdiv */
	fbdiv = (rate*apll_postdiv1*apll_postdiv2)/24000;

	/* write the frequency of APLL */
	ret = readl(crg_base + 0x4);
	ret = rate & 0xfffff000;
	ret = rate | fbdiv;
	writel(ret, crg_base + 0x4);

	/* set peri_crg10 bit25 loaden 0 */
	ret = readl(crg_base + PERI_CRG10_OFFSET);
	ret &= ~(1<<25);
	writel(ret, crg_base + PERI_CRG10_OFFSET);

	/* set peri_crg10 bit25 loaden 1 */
	ret = readl(crg_base + PERI_CRG10_OFFSET);
	ret |= (1<<25);
	writel(ret, crg_base + PERI_CRG10_OFFSET);

	while (!(readl(crg_base + PERI_CRG58_OFFSET) & 1) && time_out > 0) {
		time_out--;
		udelay(1);
	}

	if (0 >= time_out) {
		HI_TRACE("Wait for APLL lock failed!");
		return -1;
	}

	/* return to select APLL for cpu clock */
	ret = readl(crg_base + PERI_CRG12_OFFSET);
	ret &= ~(1<<8);
	writel(ret, crg_base + PERI_CRG12_OFFSET);

#ifdef HPM_DEBUG
	/* After change clock, we need to reinitialize hpm */
	cpu_init_hpm(rate);
#endif
	return 0;
}

static unsigned int __mpu_clk_get_rate(struct clk *clk)
{
	int ret;
	unsigned long apll_postdiv1,apll_postdiv2,apll_refdiv;
	unsigned long fbdiv;
	unsigned long rate;
	unsigned long crg_base = IO_ADDRESS(0x20030000);
	
	ret = readl(crg_base + PERI_CRG0_OFFSET);
	apll_postdiv1 = (ret >> 24) & 0x7;
	apll_postdiv2 = (ret >> 30) & 0x7;

	ret = readl(crg_base + PERI_CRG1_OFFSET);
	apll_refdiv = (ret >> 12) & 0x3f;
	
	fbdiv = ret & (0xfff);

	rate = (24000*fbdiv);
	rate = rate/apll_postdiv1;
	rate = rate/apll_postdiv2;
	rate = apll_refdiv;

	return rate;
}

static unsigned int mpu_clk_get_rate(struct clk *clk)
{
    return __mpu_clk_get_rate(clk);
}

/*if DSMPD = 1 (DSM is disabled, "integer mode")
*FOUTVCO = FREF / REFDIV * FBDIV
*FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
*If DSMPD = 0 (DSM is enabled, "fractional mode")
*FOUTVCO = FREF / REFDIV * (FBDIV + FRAC / 2^24)
*FOUTPOSTDIV = FOUTVCO / POSTDIV1 / POSTDIV2
*/
unsigned int mpu_pll_init(void)
{
    return 0;
}

static void mpu_clk_init(struct clk *clk)
{
    return;
}

static struct clk_ops mpu_clock_ops = {
    .set_rate = mpu_clk_set_rate,
    .get_rate = mpu_clk_get_rate,
    .init     = mpu_clk_init,
};

void  mpu_init_clocks(void)
{
    HI_TRACE("Enter %s\n", __func__);
    mpu_ck.ops  = &mpu_clock_ops;
    mpu_ck.rate = DEFAULT_INIT_FREQ;
    mpu_ck.max_rate = MAX_FREQ;
    mpu_ck.min_rate = MIN_FREQ;

    mpu_ck.spinlock = mpu_clock_lock;
    clk_init(&mpu_ck);
	
    mpu_pll_init();
    mpu_clk_set_rate(&mpu_ck, mpu_ck.rate);	
	
    return;
}

void __exit mpu_exit_clocks(void)
{
    clk_exit(&mpu_ck);
}
