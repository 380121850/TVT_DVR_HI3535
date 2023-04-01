
##########################################################################################
#	prepare param
##########################################################################################
export OSDRV_DIR=$(shell pwd)
export OSDRV_CROSS
export OSDRV_SIZE ?= full
export CHIP=hi3535
export REG_INFO_BIN
export UBOOT_IMAGE_BIN
export ARCH=arm
export CROSS_COMPILE=$(OSDRV_CROSS)-

export OSDRV_DIR_MODIFY=$(OSDRV_DIR)/../../modify/osdrv/

# if CHIP is support, we set SUPPORT_CHIP = "yes"
SUPPORT_CHIP := "no"

ifeq ($(OSDRV_CROSS), )
$(error you must set OSDRV_CROSS first!)
endif

ifeq ($(CHIP), )
$(error you must set CHIP first!)
endif

ifeq ($(OSDRV_CROSS),arm-hisiv100nptl-linux)
OSDRV_CROSS_LIB=uclibc
BUSYBOX_CFG:=busybox_cfg_hi3535_nptl
TOOLCHAIN_RUNTIME_LIB:=armv7a_soft
TOOLCHAIN_RUNTIME_LIB_C:=lib.uClibc.tgz
export OSDRV_CROSS_CFLAGS:= -march=armv7-a -mcpu=cortex-a9
endif

ifeq ($(OSDRV_CROSS),arm-hisiv200-linux)
OSDRV_CROSS_LIB=glibc
BUSYBOX_CFG:=busybox_cfg_hi3535_v200
TOOLCHAIN_RUNTIME_LIB:=armv7a_soft
TOOLCHAIN_RUNTIME_LIB_C:=lib.glibc.tgz
export OSDRV_CROSS_CFLAGS:= -march=armv7-a -mcpu=cortex-a9
endif

export PUB_IMAGE:=image_$(OSDRV_CROSS_LIB)
export PUB_BOARD:=board_$(OSDRV_CROSS_LIB)
export PUB_ROOTFS:=rootfs_$(OSDRV_CROSS_LIB)

ifeq ($(CHIP),hi3535)#{
ifeq ($(PCI_MODE),slave)
KERNEL_IMAGE_BIN:=uImage_hi3535_slave
CHIP_KERNEL_CFG:=hi3535_full_slave_defconfig
endif
ifeq ($(PCI_MODE),master)
CHIP_KERNEL_CFG:=hi3535_full_defconfig
KERNEL_IMAGE_BIN:=uImage_hi3535_master
endif
ifeq ($(PCI_MODE),none)
KERNEL_IMAGE_BIN:=uImage_hi3535
ifeq ($(RLS_VERSION),debug)
CHIP_KERNEL_CFG:=hi3535_dbg_defconfig
else
CHIP_KERNEL_CFG:=hi3535_full_slave_defconfig
endif
endif

YAFFS2_IMAGE_BIN:=rootfs_hi3535_2k_4bit.yaffs2

CHIP_BOOT_CFG:=hi3535_config
REG_INFO_BIN:=reg_info_hi3535.bin
UBOOT_IMAGE_BIN:=u-boot-hi3535.bin
SUPPORT_CHIP = "yes"
endif ###end hi3535

JFFS2_IMAGE_BIN_64K:=rootfs_$(CHIP)_64k.jffs2
JFFS2_IMAGE_BIN_256K:=rootfs_$(CHIP)_256k.jffs2

ifneq ($(SUPPORT_CHIP), "yes")
$(error $(CHIP) not surpport!)
endif


##########################################################################################
#	set task
##########################################################################################
all: prepare hiboot hikernel hidrv hi_dvfs hirootfs_prepare hibusybox hiboardtools hipctools hirootfs_build

notools: prepare hiboot hikernel hidrv hi_dvfs hirootfs_prepare hibusybox hirootfs_build

clean: hiboot_clean hidrv_clean hi_dvfs_clean hikernel_clean hibusybox_clean hiboardtools_clean hipctools_clean hirootfs_clean
distclean:clean pub_clean

##########################################################################################
#task [0]	prepare
##########################################################################################
prepare:
ifeq ($(PCI_MODE),slave)
ifeq ($(RLS_VERSION),debug)
	$(error parameter error! we just support debug in debug full version!)
endif
endif
ifeq ($(PCI_MODE),master)
ifeq ($(RLS_VERSION),debug)
	$(error parameter error! we just support debug in debug full version!)
endif
endif
	mkdir $(OSDRV_DIR)/pub/$(PUB_IMAGE) -p
	mkdir $(OSDRV_DIR)/pub/bin/$(PUB_BOARD) -p
	mkdir $(OSDRV_DIR)/pub/bin/pc -p

##########################################################################################
#task [1]	build uboot
##########################################################################################
hiboot:
	@echo "---------task [1]	build boot"
	rm $(OSDRV_DIR)/uboot/u-boot-2010.06 -rf
	tar xzf $(OSDRV_DIR)/uboot/u-boot-2010.06.tgz -C $(OSDRV_DIR)/uboot
	find $(OSDRV_DIR)/uboot/u-boot-2010.06 | xargs touch
	cp -rf $(OSDRV_DIR_MODIFY)/uboot/u-boot-2010.06/* $(OSDRV_DIR)/uboot/u-boot-2010.06/
	make -C $(OSDRV_DIR)/uboot/u-boot-2010.06 ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- $(CHIP_BOOT_CFG)
	pushd $(OSDRV_DIR)/uboot/u-boot-2010.06;make ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- -j 20 >/dev/null;popd
	cp $(OSDRV_DIR)/uboot/u-boot-2010.06/u-boot.bin $(OSDRV_DIR)/pub/$(PUB_IMAGE)
ifeq ($(CHIP), hi3535)
	cp $(OSDRV_DIR)/tools/pc_tools/uboot_tools/mkboot.sh $(OSDRV_DIR)/pub/$(PUB_IMAGE)
	cp $(OSDRV_DIR)/tools/pc_tools/uboot_tools/$(REG_INFO_BIN) $(OSDRV_DIR)/pub/$(PUB_IMAGE)
	chmod 777 $(OSDRV_DIR)/pub/$(PUB_IMAGE)/mkboot.sh
	pushd $(OSDRV_DIR)/pub/$(PUB_IMAGE); ./mkboot.sh $(REG_INFO_BIN) $(UBOOT_IMAGE_BIN); popd;
	rm $(OSDRV_DIR)/pub/$(PUB_IMAGE)/u-boot.bin -f
	rm $(OSDRV_DIR)/pub/$(PUB_IMAGE)/mkboot.sh -f
	rm $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(REG_INFO_BIN) -f
endif
hiboot_clean:
	rm $(OSDRV_DIR)/uboot/u-boot-2010.06 -rf

##########################################################################################
#task [2]	build kernel
##########################################################################################
hikernel:
	@echo "---------task [2] build kernel"
	#rm $(OSDRV_DIR)/kernel/linux-3.4.y -rf
	#tar xzf $(OSDRV_DIR)/kernel/linux-3.4.y.tgz -C $(OSDRV_DIR)/kernel/
	find $(OSDRV_DIR)/kernel/linux-3.4.y | xargs touch
	cp $(OSDRV_DIR_MODIFY)/kernel/linux-3.4.y/arch/arm/configs/$(CHIP_KERNEL_CFG) $(OSDRV_DIR)/kernel/linux-3.4.y/arch/arm/configs/$(CHIP_KERNEL_CFG)
	make -C $(OSDRV_DIR)/kernel/linux-3.4.y ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- $(CHIP_KERNEL_CFG)
	pushd $(OSDRV_DIR)/kernel/linux-3.4.y;make ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- -j 20 >/dev/null;popd
	pushd $(OSDRV_DIR)/kernel/linux-3.4.y;make ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- uImage -j 20 >/dev/null;popd
	cp $(OSDRV_DIR)/kernel/linux-3.4.y/arch/arm/boot/uImage $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(KERNEL_IMAGE_BIN)

hikernel_clean:
	#rm $(OSDRV_DIR)/kernel/linux-3.4.y -rf
	pushd $(OSDRV_DIR)/kernel/linux-3.4.y;make ARCH=arm CROSS_COMPILE=$(OSDRV_CROSS)- distclean >/dev/null;popd

##########################################################################################
#task [3]	prepare drv
##########################################################################################
hidrv:
	@echo "---------task [3] build drv"
ifeq ($(PCI_MODE),slave)
	pushd $(OSDRV_DIR)/drv/pcie_mcc;make clean;popd
	pushd $(OSDRV_DIR)/drv/pcie_mcc;make VER=2 target=slave >/dev/null;popd
	mkdir $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci -p
	cp $(OSDRV_DIR)/drv/pcie_mcc/out/*.ko $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci/
	cp $(OSDRV_DIR)/drv/pcie_mcc/include $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci/ -af
endif
ifeq ($(PCI_MODE),master)
	pushd $(OSDRV_DIR)/drv/pcie_mcc;make clean;chmod 777 ./mkbooter.sh;popd
	pushd $(OSDRV_DIR)/drv/pcie_mcc;make VER=2 >/dev/null;popd
	mkdir $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci -p
	cp $(OSDRV_DIR)/drv/pcie_mcc/out/*.ko $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci/
	cp $(OSDRV_DIR)/drv/pcie_mcc/out/booter $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci/
	cp $(OSDRV_DIR)/drv/pcie_mcc/include $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pci/ -af
endif

hidrv_clean:
	make -C $(OSDRV_DIR)/drv/pcie_mcc clean

##########################################################################################
#task [4]	dvfs
##########################################################################################
hi_dvfs:
	@echo "---------task build pm"
	pushd $(OSDRV_DIR)/drv/pm;make clean;popd
	pushd $(OSDRV_DIR)/drv/pm;make;popd
	mkdir $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pm -p
	cp $(OSDRV_DIR)/drv/pm/*.ko $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pm/
	cp $(OSDRV_DIR)/drv/pm/hi_cpu_load.h $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/pm/hi_cpu_load.h -af

hi_dvfs_clean:
	make -C $(OSDRV_DIR)/drv/pm clean

##########################################################################################
#task [4]	prepare rootfs
##########################################################################################
hirootfs_prepare:
	@echo "---------task [4] prepare rootfs"
	tar xzf $(OSDRV_DIR)/rootfs_scripts/rootfs.tgz -C $(OSDRV_DIR)/pub
	mv $(OSDRV_DIR)/pub/rootfs $(OSDRV_DIR)/pub/$(PUB_ROOTFS)
	tar xzf $(OSDRV_DIR)/toolchain/$(OSDRV_CROSS)/runtime_lib/$(TOOLCHAIN_RUNTIME_LIB)/$(TOOLCHAIN_RUNTIME_LIB_C) \
		-C $(OSDRV_DIR)/pub/$(PUB_ROOTFS)
	tar xzf $(OSDRV_DIR)/toolchain/$(OSDRV_CROSS)/runtime_lib/$(TOOLCHAIN_RUNTIME_LIB)/lib.stdc++.tgz \
		-C $(OSDRV_DIR)/pub/$(PUB_ROOTFS)
ifeq ($(CHIP),hi3535)
	cp $(OSDRV_DIR)/kernel/linux-3.4.y/drivers/net/ethernet/stmmac/stmmac.ko $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/hitoe
	chmod 777 $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/hitoe/*
endif

##########################################################################################
#task [5]	build busybox
##########################################################################################
hibusybox:
	@echo "---------task [5] build busybox"
	rm $(OSDRV_DIR)/busybox/busybox-1.16.1 -rf
	tar xzf $(OSDRV_DIR)/busybox/busybox-1.16.1.tgz -C $(OSDRV_DIR)/busybox
	find $(OSDRV_DIR)/busybox/busybox-1.16.1 | xargs touch
	cp $(OSDRV_DIR)/busybox/busybox-1.16.1/$(BUSYBOX_CFG) $(OSDRV_DIR)/busybox/busybox-1.16.1/.config
	cp $(OSDRV_DIR_MODIFY)/busybox/busybox-1.16.1/Makefile $(OSDRV_DIR)/busybox/busybox-1.16.1/
	pushd $(OSDRV_DIR)/busybox/busybox-1.16.1/;make -j 20 >/dev/null;popd
	make -C $(OSDRV_DIR)/busybox/busybox-1.16.1 install
	cp -af $(OSDRV_DIR)/busybox/busybox-1.16.1/_install/* $(OSDRV_DIR)/pub/$(PUB_ROOTFS)

hibusybox_clean:
	rm $(OSDRV_DIR)/busybox/busybox-1.16.1 -rf

##########################################################################################
#task [6]	build board tools
##########################################################################################
hiboardtools:
	@echo "---------task [6] build tools which run on board"
	make -C $(OSDRV_DIR)/tools/board_tools/e2fsprogs
	make -C $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0 CHIP=$(CHIP)
	make -C $(OSDRV_DIR)/tools/board_tools/parted
	cp -af $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0/btools $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/bin
	cp -af $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0/him* $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/bin
	cp -af $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0/hiddrs $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/bin
	cp -af $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0/hil2s $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/bin
	cp -af $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0/hie* $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/bin
	make -C $(OSDRV_DIR)/tools/board_tools/udev-100
	cp $(OSDRV_DIR)/tools/board_tools/gdb/gdb-$(OSDRV_CROSS) $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)
	find $(OSDRV_DIR)/tools/board_tools/mtd-utils/ -name 'configure' | xargs chmod +x
	pushd $(OSDRV_DIR)/tools/board_tools/mtd-utils;make >/dev/null;popd
	cp $(OSDRV_DIR)/tools/board_tools/mtd-utils/bin/arm/* $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)
	rm $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/ubi* -rf
	cp $(OSDRV_DIR)/tools/board_tools/parted/parted-3.0/bin/sbin/parted $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/;
	cp $(OSDRV_DIR)/tools/board_tools/parted/parted-3.0/bin/sbin/partprobe $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/;

hiboardtools_clean:
	make -C $(OSDRV_DIR)/tools/board_tools/e2fsprogs clean
	make -C $(OSDRV_DIR)/tools/board_tools/reg-tools-1.0.0 clean
	make -C $(OSDRV_DIR)/tools/board_tools/parted clean
	make -C $(OSDRV_DIR)/tools/board_tools/udev-100 clean
	pushd $(OSDRV_DIR)/tools/board_tools/mtd-utils;make distclean;popd

##########################################################################################
#task [7]	build pc tools
##########################################################################################
hipctools:
	@echo "---------task [7] build tools which run on pc"
	make -C $(OSDRV_DIR)/tools/pc_tools/mkyaffs2image.300_301_504
	cp $(OSDRV_DIR)/tools/pc_tools/mkyaffs2image.300_301_504/mkyaffs2image504 $(OSDRV_DIR)/pub/bin/pc
	cp $(OSDRV_DIR)/tools/pc_tools/mkfs.jffs2 $(OSDRV_DIR)/pub/bin/pc
	cp $(OSDRV_DIR)/tools/pc_tools/mkfs.cramfs $(OSDRV_DIR)/pub/bin/pc
	cp $(OSDRV_DIR)/tools/pc_tools/mkimage $(OSDRV_DIR)/pub/bin/pc
	cp $(OSDRV_DIR)/tools/pc_tools/mksquashfs $(OSDRV_DIR)/pub/bin/pc

hipctools_clean:
	make -C $(OSDRV_DIR)/tools/pc_tools/mkyaffs2image.300_301_504 clean

##########################################################################################
#task [8]	build rootfs
##########################################################################################
hirootfs_build:
	@echo "---------task [8] build rootfs"
	chmod 777 $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/*
	chmod 777 $(OSDRV_DIR)/pub/bin/pc/*
	rm $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/dev/* -rf
	pushd $(OSDRV_DIR)/pub/$(PUB_ROOTFS);ln -s sbin/init init;popd
	pushd $(OSDRV_DIR)/pub/bin/pc; ./mkfs.jffs2 -d $(OSDRV_DIR)/pub/$(PUB_ROOTFS) -l -e 0x40000 -o $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(JFFS2_IMAGE_BIN_256K);popd
	pushd $(OSDRV_DIR)/pub/bin/pc; ./mkfs.jffs2 -d $(OSDRV_DIR)/pub/$(PUB_ROOTFS) -l -e 0x10000 -o $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(JFFS2_IMAGE_BIN_64K);popd
	pushd $(OSDRV_DIR)/pub/bin/pc; ./mkyaffs2image504 $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/ $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(YAFFS2_IMAGE_BIN) 1 2;popd
	chmod 644 $(OSDRV_DIR)/pub/$(PUB_IMAGE)/$(YAFFS2_IMAGE_BIN)
	find $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/ -name '*svn' | xargs rm -rf
	pushd $(OSDRV_DIR)/pub;tar czf $(PUB_ROOTFS).tgz $(PUB_ROOTFS);rm $(PUB_ROOTFS) -rf;popd
	@echo "---------finish osdrv work"

hirootfs_clean:
	rm $(OSDRV_DIR)/pub/$(PUB_ROOTFS)/ -rf

##########################################################################################
#task [9]	clean pub
##########################################################################################
pub_clean:
	rm $(OSDRV_DIR)/pub/$(PUB_IMAGE)/ -rf
	rm $(OSDRV_DIR)/pub/bin/$(PUB_BOARD)/ -rf
	rm $(OSDRV_DIR)/pub/$(PUB_ROOTFS).tgz -f
	rm $(OSDRV_DIR)/pub/bin/pc -rf
