
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

#define MESSAGE_IRQ_ENABLE
//#undef MESSAGE_IRQ_ENABLE

static char version[] __devinitdata
		= KERN_INFO "" MCC_DRV_MODULE_NAME " :" MCC_DRV_VERSION "\n";

/*
 * Size of message shared-buffer
 */
unsigned int shm_size = 0;
module_param(shm_size, uint, S_IRUGO);

/*
 * Base address of message shared-buffer
 */
unsigned int shm_phys_addr = 0;
module_param(shm_phys_addr, uint, S_IRUGO);

spinlock_t g_mcc_list_lock;
spinlock_t s_msg_recv_lock;

struct hisi_transfer_handle
		*hisi_handle_table[HANDLE_TABLE_SIZE] = {0};

#ifndef MESSAGE_IRQ_ENABLE
static struct timer_list recv_msg_timer;
static struct task_struct *s_mcc_recv_thread;

static DECLARE_WAIT_QUEUE_HEAD(s_mcc_recv_msg_wait);
static int s_mcc_module_stop = 1;
static int s_recv_new_msg = 0;

#ifdef MCC_PROC_ENABLE
static unsigned int s_msgrecv_timer_last_jf = 0;
static unsigned int s_msgrecv_queue_last_jf = 0;
#endif

#else
static struct timer_list msg_trigger_timer;
#endif /* MESSAGE_IRQ_ENABLE */

#include "pci_data_common.c"

static void recv_irq_mem(struct hi35xx_dev *hi_dev)
{
	struct hi35xx_dev *dev;
	struct hisi_transfer_handle * handle = NULL;
	struct hisi_pci_transfer_head *head = NULL;
	unsigned int ret, pos = 0;
	unsigned int len;
	void *data;
	unsigned long flag;

	dev = hi_dev;

	spin_lock_irqsave(&s_msg_recv_lock, flag);

	while (!hisi_is_irq_buffer_empty(dev)) {

		head = hisi_get_irq_head(dev);

		data = (void *)head + _HEAD_SIZE;
		len = head->length;
		mcc_trace(MCC_INFO, "head->magic 0x%x head 0x%08x len 0x%x.",
				head->magic,
				(unsigned int)head,
				head->length);

		if (head->magic == _HEAD_MAGIC_INITIAL) {

			/*
			 * This branch will not be used any more.
			 * _HEAD_MAGIC_INITIAL means initial data from host
			 */

			/*
			 * Slave itself does not know its own slot_index,
			 * so, host tells slave its index number.
			 */
			dev->slot_index = readl(data);

			printk(KERN_INFO "dev->slot_id 0x%x, data 0x%08x\n",
					(unsigned int)dev->slot_index,
					(unsigned int)data);
			mcc_trace(MCC_INFO, "dev->slot_id 0x%x, data 0x%08x",
					(unsigned int)dev->slot_index,
					(unsigned int)data);
		} else {
			/* normal pre-view message */
			pos = head->src << MSG_PORT_NEEDBITS | head->port;
			handle = hisi_handle_table[pos];

			mcc_trace(MCC_INFO, "handle 0x%08x",
					(unsigned int)handle);

			mcc_trace(MCC_DEBUG, "Message head:0x[%08x  %08x]",
					*(unsigned int *)head,
					*((unsigned int *)head + 1));

			mcc_trace(MCC_DEBUG, "Message length[0x%x]"
					" head->magic[0x%x]"
					" head->src[0x%x]",
					head->length,
					head->magic,
					head->src);
#ifdef MCC_PROC_ENABLE
			if (PROC_LEVEL_I <= g_proc.proc_enable) {
				int i;

				for (i = 0; i < g_local_handler->remote_device_number; i++) {
					if (g_hi35xx_dev_map[i]->slot_index == handle->target_id)
						break;
				}

				((struct msg_number *)g_proc._msg_number.data)->target[i].irq_recv++;
			}
#endif

			if (handle && handle->vdd_notifier_recvfrom) {

#if 0
				int _index;
				unsigned int *tmp = data;

				printk(KERN_INFO "func %s, Message content:\n",
						__func__);
				for (_index = 0; _index < data_len/4;_index += 4) {
					printk(KERN_INFO "%x8: %08x  %08x  %08x  %08x",
							_index,
							*tmp,
							*(tmp + 1),
							*(tmp + 2),
							*(tmp + 3));
					tmp += 4;
				}
				printk(KERN_INFO "\n\n");
#endif

				mcc_trace(MCC_INFO, "handle 0x%08x notify recv",
						(unsigned int)handle);

				ret = handle->vdd_notifier_recvfrom(handle,
						data, len);
				if (ret) {
					mcc_trace(MCC_ERR, "hanlde 0x%08x "
						"notify recv data failed!",
						(unsigned int)handle);
					spin_unlock_irqrestore(&s_msg_recv_lock, flag);
					return;
				}

			} else {
				if (handle) {
					mcc_trace(MCC_ERR, "handler[0x%x: 0x%x/0x%x] and recv_notifier is NULL!",
							(unsigned int)handle, head->src, head->port);
				} else {
					mcc_trace(MCC_ERR, "handler is NULL[port/target : 0x%x/0x%x!]",
							head->src, head->port);
				}

				mcc_trace(MCC_ERR, "Nobody care about this message,"
						"just clear it!");
			}
		}

		/*
		 * If nobody cares about the coming message, simply clear it.
		 * If failed while reading the coming message, leave the
		 * message alone and return.
		 */

		/* Update read pointer */
		hisi_irq_data_recieved(dev, data, len);
	}
	spin_unlock_irqrestore(&s_msg_recv_lock, flag);
}

static void recv_thread_mem(struct hi35xx_dev *hi_dev)
{
	struct hi35xx_dev *dev;
	struct hisi_transfer_handle *handle = NULL;
	struct hisi_pci_transfer_head *head = NULL;
	unsigned int pos,ret;
	void *data;
	unsigned long flag;

	dev = hi_dev;

	spin_lock_irqsave(&s_msg_recv_lock, flag);

	while (!hisi_is_thread_buffer_empty(dev)) {

		mcc_trace(MCC_INFO, "dev->slot_id 0x%x",
				(unsigned int)dev->slot_index);

		head = hisi_get_thread_head(dev);
		pos = ((head->src << MSG_PORT_NEEDBITS) | head->port);
		handle = hisi_handle_table[pos];
		data = (void *)head + _HEAD_SIZE;

		mcc_trace(MCC_INFO, "src 0x%x port 0x%x", head->src, head->port);
		mcc_trace(MCC_INFO, "pos %d handle 0x%08x",
				pos, (unsigned int)handle);

#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable) {
			int i;

			for (i = 0; i < g_local_handler->remote_device_number; i++) {
				if (g_hi35xx_dev_map[i]->slot_index == handle->target_id)
					break;
			}

			((struct msg_number *)g_proc._msg_number.data)->target[i].thread_recv++;
		}
#endif

		if (handle && handle->vdd_notifier_recvfrom) {

			mcc_trace(MCC_INFO, "hanlde 0x%08x data %p  "
					"len %d notify recv",
					(unsigned int)handle,
					data,
					head->length);

			ret = handle->vdd_notifier_recvfrom(handle,
					data, head->length);
			if (ret) {
				mcc_trace(MCC_ERR, "hanlde 0x%08x notify "
						"recv data failed!",
						(unsigned int)handle);
				spin_unlock_irqrestore(&s_msg_recv_lock, flag);
				return;
			}

		} else {
			if (handle) {
				mcc_trace(MCC_ERR, "handler[0x%x: 0x%x/0x%x] and recv_notifier is NULL!",
						(unsigned int)handle, head->src, head->port);
			} else {
				mcc_trace(MCC_ERR, "handler is NULL[port/target : 0x%x/0x%x!]",
						head->src, head->port);
			}

			mcc_trace(MCC_ERR, "Nobody care about this message,"
				       "just clear it!");
		}

		/*
		 * If nobody cares about the coming message, simply clear it.
		 * If failed while reading the coming message, leave the
		 * message alone and return.
		 */

		/* Update read pointer */
		hisi_thread_data_recieved(dev, data, head->length);
	}
	spin_unlock_irqrestore(&s_msg_recv_lock, flag);
}

#ifdef MESSAGE_IRQ_ENABLE
static irqreturn_t message_irq_handler(int irq, void *id)
{
	unsigned int status = 0;
	int i;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		status = g_local_handler->clear_msg_irq(g_hi35xx_dev_map[i]);
		if (status) {
			recv_irq_mem(g_hi35xx_dev_map[i]);
			recv_thread_mem(g_hi35xx_dev_map[i]);
		}
	}

	return IRQ_HANDLED;
}
#endif

#ifndef MESSAGE_IRQ_ENABLE
/* In timer irq handler context, should never sleep or schedule. */
static void hisi_msg_irq_slave(unsigned long val)
{
#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_I <= g_proc.proc_enable)
	{
		struct timeval cur_time;
		unsigned int cur_jf = 0;
		unsigned int *cur_interval;
		unsigned int *max_interval;

		cur_interval = &((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->cur_interval;
		max_interval = &((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->max_interval;
		do_gettimeofday(&cur_time);
		cur_jf = cur_time.tv_sec * 1000000 + cur_time.tv_usec;
		if (likely(s_msgrecv_timer_last_jf != 0)) {
			*cur_interval = cur_jf - s_msgrecv_timer_last_jf;

			if (*cur_interval >= 0x100000)
				mcc_trace(MCC_INFO, "Timer interrupt gap[0x%x] too long!",
						*cur_interval);

			if ((*cur_interval > *max_interval) && (*cur_interval < 0x100000))
				*max_interval = *cur_interval;
		}
		s_msgrecv_timer_last_jf = cur_jf;
	}
#endif

	s_recv_new_msg = 1;
	wake_up_interruptible(&s_mcc_recv_msg_wait);
	//wake_up(&s_mcc_recv_msg_wait);

	while (!s_mcc_module_stop) {
		if (!timer_pending(&recv_msg_timer)) {
			recv_msg_timer.expires = jiffies + msecs_to_jiffies(10);
			add_timer(&recv_msg_timer);
			return;
		}
		mcc_trace(MCC_INFO, "Timer function excuting for tooo long!");
	}
}

static void hisi_msg_irq_host(unsigned long val)
{
#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_I <= g_proc.proc_enable)
	{
		struct timeval cur_time;
		unsigned int cur_jf = 0;
		unsigned int *cur_interval;
		unsigned int *max_interval;

		cur_interval = &((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->cur_interval;
		max_interval = &((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->max_interval;
		do_gettimeofday(&cur_time);
		cur_jf = cur_time.tv_sec * 1000000 + cur_time.tv_usec;
		if (likely(s_msgrecv_timer_last_jf != 0)) {
			*cur_interval = cur_jf - s_msgrecv_timer_last_jf;

			if (*cur_interval >= 0x100000)
				printk(KERN_ERR "Timer interrupt gap[0x%x] too long!\n",
						*cur_interval);

			if ((*cur_interval > *max_interval) && (*cur_interval < 0x100000))
				*max_interval = *cur_interval;
		}
		s_msgrecv_timer_last_jf = cur_jf;
	}
#endif

	s_recv_new_msg = 1;
	wake_up_interruptible(&s_mcc_recv_msg_wait);

	while (!s_mcc_module_stop) {
		if (!timer_pending(&recv_msg_timer)) {
			recv_msg_timer.expires = jiffies + msecs_to_jiffies(10);
			add_timer(&recv_msg_timer);
			return;
		}
		mcc_trace(MCC_INFO, "timer function excuting for tooo long!");
	}
}
#endif

static unsigned int get_time_cost_us(struct timeval *start,
		struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * 1000000
		+ (end->tv_usec - start->tv_usec);
}

int host_check_slv(struct hi35xx_dev *hi_dev)
{
	if (g_local_handler->host_handshake_step0(hi_dev)) {
		mcc_trace(MCC_ERR, "Host<=>slv[0x%x] handshake step0 failed",
				hi_dev->slot_index);
		goto host_handshake_step0_err;
	}

	if (g_local_handler->host_handshake_step1(hi_dev)) {
		mcc_trace(MCC_ERR, "Host<=>slv[0x%x] handshake step1 failed",
				hi_dev->slot_index);
		goto host_handshake_step1_err;
	}

	hi_dev->shm_size = DEV_SHM_SIZE;
	hi_dev->shm_end = hi_dev->shm_base + hi_dev->shm_size;

	mcc_trace(MCC_ERR, "shm base 0x%x", hi_dev->shm_base);
	g_local_handler->move_pf_window_in(hi_dev,
			hi_dev->shm_base, DEV_SHM_SIZE, 0);

	hi_dev->shm_base_virt = hi_dev->pci_bar0_virt;
	hi_dev->shm_end_virt = hi_dev->shm_base_virt + hi_dev->shm_size;

	hisi_shared_mem_init(hi_dev);

	if (g_local_handler->host_handshake_step2(hi_dev)) {
		mcc_trace(MCC_ERR, "Host<=>slv[0x%x] handshake step2 failed",
				hi_dev->slot_index);
		goto host_handshake_step2_err;
	}

	return 0;

host_handshake_step2_err:
host_handshake_step1_err:
host_handshake_step0_err:

	return -1;
}

int slave_check_host(struct hi35xx_dev *hi_dev)
{
	hi_dev->shm_base = shm_phys_addr;
	hi_dev->shm_size = DEV_SHM_SIZE;

	if (g_local_handler->slv_handshake_step0()) {
		mcc_trace(MCC_ERR, "Slv<=>host handshake step0 failed");
		return -1;
	}

	/* initial variables of shared memory buffer */
	hi_dev->shm_end = hi_dev->shm_base + hi_dev->shm_size;

	hi_dev->shm_base_virt = (unsigned int)ioremap(hi_dev->shm_base,
			hi_dev->shm_size);
	if (!hi_dev->shm_base_virt) {
		mcc_trace(MCC_ERR, "IO remap for shared mem failed!");
		return -ENOMEM;
	}

	hi_dev->shm_end_virt = hi_dev->shm_base_virt + hi_dev->shm_size;

	hisi_shared_mem_init(hi_dev);

	if (g_local_handler->slv_handshake_step1()) {
		mcc_trace(MCC_ERR, "Slv<=>host handshake step1 failed");
		iounmap((void *)hi_dev->shm_base_virt);
		return -1;
	}

	return 0;
}

int mcc_send_message(struct hisi_transfer_handle *handle,
		const void *buf,
		unsigned int len,
		int flag)
{
	struct hisi_pci_transfer_head head;
	struct hi35xx_dev *dev;
	int ret = 0;

#ifdef MCC_PROC_ENABLE
	struct timeval msg_start;
	struct timeval msg_done;

	if (PROC_LEVEL_I <= g_proc.proc_enable)
		do_gettimeofday(&msg_start);
#endif

	dev = g_local_handler->slot_to_hidev(handle->target_id);
	if (!dev) {
		mcc_trace(MCC_ERR, "Target dev[0x%x] is NULL!",
				handle->target_id);
		return -1;
	}

	memset(&head, 0, sizeof(struct hisi_pci_transfer_head));
	head.target_id  = handle->target_id;
	head.port       = handle->port;
	head.src        = g_local_handler->local_devid;
	head.length     = len;
	head.check      = _HEAD_CHECK_FINISHED;
	mcc_trace(MCC_INFO, "target 0x%x, src 0x%x, port 0x%x",
			head.target_id,
			head.src,
			head.port);

	if (flag & PRIORITY_DATA) {
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			int i = 0;
			struct timeval cur_time;
			unsigned int cur_jf;
			unsigned int *plast_jf;
			unsigned int *pcur_interval;
			unsigned int *pmax_interval;

			for (i = 0; i < g_local_handler->remote_device_number; i++) {
				if (g_hi35xx_dev_map[i]->slot_index == handle->target_id)
					break;
			}
			/*
			 * PCIV message number record.
			 */
			((struct msg_number *)g_proc._msg_number.data)->target[i].irq_send++;

			/*
			 * Interval between two PCIV message.
			 */
			plast_jf = &((struct pre_v_msg_send*)g_proc._pmsg_interval.data)->target[i].last_jf;
			pcur_interval = &((struct pre_v_msg_send*)g_proc._pmsg_interval.data)->target[i].cur_interval;
			pmax_interval = &((struct pre_v_msg_send*)g_proc._pmsg_interval.data)->target[i].max_interval;

			do_gettimeofday(&cur_time);
			cur_jf = cur_time.tv_sec * 1000000 + cur_time.tv_usec;
			if (0 != *plast_jf) {
				*pcur_interval = cur_jf - *plast_jf;
			}
			*plast_jf = cur_jf;

			if (*pcur_interval >= 0x100000) {
				mcc_trace(MCC_INFO, "Timer gap[0x%x] between two pciv message too long!!",
						*pcur_interval);
			}

			if ((*pcur_interval > *pmax_interval) && (*pcur_interval < 0x100000))
				*pmax_interval = *pcur_interval;

		}
#endif
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			unsigned int *cur_cost;
			unsigned int *max_cost;

			cur_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->cur_pciv_cost;
			max_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->max_pciv_cost;
			do_gettimeofday(&msg_done);
			*cur_cost = get_time_cost_us(&msg_start, &msg_done);

			if ((*cur_cost > *max_cost) && (*cur_cost < 0x10000000))
				*max_cost = *cur_cost;
		}
#endif
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			unsigned int *cur_len;
			unsigned int *max_len;
			cur_len = &((struct msg_len *)g_proc._msg_send_len.data)->cur_irq_send;
			max_len = &((struct msg_len *)g_proc._msg_send_len.data)->max_irq_send;
			*cur_len = len;
			if (*cur_len > *max_len)
				*max_len = *cur_len;

		}
#endif
		ret = hisi_send_data_irq(dev, buf, len, &head);

	} else {
		ret = hisi_send_data_thread(dev, buf, len, &head);
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			int i = 0;

			for (i = 0; i < g_local_handler->remote_device_number; i++) {
				if (g_hi35xx_dev_map[i]->slot_index == handle->target_id)
					break;
			}
			((struct msg_number *)g_proc._msg_number.data)->target[i].thread_send++;
		}
#endif
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			unsigned int *cur_cost;
			unsigned int *max_cost;

			cur_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->cur_ctl_cost;
			max_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->max_ctl_cost;
			do_gettimeofday(&msg_done);
			*cur_cost = get_time_cost_us(&msg_start, &msg_done);

			if ((*cur_cost > *max_cost) && (*cur_cost < 0x10000000))
				*max_cost = *cur_cost;
		}
#endif
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			unsigned int *cur_len;
			unsigned int *max_len;
			cur_len = &((struct msg_len *)g_proc._msg_send_len.data)->cur_thd_send;
			max_len = &((struct msg_len *)g_proc._msg_send_len.data)->max_thd_send;
			*cur_len = len;
			if (*cur_len > *max_len)
				*max_len = *cur_len;

		}
#endif
	}

	return ret;
}

int host_send_msg(struct hisi_transfer_handle *handle,
		const void *buf,
		unsigned int len,
		int flag)
{
	return mcc_send_message(handle, buf, len, flag);
}

int slave_send_msg(struct hisi_transfer_handle *handle,
		const void *buf,
		unsigned int len,
		int flag)
{
	return mcc_send_message(handle, buf, len, flag);
}

#ifndef MESSAGE_IRQ_ENABLE
static void host_recv_mem(void)
{
	int i = 0;

	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		if (g_hi35xx_dev_map[i]->vdd_checked_success != DEVICE_CHECKED_FLAG) {
			mcc_trace(MCC_INFO, "Device[0x%x] has not been checked yet!",
					g_hi35xx_dev_map[i]->slot_index);
			continue;
		}

		recv_irq_mem(g_hi35xx_dev_map[i]);
		recv_thread_mem(g_hi35xx_dev_map[i]);
	}
}

static int mcc_recv_message(void)
{
#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_I <= g_proc.proc_enable) {
		struct timeval cur_time;
		unsigned int cur_jf = 0;
		unsigned int *cur_interval;
		unsigned int *max_interval;

		cur_interval = &((struct msg_recv_timer *)g_proc._msg_recv_queue.data)->cur_interval;
		max_interval = &((struct msg_recv_timer *)g_proc._msg_recv_queue.data)->max_interval;
		do_gettimeofday(&cur_time);
		cur_jf = cur_time.tv_sec * 1000000 + cur_time.tv_usec;
		if (likely(s_msgrecv_queue_last_jf != 0)) {
			*cur_interval = cur_jf - s_msgrecv_queue_last_jf;

			if (*cur_interval >= 0x100000)
				printk(KERN_ERR "Message recv schedule too long[0x%x]\n",
						*cur_interval);

			if ((*cur_interval > *max_interval) && (*cur_interval < 0x100000))
				*max_interval = *cur_interval;
		}
		s_msgrecv_queue_last_jf = cur_jf;
	}
#endif

	host_recv_mem();

	return 0;
}

static int mcc_recvmsg_thread(void * _unused)
{
	do {
		if (mcc_recv_message()) {
			mcc_trace(MCC_ERR, "Recv message failed!");
			BUG_ON(1);
		}

		s_recv_new_msg = 0;
		//wait_event(s_mcc_recv_msg_wait, s_recv_new_msg);
		wait_event_interruptible(s_mcc_recv_msg_wait, s_recv_new_msg);
	} while (!s_mcc_module_stop);

	return 0;
}
#endif

static int __init msg_com_init(void)
{
#ifdef MESSAGE_IRQ_ENABLE
	irqreturn_t (*handler)(int irq, void *id);
#else
	void (*handler)(unsigned long);
	struct sched_param param_r = { .sched_priority = 99 };
#endif

	printk(version);

	spin_lock_init(&g_mcc_list_lock);
	spin_lock_init(&s_msg_recv_lock);

#ifdef MESSAGE_IRQ_ENABLE
	handler = message_irq_handler;

	if (g_local_handler->request_msg_irq(handler)) {
		mcc_trace(MCC_ERR, "Request msg irq failed!");
		return -1;
	}

	init_timer(&msg_trigger_timer);
#else
	s_mcc_module_stop = 0;

	s_mcc_recv_thread = kthread_run(mcc_recvmsg_thread, NULL, "kmcc_recv");
	if (IS_ERR(s_mcc_recv_thread)) {
		mcc_trace(MCC_ERR, "Start pci message receiving thread failed!");
		return -1;
	}

	if (sched_setscheduler(s_mcc_recv_thread, SCHED_FIFO, &param_r))
		mcc_trace(MCC_ERR, "Set thread priority failed!");

	if (g_local_handler->is_host()) {
		handler = hisi_msg_irq_host;
	} else {
		handler = hisi_msg_irq_slave;
	}

	init_timer(&recv_msg_timer);
	recv_msg_timer.expires = jiffies + msecs_to_jiffies(1000);
	recv_msg_timer.data = 0x0;
	recv_msg_timer.function = handler;

	add_timer(&recv_msg_timer);
#endif

#ifdef MCC_PROC_ENABLE
	g_proc._get_rp_wp = update_proc_rp_wp;
#endif
	return 0;
}

static void __exit msg_com_exit(void)
{
#ifdef MCC_PROC_ENABLE
	g_proc._get_rp_wp = NULL;
#endif

#ifdef MESSAGE_IRQ_ENABLE
	g_local_handler->release_msg_irq();
#else
	del_timer(&recv_msg_timer);
	s_mcc_module_stop = 1;
	s_recv_new_msg = 1;
	kthread_stop(s_mcc_recv_thread);
#endif

}

module_init(msg_com_init);
module_exit(msg_com_exit);

MODULE_AUTHOR("Hisilicon");
MODULE_DESCRIPTION("Hisilicon Hi35XX");
MODULE_LICENSE("GPL");
MODULE_VERSION("HI_VERSION=" __DATE__);

