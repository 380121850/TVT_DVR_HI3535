# TVT_DVR_HI3535
TW2808

一、交叉编译工具的安装
取 Hi3535_V100R001C01SPC020 里的toolchain，分别安装
arm-hisiv100nptl-linux 和 arm-hisiv200-linux

二、目录说明
hi3535_sdk目录基本和Hi3535_SDK_V1.0.2.0保持一致，方便基于从原来SDK环境直接适配
可以在 hi3535_sdk/osdrv/下的Makefile进行编译，并且内容也和SDK保持一致，对于修改部分，
全部位于对应的 modify下面；

├── binSource  // 原TVT 单板上DUMP出来的二进制
├── hi3535_sdk // 对应 Hi3535_SDK_V1.0.2.0 
│   ├── osdrv
│   │   ├── busybox
│   │   ├── drv
│   │   ├── kernel
│   │   ├── pub
│   │   ├── rootfs_scripts
│   │   ├── toolchain
│   │   ├── tools
│   │   └── uboot
│   └── scripts
└── modify   // 所有对hi3535_sdk的修改都放在这里
    └── osdrv
        ├── busybox
        └── kernel

三、编译
OSDRV_CROSS有 arm-hisiv100nptl-linux 和 arm-hisiv200-linux两种选择，按需选择
特别注意，按照单板，应该选择 PCI_MODE=none

本目录设计思路为一套源代码支持两种工具链编译，因此需要通过编译参数指定不同的工具链。其中arm-hisiv100nptl-linux是uclibc工具链，arm-hisiv200-linux是glibc工具链。具体命令如下
(1)	a.编译不带PCIE、debug版本目录：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 RLS_VERSION=debug PCI_MODE=none all
	b.编译不带PCIE、release版本目录：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 RLS_VERSION=release PCI_MODE=none all
	c.编译PCIE master、release版本目录：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 RLS_VERSION=release PCI_MODE=master all
	d.编译PCIE slave、release版本目录：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 RLS_VERSION=release PCI_MODE=slave all

	注意：
	使用glibc库时：OSDRV_CROSS=arm-hisiv200-linux 替换OSDRV_CROSS=arm-hisiv100nptl-linux。在此不再累述。

(2)清除整个osdrv目录的编译文件：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 clean
	或者
	make OSDRV_CROSS=arm-hisiv200-linux CHIP=hi3535 clean
(3)彻底清除整个osdrv目录的编译文件，除清除编译文件外，还删除已编译好的镜像：
	make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 distclean
	或者
	make OSDRV_CROSS=arm-hisiv200-linux CHIP=hi3535 distclean
(4)单独编译kernel：
	内核源代码目录arch/arm/configs下hi3535有4个config文件，请根据需要进行选择
	hi3535_full_defconfig 		用来打开hi3535小系统及所有外设模块；
	hi3535_full_slave_defconfig 	去掉PCIE模块后的小系统及其他外设模块；
	hi3535_mini_defconfig 		用来制作最小系统参考使用；
	hi3535_dbg_defconfig 		打开debug开关用于调试，包含小系统及所有外设模块；

	待进入内核源代码目录后，执行以下操作
	cp arch/arm/configs/hi3535_full_defconfig .config
	make ARCH=arm CROSS_COMPILE=arm-hisiv100nptl-linux- menuconfig
	make ARCH=arm CROSS_COMPILE=arm-hisiv100nptl-linux- uImage
	或者
	cp arch/arm/configs/hi3535_full_defconfig .config
	make ARCH=arm CROSS_COMPILE=arm-hisiv200-linux- menuconfig
	make ARCH=arm CROSS_COMPILE=arm-hisiv200-linux- uImage
(5)单独编译uboot：
	待进入boot源代码目录后，执行以下操作
	make ARCH=arm CROSS_COMPILE=arm-hisiv100nptl-linux- hi3535_config
	make ARCH=arm CROSS_COMPILE=arm-hisiv100nptl-linux-
	或者
	make ARCH=arm CROSS_COMPILE=arm-hisiv200-linux- hi3535_config
	make ARCH=arm CROSS_COMPILE=arm-hisiv200-linux-

