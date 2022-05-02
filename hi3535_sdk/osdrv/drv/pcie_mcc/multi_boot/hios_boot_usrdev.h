#include <linux/ioctl.h>

#ifndef __HISI_BOOT_USERDEV_HEADER__
#define __HISI_BOOT_USERDEV_HEADER__

#define PCIE_BOOT_MODULE_NAME		"PCIe BOOT SLAVE"
#define PCIE_BOOT_VERSION		__DATE__", "__TIME__

#define MAX_HI_DEVICE_NUM	0x32
#define MAX_HI_DIR_LEN		0x128

#define HI_IOC_BOOT_BASE	'B'

#define HI_GET_ALL_DEVICES		_IOW(HI_IOC_BOOT_BASE, 1, struct boot_attr)
#define HI_PCIE_TRANSFER_DATA		_IOW(HI_IOC_BOOT_BASE, 2, struct boot_attr)
#define HI_START_TARGET_DEVICE		_IOW(HI_IOC_BOOT_BASE, 3, struct boot_attr)
#define HI_RESET_TARGET_DEVICE		_IOW(HI_IOC_BOOT_BASE, 4, struct boot_attr)

#define NEED_DDR_INIT		(1<<0)
#define NEED_DDR_TRAINING	(1<<1)

#define BOOT_DEBUG	4
#define BOOT_INFO	3
#define BOOT_ERR	2
#define BOOT_FATAL	1
#define BOOT_DBG_LEVEL	2
#define boot_trace(level, s, params...) do{ if(level <= BOOT_DBG_LEVEL)\
	printk(KERN_INFO "[%s, %d]: " s "\n", __func__, __LINE__, ##params);\
}while(0)

enum pcie_boot_cfg_offset {
	CFG_DEVICE_ID		= 0x0,
	CFG_COMMAND		= 0x4,
	CFG_CLASS		= 0x8,
	CFG_BAR0		= 0x10,
	CFG_BAR1		= 0x14,
	CFG_BAR2		= 0x18,

	CFG_VIEWPORT		= 0x900,
	CFG_REGION_CTR1		= 0x904,
	CFG_REGION_CTR2		= 0x908,
	CFG_LOWER_BASE		= 0x90C,
	CFG_UPPER_BASE		= 0x910,
	CFG_LIMIT		= 0x914,
	CFG_LOWER_TARGET	= 0x918,
	CFG_UPPER_TARGET	= 0x91C,
};

struct pci_cfg_context {
	unsigned int vendor_device_id;
	unsigned int status_command;
	unsigned int class_revision;
	unsigned int bist_cacheline;
	unsigned int bar0;
	unsigned int bar1;
	unsigned int bar2;
	unsigned int bar3;
	unsigned int bar4;
	unsigned int bar5;
	unsigned int cardbus_ptr;
	unsigned int sys_sub_vendo_id;
	unsigned int expansion_rom_base;
	unsigned int capabity_ptr;
	unsigned int reserverd;
	unsigned int intr_line_num;
	unsigned int fill_len[48];
};

struct hi_device_info {
	unsigned int id;
	unsigned int dev_type;
};

struct boot_attr {
	unsigned int id;
	unsigned int type;
	unsigned int src;
	unsigned int dest;
	unsigned int len;

	struct hi_device_info remote_devices[MAX_HI_DEVICE_NUM];
};

struct pcie_boot_opt {
	int (*reset_device)(struct hi35xx_dev *hi_dev);
	int (*start_device)(struct hi35xx_dev *hi_dev);
	int (*init_ddr)(struct hi35xx_dev *hi_dev, struct boot_attr *attr);
	int (*transfer_data)(struct hi35xx_dev *hi_dev, struct boot_attr *attr);
};

void pcie_arch_init(struct pcie_boot_opt *boot_opt);

#endif
