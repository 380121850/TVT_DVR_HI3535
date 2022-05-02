
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
#include <asm/processor.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>

#include "../../ddr_reg_init.h"

#define MAX_PCIE_CFG_LEN_TO_SAVE	(256/4)	

#define CRG_REG_BASE		0x20030000
#define SYS_CTR_REG_BASE	0x20050000

#define PERI_CRG10		0x28
#define HI3531_SC_SYSRES	0x4
#define HI3532_SC_SYSRES	0x4
#define HI3535_SC_SYSRES	0x4

#define HI3535_SYSBOOT9		0x154

#define HI3535_SRAM_BASE	0x4010000

#define HI3531_DEV_ID		0x353119e5
#define HI3532_DEV_ID		0x353219e5
#define HI3535_DEV_ID		0x353519e5

#define HI3531_UNRESET_BIT	(1<<5)
#define HI3532_UNRESET_BIT	(1<<4)
#define HI3535_UNRESET_BIT	(1<<0)

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
static int store_pci_context(struct hi35xx_dev *hi_dev)
{
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

	for (i = 0; i < MAX_PCIE_CFG_LEN_TO_SAVE; i++) {
		g_local_handler->pci_config_read(hi_dev, i*4, pci_ptr);
		pci_ptr++;
	}

	return 0;
}

/*
 * After a device was reset, we need to re-configure the pci space
 * with the information stored before.[0x0 ~ 0x91c]
 */
static int restore_pci_context(struct hi35xx_dev *hi_dev)
{
	unsigned int i = 0;
	unsigned int *pci_ptr = NULL;

	struct pci_cfg_context *tmp_cfg_context;

	msleep(10);

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
		g_local_handler->pci_config_write(hi_dev, i*4, *pci_ptr);
		pci_ptr ++;
	}

	return 0;
}


static int start_device(struct hi35xx_dev *hi_dev)
{
	unsigned int device_type = 0x0;
	unsigned int value = 0;
	int ret = 0;

	g_local_handler->move_pf_window_in(hi_dev, CRG_REG_BASE, 0xffff, 1);

	device_type = hi_dev->device_id;

	switch (device_type) {
		case HI3531_DEV_ID:
			printk(KERN_INFO "Starting device[0x%x][0x%x]\n",
					HI3535_DEV_ID, hi_dev->slot_index);
			value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
			value &= ~HI3531_UNRESET_BIT;
			writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);
			break;
		case HI3532_DEV_ID:
			printk(KERN_INFO "Starting device[0x%x][0x%x]\n",
					HI3535_DEV_ID, hi_dev->slot_index);
			value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
			value &= ~HI3532_UNRESET_BIT;
			writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);
			break;
		case HI3535_DEV_ID:
			printk(KERN_INFO "Starting device[0x%x][0x%x]\n",
					HI3535_DEV_ID, hi_dev->slot_index);
			value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
			value &= ~HI3535_UNRESET_BIT;
			writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);

			/* clear PCIE MCC handshake flag */
			g_local_handler->move_pf_window_in(hi_dev,
					SYS_CTR_REG_BASE, 0xffff, 1);
			writel(0x0, hi_dev->pci_bar1_virt + HI3535_SYSBOOT9);
			break;
		default:
			boot_trace(BOOT_ERR, "unknow chip type, "
					"start dev[%d] failed!",
					hi_dev->slot_index);
			ret = -1;
	}

	g_local_handler->move_pf_window_in(hi_dev, SYS_CTR_REG_BASE, 0xffff, 1);

	return ret;
}

/*
 * reset the slave board
 */
static int reset_device(struct hi35xx_dev *hi_dev)
{
	unsigned int device_type = 0x0;

	g_local_handler->move_pf_window_in(hi_dev, SYS_CTR_REG_BASE, 0xffff, 1);

	device_type = hi_dev->device_id;

	switch (device_type) {
		case HI3531_DEV_ID:
			writel(0xeeeeffff, hi_dev->pci_bar1_virt + HI3531_SC_SYSRES);
			break;
		case HI3532_DEV_ID:
			writel(0xeeeeffff, hi_dev->pci_bar1_virt + HI3532_SC_SYSRES);
			break;
		case HI3535_DEV_ID:
			writel(0xeeeeffff, hi_dev->pci_bar1_virt + HI3535_SC_SYSRES);
			break;
		default:
			boot_trace(BOOT_ERR, "unknow chip type, "
					"reset dev[%d] failed!",
					hi_dev->slot_index);
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
	printk(KERN_INFO "wait ......\n");

	restore_pci_context(hi_dev);

	if (DEVICE_CHECKED_FLAG == hi_dev->vdd_checked_success)
		hi_dev->vdd_checked_success = DEVICE_RESTART_FLAG;

	return 0;
}

static int transfer_data(struct hi35xx_dev *hi_dev, struct boot_attr *attr)
{
	unsigned int target_addr = 0;

	boot_trace(BOOT_INFO, "transfer data called[target_addr:0x%x].", attr->dest);

	target_addr = attr->dest;

	g_local_handler->move_pf_window_in(hi_dev, target_addr, 0xffff, 0);

	if (copy_from_user((void *)hi_dev->pci_bar0_virt, (void *)attr->src, attr->len))
	{
		boot_trace(BOOT_ERR, "transfer data to device failed.");
		return -1;
	}

	boot_trace(BOOT_INFO, "data transfered!");
	return 0;
}

/*
 * Structure of file booter
 *
 *	**************** \					\
 *	**************** |					|
 *	**************** | booter code [16 KBytes]		|
 *	**************** |					|
 *	**00000000000000 /					|
 *	^^^^000000000000 > pc jump code [512 Bytes]		|total:24 KBytes
 *	################ \					|
 *	################ |					|
 *	################ | ddr training [8 KBytes - 512 Bytes]	|
 *	################ |					|
 *	###0000000000000 /					/
 */
#define FILE_BOOTER_SIZE	0x6000	/* 24 KBytes */
#define PC_JUMP_OFFSET		0x4000	/* 4 KBytes */
#define PC_JUMP_SIZE		0x200	/* 512 Bytes */
#define DDRT_CODE_OFFSET	(PC_JUMP_OFFSET + PC_JUMP_SIZE)
#define DDRT_CODE_SIZE		(FILE_BOOTER_SIZE - DDRT_CODE_OFFSET)

#define INNER_RAM_SIZE		0x2800	/* 10 KBytes */
#define DDRT_OFFSET_IN_RAM	0xc00	/* addr ahead is reserved for stack */

static int ddr_soft_training(struct hi35xx_dev *hi_dev)
{
	char *pfile_buf;
	struct file *fp;
	mm_segment_t old_fs;
	loff_t pos = 0;
	unsigned int read_count = 0;

	printk(KERN_INFO "# start DDR training!!\n");

	/* size of file booter is always 24KB */
	pfile_buf = kmalloc(FILE_BOOTER_SIZE, GFP_KERNEL);
	memset(pfile_buf, 0, FILE_BOOTER_SIZE);
	old_fs = get_fs();
	set_fs(KERNEL_DS);
	fp = filp_open("./booter", O_RDONLY, 0);
	if (IS_ERR(fp)) {
		set_fs(old_fs);
		kfree(pfile_buf);
		boot_trace(BOOT_ERR, "open booter failed!");
		return IS_ERR(fp);
	}
	read_count = fp->f_op->read(fp, pfile_buf, FILE_BOOTER_SIZE, &pos);
	set_fs(old_fs);
	filp_close(fp, 0);

	boot_trace(BOOT_DEBUG, "Size of booter: 0x%x", read_count);
	boot_trace(BOOT_DEBUG, "pc_jump: addr[0x%x]:0x%x  0x%x",
			(unsigned int)pfile_buf + PC_JUMP_OFFSET,
			*((unsigned int *)(pfile_buf + PC_JUMP_OFFSET)),
			*((unsigned int *)(pfile_buf + PC_JUMP_OFFSET + 4)));

	/* map pcie window to DDR in slave side */
	g_local_handler->move_pf_window_in(hi_dev, 0x80000000, 0xfffff, 0);

	/* copy pc jump code to slave DDR base */
	memcpy((void *)hi_dev->pci_bar0_virt, pfile_buf + PC_JUMP_OFFSET, 32);

	boot_trace(BOOT_DEBUG, "ddr_training: ddr[0x%x]:0x%x  0x%x",
			(unsigned int)pfile_buf + DDRT_CODE_OFFSET,
			*((unsigned int *)(pfile_buf + DDRT_CODE_OFFSET)),
			*((unsigned int *)(pfile_buf + DDRT_CODE_OFFSET + 4)));

	/* map pcie window to internal RAM in slave side */
	g_local_handler->move_pf_window_in(hi_dev,
			HI3535_SRAM_BASE & (~0x7fffff),
			0xfffff,
			0);

	g_local_handler->move_pf_window_out(hi_dev, 0x88000000, 0xffff, 0);

	printk(KERN_INFO "DDRT code to cpy [0x%x bytes]\n", read_count - DDRT_CODE_OFFSET);

	/*
	 * copy DDR training code to slave RAM
	 * starts at offset 0xc00
	 */
	memcpy((void *)(hi_dev->pci_bar0_virt
				+ (HI3535_SRAM_BASE
					- (HI3535_SRAM_BASE & (~0x7fffff))
					+ DDRT_OFFSET_IN_RAM)),
			pfile_buf + DDRT_CODE_OFFSET,
			((read_count - DDRT_CODE_OFFSET)
				< (INNER_RAM_SIZE - DDRT_OFFSET_IN_RAM)
				? (read_count - DDRT_CODE_OFFSET)
				: (INNER_RAM_SIZE - DDRT_OFFSET_IN_RAM)));

	kfree(pfile_buf);

	/* start CPU0 of slave side, to start DDR training */
	g_local_handler->move_pf_window_in(hi_dev, CRG_REG_BASE, 0xffff, 1);
	{
		unsigned int value;
		value = readl(hi_dev->pci_bar1_virt + PERI_CRG10);
		boot_trace(BOOT_DEBUG, "PERI_CRG10: 0x%x [before cpu unreset]",
				value);
		value &= ~HI3535_UNRESET_BIT;
		writel(value, hi_dev->pci_bar1_virt + PERI_CRG10);
		boot_trace(BOOT_DEBUG, "PERI_CRG10: 0x%x [after cpu unreset]",
				value);
	}

	/* wait here until slave ddr training is done! */
	{
		unsigned int value = 0;
		int try_count = 0;
		do {
			if (50 <= ++try_count) {
				boot_trace(BOOT_ERR, "dev[%d] DDR training timeout!",
						hi_dev->slot_index);
				return -1;
			}

			g_local_handler->move_pf_window_in(hi_dev,
					SYS_CTR_REG_BASE, 0xffff, 1);
			value = readl(hi_dev->pci_bar1_virt + HI3535_SYSBOOT9);
			msleep(100);

		} while (0x8080 != value);
	}

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
		boot_trace(BOOT_ERR, "kmalloc for ddr scripts failed.");
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
	ret = copy_from_user(kernel_buf, (void *)attr->src + 0x40, 0x1000);
	if (ret) {
		boot_trace(BOOT_ERR, "cpy ddr scripts from usr failed.");
		kfree(kernel_buf);
		return -1;
	}

	pentry = (struct regentry *)kernel_buf;

	boot_trace(BOOT_INFO, "copy script from usr done.");
	boot_trace(BOOT_INFO, "about to init registers.");

	/* init ddr */
	pcie_init_registers(pentry, hi_dev, 0);

	kfree(kernel_buf);

	/* Need to do DDR soft training ? */
	if (HI3535_DEV_ID == hi_dev->device_id) {
		if (ddr_soft_training(hi_dev)) {
			boot_trace(BOOT_ERR, "slave ddr training failed!");
			return -1;
		}
	}

	return 0;
}

void pcie_arch_init(struct pcie_boot_opt * boot_opt)
{
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

