/* 
   Copyright (c) 2011 Hisilicon Co., Ltd.
   ..................

author: wucaiyuan
 */


#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>

#include "boot_test.h"

#define MAX_DEVICE_NUM	32
#define MAX_PATH_LEN	128

#define MAX_UBOOT_SIZE	0x100000
#define MAX_UIMAGE_SIZE 0x700000
#define MAX_FS_SIZE	0x700000

#define UBOOT_BASE	0x80000000
#define UIMAGE_BASE	0x81000000
#define INITRD_FS_BASE	0x82000000

#define ACTION_NONE		0x0
#define ACTION_START_DEVICE	(0x1<<0)
#define ACTION_RESET_DEVICE	(0x1<<1)
#define ACTION_DDRT		(0x1<<2)

static int reset_device(int fd, struct boot_attr *attr)
{
	int i = 0;
	int ret = 0;
	struct boot_attr attr_arg;

	memset(&attr_arg, 0, sizeof(attr_arg));

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr->remote_devices[i].id)
			break;

		attr_arg.id = attr->remote_devices[i].id;

		printf("reset device[%d][].\n", attr_arg.id);

		ret = ioctl(fd, HI_RESET_TARGET_DEVICE, &attr_arg);
		if (ret) {
			printf("reset device[%d][] failed.\n", attr_arg.id);
			return -1;
		}
	}

	return 0;
}

static int unreset_slave_devices(int fd, struct boot_attr *attr)
{
	int i = 0;
	int ret = 0;
	struct boot_attr attr_arg;

	memset(&attr_arg, 0, sizeof(attr_arg));

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr->remote_devices[i].id)
			break;

		attr_arg.id = attr->remote_devices[i].id;

		printf("start device[%d][].\n", attr_arg.id);

		ret = ioctl(fd, HI_START_TARGET_DEVICE, &attr_arg);
		if (ret) {
			printf("start device[%d][] failed.\n", attr_arg.id);
			return -1;
		}
	}

	return 0;
}

static int transfer_fs_to_devices(int fd, struct boot_attr *attr)
{
	int data_fd;
	int i = 0;
	int count = 0;
	int ret = 0;
	void *buf = NULL;
	struct boot_attr attr_arg;

	memset(&attr_arg, 0, sizeof(attr_arg));

	data_fd = open("/hisi-pci/cramfs.initrd.img", O_RDWR);
	if (-1 == data_fd) {
		printf("open hisi-pci/cramfs.initrd.img failed!\n");
		return -1;
	}

	buf = malloc(MAX_FS_SIZE);
	if (NULL == buf) {
		printf("malloc for cramfs.initrd.img failed!\n");
		ret = -1;
		goto T_FS_ERR;
	}

	count = read(data_fd, buf, MAX_FS_SIZE);

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr->remote_devices[i].id)
			break;

		attr_arg.id = attr->remote_devices[i].id;
		//attr_arg.type = 1;
		attr_arg.len = count;
		attr_arg.src = (unsigned int)buf;
		attr_arg.dest =	INITRD_FS_BASE;

		ret = ioctl(fd, HI_PCIE_TRANSFER_DATA, &attr_arg);
		if (ret) {
			printf("pcie transfer cramfs.initrd.img to device[%d] failed!\n", attr_arg.id);
			break;
		}
	}

	free(buf);

T_FS_ERR:
	close(data_fd);
	return ret;
}

static int transfer_uImage_to_devices(int fd, struct boot_attr *attr)
{
	int data_fd;
	int i = 0;
	int count = 0;
	int ret = 0;
	void *buf = NULL;
	struct boot_attr attr_arg;

	memset(&attr_arg, 0, sizeof(attr_arg));

	data_fd = open("/hisi-pci/uImage", O_RDWR);
	if (-1 == data_fd) {
		printf("open hisi-pci/uImage failed!\n");
		return -1;
	}

	buf = malloc(MAX_UIMAGE_SIZE);
	if (NULL == buf) {
		printf("malloc for uImage failed!\n");
		ret = -1;
		goto T_UIMAGE_ERR;
	}

	count = read(data_fd, buf, MAX_UIMAGE_SIZE);

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr->remote_devices[i].id)
			break;

		attr_arg.id = attr->remote_devices[i].id;
		//attr_arg.type = 1;
		attr_arg.len = count;
		attr_arg.src = (unsigned int)buf;
		attr_arg.dest = UIMAGE_BASE;

		ret = ioctl(fd, HI_PCIE_TRANSFER_DATA, &attr_arg);
		if (ret) {
			printf("pcie transfer uImage to device[%d] failed!\n", attr_arg.id);
			break;
		}
	}

	free(buf);

T_UIMAGE_ERR:
	close(data_fd);
	return ret;
}

static int transfer_uboot_to_devices(int fd, struct boot_attr *attr)
{
	int data_fd;
	int i = 0;
	int count = 0;
	int ret = 0;
	void *buf = NULL;
	struct boot_attr attr_arg;

	memset(&attr_arg, 0, sizeof(attr_arg));

	data_fd = open("/hisi-pci/u-boot.bin", O_RDWR);
	if (-1 == data_fd) {
		printf("open hisi-pci/u-boot.bin failed!\n");
		return -1;
	}

	buf = malloc(MAX_UBOOT_SIZE);
	if (NULL == buf) {
		printf("malloc for uboot failed!\n");
		ret = -1;
		goto T_UBOOT_ERR;
	}

	count = read(data_fd, buf, MAX_UBOOT_SIZE);

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr->remote_devices[i].id)
			break;

		attr_arg.id = attr->remote_devices[i].id;
		attr_arg.type = attr->type;
		attr_arg.type |= NEED_DDR_INIT;
		attr_arg.len = count;
		attr_arg.src = (unsigned int)buf;
		attr_arg.dest = UBOOT_BASE;

		ret = ioctl(fd, HI_PCIE_TRANSFER_DATA, &attr_arg);
		if (ret) {
			printf("pcie transfer u-boot.bin to device[%d] failed!\n", attr_arg.id);
			break;
		}
	}

	free(buf);
T_UBOOT_ERR:
	close(data_fd);
	return ret;
}

int main(int argc, char *argv[])
{
	int ret = 0;
	int dev_fd;
	int data_fd;

	int i = 0;

	int count = 0;

	struct boot_attr attr;

	void *buf;

	int action = ACTION_NONE;

	printf("hi35xx pcie boot application \n");

	if ((argc < 2) || (argc > 3)) {
		printf("input parameter err!\n");
		printf("usage:\n");
		printf("./booter start_device\n");
		printf(" or\n");
		printf("./booter reset_device\n");
		return -1;
	}
	
	if (0 == strcmp(argv[1], "start_device")) {
		action |= ACTION_START_DEVICE;
	} else if (0 == strcmp(argv[1], "reset_device")) {
		action |= ACTION_RESET_DEVICE;
	} else {
		printf("input parameter err!\n");
		printf("usage:\n");
		printf("./booter start_device\n");
		printf(" or\n");
		printf("./booter reset_device\n");
		return -1;
	}

	if (3 == argc) {
		if (0 == strcmp(argv[2], "ddr_soft_training")) {
			printf("DDR soft training !\n");
			action |= ACTION_DDRT;
		}
	}

	memset(&attr, 0, sizeof(attr));

	dev_fd = open("/dev/hisi_boot", O_RDWR);
	if (-1 == dev_fd) {
		printf("open /dev/hisi_boot failed!\n");
		return -1;
	}

	ret = ioctl(dev_fd, HI_GET_ALL_DEVICES, &attr);
	if (ret) {
		printf("get pcie devices information failed!\n");
		goto out;
	}

	for (i = 0; i < MAX_DEVICE_NUM; i++) {
		if (0 == attr.remote_devices[i].id) {
			if (0 == i) {
				printf("no slave device connect to host.\n");
				goto out;
			}

			break;
		}
		printf("we get device %d[%x].\n", attr.remote_devices[i].id,
				attr.remote_devices[i].dev_type);
	}

	if (action & ACTION_START_DEVICE) {
		if (action & ACTION_DDRT)
			attr.type |= NEED_DDR_TRAINING;

		ret = transfer_uboot_to_devices(dev_fd, &attr);
		if (ret) {
			printf("tranfer uboot to devices failed.\n");
			goto out;
		}

		ret = transfer_uImage_to_devices(dev_fd, &attr);
		if (ret) {
			printf("tranfer uImage to devices failed.\n");
			goto out;
		}

		ret = transfer_fs_to_devices(dev_fd, &attr);
		if (ret) {
			printf("tranfer fs_img to devices failed.\n");
			goto out;
		}

		ret = unreset_slave_devices(dev_fd, &attr);
		if (ret) {
			printf("unreset devices failed.\n");
			goto out;
		}

		printf("\n All devices start up success!\n");
	}

	if (action & ACTION_RESET_DEVICE) {
		ret = reset_device(dev_fd, &attr);
		if (ret) {
			printf("reset device failed!\n");
			goto out;
		}
		printf("device reset done!\n");
	}

out: 
	close(dev_fd);

	return ret;
}

