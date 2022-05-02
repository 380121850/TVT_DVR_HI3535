
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <linux/kthread.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/delay.h>

#include "../arch/config.h"
#include "../include/hi35xx_dev.h"
#include "../../proc_msg/mcc_proc_msg.h"

#if defined (SLV_ARCH_HI3531)
#include "../arch/hi3531_opt.c"
#elif defined (SLV_ARCH_HI3532)
#include "../arch/hi3532_opt.c"
#elif defined (SLV_ARCH_HI3535)
#include "../arch/hi3535_opt.c"
#else
#error "Error: No proper slave arch selected!"
#endif

struct hi35xx_dev *g_hi35xx_dev_map[MAX_PCIE_DEVICES] = {0};
EXPORT_SYMBOL(g_hi35xx_dev_map);

struct pci_operation *g_local_handler = NULL;
EXPORT_SYMBOL(g_local_handler);

static char version[] __devinitdata = KERN_INFO "" HI35xx_DEV_MODULE_NAME ":" HI35xx_DEV_VERSION "\n";

static int is_host(void)
{
	return 0;
}

static struct hi35xx_dev *alloc_hi35xxdev(void)
{
	struct hi35xx_dev *dev;
	int size;

	size = sizeof(*dev);
	dev = kmalloc(size, GFP_KERNEL);
	if (!dev) {
		HI_TRACE(HI35XX_ERR, "Alloc hi35xx_dev failed!");
		return NULL;
	}

	memset(dev, 0, size);

	return dev;
}

struct hi35xx_dev *slot_to_hidev(unsigned int slot)
{
	/* For slave, the host remains the one and only remote device */
	return g_hi35xx_dev_map[0];
}

static int hidev_slv_init(struct pci_operation *opt)
{
	struct pci_operation * pci_opt = opt;

	if (NULL == pci_opt) {
		HI_TRACE(HI35XX_ERR, "hidev_slv_init failed, pci_operation is NULL!");
		return -1;
	}

	pci_opt->is_host = is_host;
	pci_opt->slot_to_hidev = slot_to_hidev;
	pci_opt->remote_device_number = 1;

	return 0;
}

static void hidev_slv_exit(void)
{
}

static int __init hi35xx_pcie_module_init(void)
{
	int ret = 0;

	printk(version);

	g_local_handler = kmalloc(sizeof(struct pci_operation), GFP_KERNEL);
	if (!g_local_handler) {
		HI_TRACE(HI35XX_ERR, "alloc pci_operation failed!");
		return -ENOMEM;
	}

	ret = hidev_slv_init(g_local_handler);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "hidev_slv_init failed!");
		goto hidev_slv_init_err;
	}

	ret = pci_arch_init(g_local_handler);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "pci_arch_init failed!");
		goto pci_arch_init_err;
	}

	g_hi35xx_dev_map[0] = alloc_hi35xxdev();
	if (!g_hi35xx_dev_map[0]) {
		HI_TRACE(HI35XX_ERR, "alloc hi-device err!");
		ret = -ENOMEM;
		goto allo_hidev_err;
	}

	g_hi35xx_dev_map[0]->controller = get_pcie_controller(0);
	g_hi35xx_dev_map[0]->slot_index = 0;

	init_timer(&(g_hi35xx_dev_map[0]->timer));

#ifdef MCC_PROC_ENABLE
	if (mcc_proc_init()) {
		HI_TRACE(HI35XX_ERR, "init proc failed!");
		ret = -1;
		goto proc_init_err;
	}

	mcc_create_proc("mcc_info", NULL, NULL);

	ret = mcc_init_sysctl();
	if (ret) {
		HI_TRACE(HI35XX_ERR, "create mcc sys ctr node failed");
		goto mcc_sysctl_init_err;
	}
#endif /* MCC_PROC_ENABLE */

	return 0;

#ifdef MCC_PROC_ENABLE
mcc_sysctl_init_err:
	mcc_remove_proc("mcc_info");
	mcc_proc_exit();
proc_init_err:
	kfree(g_hi35xx_dev_map[0]);
#endif
allo_hidev_err:
	pci_arch_exit();
pci_arch_init_err:
	hidev_slv_exit();
hidev_slv_init_err:
	kfree(g_local_handler);

	return ret;
}

static void __exit hi35xx_pcie_module_exit(void)
{
#ifdef MCC_PROC_ENABLE
	mcc_exit_sysctl();
	mcc_remove_proc("mcc_info");
	mcc_proc_exit();
#endif

	kfree(g_hi35xx_dev_map[0]);
	pci_arch_exit();
	hidev_slv_exit();
	kfree(g_local_handler);
}

module_init(hi35xx_pcie_module_init);
module_exit(hi35xx_pcie_module_exit);

MODULE_AUTHOR("Hisilicon");   
MODULE_DESCRIPTION("Hisilicon Hi35XX");
MODULE_LICENSE("GPL");                 
MODULE_VERSION("HI_VERSION=" __DATE__);

