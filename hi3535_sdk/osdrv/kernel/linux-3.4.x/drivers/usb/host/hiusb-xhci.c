#ifndef MY_ABC_HERE
#define MY_ABC_HERE
#endif
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/dma-mapping.h>
#include <linux/types.h>
#include <mach/io.h>

#ifdef CONFIG_ARCH_HI3535
#include "hiusb-xhci-3535.c"
#endif

static struct hiusb_plat_data  hiusb_data = {
	.start_hcd = hiusb_start_hcd,
	.stop_hcd = hiusb_stop_hcd,
};

static struct resource hiusb_xhci_res[] = {
	[0] = {
		.start	= CONFIG_HIUSB_XHCI_IOBASE,
		.end	= CONFIG_HIUSB_XHCI_IOBASE
			+ CONFIG_HIUSB_XHCI_IOSIZE - 1,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= CONFIG_HIUSB_XHCI_IRQNUM,
		.end	= CONFIG_HIUSB_XHCI_IRQNUM,
		.flags	= IORESOURCE_IRQ,
	},
};

static u64 usb_dmamask = DMA_BIT_MASK(32);

#if defined(CONFIG_SYNO_HI3535_VS) || defined(MY_ABC_HERE)
void dummy_hiusb_xhci_release() {
}
#endif

static struct platform_device hiusb_xhci_platdev = {
	.name = "xhci-hcd",
	.id = -1,
	.dev = {
		.init_name = "hiusb3.0",
		.platform_data = &hiusb_data,
		.dma_mask = &usb_dmamask,
		.coherent_dma_mask = DMA_BIT_MASK(32),
#if defined(CONFIG_SYNO_HI3535_VS) || defined(MY_ABC_HERE)
		.release = dummy_hiusb_xhci_release,
#endif
	},
	.num_resources = ARRAY_SIZE(hiusb_xhci_res),
	.resource = hiusb_xhci_res,
};

#if defined(CONFIG_SYNO_HI3535_VS) || defined(MY_ABC_HERE)
int xhci_register_hi3535(void)
#else
static int __init xhci_device_init(void)
#endif
{
	if (usb_disabled())
		return -ENODEV;
	return platform_device_register(&hiusb_xhci_platdev);
}

#if defined(CONFIG_SYNO_HI3535_VS) || defined(MY_ABC_HERE)
void xhci_unregister_hi3535(void)
#else
static void __exit xhci_device_exit(void)
#endif
{
	platform_device_unregister(&hiusb_xhci_platdev);
}

#if defined(CONFIG_SYNO_HI3535_VS) || defined(MY_ABC_HERE)
#else
module_init(xhci_device_init);
module_exit(xhci_device_exit);
#endif
