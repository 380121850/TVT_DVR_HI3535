
#include <linux/interrupt.h>
#include <linux/spinlock.h>
#include <linux/kthread.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/string.h>
#include <linux/delay.h>
#include <linux/timer.h>
#include <linux/wait.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <asm/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/moduleparam.h>

#include "../../hi35xx_dev/include/hi35xx_dev.h"
#include "../../dma_trans/dma_trans.h"
#include "../include/pci_proto_common.h"
#include "../../proc_msg/mcc_proc_msg.h"


//spinlock_t g_mcc_list_lock;

/* export interface begin */
void *pci_vdd_open(unsigned int target_id,
		unsigned int port,
		unsigned int priority)
{
	struct hisi_transfer_handle *handle = NULL;
	unsigned int pos;
	struct hi35xx_dev *hi_dev;

	if ((target_id >= MAX_DEV_NUMBER)
			|| (target_id == pci_vdd_localid()))
	{
		mcc_trace(MCC_ERR, "Try to send message to yourself "
				"or target_id[0x%x] out of range[0 - 0x%x].",
				target_id, MAX_DEV_NUMBER);

		return NULL;
	}

	hi_dev = g_local_handler->slot_to_hidev(target_id);
	if (NULL == hi_dev) {
		mcc_trace(MCC_ERR, "Try to open a non-exist dev[0x%x].",
				target_id);
		return NULL;
	}

#if 0
	if (DEVICE_CHECKED_FLAG != hi_dev->vdd_checked_success) {
		mcc_trace(MCC_ERR, "Please check up dev[0x%x]"
				" before open it!",
				target_id);
		return NULL;
	}
#endif

	if (port >= MAX_MSG_PORTS) {
		mcc_trace(MCC_ERR, "Port[0x%x] out of range[0 - 0x%x].",
				port, MAX_MSG_PORTS);

		return NULL;
	}

	pos = ((target_id << MSG_PORT_NEEDBITS) | port);
	handle = hisi_handle_table[pos];

	if (handle) {
		mcc_trace(MCC_ERR, "Message handle[target:0x%x,port:0x%x]"
			       " already exists!",
			       target_id, port);
		return handle;
	}

	if (in_interrupt()) {
		handle = kmalloc(sizeof(struct hisi_transfer_handle),
				GFP_ATOMIC);
	} else {
		handle = kmalloc(sizeof(struct hisi_transfer_handle),
				GFP_KERNEL);
	}

	if (!handle) {
		mcc_trace(MCC_ERR, "kmalloc for message handle"
				"[target:0x%x,port:0x%x] failed!",
				target_id, port);
		return NULL;
	}

	handle->target_id = target_id & MSG_DEV_BITS;
	handle->port = port & MSG_PORT_BITS;
	handle->priority = priority;
	handle->vdd_notifier_recvfrom = NULL;

	hisi_handle_table[pos] = handle;

	mcc_trace(MCC_INFO, "handler[target:0x%x,port:0x%x] open success!",
			handle->target_id,
			handle->port);

	return handle;

}
EXPORT_SYMBOL(pci_vdd_open);

int pci_vdd_localid(void)
{
	return g_local_handler->local_slot_number();
}
EXPORT_SYMBOL(pci_vdd_localid);

int pci_vdd_remoteids(int ids[])
{
	int i = 0;

	/*
	 * host[0] <====> slave[x]
	 * slave id should never be zero
	 */
	if (g_local_handler->is_host()) {
		for (; i < g_local_handler->remote_device_number; i++)
			ids[i] = g_hi35xx_dev_map[i]->slot_index;
	} else {
		ids[0] = 0;
		i = 1;
	}

	return i;
}
EXPORT_SYMBOL(pci_vdd_remoteids);

/*
 * This function should be called by both host and slave before
 * any message communication starts.
 *
 * Protocol:
 * host_handshake_step0 should be finished first, then slv_handshake_step0
 * will be able to be finish second, and then host_handshake_step1, then
 * slv_handshake_step1, and finally host_handshake_step2 finished, handshake
 * is done.
 * Before host_handshake_step0, host must have applied the shared memory
 * buffer for the slave to be checked.
 * After slv_handshake_step0, slv must have mapped the PCI window to shared
 * memory buffer(if need).
 */
int pci_vdd_check_remote(int remote_id)
{
	struct hi35xx_dev *hi_dev;
	int ret = 0;

	printk(KERN_ERR "## check device 0x%x\n", remote_id);

	hi_dev = g_local_handler->slot_to_hidev(remote_id);
	if (!hi_dev) {
		mcc_trace(MCC_ERR, "Device[0x%x] does not exist!",
				remote_id);
		return -1;
	}

	/*
	 * Sometimes a remote device may be checked more than once by mistakes.
	 * If a remote device is checked before(and the checked remote device is ready
	 * for further message communication), the hi35xx_dev.vdd_checked_success
	 * variable would be marked as DEVICE_CHECKED_FLAG. A zero(checked success)
	 *  will be returned after the given device is checked.
	 */
	if (hi_dev->vdd_checked_success == DEVICE_CHECKED_FLAG) {
		printk("Device[0x%x] already checked!\n", remote_id);
		return 0;
	}

	if (g_local_handler->is_host())
		ret = host_check_slv(hi_dev);
	else
		ret = slave_check_host(hi_dev);

	if (!ret)
		hi_dev->vdd_checked_success = DEVICE_CHECKED_FLAG;

	return ret;
}
EXPORT_SYMBOL(pci_vdd_check_remote);

void pci_vdd_close(void *handle)
{
	struct hisi_transfer_handle *p = handle;
	unsigned int pos = 0 ;

	if (handle) {
		pos = ((p->target_id << MSG_PORT_NEEDBITS) | p->port);
		hisi_handle_table[pos] = 0;
		kfree(handle);
	}
}
EXPORT_SYMBOL(pci_vdd_close);

int pci_vdd_sendto(void *handle,
		const void *buf,
		unsigned int len,
		int flag)
{
	struct hisi_transfer_handle *p = handle;

	int ret;

	if (!buf) {
		mcc_trace(MCC_ERR, "Message to be sent is NULL!");
		return -1;
	}
	if (!handle) {
		mcc_trace(MCC_ERR, "Message handle is NULL!");
		return -1;
	}
	if (len >= HISI_SHARED_SENDMEM_SIZE) {
		mcc_trace(MCC_ERR, "Data len out of range!");
		return -1;
	}

	if (g_local_handler->is_host())
		ret = host_send_msg(p, buf, len, flag);
	else
		ret = slave_send_msg(p, buf, len, flag);

	return ret;
}
EXPORT_SYMBOL(pci_vdd_sendto);

int pci_vdd_getopt(void *handle, pci_vdd_opt_t *opt)
{
	struct hisi_transfer_handle *p = handle;
	unsigned long flags;

	if (!p) {
		mcc_trace(MCC_ERR, "Handle get is null!");
		return -1;
	}

	spin_lock_irqsave(&g_mcc_list_lock, flags);

	opt->recvfrom_notify = p->vdd_notifier_recvfrom;
	opt->data = p->data;

	spin_unlock_irqrestore(&g_mcc_list_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pci_vdd_getopt);

int pci_vdd_setopt(void *handle, pci_vdd_opt_t *opt)
{
	struct hisi_transfer_handle *p = handle;
	unsigned long flags;

	if (!p) {
		mcc_trace(MCC_ERR, "Handle set is null!");
		return -1;
	}

	spin_lock_irqsave(&g_mcc_list_lock, flags);

	p->data = opt->data;
	p->vdd_notifier_recvfrom = opt->recvfrom_notify;

	spin_unlock_irqrestore(&g_mcc_list_lock, flags);

	return 0;
}
EXPORT_SYMBOL(pci_vdd_setopt);
/* export interface end */


