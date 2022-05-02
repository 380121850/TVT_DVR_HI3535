#include <linux/module.h>
#include <linux/fs.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/miscdevice.h>

#include "../hi35xx_dev/include/hi35xx_dev.h"
#include "hios_boot_usrdev.h"

#ifdef CONFIG_ARCH_GODNET
	#include "arch/hi3531/hi3531_boot_opt.c"
#elif defined CONFIG_ARCH_HI3535
	#include "arch/hi3535/hi3535_boot_opt.c"
#else
	#error "No proper arch selected!\n"
#endif

static struct semaphore handle_sem;

static char version[] __devinitdata = KERN_INFO "" PCIE_BOOT_MODULE_NAME " :" PCIE_BOOT_VERSION "\n";

struct pcie_boot_opt g_boot_opt;

static int pcie_boot_get_devices_info(struct boot_attr *attr)
{
	struct hi35xx_dev *hi_dev;
	int i = 0;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		hi_dev = g_hi35xx_dev_map[i];
		if (hi_dev->slot_index != 0) {
			attr->remote_devices[i].id = hi_dev->slot_index;
			attr->remote_devices[i].dev_type = hi_dev->device_id;
			boot_trace(BOOT_INFO, "device slot[%d],vendor-device id[%x]",
					attr->remote_devices[i].id,
					attr->remote_devices[i].dev_type);
		}
	}

	return 0;
}

static int pcie_boot_transfer_data(struct boot_attr *attr)
{
	struct hi35xx_dev *hi_dev;
	int i = 0;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		hi_dev = g_hi35xx_dev_map[i];
		if (attr->id == hi_dev->slot_index) {
			if (NEED_DDR_INIT & attr->type) {
				boot_trace(BOOT_INFO, "transfer uboot.");

				if (hi_dev->vdd_checked_success
						== DEVICE_RESTART_FLAG)
				{
					if (restore_pci_context(hi_dev)) {
						boot_trace(BOOT_ERR, 
							"restore pci context failed");
						return -1;
					}
				}

				if (g_boot_opt.init_ddr(hi_dev, attr)) {
					boot_trace(BOOT_ERR, "DDR init failed!");
					return -1;
				}

				boot_trace(BOOT_INFO, "ddr initialized.");
			}

			boot_trace(BOOT_INFO, "about to transfer data.");

			if (g_boot_opt.transfer_data(hi_dev, attr)) {
				boot_trace(BOOT_ERR, "cpy data to dev[%d] failed!",
						hi_dev->slot_index);
				return -1;
			}

			return 0;	
		}
	}

	boot_trace(BOOT_ERR, "device[%d] not found.", attr->id);
	return -1;
}

static int pcie_boot_start_device(struct boot_attr *attr)
{
	struct hi35xx_dev *hi_dev;
	int i = 0;

	boot_trace(BOOT_INFO, "starting device[%d]", attr->id);

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		hi_dev = g_hi35xx_dev_map[i];
		if (attr->id == hi_dev->slot_index) {
			g_boot_opt.start_device(hi_dev);
			return 0;
		}
	}

	boot_trace(BOOT_ERR, "device[%d] not found.", attr->id);
	return -1;
}

static int pcie_boot_reset_device(struct boot_attr *attr)
{
	struct hi35xx_dev *hi_dev;
	int i = 0;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		hi_dev = g_hi35xx_dev_map[i];
		if (attr->id == hi_dev->slot_index) {
			g_boot_opt.reset_device(hi_dev);
			return 0;
		}
	}

	boot_trace(BOOT_ERR, "device[%d] not found.", attr->id);
	return -1;
}


static int hisi_boot_usrdev_open(struct inode *inode, struct file *file)
{
	file->private_data = 0;

	sema_init(&handle_sem, 1);

	return 0;
}

static int hisi_boot_usrdev_release(struct inode *inode, struct file *file)
{
	return 0;
}

static int hisi_boot_usrdev_read(struct file *file, char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}

static int hisi_boot_usrdev_write(struct file *file, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	return 0;
}

static long hisi_boot_usrdev_ioctl(struct file *file, unsigned int cmd,
		unsigned long arg)
{
	struct boot_attr attr;

	boot_trace(BOOT_INFO, "ioctl cmd received!");

	if (down_interruptible(&handle_sem)) {
		boot_trace(BOOT_ERR, "acquire handle sem failed!");
		return -1;
	}

	if (copy_from_user((void *)&attr, (void *)arg, sizeof(struct boot_attr)))
	{
		boot_trace(BOOT_ERR, "cpy params from usr failed.");
		up(&handle_sem);
		return -1;
	}

	if (_IOC_TYPE(cmd) == 'B') {
		switch (_IOC_NR(cmd)) {
			case _IOC_NR(HI_GET_ALL_DEVICES):
				boot_trace(BOOT_INFO, "# HI_GET_ALL_DEVICES #");
				pcie_boot_get_devices_info(&attr);

				if (copy_to_user((void *)arg, (void *)&attr,
							sizeof(struct boot_attr)))
				{
					boot_trace(BOOT_ERR, "cpy data to usr space failed.");
					up(&handle_sem);
					return -1;
				}

				up(&handle_sem);
				break;
			case _IOC_NR(HI_PCIE_TRANSFER_DATA):
				boot_trace(BOOT_INFO, "# HI_PCIE_TRANSFER_DATA #");

				if (pcie_boot_transfer_data(&attr)) {
					boot_trace(BOOT_ERR, "pcie transfer data failed.");
					up(&handle_sem);
					return -1;
				}

				up(&handle_sem);
				break;

			case _IOC_NR(HI_START_TARGET_DEVICE):
				boot_trace(BOOT_INFO, "# HI_START_TARGET_DEVICE #");
				if (pcie_boot_start_device(&attr)) {
					boot_trace(BOOT_ERR, "pcie start device failed.");
					up(&handle_sem);
					return -1;
				}

				up(&handle_sem);
				break;

			case _IOC_NR(HI_RESET_TARGET_DEVICE):
				boot_trace(BOOT_INFO, "# HI_RESET_TARGET_DEVICE #");

				if (pcie_boot_reset_device(&attr)) {
					boot_trace(BOOT_ERR, "pcie reset device failed.");
					up(&handle_sem);
					return -1;
				}

				up(&handle_sem);
				break;

			default:
				up(&handle_sem);
				boot_trace(BOOT_ERR, "ioctl cmd not defined!");
				break;
		}
	}

	return 0;
}

static unsigned int hisi_boot_usrdev_poll(struct file *file,
		struct poll_table_struct *table)
{
	return 0;
}

static struct file_operations boot_usrdev_fops = {
	.owner		= THIS_MODULE,
	.open		= hisi_boot_usrdev_open,
	.release	= hisi_boot_usrdev_release,
	.unlocked_ioctl	= hisi_boot_usrdev_ioctl,
	.write		= hisi_boot_usrdev_write,
	.read		= hisi_boot_usrdev_read,
	.poll		= hisi_boot_usrdev_poll,
};


static struct miscdevice hisi_boot_usrdev = {
	.minor	= MISC_DYNAMIC_MINOR,
	.fops	= &boot_usrdev_fops,
	.name	= "hisi_boot"
};

int __init hisi_pci_boot_init(void)
{
	printk(version);

	pcie_arch_init(&g_boot_opt);

	misc_register(&hisi_boot_usrdev);

	return pcie_cfg_store();
}

void __exit hisi_pci_boot_exit(void)
{
	misc_deregister(&hisi_boot_usrdev);
}


module_init(hisi_pci_boot_init);
module_exit(hisi_pci_boot_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hisilicon");

