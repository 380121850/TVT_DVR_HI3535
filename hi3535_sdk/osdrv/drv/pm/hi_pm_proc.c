#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/kernel.h>
#include <asm/io.h>
#include <mach/platform.h>
#include <mach/hardware.h>
#include <asm/memory.h>
#include <linux/linkage.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <asm/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/errno.h>
#include <mach/clock.h>
#include "hi_dvfs.h"

#define HI_CPU_PROC_NAME "hi_cpu_dvfs"
#define HI_PM_PROC_DIR_NAME "hi_pm"

extern void mpu_init_volt(void);
extern void hi_cpufreq_hotplug_exit(void);
extern void hi_cpufreq_exit(void);
extern void mpu_exit_clocks(void);
extern void core_init_volt(void);
extern void core_init_hpm(void);

extern int	hi_cpufreq_hotplug_init(void);
extern int	hi_cpufreq_init(void);
extern int	cpu_volt_scale(unsigned int volt);
extern int	core_volt_scale(unsigned int volt);
extern int mpu_clk_set_rate(struct clk *clk, unsigned rate);

extern unsigned long cur_cpu_volt;
extern struct clk mpu_ck;

extern struct device_opp *find_device_opp(struct device *dev);

static struct proc_dir_entry *hi_pm_proc_dir = NULL;

//#ifdef HI_DVFS_CPU_PROC_SUPPORT


#if 0
static ssize_t hi_cpu_proc_read(struct file *file, char __user *buffer,
					size_t count, loff_t * offset)
#endif
int hi_dvfs_meminfo_proc_show(struct seq_file *m, void *v)
{
    unsigned long cpu_freq, cpu_volt;

    cpu_volt = cur_cpu_volt;
    cpu_freq = clk_get_rate(&mpu_ck);

    printk("cpu_freq = 0x%ld cpu_volt = 0x%ld  \n", cpu_freq, cpu_volt);

    return 0;
}

static ssize_t hi_cpu_proc_write(struct file *file,
		const char __user *user_buffer, size_t count, loff_t *ppos)
{
	long unsigned int cpu_freq = 0;
	struct device_opp *dev_opp;
	struct opp *temp_opp;
	char buf[10];
	ssize_t retval = 0;
	unsigned int frequency;
	loff_t size;

	size = *ppos;

	printk("============\n");
#if 0
	dev_opp = find_device_opp(&mpu_dev);
	if (IS_ERR(dev_opp))
	{
		int r = PTR_ERR(dev_opp);
		HI_ERR_PM( "OPP not found (%d)\n", __func__);
		return r;
	}
#endif
	if (count > 10) {
		HI_TRACE("The frequency is too big.\n");
		goto error;
	}

	if (copy_from_user(buf, user_buffer, count)) {
	//	retval = -EFAULT;
		goto error;
	}  

	buf[count] = '\0';

	printk("The str is || %s\n", buf);

	retval = strict_strtoul(buf, 10, &cpu_freq);
	if (retval < 0) {
		HI_TRACE("Please input a valid frequency!.\n");
		goto error;
	}

	*ppos = size + count;

#if 0
	list_for_each_entry_rcu(temp_opp, dev_opp->opp_list, node)
	{   
		if (temp_opp->available)
		{   
			if (temp_opp.freq == cpu_freq)
				break;
		}   
		HI_TRACE("Do not found a valid opp frequecy for input frequency!\n");
		return -ENOSYS;
	}   
#endif
	retval = mpu_clk_set_rate(&mpu_ck, cpu_freq);

error:
	return retval;
}


static int hi_cpu_proc_open(struct inode *inode, struct file *file)
{
	return single_open(file, hi_dvfs_meminfo_proc_show, NULL);
}


static struct file_operations hi_cpu_proc_ops = {
	.owner	= THIS_MODULE,
	.open	= hi_cpu_proc_open,
	.read	= seq_read,
	.write	= hi_cpu_proc_write,
	.llseek	= seq_lseek,
	.release= single_release
};


void hi_cpu_create_proc(void)
{
	struct proc_dir_entry *entry;

	entry = create_proc_entry(HI_CPU_PROC_NAME, 0, hi_pm_proc_dir);
	if (!entry)
		return ;

	entry->proc_fops	= &hi_cpu_proc_ops;

	return ;
}
//#endif

void hi_cpu_remove_proc(void)
{
	remove_proc_entry(HI_CPU_PROC_NAME, hi_pm_proc_dir);
	return ;
}

int hi_pm_proc_init(void)
{
	hi_pm_proc_dir = proc_mkdir(HI_PM_PROC_DIR_NAME, NULL);

	if (NULL == hi_pm_proc_dir) {
		HI_TRACE("make proc dir failed!");
		return -1;
	}

	return 0;
}

void hi_pm_proc_exit(void)
{
	remove_proc_entry(HI_PM_PROC_DIR_NAME, NULL);
}
