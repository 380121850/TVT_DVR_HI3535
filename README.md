# TVT_DVR_HI3535
TW2808

1.交叉编译工具的安装
取 Hi3535_V100R001C01SPC020 里的toolchain，分别安装
arm-hisiv100nptl-linux 和 arm-hisiv200-linux

2.目录说明
hi3535_sdk目录基本和Hi3535_SDK_V1.0.2.0保持一致，方便基于从原来SDK环境直接适配
可以在 hi3535_sdk/osdrv/下的Makefile进行编译，并且内容也和SDK保持一致，对于修改部分，
全部位于对应的 modify下面；

├── binSource    // 原TVT 单板上DUMP出来的二进制
├── hi3535_sdk   // 对应 Hi3535_SDK_V1.0.2.0 
│   ├── drv
│   ├── mpp
│   ├── osdrv
│   └── scripts
└── modify      // 所有对hi3535_sdk的修改都放在这里
    ├── dsm62
    ├── hda1
    ├── modules
    ├── osdrv
    └── rd

3.编译
OSDRV_CROSS有 arm-hisiv100nptl-linux 和 arm-hisiv200-linux两种选择，按需选择
特别注意，按照单板，应该选择 PCI_MODE=none；其它的可以参考SDK里的 hi3535_sdk/osdrv/readme_cn.txt


make OSDRV_CROSS=arm-hisiv200-linux CHIP=hi3535 RLS_VERSION=release PCI_MODE=none all

make OSDRV_CROSS=arm-hisiv100nptl-linux CHIP=hi3535 RLS_VERSION=release PCI_MODE=none all

make ARCH=arm CROSS_COMPILE=arm-hisiv200-linux- menuconfig

4.单板环境准备
单板串口是UART；binSource里放的是最初始从单板DUMP保存出来的各分区的镜像文件（实测只有UBOOT和KERNEL能用，其它带YAFFS2格式的DUMP方式不对，用不了）
单板有1个网口，但是UBOOT和KERNEL下是有2个网口配置的；实际网口只有eth1

在设置好tftp网络环境后，便可以通过如下几个命令重新烧写kernel和rootfs分区；其它分区类似
setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.118;
nand erase 0x0 0x400000;tftp 0x82000000 kernel;nand write 0x82000000 0x0 0x400000
nand erase 0x400000 0x4000000;mw.b 0x82000000 0xff 0x4000000;tftp 0x82000000 rootfs;nand write.yaffs 0x82000000 0x400000 0x4000000

// 烧写新编译出来的内核和ROOTFS
setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.112;nand erase zImage;tftp  0x82000000 uImage_hi3535;nand write 0x82000000 zImage

setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.112;nand erase user;mw.b 0x82000000 0xff 0x4000000;tftp 0x82000000 rootfs_hi3535_2k_4bit.yaffs2;nand write.yaffs 0x82000000 user $(filesize)

setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.112;nand erase user;mw.b 0x82000000 0xff 0x4000000;tftp 0x82000000 rootfs_hi3535_128k.jffs2;nand write 0x82000000 user $(filesize)

setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/mtdblock7 rootfstype=jffs2 rw init=/sbin/init mtdparts=hi_sfc:2M(boot);hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user);'

setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/mtdblock7 rootfstype=yaffs2 rw init=/sbin/init mtdparts=hi_sfc:2M(boot);hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user);'

setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/sdq1 rw rootwait init=/sbin/init mtdparts=hi_sfc:2M(boot);hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user);'


setenv bootargs 'mem=496M console=ttyAMA0,115200 root=/dev/mtdblock2 rootfstype=yaffs2 rw mtdparts=hi_sfc:2M(boot);hinand:4M(kernel),64M(rootfs),28M(user),32M(nva010000);';

setenv mtdparts 'mtdparts=hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user)'
print


// 通过TFTP使用
setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.112;tftp  0x84000000 u-boot-hi3535.bin;go 0x82000000;

setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.112;tftp  0x82000000 uImage_hi3535;tftp  0x84000000 rd.bin;bootm 0x82000000 0x84000000;



// DSM216的启动参数
setenv bootargs 'console=ttyS0,115200 mem=896M syno_hw_version=NVR216 syno_phys_memsize=1024 initrd=0x82000000 root=/dev/md0 rw init=/sbin/init sn=C7LWN09771 mtdparts=hi_sfc:2M(boot);hinand:4M(kernel),64M(rootfs),28M(user),32M(nva010000) hd_power_on_seq=2 ihd_num=5 netif_num=1 flash_size=128;'

setenv ipaddr 192.168.50.81;setenv serverip 192.168.50.118;tftp  0x84000000 rd.bin;tftp  0x82000000 uImage_hi3535;bootm 0x82000000 0x84000000;

// 在linux下挂载NFS
mkdir -p /nfs;ifconfig eth0 192.168.50.86;mount -t nfs 192.168.50.100:/volume3/docker/ /nfs/


setenv bootargs 'mem=496M console=ttyAMA0,115200 root=/dev/mtdblock2 rootfstype=yaffs2 rw mtdparts=hi_sfc:2M(boot);hinand:4M(kernel),64M(rootfs),28M(user),32M(nva010000);';


sf probe 0;mw.b 82000000 ff 100000;tftp 0x82000000 u-boot-hi3535.bin;sf erase 0 100000;sf write 82000000 0 100000	
	 

setenv bootargs 'console=ttyS0,115200 mem=320M syno_hw_version=NVR216 syno_phys_memsize=512 root=/dev/sda1 rw rootwait init=/sbin/init mtdparts=hi_sfc:704k(RedBoot),3008k(zImage),4288k(rd.gz),64k(vendor),64k(RedBoot+Config),-(FIS+directory) hd_power_on_seq=2 ihd_num=5 netif_num=1 flash_size=16;

setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/sda1 rw rootwait init=/sbin/init mtdparts=hi_sfc:2M(boot);hinand:4M(kernel),64M(rootfs),28M(user),32M(nva010000) netif_num=1 flash_size=128;'
setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/sdq1 rw rootwait init=/sbin/init mac=00:00:23:34:45:66 mtdparts=hi_sfc:2M(boot);hinand:1M(RedBoot),8M(zImage),10M(rd.gz),128K(vendor),128K(RedBoot+Config),9M(FIS+directory),-(user);'



setenv bootargs 'console=ttyS0,115200 mem=896M root=/dev/sdq rw rootwait init=/sbin/init mtdparts=hi_sfc:2M(boot);hinand:4M(kernel),64M(rootfs),28M(user),32M(nva010000) netif_num=1 flash_size=128;'


echo "deb http://packages.openmediavault.org/public arrakis main" > /etc/apt/sources.list.d/openmediavault.list

apt-key adv --recv-keys --keyserver keyserver.ubuntu.com 7E7A6C592EF35D13 24863F0C716B980B

export LANG=C
export DEBIAN_FRONTEND=noninteractive
export APT_LISTCHANGES_FRONTEND=none



deb http://mirrors.ustc.edu.cn/debian buster main contrib non-free
deb http://mirrors.ustc.edu.cn/debian buster-updates main contrib non-free
deb http://mirrors.ustc.edu.cn/debian buster-backports main contrib non-free
deb http://mirrors.ustc.edu.cn/debian-security/ buster/updates main contrib non-free

deb http://mirrors.aliyun.com/ubuntu-ports/ xenial main
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial main

deb http://mirrors.aliyun.com/ubuntu-ports/ xenial-updates main
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial-updates main

deb http://mirrors.aliyun.com/ubuntu-ports/ xenial universe
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial universe
deb http://mirrors.aliyun.com/ubuntu-ports/ xenial-updates universe
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial-updates universe

deb http://mirrors.aliyun.com/ubuntu-ports/ xenial-security main
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial-security main
deb http://mirrors.aliyun.com/ubuntu-ports/ xenial-security universe
deb-src http://mirrors.aliyun.com/ubuntu-ports/ xenial-security universe

deb http://mirrors.tuna.tsinghua.edu.cn/armbian/ bionic main bionic-utils bionic-desktop


echo PATH=/usr/local/bin:/usr/bin:/bin:/usr/local/games:/usr/games:$PATH

export PATH=/bin:/sbin:/usr/bin:/usr/sbin:/usr/local/bin:/usr/local/sbin; export LANG=C.UTF-8; omv-salt deploy run --no-color systemd-logind 2>&1

wget -O - https://github.com/OpenMediaVault-Plugin-Developers/packages/raw/master/install | bash