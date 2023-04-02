#include <linux/platform_device.h>
#include <mach/irqs.h>
#include <linux/i2c-gpio.h>

static struct i2c_gpio_platform_data syno_i2c_data = {
	.sda_pin			= 46,
	.sda_is_open_drain	= 1,
	.scl_pin			= 47,
	.scl_is_open_drain	= 1,
	.udelay				= 8,
	.timeout			= 100,
};

static struct platform_device syno_i2c_device = {
	.name				= "i2c-gpio",
	.id					= -1,
	.dev.platform_data	= &syno_i2c_data,
};

static int __init syno_i2c_device_init(void)
{
	platform_device_register(&syno_i2c_device);
	return 0;
};

arch_initcall(syno_i2c_device_init);
