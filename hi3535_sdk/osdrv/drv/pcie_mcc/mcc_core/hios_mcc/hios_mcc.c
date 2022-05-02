#include <linux/module.h>
#include <linux/poll.h>
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include "../include/pci_proto_common.h"

#include "../include/hi_mcc_usrdev.h"

/*
 * This function has to be called before any other operations
 * on attr. Otherwise some unexpected consequence may happen
 * due to uninitialized variable.
 */
void hios_mcc_handle_attr_init(struct hi_mcc_handle_attr *attr)
{
	int i;

	/* invalid target_id is set */
	attr->target_id = -1;

	/* invalid port id is set */
	attr->port = MAX_MSG_PORTS + 1;

	attr->priority = 1;

	for(i=0; i < MAX_DEV_NUMBER; i++) {
		/* invalid remote device id is set */
		attr->remote_id[i] = -1;
	}
}
EXPORT_SYMBOL(hios_mcc_handle_attr_init);

int hios_mcc_close(hios_mcc_handle_t *handle)
{
	if (NULL == handle) {
		hi_mcc_trace(HI_MCC_ERR, "The mcc handler to close is NULL!");
		return -1;
	}

	pci_vdd_close((void*)handle->pci_handle);
	kfree(handle);
	return 0;
}
EXPORT_SYMBOL(hios_mcc_close);

hios_mcc_handle_t *hios_mcc_open(struct hi_mcc_handle_attr *attr)
{
	hios_mcc_handle_t *handle = NULL;
	unsigned long data;

	if (attr == NULL) {
		hi_mcc_trace(HI_MCC_ERR, "Can not open a invalid handler,"
				" attr is NULL!");
		return NULL;
	}

	if (in_interrupt()) {
		handle = kmalloc(sizeof(hios_mcc_handle_t), GFP_ATOMIC);
	} else {
		handle = kmalloc(sizeof(hios_mcc_handle_t), GFP_KERNEL);
	}

	if (NULL == handle) {
		hi_mcc_trace(HI_MCC_ERR, "Can not open target[0x%x:0x%x],"
				" kmalloc for handler failed!",
				attr->target_id,
				attr->port);
		return NULL;
	}
	
	hi_mcc_trace(HI_MCC_INFO, "hios_mcc_open:attr[target_id:%d,port:%d]",
			attr->target_id, attr->port);

	data = (unsigned long) pci_vdd_open(attr->target_id,
			attr->port,
			attr->priority);
	if (data) {
		handle->pci_handle = data;
		return handle;
	}

	kfree(handle);

	return NULL;
}
EXPORT_SYMBOL(hios_mcc_open);

int hios_mcc_sendto(hios_mcc_handle_t *handle, 
		const void *buf,
		unsigned int len)
{
	if (handle)
		return pci_vdd_sendto((void *)handle->pci_handle,
				buf,
				len,
				PRIORITY_DATA);

	return -1;
}
EXPORT_SYMBOL(hios_mcc_sendto);

int hios_mcc_sendto_user(hios_mcc_handle_t *handle, 
		const void *buf,
		unsigned int len,
		int flag)
{
	if (handle)
		return pci_vdd_sendto((void *)handle->pci_handle,
				buf,
				len,
				flag);

	return -1;
}
EXPORT_SYMBOL(hios_mcc_sendto_user);

int hios_mcc_getopt(hios_mcc_handle_t *handle,
		hios_mcc_handle_opt_t *opt)
{
	pci_vdd_opt_t vdd_opt = {0};

	if (handle) {
		pci_vdd_getopt((void *)handle->pci_handle, &vdd_opt);
		opt->recvfrom_notify = vdd_opt.recvfrom_notify;
		opt->data = vdd_opt.data;
		return 0;
	}

	return -1;
}
EXPORT_SYMBOL(hios_mcc_getopt);


int hios_mcc_setopt(hios_mcc_handle_t *handle, 
		const hios_mcc_handle_opt_t *opt)
{
	pci_vdd_opt_t vdd_opt;

	if (opt) {
		vdd_opt.recvfrom_notify = opt->recvfrom_notify;
		vdd_opt.data = opt->data;
	}

	if (handle)
		return pci_vdd_setopt((void *)handle->pci_handle, &vdd_opt);

	return -1;
}
EXPORT_SYMBOL(hios_mcc_setopt);


int hios_mcc_getlocalid(hios_mcc_handle_t *handle)
{
	return pci_vdd_localid();
}
EXPORT_SYMBOL(hios_mcc_getlocalid);


int hios_mcc_getremoteids(int ids[], hios_mcc_handle_t *handle)
{
	return pci_vdd_remoteids(&ids[0]);
}
EXPORT_SYMBOL(hios_mcc_getremoteids);


int hios_mcc_check_remote(int remote_id, hios_mcc_handle_t *handle)
{
	return pci_vdd_check_remote(remote_id);
}
EXPORT_SYMBOL(hios_mcc_check_remote);


MODULE_LICENSE("GPL");
MODULE_AUTHOR("Hisilicon");


