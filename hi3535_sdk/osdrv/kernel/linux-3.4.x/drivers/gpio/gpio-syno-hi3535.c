#include <linux/init.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/irqdomain.h>

#include <asm/mach/irq.h>
#include <mach/gpio.h>

static int syno_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	/* 0: input , 1: output */
	unsigned int value = 0x1;
	unsigned int origin = 0;
	int group;
	unsigned int addr;

	group = GET_GPIO_GRP(offset);
	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DIR_OFT);
	value = value << (offset - GRP_SIZE*group);

	origin = HW_REG(addr);
	origin &= ~value;
	HW_REG(addr) = origin;

	return 0;
}

static void syno_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	int group;
	int pin;
	unsigned int addr;
	unsigned int output_mask = 0x1;
	unsigned int origin = 0;

	group = GET_GPIO_GRP(offset);
	pin = GET_GPIO_GRP_PIN(offset);

	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DIR_OFT);
	output_mask = output_mask << (offset - GRP_SIZE*group);

	origin = HW_REG(addr);
	origin |= output_mask;
	HW_REG(addr) = origin;

	value = HI_GPIO_DATA(value, pin);
	addr = HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DATA_ADDR(pin);
	addr = IO_ADDRESS(addr);
	HW_REG(addr) = value;
}

static int syno_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	int group;
	int pin;
	unsigned int value, addr;
	unsigned int input_mask = 0x1;
	unsigned int origin = 0;

	group = GET_GPIO_GRP(offset);
	pin = GET_GPIO_GRP_PIN(offset);

	/* set direction to input */
	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DIR_OFT);
	input_mask = input_mask << (offset - GRP_SIZE * group);
	origin = HW_REG(addr);
	origin &= ~input_mask;
	HW_REG(addr) = origin;

	/* get value */
	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DATA_ADDR(pin));
	value = HW_REG(addr);
	value = HI_GPIO_DATA_USER(value, pin);

	return value;
}

static int syno_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
					int temp_value)
{
	/* 0: input , 1: output */
	unsigned int value = 0x1;
	unsigned int origin = 0;
	int group;
	unsigned int addr;

	group = GET_GPIO_GRP(offset);
	addr = IO_ADDRESS(HI_GPIO00_ADDR + (group*GI_GPIO_OFFSET) + HI_GPIO_DIR_OFT);

	value = value << (offset - GRP_SIZE * group);

	origin = HW_REG(addr);
	origin |= value;
	HW_REG(addr) = origin;

	return 0;
}

static int syno_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return 0;
}

static struct gpio_chip syno_gpio_chip = {
	.label				= "syno-gpio-device",
	.direction_input	= syno_gpio_direction_input,
	.get				= syno_gpio_get,
	.direction_output	= syno_gpio_direction_output,
	.set				= syno_gpio_set,
	.to_irq				= syno_gpio_to_irq,
	.base				= 0,
	.ngpio				= 120,
};

static int __devinit syno_gpio_probe(struct platform_device *pdev)
{
	gpiochip_add(&syno_gpio_chip);
	return 0;
}

static struct platform_driver syno_gpio_driver = {
	.driver		= {
		.name	= "syno-gpio-device",
		.owner	= THIS_MODULE,
	},
	.probe		= syno_gpio_probe,
};

static int __init syno_gpio_init(void)
{
	return platform_driver_register(&syno_gpio_driver);
}

postcore_initcall(syno_gpio_init);
