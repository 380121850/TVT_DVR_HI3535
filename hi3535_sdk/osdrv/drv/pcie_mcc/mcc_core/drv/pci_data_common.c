#include <asm/io.h>
#include <linux/semaphore.h>
#include <linux/slab.h>
#include <asm/delay.h>

//#define PCIE_MAX_DATA_ONE_TIME	128

const static unsigned char fixed_mem[_MEM_ALIAGN_CHECK];
struct hisi_map_shared_memory_info hisi_slave_info_map;
struct hisi_map_shared_memory_info hisi_host_info_map[MAX_PCIE_DEVICES];

#ifdef DMA_MESSAGE_ENABLE
struct hisi_dma_exchange_buffer hisi_slv_exchg_buf;
#endif

#ifdef __FOR_SPECIAL_MEM_DEBUG
static unsigned int s_tcm_virt = 0;
#endif

#ifdef DMA_MESSAGE_ENABLE
static unsigned int s_irqmsg_already_go = 0;
static unsigned int s_threadmsg_already_go = 0;
static unsigned int s_sendirq_last_wp = 0;
static unsigned int s_sendthread_last_wp = 0;
#endif
static unsigned int s_send_irq_base = 0;
static unsigned int s_send_thread_base = 0;

spinlock_t g_mcc_data_lock;

inline static void write_to_buffer(unsigned int value, unsigned long addr)
{
	volatile unsigned int tmp_rp;
	unsigned int times = 0, time_max = 100;

	do {
		/*
		 * Think about it: we may not need this lock here.
		 */
		writel(value, addr);

		tmp_rp = readl(addr);
		if (tmp_rp == value)
			return;

		times++;
	} while (times < time_max);

	mcc_trace(MCC_ERR, "Update msg read/write pointer failed!");
	BUG_ON(1);
}

int hisi_is_irq_buffer_empty(void *dev)
{
	struct hisi_shared_memory_info *p;
	unsigned int rp, wp;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv;
	}

	mcc_trace(MCC_DEBUG, "rp_addr[0x%08x], wp_addr[0x%08x]",
			(unsigned int)p->irqmem.rp_addr,
			(unsigned int)p->irqmem.wp_addr);

	rp = readl(p->irqmem.rp_addr);
	wp = readl(p->irqmem.wp_addr);

	mcc_trace(MCC_DEBUG, "rp 0x%x, wp 0x%x", rp, wp);

	return (rp == wp);
}

int hisi_is_thread_buffer_empty(void *dev)
{
	struct hisi_shared_memory_info *p;
	unsigned int rp, wp;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv;
	}

	rp = readl(p->threadmem.rp_addr);
	wp = readl(p->threadmem.wp_addr);

	mcc_trace(MCC_DEBUG, "rp 0x%x, wp 0x%x", rp, wp);

	return (rp == wp);
}

static void * _wait_data_ready(struct hisi_memory_info *p)
{
	struct hisi_pci_transfer_head * head = NULL;
	unsigned int rp, wp;
	unsigned int dst;
	unsigned int len, aliagn;
	unsigned int head_check = 0;
	unsigned int try_times = 100;
	unsigned int try = 0;

	rp = readl(p->rp_addr);
	wp = readl(p->wp_addr);
	mcc_trace(MCC_INFO, "rp 0x%x, wp 0x%x", rp, wp);

	head = (struct hisi_pci_transfer_head *)(rp + p->base_addr);

	mcc_trace(MCC_DEBUG, "head 0x%08x head->check 0x%x head->magic 0x%x",
			(unsigned int)head, head->check, head->magic);

	head_check = (unsigned int)(head->check);

	while (head_check != _HEAD_CHECK_FINISHED) {
		/*
		 * Normally, it should never get here!
		 * The head_check of each msg should always match
		 * _HEAD_CHECK_FINISHED unless some unexpected
		 * problems occur in PCIe physical link.
		 */

		if (unlikely((++try) >= try_times)) {
			mcc_trace(MCC_ERR, "PCIe message head_check error "
					"[0x%x/0x%x]!", head_check,
					_HEAD_CHECK_FINISHED);
			mcc_trace(MCC_ERR, "rp[0x%x], wp[0x%x]", rp, wp);
			//BUG_ON((try) >= try_times);
			rp = readl(p->rp_addr);
			wp = readl(p->wp_addr);
			msleep(1000);
		}

		udelay(10);

		head_check = (unsigned int)(head->check);
	}

	/* If head has been marked as a jump to top */
	if (head->magic == _HEAD_MAGIC_JUMP) {

		mcc_trace(MCC_INFO, "head.magic==_HEAD_MAGIC_JUMP.");

		memset(head, 0xCD, _HEAD_SIZE);
		write_to_buffer(0, p->rp_addr);

		head = (struct hisi_pci_transfer_head *)(p->base_addr);
		rp = 0;

		mcc_trace(MCC_INFO, "jump head 0x%08x, head->check 0x%x, "
				"head->magic 0x%x",
				(unsigned int)head, head->check,
				head->magic);

		head_check = (unsigned int)(head->check);

		try = 0;
		while (head_check != _HEAD_CHECK_FINISHED) {
			/*
			 * Normally, it should never get here!
			 * The head_check of each msg should always match
			 * _HEAD_CHECK_FINISHED unless some unexpected
			 * problems occur in PCIe physical link.
			 */
			udelay(10);

			if (unlikely((++try) >= try_times)) {
				mcc_trace(MCC_ERR, "Message head_check "
					"error[0x%x/0x%x]!",
					head_check, _HEAD_CHECK_FINISHED);
				mcc_trace(MCC_ERR, "rp[0x%x], wp[0x%x]", rp, wp);

				//BUG_ON((try) >= try_times);
				msleep(10);
			}

			head_check = (unsigned int)(head->check);
		}

	}

	len = head->length;
	aliagn = _MEM_ALIAGN_VERS & (_HEAD_SIZE + len);
	dst = rp + p->base_addr + len + _HEAD_SIZE + _MEM_ALIAGN - aliagn;
	mcc_trace(MCC_DEBUG, "dst 0x%08x, len 0x%x", (unsigned int)dst, len);

	/* wait for data finished */
	while (memcmp((void *)dst, (void *)&fixed_mem, _MEM_ALIAGN_CHECK)) {
		printk(KERN_ERR "rp 0x%x, wp 0x%x\n", rp, wp);
		mcc_trace(MCC_ERR, "read data is error!");
		BUG_ON(1);
	}

	return head;
}

void *hisi_get_thread_head(void *dev)
{
	struct hisi_memory_info *p;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv.threadmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv.threadmem;
	}

	return _wait_data_ready(p);
}

void *hisi_get_irq_head(void *dev)
{
	struct hisi_memory_info *p;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv.irqmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv.irqmem;
	}

	return _wait_data_ready(p);
}

static void _data_recv(struct hisi_memory_info *p,
		void *data,
		unsigned int len)
{
	unsigned int aliag = (len + _HEAD_SIZE) & _MEM_ALIAGN_VERS;
	unsigned int fixlen = _MEM_ALIAGN + _MEM_ALIAGN - aliag;
	unsigned int next_rp;
	void *start = data;
	void *end;

	next_rp = len + (unsigned long)data - p->base_addr + fixlen;

	/* clean shared memory */
	memset(data, 0xA5, len + fixlen);

	mcc_trace(MCC_INFO, "data 0x%08x, len 0x%x",
			(unsigned int)data, len + fixlen);

	/* clean head memory place */
	/* set slice head to 0xCD */
	end = data + len + fixlen;

	for (; start < end ; start += _MEM_ALIAGN) {
		mcc_trace(MCC_INFO, "start 0x%08x, end 0x%08x",
				(unsigned int)(start - _HEAD_SIZE),
				(unsigned int)end);
		memset(start - _HEAD_SIZE, 0xCD, _HEAD_SIZE);
	}

	mcc_trace(MCC_INFO, "next_rp %x, p->rp_addr 0x%08x",
			next_rp, p->rp_addr);

	write_to_buffer(next_rp, p->rp_addr);
}


void hisi_thread_data_recieved(void *dev, void *data, unsigned int len)
{
	struct hisi_memory_info *p;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv.threadmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv.threadmem;
	}

	_data_recv(p, data, len);
}

void hisi_irq_data_recieved(void *dev, void *data, unsigned int len)
{
	struct hisi_memory_info *p = NULL;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->recv.irqmem;
	}else{
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->recv.irqmem;
	}

	_data_recv(p, data, len);

}

static inline unsigned long _memcpy_shbuf(unsigned long wp,
		unsigned long base,
		const void *buf,
		const void *head,
		unsigned int len)
{
	unsigned int fixlen;
	unsigned int aliagn = (len + _HEAD_SIZE) & _MEM_ALIAGN_VERS;
	unsigned int next_wp;
	unsigned int addr;

	fixlen =  _MEM_ALIAGN + _MEM_ALIAGN - aliagn;
	next_wp = wp + len + fixlen + _HEAD_SIZE;
	addr = wp + base + _HEAD_SIZE;

	mcc_trace(MCC_INFO, "addr 0x%08x, len %x", addr, len);

	/* bottom of buf has enough space */
#ifdef PCIE_MAX_DATA_ONE_TIME
	{
		unsigned int tmp = len;
		unsigned int addr_tmp = 0;
		unsigned int buf_tmp = 0;
		addr_tmp = addr;
		buf_tmp = (unsigned int)buf;
		while (tmp) {
			if (PCIE_MAX_DATA_ONE_TIME < tmp) {
				memcpy((void *)addr_tmp, (void *)buf_tmp,
						PCIE_MAX_DATA_ONE_TIME);
				tmp -= PCIE_MAX_DATA_ONE_TIME;
				addr_tmp += PCIE_MAX_DATA_ONE_TIME;
				buf_tmp += PCIE_MAX_DATA_ONE_TIME;
				udelay(500);
			} else {
				memcpy((void *)addr_tmp, (void *)buf_tmp, tmp);
				tmp = 0;
			}
		}
	}
#else
	memcpy((void *)addr, buf, len);

#endif

	mcc_trace(MCC_INFO, "addr 0x%08x, fixlen %x", addr + len, fixlen);

	memset((void *)(addr + len), _FIX_ALIAGN_DATA, fixlen);

	memcpy((void *)(addr - _HEAD_SIZE), head, _HEAD_SIZE);

	mcc_trace(MCC_DEBUG, "buf[0x%08x], head[0x%08x %08x], len[0x%x]",
			*(unsigned int *)buf,
			*(int *)head,
			*((int *)head+4),
			len);
	mcc_trace(MCC_DEBUG, "wp 0x%x, next_wp 0x%08x",
			(unsigned int)wp, next_wp);

	return next_wp;
}

static int _sendto_shmbuf(struct hisi_memory_info *p,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	unsigned int  aliagn = (len + _HEAD_SIZE) & _MEM_ALIAGN_VERS;
	unsigned int base = p->base_addr;
	unsigned int rp, wp;
	unsigned int next_wp;
	unsigned int ret;

	aliagn = _MEM_ALIAGN + _MEM_ALIAGN - aliagn;

	rp = readl(p->rp_addr);
	wp = readl(p->wp_addr);

	mcc_trace(MCC_INFO, "head->magic 0x%x head 0x%08x len %x",
			head->magic, (unsigned int)head, head->length);
	mcc_trace(MCC_INFO, "wp 0x%x, rp 0x%x", wp, rp);

	if (wp >= rp) {

		if ((wp + base + _HEAD_SIZE + len + aliagn) < p->end_addr) {

			mcc_trace(MCC_INFO, "wp >= rp, len %x", len);
			next_wp = _memcpy_shbuf(wp, base, buf, head, len);

			mcc_trace(MCC_INFO, "writel to wp_addr 0x%08x, len %x",
					p->wp_addr, len);
			write_to_buffer(next_wp, p->wp_addr);
			ret = len;

		} else if (rp > (_HEAD_SIZE + len + aliagn)) {
			/*
			 * bottom do not have enough space, check the top
			 */
			struct hisi_pci_transfer_head mark;

			mcc_trace(MCC_INFO, "Before jump:wp[0x%x],rp[0x%x],len[0x%x]",
					wp, rp, len);

			memcpy((void *)&mark, (void *)head, _HEAD_SIZE);

			mark.magic = _HEAD_MAGIC_JUMP;

			mcc_trace(MCC_INFO, "Send head: 0x%08x 0x%08x.",
					*(unsigned int *)&mark,
					*((unsigned int *)&mark + 1));

			memcpy((void *)(wp + base),
					&mark,
					_HEAD_SIZE);

			next_wp = _memcpy_shbuf(0, base, buf, head, len);

			mcc_trace(MCC_INFO, "After jump:next_wp[0x%x].", next_wp);

			write_to_buffer(next_wp, p->wp_addr);
			ret = len;
		} else {
			/* top do not have enough space */
			mcc_trace(MCC_ERR, "Top doesn't have enough space,"
					"wp 0x%x,rp 0x%x;", wp, rp);
			ret = -1;
		}

	} else if ((rp - wp) > (_HEAD_SIZE + len + aliagn)) {
		/* read point larger than write point */
		mcc_trace(MCC_INFO, "wp < rp, len %x", len);

		/* have enough space between rp and wp */
		next_wp = _memcpy_shbuf(wp, base, buf, head, len);

		write_to_buffer(next_wp, p->wp_addr);

		ret = len;
	} else {
		/* top do not have enough space */
		mcc_trace(MCC_ERR, "Top doesn't have enough space,"
				"wp 0x%x,rp 0x%x;", wp, rp);
		ret = -1;
	}

	return ret;
}

#ifdef DMA_MESSAGE_ENABLE
static void _dma_rw_done(struct pcit_dma_task *task)
{
	wake_up_interruptible((wait_queue_head_t *)task->private_data);
}

static int get_shm_rw_pointer(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_buf)
{
	struct pcit_dma_task task;
	struct pcit_dma_task *ptask = NULL;
	/* timeout 3000 ms */
	unsigned int timeout = 3000;
	int ret;

	task.state = DMA_TASK_TODO;
	task.dir = PCI_DMA_READ;
	task.src = p->rp_addr;
	task.dest = exchg_buf->rp_addr;
	/*
	 * get rp and wp from shared memroy, and DMA size 64Bytes.
	 *
	 * 00   --[rp]-- ******** ******** ********
	 * 01   ******** ******** ******** ********
	 * 02   --[wp]-- ******** ******** ********
	 * 03   ******** ******** ******** ********
	 */
	task.len = 64;
	task.finish = _dma_rw_done;
	task.private_data = &exchg_buf->wait;

	do {
		ptask = __pcit_create_task(&task, __CMESSAGE);
		if (NULL == ptask) {
			mcc_trace(MCC_ERR, "Pcit create DMA task failed!");
			msleep(10);
			continue;
		}

		mcc_trace(MCC_DEBUG, "Task: src[0x%08x],dest[0x%08x],len[0x%x]",
				task.src, task.dest, task.len);

		ret = wait_event_interruptible_timeout(exchg_buf->wait,
				(DMA_TASK_FINISHED == ptask->state),
				((timeout * HZ)/1000));
		if (-ERESTARTSYS == ret) {
			mcc_trace(MCC_ERR, "error! we are interrupted by a signal[%d]",
					ret);
			BUG_ON(1);
		}

		if (DMA_TASK_FINISHED != ptask->state) {
			mcc_trace(MCC_ERR, "Thread get rp/wp from shared mem wait timeout,"
					" system is not stable![0x%x]", ptask->state);
			BUG_ON(1);
		} else
			break;
	} while (1);

	ptask->state = DMA_TASK_EMPTY;

	return 0;
}

static int set_shm_rw_pointer(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_buf)
{
	struct pcit_dma_task task;
	struct pcit_dma_task *ptask;
	/* timeout 3000 ms */
	unsigned int timeout = 3000;
	int ret;

	task.state = DMA_TASK_TODO;
	task.dir = PCI_DMA_WRITE;
	if (DMA_EXCHG_READ == exchg_buf->type) {
		task.src = exchg_buf->rp_addr;
		task.dest = p->rp_addr;
	} else {
		task.src = exchg_buf->wp_addr;
		task.dest = p->wp_addr;
	}
	/*
	 * get rp or wp from shared memroy, and DMA size 32Bytes.
	 *
	 * 00   --[rp]-- ******** ******** ********
	 * 01   ******** ******** ******** ********
	 * 02   --[wp]-- ******** ******** ********
	 * 03   ******** ******** ******** ********
	 */
	task.len = 32;
	task.finish = _dma_rw_done;
	task.private_data = &exchg_buf->wait;

	do {
		ptask = __pcit_create_task(&task, __CMESSAGE);
		if (NULL == ptask) {
			mcc_trace(MCC_ERR, "Pcit create DMA task failed!");
			msleep(10);
			continue;
		}

		mcc_trace(MCC_DEBUG, "Task: src[0x%08x],dest[0x%08x],len[0x%x]",
				task.src, task.dest, task.len);

		ret = wait_event_interruptible_timeout(exchg_buf->wait,
				(DMA_TASK_FINISHED == ptask->state),
				((timeout * HZ)/1000));
		if (-ERESTARTSYS == ret) {
			mcc_trace(MCC_ERR, "error! we are interrupted by a signal[%d]",
					ret);
			BUG_ON(1);
		}

		if (DMA_TASK_FINISHED != ptask->state) {
			mcc_trace(MCC_ERR, "Thread set rp/wp to shared mem wait timeout,"
					" system is not stable![0x%x]", ptask->state);
			BUG_ON(1);
		} else {
			break;
		}
	} while (1);

#if 0
	if (DMA_EXCHG_READ == exchg_buf->type) {
		unsigned int pp = *(unsigned int*)s_tcm_virt;
		if (pp == 0x2000)
			pp = 0;
		writel(*(unsigned int*)exchg_buf->rp_addr_virt, s_tcm_virt + (4 + pp));
		writel(0xffffffff, s_tcm_virt + (8 + pp));
		*(unsigned int *)s_tcm_virt = pp + 4;

	}
#endif
	ptask->state = DMA_TASK_EMPTY;

	return 0;
}

static int write_dma_msg(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_buf,
		unsigned int dma_len)
{
	struct pcit_dma_task task;
	struct pcit_dma_task *ptask;
	unsigned int timeout = 3000;
	int ret;

	task.state = DMA_TASK_TODO;
	task.dir = PCI_DMA_WRITE;
	task.src = exchg_buf->data_base;
	task.dest = p->base_addr + *(unsigned int *)(exchg_buf->wp_addr_virt);
	task.len = dma_len;
	task.finish = _dma_rw_done;
	task.private_data = &exchg_buf->wait;

	do {
		ptask = __pcit_create_task(&task, __CMESSAGE);
		if (NULL == ptask) {
			mcc_trace(MCC_ERR, "Pcit create DMA task failed!");
			msleep(10);
			continue;
		}

		/* No need to sleep and wait here. */
		ret = wait_event_interruptible_timeout(exchg_buf->wait,
				(DMA_TASK_FINISHED == ptask->state),
				((timeout * HZ)/1000));

		if (-ERESTARTSYS == ret) {
			mcc_trace(MCC_ERR, "error! we are interrupted by a signal[%d]",
					ret);
			BUG_ON(1);
		}

		if (DMA_TASK_FINISHED != ptask->state) {
			mcc_trace(MCC_ERR, "Thread dma msg write lost,"
				       " system not stable![0x%x]",
					ptask->state);
			BUG_ON(1);
		} else
			break;
	} while (1);

	ptask->state = DMA_TASK_EMPTY;

	return 0;
}

/*
 * Send DMA message:
 * return: -1 if failed.
 *	   len, if success.
 */
static int _dma_send_msg(struct hisi_memory_info *p,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	struct dma_exchange_buf *exchg_write_buf = &hisi_slv_exchg_buf.write_buf;
	unsigned int align = (len + _HEAD_SIZE) & _MEM_ALIAGN_VERS;
	void *dest;
	unsigned int dma_size = 0;
	unsigned int rp = 0;
	unsigned int wp = 0;

	align = _MEM_ALIAGN + _MEM_ALIAGN - align;

#if 0
	if (in_interrupt()) {
		while (1) {
			printk("INTERRUP.\n");
			udelay(1000);
		}
	}
#endif

	if (down_interruptible(&exchg_write_buf->sem)) {
		mcc_trace(MCC_ERR, "acquire shared handle sem failed!");
		return -1;
	}

	/* Assemble DMA data in exchange buffer */
	dest = (void *)exchg_write_buf->data_base_virt;
	memcpy(dest, (void *)head, sizeof(struct hisi_pci_transfer_head));
	dest += sizeof(struct hisi_pci_transfer_head);
	memcpy(dest, buf, len);
	dest += len;
	memset(dest, _FIX_ALIAGN_DATA, align);
	dma_size = sizeof(struct hisi_pci_transfer_head) + len + align;

	do {
		if (get_shm_rw_pointer(p, exchg_write_buf)) {
			mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
			goto write_dma_err;
		}

		rp = readl(exchg_write_buf->rp_addr_virt);
		wp = readl(exchg_write_buf->wp_addr_virt);

		if (s_send_irq_base == p->base_addr) {
			if (0 == s_irqmsg_already_go) {
				s_irqmsg_already_go = 1;
				s_sendirq_last_wp = wp;
				break;
			}

			if (s_sendirq_last_wp == wp) {
				mcc_trace(MCC_INFO, "Shared mem[0x%x] wp[0x%x]"
						"not updated yet, Retry!",
						p->base_addr, wp);
				continue;
			} else {
				s_sendirq_last_wp = wp;
				break;
			}
		} else if (s_send_thread_base == p->base_addr) {
			if (0 == s_threadmsg_already_go) {
				s_threadmsg_already_go = 1;
				s_sendthread_last_wp = wp;
				break;
			}

			if (s_sendthread_last_wp == wp) {
				mcc_trace(MCC_INFO, "Shared mem[0x%x] wp[0x%x]"
						"not updated yet, Retry!",
						p->base_addr, wp);
				continue;
			} else {
				s_sendthread_last_wp = wp;
				break;
			}
		} else {
			mcc_trace(MCC_ERR, "Message destination err[0x%x]",
					p->base_addr);
			BUG_ON(1);
		}
	} while (1);

#if 0
	{
		int __index;
		unsigned int *tmp;

		mcc_trace(MCC_DEBUG, "align[0x%x], dma_len[0x%x].",
				align, dma_size);
		mcc_trace(MCC_DEBUG, " EXCHANGE BUF :");

		tmp = (unsigned int *)exchg_write_buf->data_base_virt;
		for (__index = 0; __index < dma_size/4; __index += 4, tmp += 4)
			printk(KERN_INFO "%x0:	0x%08x  0x%08x  0x%08x  0x%08x\n",
					__index,
					*tmp,
					*(tmp + 1),
					*(tmp + 2),
					*(tmp + 3));

		printk(KERN_ERR "\n");
	}
#endif

	/*
	 * Think about it here, very very carefully ......
	 */
	//rp = *(volatile unsigned int *)(exchg_write_buf->rp_addr_virt);
	//wp = *(volatile unsigned int *)(exchg_write_buf->wp_addr_virt);


	if (wp >= rp) {
		if (dma_size < (p->end_addr - p->base_addr - wp)) {

			if (write_dma_msg(p, exchg_write_buf, dma_size)) {
				mcc_trace(MCC_ERR, "Create message DMA failed!");
				goto write_dma_err;
			}

		} else if (dma_size < rp) {

			unsigned int _magic;

			mcc_trace(MCC_INFO, "Before jump:wp[0x%x],rp[0x%x],len[0x%x]",
					wp, rp, len);
			mcc_trace(MCC_INFO, "	shm_section:base[0x%x],end[0x%x]",
					p->base_addr, p->end_addr);

			_magic = head->magic;
			head->magic = _HEAD_MAGIC_JUMP;

			mcc_trace(MCC_INFO, "Jump head: 0x%08x  0x%08x",
					*(unsigned int *)head,
					*((unsigned int *)head + 1));

			memcpy((void *)exchg_write_buf->data_base_virt,
					(void *)head,
					sizeof(struct hisi_pci_transfer_head));

			if (write_dma_msg(p, exchg_write_buf,
					sizeof(struct hisi_pci_transfer_head))) {
				mcc_trace(MCC_ERR, "Create message DMA failed!");
				goto write_dma_err;
			}
			/*
			 * Update write pointer here is to make sure data has
			 * been carried to shared memory buffer.
			 */
			if (set_shm_rw_pointer(p, exchg_write_buf)) {
				mcc_trace(MCC_ERR, "Update DMA msg write pointer failed!");
				goto write_dma_err;
			}

			/* Transferring real message body to shared memory buffer. */
			*(volatile unsigned int *)(exchg_write_buf->wp_addr_virt) = 0x0;

			head->magic = _magic;

			mcc_trace(MCC_INFO, "Start head: 0x%08x  0x%08x",
					*(unsigned int *)head,
					*((unsigned int *)head + 1));

			memcpy((void *)exchg_write_buf->data_base_virt,
					(void *)head,
					sizeof(struct hisi_pci_transfer_head));

			if (write_dma_msg(p, exchg_write_buf, dma_size)) {
				mcc_trace(MCC_ERR, "Create message DMA failed!");
				goto write_dma_err;
			}

		} else {

			mcc_trace(MCC_ERR, "Not enough space left for DMA msg!");
			goto write_dma_err;

		}
	} else if ((rp - wp) > dma_size) {

			if (write_dma_msg(p, exchg_write_buf, dma_size)) {
				mcc_trace(MCC_ERR, "Create message DMA failed!");
				goto write_dma_err;
			}

	} else {

		mcc_trace(MCC_ERR, "Not enough space left for DMA msg!");
		goto write_dma_err;

	}

	//*(volatile unsigned int *)(exchg_write_buf->wp_addr_virt) += dma_size;
	wp = readl(exchg_write_buf->wp_addr_virt);
	writel((wp + dma_size), exchg_write_buf->wp_addr_virt);
	//barrier();

	if (set_shm_rw_pointer(p, exchg_write_buf)) {
		mcc_trace(MCC_ERR, "Update DMA msg write pointer failed!");
		goto write_dma_err;
	}

	up(&exchg_write_buf->sem);

	return len;

write_dma_err:

	up(&exchg_write_buf->sem);

	return -1;
}
#endif

int hisi_send_data_irq(void *dev,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	struct hisi_memory_info *p;
	int ret;
	struct hi35xx_dev *pdev = (struct hi35xx_dev *)dev;
	unsigned long flags;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->send.irqmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[pdev->slot_index];
		p = &pinfo->send.irqmem;
	}

	spin_lock_irqsave(&g_mcc_data_lock, flags);

	ret = _sendto_shmbuf(p, buf, len, head);
	if (ret < 0) {
		spin_unlock_irqrestore(&g_mcc_data_lock, flags);

		mcc_trace(MCC_ERR,"head.check %x", head->check);
		return ret;
	}

#ifdef MESSAGE_IRQ_ENABLE
	g_local_handler->trigger_msg_irq((unsigned int)pdev);
#endif

	spin_unlock_irqrestore(&g_mcc_data_lock, flags);

	return ret;
}

#ifdef DMA_MESSAGE_ENABLE
/* Slave send pre-view message */
int hisi_send_pciv_message(void *pdev,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	struct hisi_memory_info *p;
	int ret = 0;
	struct hi35xx_dev *dev = (struct hi35xx_dev *)pdev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->send.irqmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[dev->slot_index];
		p = &pinfo->send.irqmem;
	}

	if  (down_interruptible(&p->sem)) {
		mcc_trace(MCC_ERR, "acquire shared handle sem failed!");
		return -1;
	}

	ret = _dma_send_msg(p, buf, len, head);
	if (ret < 0) {
		mcc_trace(MCC_ERR, "DMA send msg failed![head.check:0x%x]",
				head->check);
	}

	up(&p->sem);

	return ret;
}

/* Slave send pre-view message */
int hisi_send_ctrl_message(void *pdev,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	struct hisi_memory_info *p;
	int ret = 0;
	struct hi35xx_dev *dev = (struct hi35xx_dev *)pdev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->send.threadmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[dev->slot_index];
		p = &pinfo->send.threadmem;
	}

	if  (down_interruptible(&p->sem)) {
		mcc_trace(MCC_ERR, "acquire shared handle sem failed!");
		return -1;
	}

	ret = _dma_send_msg(p, buf, len, head);
	if (ret < 0) {
		mcc_trace(MCC_ERR, "DMA send msg failed![head.check:0x%x]",
				head->check);
	}

	up(&p->sem);

	return ret;
}
#endif

int hisi_send_data_thread(void *pdev,
		const void *buf,
		unsigned int len,
		struct hisi_pci_transfer_head *head)
{
	struct hisi_memory_info *p;
	int ret;
	struct hi35xx_dev *dev = (struct hi35xx_dev *)pdev;

	if (!g_local_handler->is_host()) {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_slave_info_map;
		p = &pinfo->send.threadmem;
	} else {
		struct hisi_map_shared_memory_info *pinfo
			= &hisi_host_info_map[dev->slot_index];
		p = &pinfo->send.threadmem;
	}

	if (down_interruptible(&p->sem)) {
		mcc_trace(MCC_ERR, "Acquire shared handle sem failed!");
		return -1;
	}

#ifdef DMA_MESSAGE_ENABLE
	if (g_local_handler->is_host()) {
		/* CPU memcpy to remote side */
		ret = _sendto_shmbuf(p, buf, len, head);
		if (ret < 0) {
			mcc_trace(MCC_ERR, "######## head.check ######## 0x%x",
					head->check);
		}
	} else {
		/* DMA transferring to remote side */
		ret = _dma_send_msg(p, buf, len, head);
		if (ret < 0) {
			mcc_trace(MCC_ERR, "DMA send msg failed!head.check 0x%x",
					head->check);
		}
	}
#else
	ret = _sendto_shmbuf(p, buf, len, head);
	if (ret < 0) {
		mcc_trace(MCC_ERR, "######## head.check ######## 0x%x",
				head->check);
	}

#endif

#ifdef MESSAGE_IRQ_ENABLE
#if 0
	if (!timer_pending(&msg_trigger_timer)) {
		msg_trigger_timer.expires = jiffies + msecs_to_jiffies(10);
		msg_trigger_timer.data = (unsigned int)dev;
		msg_trigger_timer.function = g_local_handler->trigger_msg_irq;
		add_timer(&msg_trigger_timer);
	}
#endif
	if (!timer_pending(&dev->timer)) {
		dev->timer.expires = jiffies + msecs_to_jiffies(10);
		dev->timer.data = (unsigned int)dev;
		dev->timer.function = g_local_handler->trigger_msg_irq;
		add_timer(&dev->timer);
	}

#endif
	up(&p->sem);

	return ret;
}

#ifdef DMA_MESSAGE_ENABLE
static int dma_clear_shm(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_buf,
		unsigned int dma_len)
{
	unsigned int start = exchg_buf->data_base_virt;
	unsigned int end = start + dma_len;
	struct pcit_dma_task task;
	struct pcit_dma_task *ptask;
	unsigned int timeout = 3000;
	int ret;

	memset((void *)start, 0xA5, dma_len);
	for (;start < end; start += _MEM_ALIAGN)
		memset((void *)start, 0xCD, _HEAD_SIZE);

	task.state = DMA_TASK_TODO;
	task.dir = PCI_DMA_WRITE;
	task.src = exchg_buf->data_base;
	task.dest = p->base_addr + *(unsigned int *)(exchg_buf->rp_addr_virt);
	task.len = dma_len;
	task.finish = _dma_rw_done;
	task.private_data = &exchg_buf->wait;

	do {
		ptask = __pcit_create_task(&task, __CMESSAGE);
		if (NULL == ptask) {
			mcc_trace(MCC_ERR, "Pcit create DMA task failed!");
			msleep(10);
			continue;
		}

		ret = wait_event_interruptible_timeout(exchg_buf->wait,
				(DMA_TASK_FINISHED == ptask->state),
				((timeout * HZ)/1000));
		if (-ERESTARTSYS == ret) {
			mcc_trace(MCC_ERR, "error! we are interrupted by a signal[%d]",
					ret);
			BUG_ON(1);
		}

		if (DMA_TASK_FINISHED != ptask->state) {
			mcc_trace(MCC_ERR, "Thread clear shm wait timeout,"
					" system is not stable![0x%x]",
					ptask->state);
			BUG_ON(1);
		} else
			break;
	} while (1);

	ptask->state = DMA_TASK_EMPTY;

	return 0;
}

static int read_dma_msg(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_buf,
		unsigned int dma_len,
		void (*finish)(struct pcit_dma_task *task))
{
	struct pcit_dma_task task;
	struct pcit_dma_task *ptask;
	/* timeout 3000 ms */
	unsigned int timeout = 3000;
	int ret;

	task.state = DMA_TASK_TODO;
	task.dir = PCI_DMA_READ;
	task.src = p->base_addr + *(unsigned int *)(exchg_buf->rp_addr_virt);
	task.dest = exchg_buf->data_base;
	task.len = dma_len;
	task.finish = finish;
	task.private_data = &exchg_buf->wait;

	do {
		ptask = __pcit_create_task(&task, __CMESSAGE);
		if (NULL == ptask) {
			mcc_trace(MCC_ERR, "Pcit create DMA task failed!");
			msleep(10);
			continue;
		}

		if (finish) {
			ret = wait_event_interruptible_timeout(exchg_buf->wait,
					(DMA_TASK_FINISHED == ptask->state),
					((timeout * HZ)/1000));
			if (-ERESTARTSYS == ret) {
				mcc_trace(MCC_ERR, "error! we are interrupted by a signal[%d]",
						ret);
				BUG_ON(1);
			}

			if (DMA_TASK_FINISHED != ptask->state) {
				mcc_trace(MCC_ERR, "Thread read message wait timeout,"
						" system is not stable![0x%x]",
						ptask->state);
				BUG_ON(1);
			} else
				break;
		}
	} while (1);

	ptask->state = DMA_TASK_EMPTY;

	return 0;
}

static int dma_receive_message(struct hisi_memory_info *p,
		struct dma_exchange_buf *exchg_read_buf,
		unsigned int flag)
{
	struct hisi_pci_transfer_head *head;
	unsigned int rp = 0;
	unsigned int wp = 0;

	if (down_interruptible(&exchg_read_buf->sem)) {
		mcc_trace(MCC_ERR, "Down exchange buf sem failed!");
		return -1;
	}

	if (get_shm_rw_pointer(p, exchg_read_buf)) {
		mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
		goto dma_recv_irq_msg_out;
	}

	rp = *(volatile unsigned int *)(exchg_read_buf->rp_addr_virt);
	wp = *(volatile unsigned int *)(exchg_read_buf->wp_addr_virt);

	mcc_trace(MCC_DEBUG, "rp[0x%x], wp[0x%x]", rp, wp);

	if (rp == wp)
		goto dma_receive_normal_out;

	if (wp > rp) {

		if (read_dma_msg(p, exchg_read_buf, (wp - rp), _dma_rw_done)) {
			mcc_trace(MCC_ERR, "Dma read message failed!");
			goto dma_recv_irq_msg_out;
		}

	} else {

		if (read_dma_msg(p, exchg_read_buf,
					(p->end_addr - p->base_addr - rp),
					_dma_rw_done))
		{
			mcc_trace(MCC_ERR, "Dma read message failed!");
			goto dma_recv_irq_msg_out;
		}

		mcc_trace(MCC_DEBUG, "Shared mem section: base[0x%08x],"
				" end[0x%08x], DMA transfer len[0x%x]",
				p->base_addr,
				p->end_addr,
				(p->end_addr - p->base_addr - rp));

		head = (struct hisi_pci_transfer_head *)exchg_read_buf->data_base_virt;

		if (_HEAD_MAGIC_JUMP != head->magic) {

			mcc_trace(MCC_DEBUG, "DMA message receiving, not jump yet!"
					"[rp=0x%x, wp=0x%x]",
					rp, wp);
			mcc_trace(MCC_DEBUG, "DMA message read head: 0x%08x 0x%08x",
					*(unsigned int *)head,
					*((unsigned int *)head + 1));

			if (parse_exchg_buf(exchg_read_buf,
					(p->end_addr - p->base_addr), rp, flag))
			{
				mcc_trace(MCC_ERR, "DMA read Message error!");
				goto dma_recv_irq_msg_out;
			}

		}

		if (dma_clear_shm(p, exchg_read_buf,
					(p->end_addr - p->base_addr - rp)))
		{
			mcc_trace(MCC_ERR, "DMA clear shm buf failed!");
			goto dma_recv_irq_msg_out;
		}

		mcc_trace(MCC_DEBUG, "DMA message read:rp[0x%x] jump back!", rp);

		*(volatile unsigned int *)(exchg_read_buf->rp_addr_virt) = 0;

		if (read_dma_msg(p, exchg_read_buf, wp, _dma_rw_done)) {
			mcc_trace(MCC_ERR, "Dma read message failed!");
			goto dma_recv_irq_msg_out;
		}

		rp = 0;
	}

	if (parse_exchg_buf(exchg_read_buf, wp, rp, flag)) {
		mcc_trace(MCC_ERR, "DMA read message error!");
		goto dma_recv_irq_msg_out;
	}

	if (dma_clear_shm(p, exchg_read_buf, (wp - rp))) {
		mcc_trace(MCC_ERR, "DMA clear shm buf failed!");
		goto dma_recv_irq_msg_out;
	}

	/* All messages have been received. */
	*(volatile unsigned int *)(exchg_read_buf->rp_addr_virt) = wp;
	if (set_shm_rw_pointer(p, exchg_read_buf)) {
		mcc_trace(MCC_ERR, "DMA update shm rp failed!");
		goto dma_recv_irq_msg_out;
	}

dma_receive_normal_out:
	up(&exchg_read_buf->sem);
	return 0;

dma_recv_irq_msg_out:
	up(&exchg_read_buf->sem);
	return -1;
}

static void dma_recv_irq_mem(void)
{
	struct hisi_memory_info *p = &hisi_slave_info_map.recv.irqmem;
	struct dma_exchange_buf *exchg_read_buf = &hisi_slv_exchg_buf.read_buf;

	if (dma_receive_message(p, exchg_read_buf, PRE_VIEW_MSG)) {
		mcc_trace(MCC_ERR, "DMA receive irq message failed!");
	}
}

static void dma_recv_thread_mem(void)
{
	struct hisi_memory_info *p = &hisi_slave_info_map.recv.threadmem;
	struct dma_exchange_buf *exchg_read_buf = &hisi_slv_exchg_buf.read_buf;

	if (dma_receive_message(p, exchg_read_buf, CONTROL_MSG)) {
		mcc_trace(MCC_ERR, "DMA receive thread message failed!");
	}
}
#endif

/*
 * The shared-mem layout for PCI-DMA message are a bit different from
 * PCI-CPU access message.
 * DMA mode:
 *	00   --[rp]-- ******** ******** ********
 *	01   ******** ******** ******** ********
 *	02   --[wp]-- ******** ******** ********
 *	03   ******** ******** ******** ********
 *    * 40   cdcdcdcd cdcdcdcd a5a5a5a5 a5a5a5a5
 *	50   a5a5a5a5 a5a5a5a5 a5a5a5a5 a5a5a5a5
 *	60   cdcdcdcd cdcdcdcd a5a5a5a5 a5a5a5a5
 *	70   a5a5a5a5 a5a5a5a5 a5a5a5a5 a5a5a5a5
 *	80   cdcdcdcd cdcdcdcd a5a5a5a5 a5a5a5a5
 *	... ...
 * Non-DMA mode:
 *	00   --[rp]-- --[wp]-- cdcdcdcd cdcdcdcd
 *    * 10   a5a5a5a5 a5a5a5a5 a5a5a5a5 a5a5a5a5
 *	20   a5a5a5a5 a5a5a5a5 cdcdcdcd cdcdcdcd
 *	30   a5a5a5a5 a5a5a5a5 a5a5a5a5 a5a5a5a5
 *	40   a5a5a5a5 a5a5a5a5 cdcdcdcd cdcdcdcd
 *	... ...
 *
 * There is theroy says that it would be much more efficient for DMA
 * transfer if the transferring data is 64bytes align. The modification
 * of shared-memory layout is to make sure every DMA transfer meets the
 * requirement that the data is 64bytes align, expecially the rp and wp.
 */
static void _init_meminfo(void *start,
		void *end,
		struct hisi_memory_info *p)
{
	sema_init(&p->sem, 1);

	printk(KERN_ERR "          section start: 0x%08x, section end: 0x%08x\n",
			(unsigned int)start, (unsigned int)end);

	p->base_addr = (unsigned long)start;
	p->end_addr = (unsigned long)end;

	p->rp_addr = (unsigned long)(start - 64);
	p->wp_addr = (unsigned long)(start - 32);
#if 0
	p->rp_addr = (unsigned long)(start
			- 2 * sizeof(unsigned int) - _HEAD_SIZE);
	p->wp_addr = (unsigned long)(start
			- sizeof(unsigned int) - _HEAD_SIZE);
#endif
}

static void _init_mem(void *start,
		void *end,
		struct hisi_shared_memory_info *p)
{
	unsigned int size = end - start;
	void *aliag;

	printk(KERN_INFO "     data_base: 0x%08x, data_end: 0x%08x\n",
			(unsigned int)start, (unsigned int)end);

#ifdef DMA_MESSAGE_ENABLE
	/*
	 * shared-mem buf locates in host, Only host will
	 * access this memory at this time.
	 */
	if (g_local_handler->is_host()) {
		memset(start, 0xA5, size);
		aliag = start;
		for (; aliag < end; aliag += _MEM_ALIAGN)
			memset(aliag, 0xCD, _HEAD_SIZE);
	}
#else
	/*
	 * shared-mem buf locates in slv, Only slv will
	 * access this memory at this time.
	 */
	if (!g_local_handler->is_host()) {
		memset(start, 0xA5, size);
		aliag = start;
		for (; aliag < end; aliag += _MEM_ALIAGN)
			memset(aliag, 0xCD, _HEAD_SIZE);
	}
#endif

	_init_meminfo(start,
			start + HISI_SHM_IRQSEND_SIZE - 4 * _MEM_ALIAGN,
			&p->irqmem);

	_init_meminfo((start + HISI_SHM_IRQSEND_SIZE + 4 * _MEM_ALIAGN),
			end,
			&p->threadmem);

#ifdef DMA_MESSAGE_ENABLE
	if (g_local_handler->is_host()) {
#else
	if (!g_local_handler->is_host()) {
#endif
		writel(0, p->irqmem.rp_addr);
		writel(0, p->irqmem.wp_addr);
		writel(0, p->threadmem.rp_addr);
		writel(0, p->threadmem.wp_addr);
		mcc_trace(MCC_INFO, "rp_addr[0x%08x], wp_addr[0x%08x].",
				(unsigned int)p->irqmem.rp_addr,
				(unsigned int)p->irqmem.wp_addr);
		mcc_trace(MCC_INFO, "rp_addr[0x%08x], wp_addr[0x%08x].",
				(unsigned int)p->threadmem.rp_addr,
				(unsigned int)p->threadmem.wp_addr);
	}
}

static int hisi_shared_mem_init(void *dev)
{
	unsigned int first_half_base, first_half_end;
	unsigned int second_half_base, second_half_end;
	struct hi35xx_dev *hi_dev = (struct hi35xx_dev *)dev;
	struct hisi_map_shared_memory_info *pinfo = NULL;
	unsigned int opt_address_base = 0;
	unsigned int opt_address_end = 0;

	spin_lock_init(&g_mcc_data_lock);
#ifdef DMA_MESSAGE_ENABLE
	if (!g_local_handler->is_host()) {
		/*
		 * For slave side, DMA will not use virtual address,
		 * Physical address instead.
		 */
		pinfo = &hisi_slave_info_map;
		opt_address_base = hi_dev->shm_base;
		opt_address_end = hi_dev->shm_end;

		mcc_trace(MCC_INFO, "dev->shm_base[0x%8x],dev->shm_end[0x%8x]",
				hi_dev->shm_base, hi_dev->shm_end);
		printk(KERN_INFO "dev->shm_base[0x%8x],dev->shm_end[0x%8x]\n",
				hi_dev->shm_base, hi_dev->shm_end);

	} else {
		/*
		 * For host, CPU could only access virtual address,
		 * no physical address.
		 */
		pinfo = &hisi_host_info_map[hi_dev->slot_index];
		opt_address_base = hi_dev->shm_base_virt;
		opt_address_end = hi_dev->shm_end_virt;

		mcc_trace(MCC_INFO, "dev->shm_base_virt[0x%08x],"
				" dev->shm_end_virt[0x%08x]",
				hi_dev->shm_base_virt,
				hi_dev->shm_end_virt);
	}
#else

	if (!g_local_handler->is_host()) {
		pinfo = &hisi_slave_info_map;
	} else {
		pinfo = &hisi_host_info_map[hi_dev->slot_index];
	}

	opt_address_base = hi_dev->shm_base_virt;
	opt_address_end = hi_dev->shm_end_virt;

	mcc_trace(MCC_INFO, "dev->shm_base_virt[0x%08x],"
			" dev->shm_end_virt[0x%08x]",
			hi_dev->shm_base_virt,
			hi_dev->shm_end_virt);
#endif

	first_half_base = opt_address_base
		+ (_MEM_ALIAGN - (_MEM_ALIAGN_VERS & opt_address_base))
		+ _MEM_ALIAGN
		+ 2 * _MEM_ALIAGN;
	first_half_end = opt_address_base + HISI_SHARED_SENDMEM_SIZE - 6 * _MEM_ALIAGN;

	mcc_trace(MCC_INFO, "first_half_base[0x%08x], first_half_end[0x%08x]",
			first_half_base, first_half_end);

	opt_address_base += HISI_SHARED_SENDMEM_SIZE;

	second_half_base = opt_address_base
		+ (_MEM_ALIAGN - (_MEM_ALIAGN_VERS & opt_address_base))
		+ _MEM_ALIAGN
		+ 2 * _MEM_ALIAGN;
	second_half_end = second_half_base + HISI_SHARED_RECVMEM_SIZE - 10 * _MEM_ALIAGN;

	mcc_trace(MCC_INFO, "second_half_base[0x%08x], second_half_end[0x%08x]",
			second_half_base, second_half_end);

	memset((void *)fixed_mem, _FIX_ALIAGN_DATA, _MEM_ALIAGN_CHECK);

	if (!g_local_handler->is_host()) {
		printk(KERN_INFO "    First half:\n");
		_init_mem((void *)first_half_base, (void *)first_half_end,
				&pinfo->recv);

		printk(KERN_INFO "    Second half:\n");
		_init_mem((void *)second_half_base, (void *)second_half_end,
				&pinfo->send);

		/*
		 * For a bug that DMA done irq triggerred a bit earlier.
		 */
		s_send_irq_base = pinfo->send.irqmem.base_addr;
		s_send_thread_base = pinfo->send.threadmem.base_addr;

	} else {
		printk(KERN_INFO "    First half:\n");
		_init_mem((void *)first_half_base, (void *)first_half_end,
				&pinfo->send);

		printk(KERN_INFO "    Second half:\n");
		_init_mem((void *)second_half_base, (void *)second_half_end,
				&pinfo->recv);
	}

	return 0;
}

#ifdef DMA_MESSAGE_ENABLE

static int hisi_dma_exchg_init(struct dma_exchange_buf *exchg_buf,
		unsigned int type)
{
	exchg_buf->type = type;
	exchg_buf->size = DMA_EXCHG_BUF_SIZE;

	init_waitqueue_head(&exchg_buf->wait);

	sema_init(&exchg_buf->sem, 1);

	spin_lock_init(&exchg_buf->mlock);

	exchg_buf->base_virt = (unsigned int)dma_alloc_coherent(NULL,
			(size_t)exchg_buf->size,
			(dma_addr_t *)&exchg_buf->base,
			GFP_KERNEL);
	if (!exchg_buf->base_virt) {
		mcc_trace(MCC_ERR, "DMA alloc for exchg %s buf failed!",
			(DMA_EXCHG_READ == type)? "read":"write");
		return -ENOMEM;
	}

	memset((void *)exchg_buf->base_virt, 0x0, exchg_buf->size);
	printk(KERN_INFO "exchange buf: base:0x%08x, base_virt:0x%08x, "
			"size:0x%x.\n",
			exchg_buf->base,
			exchg_buf->base_virt,
			exchg_buf->size);

	exchg_buf->rp_addr = exchg_buf->base
		+ (_MEM_ALIAGN - (_MEM_ALIAGN_VERS & exchg_buf->base))
		+ _MEM_ALIAGN;

	exchg_buf->rp_addr_virt = exchg_buf->base_virt
		+ (exchg_buf->rp_addr - exchg_buf->base);

	exchg_buf->wp_addr = exchg_buf->rp_addr + _MEM_ALIAGN;
	exchg_buf->wp_addr_virt = exchg_buf->rp_addr_virt + _MEM_ALIAGN;
	exchg_buf->data_base = exchg_buf->rp_addr + 2 * _MEM_ALIAGN;
	exchg_buf->data_end = exchg_buf->base + exchg_buf->size;
	exchg_buf->data_base_virt = exchg_buf->rp_addr_virt + 2 * _MEM_ALIAGN;
	exchg_buf->data_end_virt = exchg_buf->base_virt + exchg_buf->size;

	mcc_trace(MCC_INFO, "Exchange buf: rp_addr[phys]=0x%08x,"
			"wp_addr[phys]=0x%08x,data_base[phys]=0x%08x,"
			"data_end[phys]=0x%08x.",
			exchg_buf->rp_addr,
			exchg_buf->wp_addr,
			exchg_buf->data_base,
			exchg_buf->data_end);

	*(unsigned int *)(exchg_buf->rp_addr_virt) = 0x0;
	*(unsigned int *)(exchg_buf->wp_addr_virt) = 0x0;

	return 0;
}

#ifdef __FOR_SPECIAL_MEM_DEBUG
static void mcc_mem_debug(void)
{
	printk(KERN_ERR "mem debug initialized\n");
	s_tcm_virt = (unsigned int)ioremap(0x4010000, 0x3000);
	if (!s_tcm_virt)
		printk(KERN_ERR "IOREMAP FOR TCM FAILED!!!!!!!!!!!!!!\n");
	s_tcm_virt += 0x100;
	writel(0x0, s_tcm_virt);
}
#endif

static int hisi_dma_exchange_mem_init(void)
{
	if (hisi_dma_exchg_init(&hisi_slv_exchg_buf.read_buf, DMA_EXCHG_READ)) {
		mcc_trace(MCC_ERR, "Init read exchange buf failed!");
		return -1;
	}

	if (hisi_dma_exchg_init(&hisi_slv_exchg_buf.write_buf, DMA_EXCHG_WRITE)) {
		mcc_trace(MCC_ERR, "Init write exchange buf failed!");
		return -1;
	}

#ifdef __FOR_SPECIAL_MEM_DEBUG
	mcc_mem_debug();
#endif

	return 0;
}

static void hisi_dma_exchange_mem_exit(void)
{
	if (hisi_slv_exchg_buf.read_buf.base_virt)
		iounmap((void *)hisi_slv_exchg_buf.read_buf.base_virt);

	if (hisi_slv_exchg_buf.write_buf.base_virt)
		iounmap((void *)hisi_slv_exchg_buf.write_buf.base_virt);

}
#endif /* DMA_MESSAGE_ENABLE */

#ifdef MCC_PROC_ENABLE
static void update_proc_rp_wp(void)
{
	struct hisi_map_shared_memory_info *shm_p;
	unsigned int i;

#ifdef DMA_MESSAGE_ENABLE
	if (g_local_handler->is_host()) {
		int dev_slot;
		for (i = 0; i < g_local_handler->remote_device_number; i++) {
			if (DEVICE_CHECKED_FLAG
					!= g_hi35xx_dev_map[i]->vdd_checked_success)
				continue;
			dev_slot = g_hi35xx_dev_map[i]->slot_index;
			shm_p = &hisi_host_info_map[dev_slot];

			((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_rp
				= *(unsigned int*)shm_p->send.irqmem.rp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_wp
				= *(unsigned int*)shm_p->send.irqmem.wp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_rp
				= *(unsigned int*)shm_p->recv.irqmem.rp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_wp
				= *(unsigned int*)shm_p->recv.irqmem.wp_addr;

			((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_send_rp
				= *(unsigned int*)shm_p->send.threadmem.rp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_send_wp
				= *(unsigned int*)shm_p->send.threadmem.wp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_recv_rp
				= *(unsigned int*)shm_p->recv.threadmem.rp_addr;
			((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_recv_wp
				= *(unsigned int*)shm_p->recv.threadmem.wp_addr;
		}
	} else {
		struct dma_exchange_buf *exchg_buf = &hisi_slv_exchg_buf.read_buf;

		if (DEVICE_CHECKED_FLAG != g_hi35xx_dev_map[0]->vdd_checked_success)
			return;

		shm_p = &hisi_slave_info_map;

		if (down_interruptible(&exchg_buf->sem)) {
			mcc_trace(MCC_ERR, "acquire shared handle sem failed!");
			return;
		}

		if (get_shm_rw_pointer(&shm_p->send.irqmem, exchg_buf)) {
			mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
			goto get_rp_wp_out;
		}
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].irq_send_rp
			= *(volatile unsigned int *)(exchg_buf->rp_addr_virt);
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].irq_send_wp
			= *(volatile unsigned int *)(exchg_buf->wp_addr_virt);

		if (get_shm_rw_pointer(&shm_p->send.threadmem, exchg_buf)) {
			mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
			goto get_rp_wp_out;
		}
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].thrd_send_rp
			= *(volatile unsigned int *)(exchg_buf->rp_addr_virt);
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].thrd_send_wp
			= *(volatile unsigned int *)(exchg_buf->wp_addr_virt);

		if (get_shm_rw_pointer(&shm_p->recv.irqmem, exchg_buf)) {
			mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
			goto get_rp_wp_out;
		}
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].irq_recv_rp
			= *(volatile unsigned int *)(exchg_buf->rp_addr_virt);
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].irq_recv_wp
			= *(volatile unsigned int *)(exchg_buf->wp_addr_virt);

		if (get_shm_rw_pointer(&shm_p->recv.threadmem, exchg_buf)) {
			mcc_trace(MCC_ERR, "get shared mem rp/wp failed!");
			goto get_rp_wp_out;
		}
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].thrd_recv_rp
			= *(volatile unsigned int *)(exchg_buf->rp_addr_virt);
		((struct rp_wp *)g_proc._rp_wp.data)->target[0].thrd_recv_wp
			= *(volatile unsigned int *)(exchg_buf->wp_addr_virt);

get_rp_wp_out:
		up(&exchg_buf->sem);
	}
#else
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		if (DEVICE_CHECKED_FLAG
				!= g_hi35xx_dev_map[i]->vdd_checked_success)
			continue;
		if (g_local_handler->is_host()) {
			int dev_slot;
			dev_slot = g_hi35xx_dev_map[i]->slot_index;
			shm_p = &hisi_host_info_map[dev_slot];
		} else {
			shm_p = &hisi_slave_info_map;
		}

		((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_rp
			= *(unsigned int*)shm_p->send.irqmem.rp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_wp
			= *(unsigned int*)shm_p->send.irqmem.wp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_rp
			= *(unsigned int*)shm_p->recv.irqmem.rp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_wp
			= *(unsigned int*)shm_p->recv.irqmem.wp_addr;

		((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_send_rp
			= *(unsigned int*)shm_p->send.threadmem.rp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_send_wp
			= *(unsigned int*)shm_p->send.threadmem.wp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_recv_rp
			= *(unsigned int*)shm_p->recv.threadmem.rp_addr;
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].thrd_recv_wp
			= *(unsigned int*)shm_p->recv.threadmem.wp_addr;
	}
#endif

	return;

}
#endif

