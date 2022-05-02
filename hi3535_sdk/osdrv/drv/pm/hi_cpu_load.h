/*
 * Generic OPP Interface
 *
 * Copyright (C) 2009-2010 Texas Instruments Incorporated.
 *	Nishanth Menon
 *	Romit Dasgupta
 *	Kevin Hilman
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#ifndef __LINUX_HI_CPU_LOAD_H
#define __LINUX_HI_CPU_LOAD_H

unsigned int get_cpu_max_load(struct cpufreq_policy *cur_policy);
unsigned int get_cpu_average_load(struct cpufreq_policy *cur_policy);
unsigned int get_cpu_load(unsigned int *max_load, unsigned int *avg_load, struct cpufreq_policy *cur_policy);

#endif
