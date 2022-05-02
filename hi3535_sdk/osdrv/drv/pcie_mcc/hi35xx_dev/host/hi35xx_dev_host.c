
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

#if defined (HOST_ARCH_HI3531)
#include "../arch/hi3531_opt.c"
#elif defined (HOST_ARCH_HI3535)
#include "../arch/hi3535_opt.c"
#else
#error "Error: No proper host arch selected!"
#endif

struct hi35xx_dev *g_hi35xx_dev_map[MAX_PCIE_DEVICES] = {0};
EXPORT_SYMBOL(g_hi35xx_dev_map);

struct pci_operation *g_local_handler = NULL;
EXPORT_SYMBOL(g_local_handler);

static char version[] __devinitdata = KERN_INFO "" HI35xx_DEV_MODULE_NAME " :" HI35xx_DEV_VERSION "\n";

static struct pci_device_id hi35xx_pci_tbl[] = { 
	{PCI_VENDOR_HI3531, PCI_DEVICE_HI3531, 
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_HI3532, PCI_DEVICE_HI3532,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{PCI_VENDOR_HI3535, PCI_DEVICE_HI3535,
		PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0},
	{0,} 

};                                               
MODULE_DEVICE_TABLE (pci, hi35xx_pci_tbl);

static int is_host(void)
{
	return 1;
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

static struct hi35xx_dev *slot_to_hidev(unsigned int slot)
{
	int i;
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		if (slot == g_hi35xx_dev_map[i]->slot_index) {
			return g_hi35xx_dev_map[i];
		}
	}

	return NULL;
}

static void pci_config_read(struct hi35xx_dev *hi_dev, int offset, u32 *addr)
{
	struct pci_dev *pdev;

	if (hi_dev && hi_dev->pdev) {
		pdev = hi_dev->pdev;
	        pci_read_config_dword(pdev, offset, addr);
	}
}

static void pci_config_write(struct hi35xx_dev *hi_dev, int offset, u32 value)
{
	struct pci_dev *pdev;

	if (hi_dev && hi_dev->pdev) {
		pdev = hi_dev->pdev;
	        pci_write_config_dword(pdev, offset, value);
	}
}

static int mk_slot_index(struct pci_dev *pdev)
{
	int slot_index;
	slot_index = pdev->bus->number;
	HI_TRACE(HI35XX_DEBUG, "busnr:%d,devfn:%d,slot:%d.", 
			pdev->bus->number, pdev->devfn, slot_index);

	return slot_index;
}

static int __devinit pci_hidev_probe(struct pci_dev *pdev,
		const struct pci_device_id *pci_id)
{
	int ret = 0;

	struct hi35xx_dev *hi_dev;

	printk(KERN_ERR "PCI device[%d] probed!\n",
			g_local_handler->remote_device_number);

	ret = pci_enable_device(pdev);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "Enable pci device failed,errno=%d", ret);
		return ret;
	}

	ret = pci_request_regions(pdev, "PCI_MCC");
	if (ret) {
		HI_TRACE(HI35XX_ERR, "Request pci region failed,errno=%d", ret);
		goto pci_request_regions_err;
	}

	hi_dev = alloc_hi35xxdev();
	if (!hi_dev) {
		HI_TRACE(HI35XX_ERR, "Alloc hi-device[the %dth one] failed!",
				g_local_handler->remote_device_number);
		ret = -ENOMEM;
		goto alloc_hidev_err;
	}

	hi_dev->pdev = pdev;

	hi_dev->bar0 = pci_resource_start(pdev, 0);
	hi_dev->bar1 = pci_resource_start(pdev, 1);
	hi_dev->bar2 = pci_resource_start(pdev, 2);

	hi_dev->controller = g_local_handler->pcie_controller(hi_dev->bar0);
	hi_dev->slot_index = mk_slot_index(pdev);
	hi_dev->device_id = g_local_handler->pci_vendor_id(hi_dev);

	HI_TRACE(HI35XX_INFO, "hidev# [slot:0x%x] [controller:0x%x] [bar0:0x%08x] "
			"[bar1:0x%08x] [bar2:0x%08x].",
			hi_dev->slot_index, hi_dev->controller, hi_dev->bar0,
			hi_dev->bar1, hi_dev->bar2);

	hi_dev->pci_bar0_virt = (unsigned int)ioremap_nocache(hi_dev->bar0, 0x800000);
	if (!hi_dev->pci_bar0_virt) {
		HI_TRACE(HI35XX_ERR, "hidev[slot:0x%x] bar0 ioremap failed!",
				g_local_handler->remote_device_number);
		ret = -ENOMEM;
		goto bar0_ioremap_err;
	}

	hi_dev->pci_bar1_virt = (unsigned int)ioremap_nocache(hi_dev->bar1, 0x10000);
	if (!hi_dev->pci_bar1_virt) {
		HI_TRACE(HI35XX_ERR, "hidev[slot:0x%x] bar1 ioremap failed!",
				g_local_handler->remote_device_number);
		ret = -ENOMEM;
		goto bar1_ioremap_err;
	}

	pci_set_drvdata(pdev, hi_dev);

	//move_pf_window_bar1(pdev, SYS_CTR_REG);	/* FOR interrupt to slave */
	//g_local_handler->move_pf_window_in(hi_dev, 0x20050000, 0x1000, 1);
	//writel(0x1, hi_dev->pci_bar1_virt + 0xcc);

	ret = g_local_handler->init_hidev(hi_dev);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "Init hidev[index:0x%x] failed.",
				g_local_handler->remote_device_number);
		goto init_hidev_err;
	}

	init_timer(&(hi_dev->timer));

	if(g_hi35xx_dev_map[g_local_handler->remote_device_number]) {
		HI_TRACE(HI35XX_ERR, "error: device[%d] already exists!",
				g_local_handler->remote_device_number);
		ret = -1;
		goto init_hidev_err;
	}

	if (MAX_PCIE_DEVICES <= g_local_handler->remote_device_number) {
		HI_TRACE(HI35XX_ERR, "error: too many devices!");
		ret = -1;
		goto init_hidev_err;
	}

	g_hi35xx_dev_map[g_local_handler->remote_device_number] = hi_dev;
	g_local_handler->remote_device_number++;

	/* return success */
	return 0;

init_hidev_err:
	iounmap((volatile void *)hi_dev->pci_bar1_virt);
bar1_ioremap_err:
	iounmap((volatile void *)hi_dev->pci_bar0_virt);
bar0_ioremap_err:
	kfree(hi_dev);
alloc_hidev_err:
	pci_release_regions(pdev);
pci_request_regions_err:
	pci_disable_device(pdev);

	return ret;
}

static void __devexit pci_hidev_remove(struct pci_dev *pdev)
{
	struct hi35xx_dev *hi_dev = g_hi35xx_dev_map[g_local_handler->remote_device_number];

	if (hi_dev) {	
		iounmap((volatile void *)hi_dev->pci_bar0_virt);
		iounmap((volatile void *)hi_dev->pci_bar1_virt);
	}

	pci_release_regions(pdev);
	kfree(hi_dev);
	pci_disable_device(pdev);
	pci_set_drvdata(pdev, NULL);	

	g_local_handler->remote_device_number--;

	return;
}

#ifdef CONFIG_PM
static int pci_hidev_suspend(struct pci_dev *pdev, pm_message_t state)
{
	return 0;
}

static int pci_hidev_resume(struct pci_dev *pdev)
{
	return 0;
}
#endif

static struct pci_driver pci_hidev_driver = {
	.name		= "PCI_HIDEV_MODULE",
	.id_table	= hi35xx_pci_tbl,
	.probe		= pci_hidev_probe,
	.remove		= pci_hidev_remove,
#ifdef CONFIG_PM
	.suspend	= pci_hidev_suspend,
	.resume		= pci_hidev_resume,
#endif
};

static int hidev_host_init(struct pci_operation * opt)
{
	struct pci_operation *pci_opt = opt;

	if (NULL == pci_opt) {
		HI_TRACE(HI35XX_ERR, "hidev host init failed, pci_operation is NULL!");
		return -1;
	}

	pci_opt->is_host = is_host;
	pci_opt->slot_to_hidev = slot_to_hidev;
	pci_opt->pci_config_read = pci_config_read;
	pci_opt->pci_config_write = pci_config_write;
	pci_opt->remote_device_number = 0;

	return 0;
}

static void hidev_host_exit(void)
{
}

static int __init hi35xx_pcie_module_init(void)
{
	int ret = 0;

	printk(version);

	g_local_handler = kmalloc(sizeof(struct pci_operation), GFP_KERNEL);
	if (!g_local_handler) {
		HI_TRACE(HI35XX_ERR, "alloc pci_operation failed!");
		ret = -1;
	}

	ret = hidev_host_init(g_local_handler);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "hidev_host_init failed!");
		goto host_init_err;
	}

	ret = pci_arch_init(g_local_handler);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "pci arch init failed!");
		goto pci_arch_init_err;
	}

	ret = pci_register_driver(&pci_hidev_driver);
	if (ret) {
		HI_TRACE(HI35XX_ERR, "pci driver register failed!");
		goto pci_register_drv_err;
	}

#ifdef MCC_PROC_ENABLE
	ret = mcc_proc_init();
	if (ret) {
		HI_TRACE(HI35XX_ERR, "init proc failed!");
		goto pci_proc_init_err;
	}

	mcc_create_proc("mcc_info", NULL, NULL);

	ret = mcc_init_sysctl();
	if (ret) {
		HI_TRACE(HI35XX_ERR, "create mcc sys ctr node failed");
		goto mcc_sysctl_init_err;
	}
#endif
	return 0;

#ifdef MCC_PROC_ENABLE
mcc_sysctl_init_err:
	mcc_remove_proc("mcc_info");
	mcc_proc_exit();
#endif
pci_proc_init_err:
	pci_unregister_driver(&pci_hidev_driver);
pci_register_drv_err:
	pci_arch_exit();
pci_arch_init_err:
	hidev_host_exit();
host_init_err:
	if (g_local_handler)
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

	pci_unregister_driver(&pci_hidev_driver);

	pci_arch_exit();

	if (g_local_handler)
		kfree(g_local_handler);

	HI_TRACE(HI35XX_INFO, "module exit!");
}

module_init(hi35xx_pcie_module_init);
module_exit(hi35xx_pcie_module_exit);

MODULE_AUTHOR("Hisilicon");
MODULE_DESCRIPTION("Hisilicon Hi35XX");
MODULE_LICENSE("GPL");
MODULE_VERSION("HI_VERSION=" __DATE__);
