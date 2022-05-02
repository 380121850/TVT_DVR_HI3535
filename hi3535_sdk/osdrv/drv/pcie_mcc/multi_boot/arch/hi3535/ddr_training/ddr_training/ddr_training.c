/******************************************************************************
*    Copyright (c) 2009-2012 by Hisi.
*    All rights reserved.
* ***
*
******************************************************************************/

#include <config.h>

extern int ddr_dataeye_training(void *param);

extern int ddrphy_train_route(void);

static inline unsigned int readl(unsigned addr)
{
	return (*(volatile unsigned int *)(addr));
}

static inline void writel(unsigned val, unsigned addr)
{
	(*(volatile unsigned *) (addr)) = (val);
}

static int divider32(int dividend, int divisor)
{
	int result = 0;

	int i, tmp;
	int s = 1;

	for (i = 31; i >= 0; i--) {
		tmp = dividend >> i;
		if (tmp >= divisor) {
			dividend = dividend - (divisor << i);
			result = result + (s << i);
		}
	}

	return result;
}

static void start_core_cpu_hpm(void)
{
	unsigned int core_aver_recorder = 0, cpu_aver_recorder = 0;
	unsigned int core_tmp0 = 0, core_tmp1 = 0, cpu_tmp0 = 0, cpu_tmp1 = 0;
	unsigned int u32Value0_CORE = 0, u32Value1_CORE = 0;
	unsigned int u32Value2_CORE = 0, u32Value3_CORE = 0;
	unsigned int u32Value0_CPU = 0, u32Value1_CPU = 0;
	unsigned int u32Value2_CPU = 0, u32Value3_CPU = 0;
	unsigned int aver_num, i;

	aver_num = 5;

	/* reset core hpm, cpu hpm, div frq */
	writel(0x0a00000a, 0x2003012c);
	writel(0x02000013, 0x2003013c);

	/* Core Hpm */
	for (i = 0; i < aver_num; i++) {
		writel(0x0200000a, 0x2003012c);
		writel(0x0300000a, 0x2003012c);
		while ((readl(0x20030130) & 0x400) == 0)
			;
	}

	for (i = 0; i < aver_num; i++) {
		writel(0x0200000a, 0x2003012c);
		writel(0x0300000a, 0x2003012c);
		while ((readl(0x20030130) & 0x400) == 0)
			;
		core_tmp0 = readl(0x20030130);
		u32Value0_CORE += core_tmp0 & 0x3ff;
		u32Value1_CORE += (core_tmp0 >> 12) & 0x3ff;
		core_tmp1 = readl(0x20030134);
		u32Value2_CORE += core_tmp1 & 0x3ff;
		u32Value3_CORE += (core_tmp1 >> 12) & 0x3ff;
	}

	core_aver_recorder = divider32((u32Value0_CORE + u32Value1_CORE
				+ u32Value2_CORE + u32Value3_CORE),
			aver_num);
	core_aver_recorder = divider32(core_aver_recorder, 4);

	writel(core_aver_recorder, 0x20050098);

	/* CPU Hpm */
	for (i = 0; i < aver_num; i++) {
		writel(0x0a000013, 0x2003013c);
		writel(0x0b000013, 0x2003013c);
		while ((readl(0x20030140) & 0x400) == 0)
			;
	}

	for (i = 0; i < aver_num; i++) {
		writel(0x0a000013, 0x2003013c);
		writel(0x0b000013, 0x2003013c);
		while ((readl(0x20030140) & 0x400) == 0)
			;

		cpu_tmp0 = readl(0x20030140);
		u32Value0_CPU += cpu_tmp0 & 0x3ff;
		u32Value1_CPU += (cpu_tmp0>>12) & 0x3ff;
		cpu_tmp1 = readl(0x20030144);
		u32Value2_CPU += cpu_tmp1 & 0x3ff;
		u32Value3_CPU += (cpu_tmp1>>12) & 0x3ff;
	}

	cpu_aver_recorder = divider32((u32Value0_CPU + u32Value1_CPU
				+ u32Value2_CPU + u32Value3_CPU),
				aver_num);
	cpu_aver_recorder = divider32(cpu_aver_recorder, 4);

	writel(cpu_aver_recorder, 0x2005009c);
}
#define PWM0_CFG0 0x200e0000
#define PWM0_CFG1 0x200e0004
#define PWM1_CFG0 0x200e0020
#define PWM1_CFG1 0x200e0024
void start_svb(void)
{
	int voltage_tmp;
	unsigned int core_aver_recorder = 0, cpu_aver_recorder = 0;

	/* auto svb operation */
	cpu_aver_recorder = readl(0x2005009c);
	core_aver_recorder = readl(0x20050098);
	/* the first uboot table type */
	if (0xa7 == readl(PWM0_CFG0)) {
		if (cpu_aver_recorder < 430)
			writel(0x0000001b, PWM0_CFG1); /* 1.25V */
		else if (cpu_aver_recorder > 460)
			writel(0x00000043, PWM0_CFG1); /* 1.15V */
		else
			writel(0x0000002f, PWM0_CFG1); /* 1.20V */

		if (core_aver_recorder < 480)
			writel(0x00000015, PWM1_CFG1); /* 1.15V */
		else
			writel(0x00000029, PWM1_CFG1); /* 1.10V */
	}

	/* the second uboot table type */
	if (0x7b == readl(PWM0_CFG0)) {
		if (cpu_aver_recorder < 430)
			writel(0x0000001b, PWM1_CFG1); /* 1.25V */
		else if (cpu_aver_recorder > 460)
			writel(0x00000043, PWM1_CFG1); /* 1.15V */
		else
			writel(0x0000002f, PWM1_CFG1); /* 1.20V */

		if (core_aver_recorder < 480)
			writel(0x00000015, PWM0_CFG1); /* 1.15V */
		else
			writel(0x00000029, PWM0_CFG1); /* 1.10V */
	}

	/* manual svb operation */
	voltage_tmp = readl(0x200500a0);
	if (voltage_tmp)
		writel(voltage_tmp, PWM0_CFG0);
	voltage_tmp = readl(0x200500a4);
	if (voltage_tmp)
		writel(voltage_tmp, PWM0_CFG1);
	voltage_tmp = readl(0x200500a8);
	if (voltage_tmp)
		writel(voltage_tmp, PWM1_CFG0);
	voltage_tmp = readl(0x200500ac);
	if (voltage_tmp)
		writel(voltage_tmp, PWM1_CFG1);

	writel(0x00000005, 0x200e000c);
	writel(0x00000005, 0x200e002c);
}

void start_ddr_training(unsigned int base)
{
	unsigned int val;

	/* add SVB function */
	start_core_cpu_hpm();
	start_svb();

	if (!(readl(base + REG_SC_GEN20) & 0x1))
		ddrphy_train_route();

#if 1
	/* mark that ddrphy training is done! */
	val = readl(base + REG_SC_GEN20);
	val |= 0x1;
	writel(val, base + REG_SC_GEN20);
#endif

#if 0
	writel(0x20118210, 0x30000100);
	writel(readl(0x20118210), 0x30000104);
	writel(0x20118214, 0x30000110);
	writel(readl(0x20118214), 0x30000114);
	writel(0x20118218, 0x30000120);
	writel(readl(0x20118218), 0x30000124);
	writel(0x20118290, 0x30000130);
	writel(readl(0x20118290), 0x30000134);
	writel(0x20118294, 0x30000140);
	writel(readl(0x20118294), 0x30000144);
	writel(0x20118230, 0x30000150);
	writel(readl(0x20118230), 0x30000154);
	writel(0x201182b0, 0x30000160);
	writel(readl(0x201182b0), 0x30000164);
	writel(0x20118330, 0x30000170);
	writel(readl(0x20118330), 0x30000174);
	writel(0x201183b0, 0x30000180);
	writel(readl(0x201183b0), 0x30000184);
	writel(0x20118250, 0x30000190);
	writel(readl(0x20118250), 0x30000194);
	writel(0x20118254, 0x300001a0);
	writel(readl(0x20118254), 0x300001a4);
	writel(0x201182d0, 0x300001b0);
	writel(readl(0x201182d0), 0x300001b4);
	writel(0x201182d4, 0x300001c0);
	writel(readl(0x201182d4), 0x300001c4);
	writel(0x20118350, 0x300001d0);
	writel(readl(0x20118350), 0x300001d4);
	writel(0x20118354, 0x300001e0);
	writel(readl(0x20118354), 0x300001e4);
	writel(0x201183d0, 0x300001f0);
	writel(readl(0x201183d0), 0x300001f4);
	writel(0x201183d4, 0x30000200);
	writel(readl(0x201183d4), 0x30000204);
	writel(0x2011823c, 0x30000210);
	writel(readl(0x2011823c), 0x30000214);
	writel(0x201182bc, 0x30000220);
	writel(readl(0x201182bc), 0x30000224);
	writel(0x2011833c, 0x30000230);
	writel(readl(0x2011833c), 0x30000234);
	writel(0x201183bc, 0x30000240);
	writel(readl(0x201183bc), 0x30000244);
	writel(0x20118248, 0x30000250);
	writel(readl(0x20118248), 0x30000254);
	writel(0x201182c8, 0x30000260);
	writel(readl(0x201182c8), 0x30000264);
	writel(0x20118348, 0x30000270);
	writel(readl(0x20118348), 0x30000274);
	writel(0x201183c8, 0x30000280);
	writel(readl(0x201183c8), 0x30000284);
#endif
	if (!(readl(base + REG_SC_GEN20) & (0x1 << 16)))
		ddr_dataeye_training(0);
#if 0
	writel(0x20118210, 0x30000300);
	writel(readl(0x20118210), 0x30000304);
	writel(0x20118214, 0x30000310);
	writel(readl(0x20118214), 0x30000314);
	writel(0x20118218, 0x30000320);
	writel(readl(0x20118218), 0x30000324);
	writel(0x20118290, 0x30000330);
	writel(readl(0x20118290), 0x30000334);
	writel(0x20118294, 0x30000340);
	writel(readl(0x20118294), 0x30000344);
	writel(0x20118310, 0x30000490);
	writel(readl(0x20118310), 0x30000494);
	writel(0x20118314, 0x300004a0);
	writel(readl(0x20118314), 0x300004a4);
	writel(0x20118390, 0x300004b0);
	writel(readl(0x20118390), 0x300004b4);
	writel(0x20118394, 0x300004c0);
	writel(readl(0x20118394), 0x300004c4);
	writel(0x20118230, 0x30000350);
	writel(readl(0x20118230), 0x30000354);
	writel(0x201182b0, 0x30000360);
	writel(readl(0x201182b0), 0x30000364);
	writel(0x20118330, 0x30000370);
	writel(readl(0x20118330), 0x30000374);
	writel(0x201183b0, 0x30000380);
	writel(readl(0x201183b0), 0x30000384);
	writel(0x20118250, 0x30000390);
	writel(readl(0x20118250), 0x30000394);
	writel(0x20118254, 0x300003a0);
	writel(readl(0x20118254), 0x300003a4);
	writel(0x201182d0, 0x300003b0);
	writel(readl(0x201182d0), 0x300003b4);
	writel(0x201182d4, 0x300003c0);
	writel(readl(0x201182d4), 0x300003c4);
	writel(0x20118350, 0x300003d0);
	writel(readl(0x20118350), 0x300003d4);
	writel(0x20118354, 0x300003e0);
	writel(readl(0x20118354), 0x300003e4);
	writel(0x201183d0, 0x300003f0);
	writel(readl(0x201183d0), 0x300003f4);
	writel(0x201183d4, 0x30000400);
	writel(readl(0x201183d4), 0x30000404);
	writel(0x2011823c, 0x30000410);
	writel(readl(0x2011823c), 0x30000414);
	writel(0x201182bc, 0x30000420);
	writel(readl(0x201182bc), 0x30000424);
	writel(0x2011833c, 0x30000430);
	writel(readl(0x2011833c), 0x30000434);
	writel(0x201183bc, 0x30000440);
	writel(readl(0x201183bc), 0x30000444);
	writel(0x20118248, 0x30000450);
	writel(readl(0x20118248), 0x30000454);
	writel(0x201182c8, 0x30000460);
	writel(readl(0x201182c8), 0x30000464);
	writel(0x20118348, 0x30000470);
	writel(readl(0x20118348), 0x30000474);
	writel(0x201183c8, 0x30000480);
	writel(readl(0x201183c8), 0x30000484);

	writel(0x2011821c, 0x30000490);
	writel(readl(0x2011821c), 0x30000494);
	writel(0x20118220, 0x300004a0);
	writel(readl(0x20118220), 0x300004a4);
	writel(0x2011829c, 0x300004b0);
	writel(readl(0x2011829c), 0x300004b4);
	writel(0x201182a0, 0x300004c0);
	writel(readl(0x201182a0), 0x300004c4);
	writel(0x2011831c, 0x300004d0);
	writel(readl(0x2011831c), 0x300004d4);
	writel(0x20118320, 0x300004e0);
	writel(readl(0x20118320), 0x300004e4);
	writel(0x2011839c, 0x300004f0);
	writel(readl(0x2011839c), 0x300004f4);
	writel(0x201183a0, 0x30000500);
	writel(readl(0x201183a0), 0x30000504);

#endif
	/*the value should config after trainning, or
	  it will cause chip compatibility problems*/
	writel(0x401, 0x20111028);

}
