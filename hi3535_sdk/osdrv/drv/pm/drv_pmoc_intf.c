#include <linux/init.h>
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

extern void mpu_init_volt(void);
extern void hi_cpufreq_exit(void);
extern void mpu_exit_clocks(void);
extern void core_init_volt(void);
extern void core_init_hpm(void);

extern void hi_cpu_create_proc(void);
extern int hi_pm_proc_init(void);
extern void hi_cpu_remove_proc(void);
extern void hi_pm_proc_exit(void);

extern int	hi_cpufreq_init(void);

/***********************************************************************/
static int __init pmoc_drv_modinit(void)
{
#ifdef HI_DVFS_CPU_SUPPORT
    mpu_init_clocks();
    mpu_init_volt();
	
    hi_cpufreq_init();
#endif

#ifdef HI_DVFS_CORE_SUPPORT
    //core_init_volt();
#endif

#ifdef HPM_DEBUG
    core_init_hpm();
#endif

    hi_pm_proc_init();
    hi_cpu_create_proc();

    HI_TRACE("Load hi_pmoc.ko success.\t\n");

    return 0;
}

void __exit pmoc_drv_modexit(void)
{
#ifdef HI_DVFS_CPU_SUPPORT
    hi_cpufreq_exit();
    mpu_exit_clocks();
#endif

    hi_cpu_remove_proc();
    hi_pm_proc_exit();

    HI_TRACE(" ok! \n");

    return;
}

#ifdef MODULE
module_init(pmoc_drv_modinit);
module_exit(pmoc_drv_modexit);
#endif

MODULE_LICENSE("GPL");
