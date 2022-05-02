
#include <linux/module.h>
#include <linux/device.h>
#include <linux/mempolicy.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/timer.h>
#include <linux/errno.h>
#include <linux/ioport.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/init.h>
#include <linux/vmalloc.h>
#include <linux/skbuff.h>
#include <linux/delay.h>
#include <linux/crc32.h>
#include <linux/bitops.h>
#include <linux/dma-mapping.h>
#include <asm/processor.h>      /* Processor type for cache alignment. */
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "../ddr_reg_init.h"

#define MAX_PCIE_CFG_LEN_TO_SAVE	(256/4)	

#define CRG_REG_BASE		0x20030000
#define SYS_CTR_REG_BASE	0x20050000

#define PERI_CRG10		0x28
#define SC_SYSRES		0x4

#define HI3531_DEV_ID		0x353119e5
#define HI3532_DEV_ID		0x353219e5

#define HI3531_UNRESET_BIT	(1<<5)
#define HI3532_UNRESET_BIT	(1<<4)

extern void pcie_init_registers(struct regentry reg_table[], struct hi35xx_dev *hi_dev, unsigned int pm); 

struct pci_cfg_context *pci_cfg_entry;
volatile unsigned int *pcie_cfg_base = NULL;

struct reg_info {
	unsigned int reg_offset;
	unsigned int value;
};

/*
 * In order to store compatible configuration and iatu configuration,
 * we store content of the pcie configuration space from offset 0x0
 * to offset 0x256.
 */
static int store_pci_context(struct hi35xx_dev *hi_dev){
	u32 *pci_ptr = NULL;
	int i = 0;
	struct pci_cfg_context *tmp_cfg_context;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		if (hi_dev == g_hi35xx_dev_map[i]) {
			tmp_cfg_context = &pci_cfg_entry[i];
			pci_ptr = (unsigned int *)&tmp_cfg_context->vendor_device_id;

		}
	}

	if (NULL == pci_ptr) {
		boot_trace(BOOT_ERR, "error: pci_ptr is NULL.");
		return -1;
	}

	for(i = 0; i < MAX_PCIE_CFG_LEN_TO_SAVE; i++){
		g_local_handler->pci_config_read(hi_dev, i*4, pci_ptr);
		pci_ptr++;
	}

	return 0;
}

/*
 * After a device was reset, we need to re-configure the pci space
 * with the information stored before.[0x0 ~ 0x91c]
 */
static int restore_pci_context(struct hi35xx_dev *hi_dev){
	unsigned int i = 0;
	unsigned int *pci_ptr = NULL;

	struct pci_cfg_context *tmp_cfg_context;

	msleep(10);

	for (i=0; i < g_local_handler->remote_device_number; i++) {
		if (hi_dev == g_hi35xx_dev_map[i]) {
			tmp_cfg_context = &pci_cfg_entry[i];
			pci_ptr = (unsigned int *)&tmp_cfg_context->vendor_device_id;
		}
	}

	if (NULL == pci_ptr) {
		boot_trace(BOOT_ERR, "error: pci_ptr is NULL.");
		return -1;
	}

	for(i = 0; i < MAX_PCIE_CFG_LEN_TO_SAVE; i++){
		g_local_handler->pci_config_write(hi_dev, i*4, *pci_ptr);
		pci_ptr ++;
	}

	return 0;
}


static int start_device(struct hi35xx_dev *hi_dev)
{
	unsigned int device_type = 0x0;
	unsigned int value = 0;

	g_local_handler->move_pf_window_in(hi_dev, CRG_REG_BASE, 0xffff, 1);

	device_type = hi_dev->device_id;
	if (HI3531_DEV_ID == device_type) {
		value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
		value &= ~HI3531_UNRESET_BIT;
		writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);
	} else if (HI3532_DEV_ID == device_type) {
		value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
		value &= ~HI3532_UNRESET_BIT;
		writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);
	} else {
		boot_trace(BOOT_ERR, "unknow device type!");
		return -1;
	}

	g_local_handler->move_pf_window_in(hi_dev, SYS_CTR_REG_BASE, 0xffff, 1);

	return 0;
}

static int reset_device(struct hi35xx_dev *hi_dev)
{
	unsigned int device_type = 0x0;

	g_local_handler->move_pf_window_in(hi_dev, SYS_CTR_REG_BASE, 0xffff, 1);

	device_type = hi_dev->device_id;

	/*
	 * On hi3531/hi3532, write whatever value to register SC_SYSRES
	 * will cause a soft reset on the system.
	 * So, value 0xeeeeffff here has no specific meaning.
	 */
	if (HI3531_DEV_ID == device_type) {
		writel(0xeeeeffff, hi_dev->pci_bar1_virt + SC_SYSRES);
	} else if (HI3532_DEV_ID == device_type) {
		writel(0xeeeeffff, hi_dev->pci_bar1_virt + SC_SYSRES);
	} else {
		boot_trace(BOOT_ERR, "unknow device type!");
		return -1;
	}

	/*
	 * Wait for the device restarted to be stable,
	 * before we have any write operation.
	 * MAKE SURE there is enough delay for that.
	 */
	{
		int i = 40;
		while (i--)
			udelay(1000);
	}
	printk(KERN_ERR "wait ......\n");

	restore_pci_context(hi_dev);

	if (DEVICE_CHECKED_FLAG == hi_dev->vdd_checked_success)
		hi_dev->vdd_checked_success = DEVICE_RESTART_FLAG;

	return 0;
}

static int transfer_data(struct hi35xx_dev *hi_dev, struct boot_attr *attr)
{
	unsigned int target_addr = 0;

	boot_trace(BOOT_INFO, "target_addr:0x%x", attr->dest);

	target_addr = attr->dest;

	g_local_handler->move_pf_window_in(hi_dev, target_addr, 0xffff, 0);

	boot_trace(BOOT_INFO, "+++++++++++++++++++++++++++++++++++++++");
	if (copy_from_user((void *)hi_dev->pci_bar0_virt,
				(void *)attr->src, attr->len))
	{
		boot_trace(BOOT_ERR, "transfer data to device failed.");
		return -1;
	}

	boot_trace(BOOT_INFO, "data transferred.");

	return 0;
}

static int init_ddr(struct hi35xx_dev *hi_dev, struct boot_attr *attr)
{
	int ret = 0;
	struct regentry *pentry;
	int *kernel_buf;

	boot_trace(BOOT_INFO, "init ddr called!");
	kernel_buf = kmalloc(0x1000, GFP_KERNEL);
	if (NULL == kernel_buf) {
		boot_trace(BOOT_ERR, "kmalloc for ddr scripts failed.\n");
		return -1;
	}

	/*
	 *	uboot starts ------>	***********	-|
	 *				***********	-| uboot first 64 Bytes
	 *	scripts starts ---->	***********	---|
	 *				***********	   |4K Bytes (a hole of the memory,
	 *				***********	   |to stores ddr init scripts)
	 *				***********	---|
	 *	uboot part2 ------->	***********	-|
	 *				***********	-| second half of uboot
	 */
	boot_trace(BOOT_INFO, "after malloc kernel!");
	ret = copy_from_user(kernel_buf, (void *)attr->src + 0x40, 0x1000);
	if (ret) {
		boot_trace(BOOT_ERR, "cpy ddr scripts from usr failed.\n");
		kfree(kernel_buf);
		return -1;
	}

	pentry = (struct regentry *)kernel_buf;

	boot_trace(BOOT_INFO, "cp script from usr done.");
	boot_trace(BOOT_INFO, "about to init registers.");

	/* init ddr */
	pcie_init_registers(pentry, hi_dev, 0);

	kfree(kernel_buf);

	return 0;
}


void pcie_arch_init(struct pcie_boot_opt * boot_opt) {
	boot_opt->init_ddr = init_ddr;
	boot_opt->transfer_data = transfer_data;
	boot_opt->start_device = start_device;
	boot_opt->reset_device = reset_device;
}

int pcie_cfg_store(void)
{
	struct hi35xx_dev *hi_dev;
	int i = 0;
	int cfg_size;

	cfg_size = sizeof(struct pci_cfg_context) * g_local_handler->remote_device_number;
	pci_cfg_entry = kmalloc(cfg_size, GFP_KERNEL);
	if (NULL == pci_cfg_entry) {
		boot_trace(BOOT_ERR, "kmalloc for pci cfg entry failed.\n");
		return -1;
	}

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		hi_dev = g_hi35xx_dev_map[i];
		store_pci_context(hi_dev);
	}
	
	return 0;
}
