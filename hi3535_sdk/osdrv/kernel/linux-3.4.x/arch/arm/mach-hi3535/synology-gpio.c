#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
/*
 * Synology HI3535 Board GPIO Setup
 *
 * Copyright 2014 Synology, Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#include <linux/io.h>

#include <asm/mach-types.h>

#include <asm/mach/arch.h>
#include <asm/mach/flash.h>
#include <asm/mach/map.h>
#include <asm/sched_clock.h>
#include <mach/hardware.h>
#include <mach/early-debug.h>
#include <mach/irqs.h>
#include "mach/clock.h"
#include <mach/clkdev.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/kernel.h> /* printk() */
#include <linux/errno.h>  /* error codes */
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/time.h>

#include <linux/gpio.h>

#include <linux/synobios.h>

#ifndef HW_NVR216
#define HW_NVR216   "NVR216"     //"NVR216"
#endif

#ifdef  MY_ABC_HERE
extern char gszSynoHWVersion[];
#endif

#define	HI_GPIO00_ADDR		0x20150000
#define	HI_GPIO01_ADDR		0x20160000
#define	HI_GPIO02_ADDR		0x20170000
#define	HI_GPIO03_ADDR		0x20180000
#define	HI_GPIO04_ADDR		0x20190000
#define	HI_GPIO05_ADDR		0x201A0000
#define	HI_GPIO06_ADDR		0x201B0000
#define	HI_GPIO07_ADDR		0x201C0000
#define	HI_GPIO08_ADDR		0x201D0000
#define	HI_GPIO09_ADDR		0x201E0000
#define	HI_GPIO10_ADDR		0x201F0000
#define	HI_GPIO11_ADDR		0x20200000
#define	HI_GPIO12_ADDR		0x20210000
#define	HI_GPIO13_ADDR		0x20220000
#define	HI_GPIO14_ADDR		0x20230000

#define	GI_GPIO_OFFSET		0x00010000

#define	HI_GPIO_DATA_OFT	0x000
#define	HI_GPIO_DIR_OFT		0x400

#define	LED_BLINK_TIME_MS	500 // 500ms

#define HW_REG(reg)         *((volatile unsigned int *)(reg))

typedef struct __tag_SYNO_HDD_DETECT_GPIO {
	u8 hdd1_present_detect;
	u8 hdd2_present_detect;
	u8 hdd3_present_detect;
	u8 hdd4_present_detect;
	u8 hdd5_present_detect;
	u8 hdd6_present_detect;
	u8 hdd7_present_detect;
	u8 hdd8_present_detect;
} SYNO_HDD_DETECT_GPIO;

typedef struct __tag_SYNO_HDD_PM_GPIO {
	u8 hdd1_pm;
	u8 hdd2_pm;
	u8 hdd3_pm;
	u8 hdd4_pm;
	u8 hdd5_pm;
	u8 hdd6_pm;
	u8 hdd7_pm;
	u8 hdd8_pm;
} SYNO_HDD_PM_GPIO;

#define	GPIO_INPUT	0
#define	GPIO_OUTPUT	1

#define	GET_ENTRY(_TBL_)		(sizeof(_TBL_)/sizeof(_TBL_[0]))

#define	HI_GPIO_DATA_ADDR(PIN)			(0x04 << (PIN))
#define	HI_GPIO_DATA(VALUE,PIN)			((VALUE) << (PIN))
#define	HI_GPIO_DATA_USER(VALUE,PIN)	((VALUE) >> (PIN))
#define	GET_GPIO_GRP(PIN)				(PIN>>3)
#define	GET_GPIO_GRP_PIN(PIN)			(PIN&0x07)
	
#define GPIO_UNDEF				0xFF

typedef	enum {
	LED_LOCK_GREEN = 0,
	LED_LOCK_ORANGE,

	LED_STATUS_GREEN,
	LED_STATUS_ORANGE,

	LED_POWER_GREEN,
	LED_POWER_ORANGE,
		
	LED_END
}	GPIO_LED;

typedef	enum {
	BUTTON_RESET = 0,
		
	BUTTON_END
}	GPIO_BUTTON;

typedef	enum	{
	LED_ON = 0,
	LED_OFF,
	LED_BLINK,

	LED_STS_END
}	LED_STS;

typedef struct __tag_SYNO_OMAP_GENERIC_GPIO {
	int			led[LED_END];
	int			led_blink[LED_END];	
	GPIO_BUTTON	button[BUTTON_END];
	int			led_entry;
	int			button_entry;
	SYNO_HDD_PM_GPIO		hdd_pm;
	SYNO_HDD_DETECT_GPIO    hdd_detect;
}SYNO_HI3535_GENERIC_GPIO;

static SYNO_HI3535_GENERIC_GPIO generic_gpio;
struct task_struct * pblink_thread = NULL;

void syno_check_set_led_blink(unsigned int gpio, int value)
{
	int i;
	
	for(i = 0 ; i < generic_gpio.led_entry; i++) {
		if( generic_gpio.led[i] == gpio) {
			generic_gpio.led_blink[i] = value;
		}
	}
}

void syno_gpio_set_value(unsigned int gpio, int value)
{
	int group;
	int pin;	
	unsigned int addr;

	group = GET_GPIO_GRP(gpio);
	pin = GET_GPIO_GRP_PIN(gpio);
	value = HI_GPIO_DATA(value, pin);
	addr = HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DATA_ADDR(pin);
	addr = IO_ADDRESS(addr);
	HW_REG(addr) = value;	
}

int syno_gpio_get_value(unsigned gpio)
{
	int group;
	int pin;	
	unsigned int value, addr;

	group = GET_GPIO_GRP(gpio);
	pin = GET_GPIO_GRP_PIN(gpio);
	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DATA_ADDR(pin));
	value = HW_REG(addr);
	value = HI_GPIO_DATA_USER(value, pin);

	return value;
}

/*
	PIN Direction	Device	Description
	GPIO1_0 output	Lock green LED	low active
	GPIO1_1 output	Lock orange LED low active
	GPIO1_2 output	Status green LED	low active
	GPIO1_3 output	Status orange LED	low active
	GPIO1_4 output	Power green LED low active
	GPIO1_5 output	Power orange LED	low active
	GPIO1_6 input	Reset to default	low active
*/

static void 
Hii3535_nvr216_gpio_init(SYNO_HI3535_GENERIC_GPIO *global_gpio)
{
	memset(global_gpio, 0, sizeof(SYNO_HI3535_GENERIC_GPIO));

	/* set GPIO Direction 0 : input, 1: output */
	HW_REG(IO_ADDRESS(HI_GPIO00_ADDR + HI_GPIO_DIR_OFT)) = 0x18;
	HW_REG(IO_ADDRESS(HI_GPIO01_ADDR + HI_GPIO_DIR_OFT)) = 0xF3;
	HW_REG(IO_ADDRESS(HI_GPIO07_ADDR + HI_GPIO_DIR_OFT)) = 0x30;

	generic_gpio.hdd_pm.hdd1_pm = 8; /* GPIO1_0: HDD1_PWR_ENABLE */
	generic_gpio.hdd_pm.hdd2_pm = 9; /* GPIO1_1: HDD2_PWR_ENABLE */
	generic_gpio.hdd_detect.hdd1_present_detect = 10; /* GPIO1_2: HDD_PRZ1 */
	generic_gpio.hdd_detect.hdd2_present_detect = 11; /* GPIO1_3: HDD_PRZ2 */
}

int SYNO_CTRL_HDD_POWERON(int index, int value)
{
	int ret = -1;

	switch (index) {
	case 1:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd1_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd1_pm, value);
		break;
	case 2:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd2_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd2_pm, value);
		break;
	case 3:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd3_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd3_pm, value);
		break;
	case 4:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd4_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd4_pm, value);
		break;
	case 5:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd5_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd5_pm, value);
		break;
	case 6:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd6_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd6_pm, value);
		break;
	case 7:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd7_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd7_pm, value);
		break;
	case 8:
		WARN_ON(GPIO_UNDEF == generic_gpio.hdd_pm.hdd8_pm);
		syno_gpio_set_value(generic_gpio.hdd_pm.hdd8_pm, value);
		break;
	default:
		goto END;
	}
	ret = 0;
END:
	return ret;
}

int
SYNO_HI3535_GPIO_PIN(int pin, int *pValue, int isWrite)
{
	int ret = -1;

	if (NULL == pValue){
		WARN(NULL == pValue, "got NULL pointer\n");
		ret = -1;
		goto END;
	}

	if (isWrite) {
		if (LED_BLINK == *pValue) {
			syno_check_set_led_blink(pin, 1);
		} else {
			syno_check_set_led_blink(pin, 0);
		}
		syno_gpio_set_value(pin, *pValue ? 1 : 0 );
	}
	else {
		*pValue = syno_gpio_get_value(pin);
	}
	
	ret = 0;

END:
	return ret;
}

int SYNO_CTRL_POWER_LED_SET(int status)
{
	printk("SYNO_CTRL_POWER_LED_SET\n");
	return 0;
}

static
int syno_led_blink_thread(void* pobject)
{	
	SYNO_HI3535_GENERIC_GPIO *pgpio = (SYNO_HI3535_GENERIC_GPIO*)pobject;
	int i;
	int blink = 0;

	while (!kthread_should_stop()) {
		for(i = 0 ; i < pgpio->led_entry; i++) {
			if (pgpio->led_blink[i]) {
				syno_gpio_set_value(pgpio->led[i], blink);
			}
		}
		blink ^= 1;
		msleep(LED_BLINK_TIME_MS);
	}		

	return 0;
}

#define ARRAY_LEN(x) (sizeof(x)/sizeof(x[0]))
struct disk_info {
	char *hw_version;
	int	max_disk_id;
};
static struct disk_info hi3535_family[] = {
	{HW_NVR216, 2},
	{HW_NVR1218, 2}
};

unsigned char SYNOHI3535IsBoardNeedPowerUpHDD(u32 disk_id) {
	u8 ret = 0;
	int i;
	int def_max_disk = 0;

	if (syno_is_hw_version(HW_NVR216) || syno_is_hw_version(HW_NVR1218)){
		def_max_disk = 2;
	}

	/* lookup table for max disk
	   if not found, compare default */
	ret = (disk_id <= def_max_disk)? 1 : 0;
	for (i = 0; i < ARRAY_LEN(hi3535_family); i++) {
		if (syno_is_hw_version(hi3535_family[i].hw_version)) {
			if (disk_id <= hi3535_family[i].max_disk_id) {
				ret = 1;
			}
			break;
		}
	}

	return ret;
}

int SYNO_CHECK_HDD_PRESENT(int index)
{
    int iPrzVal = 1; /*defult is present*/

    switch (index) {
        case 1:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd1_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd1_present_detect);
            }
            break;
        case 2:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd2_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd2_present_detect);
            }
            break;
        case 3:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd3_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd3_present_detect);
            }
            break;
        case 4:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd4_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd4_present_detect);
            }
            break;
        case 5:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd5_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd5_present_detect);
            }
            break;
        case 6:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd6_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd6_present_detect);
            }
            break;
        case 7:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd7_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd7_present_detect);
            }
            break;
        case 8:
            if (GPIO_UNDEF != generic_gpio.hdd_detect.hdd8_present_detect) {
                iPrzVal = !syno_gpio_get_value(generic_gpio.hdd_detect.hdd8_present_detect);
            }
            break;
        default:
            break;
    }

    return iPrzVal;
}

/* SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER
 * Query support HDD dynamic Power .
 * output: 0 - support, 1 - not support.
 */
int SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER(void)
{
	int iRet = 0;

	/* if exist at least one hdd has enable pin and present detect pin ret=1*/
	if ((GPIO_UNDEF != generic_gpio.hdd_pm.hdd1_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd1_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd2_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd2_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd3_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd3_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd4_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd4_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd5_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd5_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd6_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd6_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd7_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd7_present_detect) ||
		(GPIO_UNDEF != generic_gpio.hdd_pm.hdd8_pm && GPIO_UNDEF != generic_gpio.hdd_detect.hdd8_present_detect)) {

		iRet = 1;
	}
	return iRet;
}

int SYNO_CTRL_HDD_ACT_NOTIFY(int index)
{
	return 0;
}

void synology_gpio_init(void)
{
#if defined(MY_ABC_HERE)
	Hii3535_nvr216_gpio_init(&generic_gpio);
	printk("Synology HI3535 NVR216 GPIO Init\n");
#else
	WARN(1, "Can't init GPIO because of unknown board ID\n");
#endif

	generic_gpio.led_blink[LED_POWER_ORANGE] = 1;
	pblink_thread = kthread_run(syno_led_blink_thread, &generic_gpio, "syno_led");
}

EXPORT_SYMBOL(SYNO_CTRL_HDD_ACT_NOTIFY);
EXPORT_SYMBOL(SYNOHI3535IsBoardNeedPowerUpHDD);
EXPORT_SYMBOL(SYNO_CTRL_HDD_POWERON);
EXPORT_SYMBOL(SYNO_CHECK_HDD_PRESENT);
EXPORT_SYMBOL(SYNO_SUPPORT_HDD_DYNAMIC_ENABLE_POWER);
EXPORT_SYMBOL(SYNO_HI3535_GPIO_PIN);
EXPORT_SYMBOL(SYNO_CTRL_POWER_LED_SET);
