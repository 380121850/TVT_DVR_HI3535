
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

#define DMA_MESSAGE_ENABLE            1

#ifdef DMA_MESSAGE_ENABLE
#define DMA_EXCHG_BUF_SIZE      (256 * 1024)
#define DMA_EXCHG_READ          0
#define DMA_EXCHG_WRITE         1
struct dma_exchange_buf {
	spinlock_t mlock;
	struct semaphore sem;
	int type;
	unsigned int rp_addr;
	unsigned int wp_addr;
	unsigned int rp_addr_virt;
	unsigned int wp_addr_virt;
	unsigned int base;
	unsigned int size;
	unsigned int base_virt;
	unsigned int data_base;
	unsigned int data_end;
	unsigned int data_base_virt;
	unsigned int data_end_virt;
	wait_queue_head_t wait;
};

struct hisi_dma_exchange_buffer {
	struct dma_exchange_buf read_buf;
	struct dma_exchange_buf write_buf;
};
#endif

static struct timer_list recv_msg_timer;

static struct semaphore s_dma_sendmsg_sem;

//extern struct hi35xx_dev *g_hi35xx_dev_map[MAX_PCIE_DEVICES];

static char version[] __devinitdata
		= KERN_INFO "" MCC_DRV_MODULE_NAME " :" MCC_DRV_VERSION "\n";

/* Size of message shared-buffer, normally (n X 1)MBytes */
unsigned int shm_size = 0;
module_param(shm_size, uint, S_IRUGO);

/* Base address of message shared buffer */
unsigned int shm_phys_addr = 0;
module_param(shm_phys_addr, uint, S_IRUGO);

/*
 * Number of all checked remote devices.
 */
static unsigned int s_device_checked = 0;

spinlock_t g_mcc_list_lock;
spinlock_t s_pciv_send_list_lock;
spinlock_t s_msg_recv_lock;

struct hisi_transfer_handle
		*hisi_handle_table[HANDLE_TABLE_SIZE] = {0};

struct pciv_message s_pciv_msg_array[MAX_PCIV_MSG_WAIT_NO];
struct list_head s_pcivmsg_list_free_head;
struct list_head s_pcivmsg_list_busy_head;
static struct task_struct *s_pciv_send_thread;
static struct task_struct *s_mcc_recv_thread;

#if 0
struct pciv_message s_stream_msg_array[2 * MAX_PCIV_MSG_WAIT_NO];
struct list_head s_streammsg_list_free_head;
struct list_head s_streammsg_list_busy_head;
#endif

static DECLARE_WAIT_QUEUE_HEAD(s_pciv_send_msg_wait);
static DECLARE_WAIT_QUEUE_HEAD(s_mcc_recv_msg_wait);
static int s_mcc_module_stop = 1;
static int s_send_new_msg = 0;
static int s_recv_new_msg = 0;

#ifdef MCC_PROC_ENABLE
static unsigned int s_msgrecv_timer_last_jf = 0;
static unsigned int s_msgrecv_queue_last_jf = 0;
#endif

static int parse_exchg_buf(struct dma_exchange_buf *exchg_buf,
		unsigned int mstart, unsigned int mend, unsigned int msg_type)
{
	unsigned int len = mstart - mend;
	struct dma_exchange_buf *exchange_buf = exchg_buf;
	unsigned int message_start = exchange_buf->data_base_virt;
	unsigned int dma_data_end = exchange_buf->data_base_virt + len;
	struct hisi_pci_transfer_head *head = NULL;
	struct hisi_transfer_handle * handle = NULL;
	void *data = NULL;
	unsigned int data_len = 0;
	unsigned int pos = 0;
	unsigned int head_check = 0x0;

	while (message_start < dma_data_end) {
		head = (struct hisi_pci_transfer_head *)message_start;

		head_check = (unsigned int)head->check;
		if (head_check != _HEAD_CHECK_FINISHED) {
			mcc_trace(MCC_ERR, "Message head check err[0x%x/0x%x]!",
					head_check, _HEAD_CHECK_FINISHED);
			mcc_trace(MCC_ERR, "exchg.data_base[0x%x],"
					"dma data end[0x%x],"
					"last msg start[0x%x],"
					"msg len[0x%x].",
				       exchange_buf->data_base_virt,
				       dma_data_end,
				       message_start,
				       len);
			mcc_trace(MCC_ERR, "start 0x%x, end 0x%x.", mstart, mend);
			BUG_ON(1);
		}

		if (_HEAD_MAGIC_JUMP == head->magic) {
			mcc_trace(MCC_INFO, "head->magic==_HEAD_MAGIC_JUMP?");
			return 0;
		}

		data = (void *)(message_start + _HEAD_SIZE);
		data_len = head->length;

		mcc_trace(MCC_DEBUG, "Message head:0x[%08x  %08x]",
				*(unsigned int *)head,
				*((unsigned int *)head + 1));

		mcc_trace(MCC_DEBUG, "Message length[0x%x]"
				" head->magic[0x%x]"
				" head->src[0x%x]",
				head->length,
				head->magic,
				head->src);

		pos = head->src << MSG_PORT_NEEDBITS | head->port;
		handle = hisi_handle_table[pos];

#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable) {
			if (PRE_VIEW_MSG == msg_type)
				((struct msg_number *)g_proc._msg_number.data)->target[0].irq_recv++;
			else
				((struct msg_number *)g_proc._msg_number.data)->target[0].thread_recv++;
		}
#endif
		if (handle && handle->vdd_notifier_recvfrom) {

#if 0
			int _index;
			unsigned int *tmp = data;

			printk(KERN_INFO "func %s, Message content:\n",
					__func__);
			for (_index = 0; _index < data_len/4;_index += 4) {
				printk(KERN_INFO "%x8: %8x  %8x  %8x  %8x",
						_index,
						*tmp,
						*(tmp + 1),
						*(tmp + 2),
						*(tmp + 3));
				tmp += 4;
			}
			printk(KERN_INFO "\n\n");
#endif

			if (handle->vdd_notifier_recvfrom(handle,
					data,
					data_len))
			{
				mcc_trace(MCC_ERR, "Handler[0x%08x] receive "
						"message failed!\n",
						(unsigned int)handle);
				return -1;
			}
		} else {
#if 1
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

			mcc_trace(MCC_ERR, "Handle[src:0x%x,port:0x%x] is NULL!",
					head->src, head->port);
		}

		message_start += (data_len
				+ _HEAD_SIZE
				+ 2 * _MEM_ALIAGN
				- ((data_len + _HEAD_SIZE) & _MEM_ALIAGN_VERS));

	}
	return 0;
}

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
			//printk(KERN_ERR "thread&&&& handler target id 0x%d", handle->target_id);
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

static unsigned int get_time_cost_us(struct timeval *start,
		struct timeval *end)
{
	return (end->tv_sec - start->tv_sec) * 1000000
		+ (end->tv_usec - start->tv_usec);
}

int host_check_slv(struct hi35xx_dev *hi_dev)
{
	/*
	 * Host Shared Memory Space
	 *
	 * dev[1]->shm_base:    ##########  \           \ <--shm_phys_addr
	 *                      ##########  1MBytes     |
	 *                      ##########  /           |
	 * dev[2]->shm_base:    ##########  \        shm_size
	 *                      ##########  1MBytes     |
	 *                      ##########  /           |
	 * ........                                     /
	 */
	hi_dev->shm_size = DEV_SHM_SIZE;
	hi_dev->shm_base = shm_phys_addr + (s_device_checked * DEV_SHM_SIZE);
	if ((hi_dev->shm_base + hi_dev->shm_size) > (shm_phys_addr + shm_size)) {

		mcc_trace(MCC_ERR, "device [0x%x] shared mem out of range",
				hi_dev->slot_index);
		mcc_trace(MCC_ERR, "dev->shm_base[0x%08x],dev->shm_size[0x%x]"
				"shm_phys_addr[0x%08x],shm_size[0x%x]",
				hi_dev->shm_base, hi_dev->shm_size,
				shm_phys_addr, shm_size);
		mcc_trace(MCC_ERR, "s_device_checked 0x%x", s_device_checked);
		return -ENOMEM;
	}

	if (0xfff & hi_dev->shm_base) {
		mcc_trace(MCC_ERR, "Warning! shm address not page align!!\n"
				"shm_base_phys: 0x%08x.",
				hi_dev->shm_base);
		return -ENOMEM;
	}

	hi_dev->shm_end = hi_dev->shm_base + hi_dev->shm_size;

	hi_dev->shm_base_virt = (unsigned int)ioremap(hi_dev->shm_base, hi_dev->shm_size);
	if (!hi_dev->shm_base_virt) {
		mcc_trace(MCC_ERR, "Alloc shared memory for dev[0x%x] failed!",
				hi_dev->slot_index);
		return -ENOMEM;
	}

	hi_dev->shm_end_virt = hi_dev->shm_base_virt + hi_dev->shm_size;
	mcc_trace(MCC_INFO, "hidev->shm_base_virt 0x%08x, hidev->shm_size 0x%x",
			hi_dev->shm_base_virt,
			hi_dev->shm_size);

	/* initialize shared memory buffer */
	hisi_shared_mem_init(hi_dev);
	s_device_checked++;

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

	if (g_local_handler->host_handshake_step2(hi_dev)) {
		mcc_trace(MCC_ERR, "Host<=>slv[0x%x] handshake step2 failed",
				hi_dev->slot_index);
		goto host_handshake_step2_err;
	}

	return 0;

host_handshake_step2_err:
host_handshake_step1_err:
host_handshake_step0_err:

	if (hi_dev->shm_base_virt)
		iounmap((void *)hi_dev->shm_base_virt);
	return -1;

}

int slave_check_host(struct hi35xx_dev *hi_dev)
{
	hi_dev->shm_size = DEV_SHM_SIZE;

	if (g_local_handler->slv_handshake_step0()) {
		mcc_trace(MCC_ERR, "Slv<=>host handshake step0 failed");
		return -1;
	}

#ifdef DMA_MESSAGE_ENABLE
	if (hisi_dma_exchange_mem_init()) {
		mcc_trace(MCC_ERR, "DMA exchange memory init failed!");
		goto init_exchange_mem_err;
	}
#endif
	/* initial variables of shared memory buffer */
	hi_dev->shm_end = hi_dev->shm_base + hi_dev->shm_size;
	hisi_shared_mem_init(hi_dev);

	if (g_local_handler->slv_handshake_step1()) {
		mcc_trace(MCC_ERR, "Slv<=>host handshake step1 failed");
		goto slv_handshake_step1_err;
	}

	return 0;

slv_handshake_step1_err:
	hisi_dma_exchange_mem_exit();
init_exchange_mem_err:
	if (hi_dev->shm_base_virt)
		iounmap((void *)hi_dev->shm_base_virt);

	return -1;

}

int host_send_msg(struct hisi_transfer_handle *handle,
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

	if (flag & PRIORITY_DATA) {
		ret = hisi_send_data_irq(dev, buf, len, &head);
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
			((struct msg_number *)g_proc._msg_number.data)->target[i].irq_send++;

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
	}
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

	return ret;
}

int slave_send_msg(struct hisi_transfer_handle *handle,
		const void *buf,
		unsigned int len,
		int flag)
{
	struct hisi_transfer_handle *p = handle;
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

	if (flag & PRIORITY_DATA) {
		struct list_head *clist;
		unsigned long flags;

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
				if (g_hi35xx_dev_map[i]->slot_index == p->target_id)
					break;
			}
			((struct msg_number *)g_proc._msg_number.data)->target[i].irq_send++;

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

		spin_lock_irqsave(&s_pciv_send_list_lock, flags);
		{
			struct pciv_message *pciv_msg;
			void * tmp;
			struct list_head *free_head;
			struct list_head *busy_head;

			free_head = &s_pcivmsg_list_free_head;
			busy_head = &s_pcivmsg_list_busy_head;

			if (list_empty(free_head)) {
				spin_unlock_irqrestore(&s_pciv_send_list_lock, flags);
				mcc_trace(MCC_ERR, "Pre-view message sent too fast!");
				return 0;
			}

			clist = free_head->next;
			list_del(clist);

			pciv_msg = container_of(clist, struct pciv_message, list);

			pciv_msg->target = dev;
			pciv_msg->head.target_id = handle->target_id;
			pciv_msg->head.port = handle->port;
			pciv_msg->head.src = g_local_handler->local_devid;
			pciv_msg->head.length = len;
			pciv_msg->head.check = _HEAD_CHECK_FINISHED;

			if (len > MAX_PCIV_MSG_SIZE) {
				spin_unlock_irqrestore(&s_pciv_send_list_lock, flags);
				mcc_trace(MCC_ERR, "Pre-view message too long[0x%x/0x%x]!",
						len, MAX_PCIV_MSG_SIZE);
				return -1;
			}

			tmp = pciv_msg->msg;
			memcpy(tmp, buf, len);
			pciv_msg->len = len;

			list_add_tail(clist, busy_head);

			s_send_new_msg = 1;

#ifdef MCC_PROC_ENABLE
			if (PROC_LEVEL_I <= g_proc.proc_enable)
				pciv_msg->time_start = msg_start.tv_sec * 1000000 + msg_start.tv_usec;
#endif

#ifdef MCC_PROC_ENABLE
			if (PROC_LEVEL_I <= g_proc.proc_enable) {
				unsigned int *cur_len;
				unsigned int *max_len;
				cur_len = &((struct msg_len *)g_proc._msg_send_len.data)->cur_irq_send;
				max_len = &((struct msg_len *)g_proc._msg_send_len.data)->max_irq_send;
				*cur_len = len;
				if ((*cur_len > *max_len) && (*cur_len < 0x10000000))
					*max_len = *cur_len;

			}
#endif
			wake_up_interruptible(&s_pciv_send_msg_wait);
			ret = len;

		}
		spin_unlock_irqrestore(&s_pciv_send_list_lock, flags);
	} else {
		struct hisi_pci_transfer_head head;

		memset(&head, 0, sizeof(struct hisi_pci_transfer_head));
		head.target_id  = handle->target_id;
		head.port       = handle->port;
		head.src        = g_local_handler->local_devid;
		head.length     = len;
		head.check      = _HEAD_CHECK_FINISHED;

		ret = hisi_send_data_thread(dev, buf, len, &head);
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable) {
			int i = 0;

			for (i = 0; i < g_local_handler->remote_device_number; i++) {
				if (g_hi35xx_dev_map[i]->slot_index == handle->target_id)
					break;
			}
			((struct msg_number *)g_proc._msg_number.data)->target[i].thread_send++;
		}
#endif
	}

#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_I <= g_proc.proc_enable) {
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
	if (PROC_LEVEL_I <= g_proc.proc_enable) {
		unsigned int *cur_len;
		unsigned int *max_len;
		cur_len = &((struct msg_len *)g_proc._msg_send_len.data)->cur_thd_send;
		max_len = &((struct msg_len *)g_proc._msg_send_len.data)->max_thd_send;
		*cur_len = len;
		if (*cur_len > *max_len)
			*max_len = *cur_len;

	}
#endif

	return ret;
}

static void dma_recv_mem(void)
{
	if (g_hi35xx_dev_map[0]->vdd_checked_success != DEVICE_CHECKED_FLAG) {
		mcc_trace(MCC_INFO, "Device[0x%x] has not been checked yet!",
				g_hi35xx_dev_map[0]->slot_index);
		return;
	}

	dma_recv_irq_mem();
	dma_recv_thread_mem();
}

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

static int send_queued_messages(void)
{
	unsigned long flag;
	struct list_head *clist;
	struct pciv_message *pciv_msg;

	while (1) {
		/*
		 * Pre-view message will be process first.
		 */
		spin_lock_irqsave(&s_pciv_send_list_lock, flag);
		if (list_empty(&s_pcivmsg_list_busy_head)) {
			spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);
			break;
		}

		clist = s_pcivmsg_list_busy_head.next;
		list_del(clist);
		spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);

		pciv_msg = container_of(clist, struct pciv_message, list);

		if (-1 == hisi_send_pciv_message(pciv_msg->target,
				pciv_msg->msg,
				pciv_msg->len,
				&pciv_msg->head))
		{
			printk(KERN_ERR "Send pre-view message failed!\n");
			return -1;
		}
#ifdef MCC_PROC_ENABLE
		if (PROC_LEVEL_I <= g_proc.proc_enable)
		{
			struct timeval msg_done;
			unsigned int *cur_cost;
			unsigned int *max_cost;

			cur_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->cur_pciv_cost;
			max_cost = &((struct msg_send_cost *)g_proc._msg_send_cost.data)->max_pciv_cost;

			do_gettimeofday(&msg_done);

			*cur_cost = msg_done.tv_sec * 1000000 + msg_done.tv_usec - pciv_msg->time_start;
			if ((*cur_cost > *max_cost) && (*cur_cost < 0x1000000))
				*max_cost = *cur_cost;

		}
#endif
		spin_lock_irqsave(&s_pciv_send_list_lock, flag);
		list_add_tail(clist, &s_pcivmsg_list_free_head);
		spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);
	}

#if 0
	while (1) {
		/*
		 * Control message second.
		 */
		spin_lock_irqsave(&s_pciv_send_list_lock, flag);
		if (list_empty(&s_streammsg_list_busy_head)) {
			spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);
			break;
		}
		clist = s_streammsg_list_busy_head.next;
		list_del(clist);
		spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);
		pciv_msg = container_of(clist, struct pciv_message, list);
		if (-1 == hisi_send_ctrl_message(pciv_msg->target,
				pciv_msg->msg,
				pciv_msg->len,
				&pciv_msg->head))
		{
			printk(KERN_ERR "Send ctrl-view message failed!\n");
			return -1;
		}
		spin_lock_irqsave(&s_pciv_send_list_lock, flag);
		list_add_tail(clist, &s_streammsg_list_free_head);
		spin_unlock_irqrestore(&s_pciv_send_list_lock, flag);
	}
#endif

	return 0;
}

static int pciv_sendmsg_thread(void * _unused)
{
	do {
		if (0 > send_queued_messages()) {
			/* PCIV message sending should never be failed! */
			mcc_trace(MCC_ERR, "Send pciv message failed!");
			BUG_ON(1);
		}

		s_send_new_msg = 0;
		//wait_event(s_pciv_send_msg_wait, s_send_new_msg);
		wait_event_interruptible(s_pciv_send_msg_wait, s_send_new_msg);
	} while (!s_mcc_module_stop);

	return 0;
}

static int mcc_recv_message(void)
{
#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_I <= g_proc.proc_enable)
	{
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

	if (g_local_handler->is_host())
		host_recv_mem();
	else
		dma_recv_mem();

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

static void pciv_msg_list_init(void)
{
	int i;

	/*
	 * Pre-view message list initialization.
	 */
	s_pcivmsg_list_free_head.next = &s_pcivmsg_list_free_head;
	s_pcivmsg_list_free_head.prev = &s_pcivmsg_list_free_head;
	s_pcivmsg_list_busy_head.next = &s_pcivmsg_list_busy_head;
	s_pcivmsg_list_busy_head.prev = &s_pcivmsg_list_busy_head;

	for (i = MAX_PCIV_MSG_WAIT_NO - 1; i >= 0; i--) {
		list_add_tail(&s_pciv_msg_array[i].list,
				&s_pcivmsg_list_free_head);
	}

#if 0
	/*
	 * Control message list initialization.
	 */
	s_streammsg_list_free_head.next = &s_streammsg_list_free_head;
	s_streammsg_list_free_head.prev = &s_streammsg_list_free_head;
	s_streammsg_list_busy_head.next = &s_streammsg_list_busy_head;
	s_streammsg_list_busy_head.prev = &s_streammsg_list_busy_head;

	for (i = 2 * MAX_PCIV_MSG_WAIT_NO - 1; i >= 0; i--) {
		list_add_tail(&s_stream_msg_array[i].list,
				&s_streammsg_list_free_head);
	}
#endif
}

static int __init msg_com_init(void)
{
	void (*handler)(unsigned long);
	struct hi35xx_dev *hi_dev;
	/*
	 * priority attribute for both DMA message sending and receiving.
	 * priority level may be adjusted from 1 to 99.
	 * zero is not available!
	 */
	struct sched_param param_s = { .sched_priority = 99 };
	struct sched_param param_r = { .sched_priority = 99 };

	printk(version);

	s_mcc_module_stop = 0;

#if 0
	pci_msg_recv_workqueue = alloc_ordered_workqueue("kmcc_rwq", 0);
	if (!pci_msg_recv_workqueue) {
		mcc_trace(MCC_ERR, "Alloc recv workqueue failed!");
		return -ENOMEM;
	}
#endif
	spin_lock_init(&s_pciv_send_list_lock);
	spin_lock_init(&g_mcc_list_lock);
	spin_lock_init(&s_msg_recv_lock);

	if (!g_local_handler->is_host()) {
		hi_dev = g_hi35xx_dev_map[0];

		sema_init(&s_dma_sendmsg_sem, 1);

		pciv_msg_list_init();

		s_pciv_send_thread = kthread_run(pciv_sendmsg_thread, NULL, "kpciv_send");
		if (IS_ERR(s_pciv_send_thread)) {
			mcc_trace(MCC_ERR, "Start preview msg-sending thread failed!");
			return -1;
		}

		if (sched_setscheduler(s_pciv_send_thread, SCHED_FIFO, &param_s))
			mcc_trace(MCC_ERR, "Set thread priority failed!\n");
	}

	s_mcc_recv_thread = kthread_run(mcc_recvmsg_thread, NULL, "kmcc_recv");
	if (IS_ERR(s_mcc_recv_thread)) {
		mcc_trace(MCC_ERR, "Start pci message receiving thread failed!");
		kthread_stop(s_pciv_send_thread);
		return -1;
	}

	if (sched_setscheduler(s_mcc_recv_thread, SCHED_FIFO, &param_r))
		mcc_trace(MCC_ERR, "Set thread priority failed!\n");

	if (g_local_handler->is_host()) {
		handler = hisi_msg_irq_host;
	} else {
		handler = hisi_msg_irq_slave;
	}

#ifdef MESSAGE_IRQ_ENABLE
	if (g_local_handler->request_msg_irq(handler)) {
		mcc_trace(MCC_ERR, "Request msg irq failed!");
		kthread_stop(s_mcc_recv_thread);
		kthread_stop(s_pciv_send_thread);
		return -1;
	}
#else
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
#endif
	del_timer(&recv_msg_timer);

	s_mcc_module_stop = 1;
	s_recv_new_msg = 1;
	s_send_new_msg = 1;

	kthread_stop(s_mcc_recv_thread);
	/* quit pciv send message thread */
	if (!g_local_handler->is_host()) {
		kthread_stop(s_pciv_send_thread);
	}

}

module_init(msg_com_init);
module_exit(msg_com_exit);

MODULE_AUTHOR("Hisilicon");
MODULE_DESCRIPTION("Hisilicon Hi35XX");
MODULE_LICENSE("GPL");
MODULE_VERSION("HI_VERSION=" __DATE__);

