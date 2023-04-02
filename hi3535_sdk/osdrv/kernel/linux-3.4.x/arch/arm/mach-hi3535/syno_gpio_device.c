#include <linux/platform_device.h>
#include <mach/irqs.h>

static struct resource gpio_resource[] = {
	[0] = {
		.start = 0,
		.end   = 0,
		.flags = IORESOURCE_MEM,
	},
};

static struct platform_device syno_gpio_device = {
	.name = "syno-gpio-device",
	.id   = -1,
	.resource = gpio_resource,
	.num_resources = ARRAY_SIZE(gpio_resource),
};

static int __init syno_gpio_init(void)
{
	platform_device_register(&syno_gpio_device);
	return 0;
};

arch_initcall(syno_gpio_init);
