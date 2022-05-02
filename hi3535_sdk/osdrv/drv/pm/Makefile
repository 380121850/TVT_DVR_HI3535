######################################################
# Hi35xx dvfs top Makefile
######################################################

ifeq ($(ARCH),)
	ARCH=arm
endif

ifeq ($(CROSS_COMPILE),)
	CROSS_COMPILE=arm-hisiv100nptl-linux-
endif

ifeq ($(OSDRV_CROSS),)
	CC := $(CROSS_COMPILE)gcc
else
	CC := $(OSDRV_CROSS)-gcc
endif

ifeq ($(OSDRV_DIR),)
	OSDRV_DIR =../../
endif

ifeq ($(PM_KERNEL_MAKE),)
	export PM_KERNEL_MAKE	= $(OSDRV_DIR)/kernel/linux-3.4.y/
endif

ifeq ($(PCIE_MCC_TOPD),)
	PCIE_MCC_TOPD	=$(shell pwd)
endif

MOD_NAME := hi_pmoc

$(MOD_NAME)-y   :=  drv_pmoc_intf.o

#ifeq ($(CFG_HI_DVFS_CPU_SUPPORT),y)
EXTRA_CFLAGS += -DHI_DVFS_CPU_SUPPORT

$(MOD_NAME)-y += clock_mpu.o	\
		 hi_opp_data.o  \
		 opp.o		\
		 hi_dvfs.o	\
		 hi_cpufreq.o   \
		 hi_cpu_load.o  \
		 hi_pm_proc.o
		 #hi_cpufreq_hotplug.o 
#endif

ifeq ($(CFG_HI_DVFS_CORE_SUPPORT),y)
EXTRA_CFLAGS += -DHI_DVFS_CORE_SUPPORT
endif

ifeq ($(CFG_HI_DVFS_PMU_VOLT),y)
EXTRA_CFLAGS += -DHI_DVFS_PMU_VOLT
endif

ifeq ($(CFG_HI_DVFS_GPU_SUPPORT),y)
#EXTRA_CFLAGS += -DHI_DVFS_GPU_SUPPORT
#$(MOD_NAME)-y += drv_pm_gpu_proc.o
endif

#obj-$(CFG_MSP_BUILDTYPE) += $(MOD_NAME).o
obj-m += $(MOD_NAME).o 

#===============================================================================
#   rules
#===============================================================================
.PHONY: all clean install uninstall

all:
	$(AT)make -C $(PM_KERNEL_MAKE) ARCH=arm CROSS_COMPILE=$(CROSS_COMPILE) M=$(CURDIR) modules

clean:
	@find $(PCIE_MCC_TOPD) -name '*.o' -o -name '.*.cmd' | xargs rm -f 
	$(AT)rm -rf  *.mod*
	$(AT)rm -rf  *.ko
	$(AT)rm -rf .tmp_versions
	$(AT)rm -rf  Module.symvers
	$(AT)rm -rf  modules.order

install: all
ifneq ($(CFG_MSP_BUILDTYPE),y)
	$(AT)cp -f $(CURDIR)/$(MOD_NAME).ko $(MODULE_DIR)/
endif

uninstall:
	$(AT)rm -rf $(MODULE_DIR)/$(MOD_NAME).ko

