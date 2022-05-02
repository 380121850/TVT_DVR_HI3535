
/*
 * CPUFreq hotplug governor
 *
 * Copyright (C) 2012 Hisilicon Instruments, Inc.
 *
 * Copyright (C) 2010 Texas Instruments, Inc.
 *   Mike Turquette <mturquette@ti.com>
 *   Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 * Based on ondemand governor
 * Copyright (C)  2001 Russell King
 *           (C)  2003 Venkatesh Pallipadi <venkatesh.pallipadi@intel.com>,
 *                     Jun Nakajima <jun.nakajima@intel.com>
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/cpu.h>
#include <linux/jiffies.h>
#include <linux/kernel_stat.h>
#include <linux/mutex.h>
#include <linux/hrtimer.h>
#include <linux/tick.h>
#include <linux/ktime.h>
#include <linux/sched.h>
#include <linux/err.h>
#include <linux/slab.h>
#include "hi_dvfs.h"


struct prev_load_info {
	cputime64_t prev_cpu_idle;
	cputime64_t prev_cpu_wall;
	cputime64_t prev_cpu_nice;
};

/*
 * percpu mutex that serializes governor limit change with
 * do_dbs_timer invocation. We do not want do_dbs_timer to run
 * when user is changing the governor or limits.
 */
struct mutex timer_mutex;
struct prev_load_info cpu_prev_load_info[2];

/*
 * A corner case exists when switching io_is_busy at run-time: comparing idle
 * times from a non-io_is_busy period to an io_is_busy period (or vice-versa)
 * will misrepresent the actual change in system idleness.  We ignore this
 * corner case: enabling io_is_busy might cause freq increase and disabling
 * might cause freq decrease, which probably matches the original intent.
 */
static inline cputime64_t get_cpu_idle_time(unsigned int cpu, cputime64_t *wall)
{
        u64 idle_time;
#if 0
        u64 iowait_time;
#endif

        /* cpufreq-hotplug always assumes CONFIG_NO_HZ */
        idle_time = get_cpu_idle_time_us(cpu, wall);

#if 0
	/* add time spent doing I/O to idle time */
        if (dbs_tuners_ins.io_is_busy) {
                iowait_time = get_cpu_iowait_time_us(cpu, wall);
                /* cpufreq-hotplug always assumes CONFIG_NO_HZ */
                if (iowait_time != -1ULL && idle_time >= iowait_time)
                        idle_time -= iowait_time;
        }
#endif
        return idle_time;
}


int get_cpu_load(unsigned int *max_load, unsigned int *avg_load, struct cpufreq_policy *cur_policy)
{
	/* combined load of all enabled CPUs */
	unsigned int total_load = 0;
	unsigned int j;

	*max_load = 0;
	*avg_load = 0;

	/*
	 * cpu load accounting
	 * get highest load, total load and average load across all CPUs	
	 */
	for_each_cpu(j, cur_policy->cpus) {
		unsigned int load;
		unsigned int idle_time, wall_time;
		cputime64_t cur_wall_time = 0, cur_idle_time;

		/* update both cur_idle_time and cur_wall_time */
		cur_idle_time = get_cpu_idle_time(j, &cur_wall_time);

		wall_time = (unsigned int)
			(cur_wall_time - cpu_prev_load_info[j].prev_cpu_wall);		
		cpu_prev_load_info[j].prev_cpu_wall = cur_wall_time;

		idle_time = (unsigned int)
			(cur_idle_time - cpu_prev_load_info[j].prev_cpu_idle);
		
		cpu_prev_load_info[j].prev_cpu_idle = cur_idle_time;

		if (unlikely(!wall_time || wall_time < idle_time))
			continue;

		/* load is the percentage of time not spent in idle */
		load = 100 * (wall_time - idle_time) / wall_time;

		/* keep track of combined load across all CPUs */
		total_load += load;

		/* keep track of highest single load across all CPUs */
		if (load > *max_load)
			*max_load = load;

		printk("++++ The NR of cpu!\n");

	}

	/* calculate the average load across all related CPUs */
	*avg_load = total_load / num_online_cpus();

	return 0;
}
EXPORT_SYMBOL(get_cpu_load);


unsigned int get_cpu_max_load(struct cpufreq_policy *cur_policy)
{
	unsigned int max_load;
	unsigned int avg_load;

	get_cpu_load(&max_load, &avg_load, cur_policy);

	return max_load;
}

unsigned int get_cpu_avg_load(struct cpufreq_policy *cur_policy)
{
	unsigned int max_load;
	unsigned int avg_load;

	get_cpu_load(&max_load, &avg_load, cur_policy);

	return avg_load;
}
