######################################################
# Hi35xx PCI MCC top Makefile
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
ifeq ($(PCIE_KERNEL_MAKE),)
	export PCIE_KERNEL_MAKE	= $(OSDRV_DIR)/kernel/linux-3.4.y/
endif
ifeq ($(PCIE_MCC_TOPD),)
	PCIE_MCC_TOPD	=$(shell pwd)
endif
ifneq ($(VER),2)
	OBJ_MCC_CORE =mcc_core/drv/pci_proto_init_v1.o
else
	OBJ_MCC_CORE =mcc_core/drv/pci_proto_init_v2.o
endif
ifeq ($(SLAVE_CHIP),)
	SLAVE_CHIP = hi3535
endif
ifeq ($(SLAVE_CHIP),hi3535)
	ccflags-y += -DSLV_ARCH_HI3535
	ifneq ($(SOFT_DDRT),no)
		SOFT_DDRT = y
	endif
endif
ifeq ($(SLAVE_CHIP),hi3532)
	ccflags-y += -DSLV_ARCH_HI3532
endif
ifeq ($(SLAVE_CHIP),hi3531)
	ccflags-y += -DSLV_ARCH_HI3531
endif

ifeq ($(target),slave)
	######################################################
	# compile slave ko
	######################################################

	obj-m := hi35xx_dev_slv.o
	obj-m += pcit_dma_slv.o
	obj-m += mcc_drv_slv.o
	obj-m += mcc_usrdev_slv.o
	hi35xx_dev_slv-objs := hi35xx_dev/slave/hi35xx_dev_slave.o proc_msg/mcc_proc_msg.o
	pcit_dma_slv-objs   := dma_trans/dma_trans.o
	mcc_drv_slv-objs    := $(OBJ_MCC_CORE) mcc_core/drv/pci_vdd_ops.o
	mcc_usrdev_slv-objs := mcc_core/hios_mcc/hios_mcc_usrdev.o mcc_core/hios_mcc/hios_mcc.o

else
	######################################################
	# compile host ko
	######################################################

	obj-m := hi35xx_dev_host.o
	obj-m += boot_device.o
	obj-m += pcit_dma_host.o
	obj-m += mcc_drv_host.o
	obj-m += mcc_usrdev_host.o
	hi35xx_dev_host-objs := hi35xx_dev/host/hi35xx_dev_host.o proc_msg/mcc_proc_msg.o
	boot_device-objs     := multi_boot/hios_boot_usrdev.o  multi_boot/ddr_reg_init.o
	pcit_dma_host-objs   := dma_trans/dma_trans.o
	mcc_drv_host-objs    := $(OBJ_MCC_CORE) mcc_core/drv/pci_vdd_ops.o
	mcc_usrdev_host-objs := mcc_core/hios_mcc/hios_mcc_usrdev.o mcc_core/hios_mcc/hios_mcc.o
	make_booter := y
endif

all:
	make -C $(PCIE_KERNEL_MAKE) M=$(PCIE_MCC_TOPD) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) modules
	@cp ./*.ko out/
	@if [ "y" = "$(make_booter)" ];then \
		echo "make application booter"; \
		$(CC) -o out/booter multi_boot/example/boot_test.c; \
		if [ "y" = "$(SOFT_DDRT)" ];then \
			make -C multi_boot/arch/$(SLAVE_CHIP)/ddr_training/ddr_training/; \
			cp multi_boot/arch/$(SLAVE_CHIP)/ddr_training/ddr_training/ddr_training.bin out/; \
			make -C multi_boot/arch/$(SLAVE_CHIP)/ddr_training/pc_jump; \
			cp multi_boot/arch/$(SLAVE_CHIP)/ddr_training/pc_jump/pc_jump.bin out/; \
			./mkbooter.sh; \
			rm out/pc_jump.bin out/ddr_training.bin;\
		fi \
	fi

clean:
	@rm $(PCIE_MCC_TOPD)/out/* -f
	@find $(PCIE_MCC_TOPD) -name '*.o' -o -name '.*.cmd' | xargs rm -f
	@find $(PCIE_MCC_TOPD) -name 'modules.order' | xargs rm -f
	@find $(PCIE_MCC_TOPD) -name 'Module.symvers' | xargs rm -f
	@find $(PCIE_MCC_TOPD) -name '.tmp_versions' | xargs rm -rf
	@find $(PCIE_MCC_TOPD) -name '*.mod.c' | xargs rm -rf
	@find $(PCIE_MCC_TOPD) -name '*.ko' | xargs rm -rf
	@make -C multi_boot/arch/hi3535/ddr_training/ddr_training/ clean
	@make -C multi_boot/arch/hi3535/ddr_training/pc_jump clean

