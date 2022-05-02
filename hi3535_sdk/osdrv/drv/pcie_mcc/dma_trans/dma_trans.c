
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/types.h>
#include <linux/timer.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/notifier.h>
#include <linux/reboot.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/irq.h>
#include <asm/io.h>
#include <asm/delay.h>
#include <linux/kthread.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/pci.h>
#include <linux/wait.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/delay.h>

#include "../hi35xx_dev/include/hi35xx_dev.h"
#include "dma_trans.h"
#include "../proc_msg/mcc_proc_msg.h"

#define DMA_CUR_TASK(x) list_entry(x.next, struct pcit_dma_ptask, list)

static struct dma_control s_pcit_dma;

static struct semaphore pcie_dma_ioctl_sem;

#if TIMING_DEBUG
static struct timeval s_timing_start;
static struct timeval s_timing_end;
static int s_timing_times = 0;
#endif

static unsigned int s_w_repeated = 0;
static unsigned int s_r_repeated = 0;

static char version[] __devinitdata = KERN_INFO "" PCIE_DMA_MODULE_NAME " :" PCIE_DMA_VERSION "\n";

spinlock_t g_dma_list_lock;

#ifdef MCC_PROC_ENABLE
void get_trans_list_info(void)
{
	unsigned int msg_read_count = 0;
	unsigned int msg_write_count = 0;
	unsigned int data_read_count = 0;
	unsigned int data_write_count = 0;

	while (s_pcit_dma.write.msg.busy_list_head.next
			!= &s_pcit_dma.write.msg.busy_list_head)
		msg_write_count++;
	while (s_pcit_dma.write.data.busy_list_head.next
			!= &s_pcit_dma.write.data.busy_list_head)
		data_write_count++;

	while (s_pcit_dma.read.msg.busy_list_head.next
			!= &s_pcit_dma.read.msg.busy_list_head)
		msg_read_count++;
	while (s_pcit_dma.read.data.busy_list_head.next
			!= &s_pcit_dma.read.data.busy_list_head)
		data_read_count++;

	((struct dma_list_count*)g_proc._dma_list.data)->msg_read_busy = msg_read_count;
	((struct dma_list_count*)g_proc._dma_list.data)->msg_write_busy = msg_write_count;
	((struct dma_list_count*)g_proc._dma_list.data)->data_read_busy = data_read_count;
	((struct dma_list_count*)g_proc._dma_list.data)->data_write_busy = data_write_count;
}
#endif

#ifdef PCI_TRANSFER_TIMER
static struct timer_list s_dma_timeout_timer;
static unsigned long s_last_jiffies = 0;
/*
 * This function is for guarding every DMA transferring task
 * to be successfully transferred. If a DMA transfer does not
 * finished in a very large time interval[10ms for example],
 * we assume that it won't be finished forever, for something
 * wrong has happened in PCI link. And the only thing to do
 * is to stop PCI DMA transferring to prevent further damage
 * to the system.
 */ 
void pcit_timer_proc(unsigned long  __data)
{
	struct pcit_dma_task *new_task;
	unsigned long flags;

	PCIT_PRINT(PCIT_ERR, "last jiffies[%lu], jiffies[%lu]",
			s_last_jiffies, jiffies);
	s_last_jiffies = jiffies;

#ifdef __IS_PCI_HOST__
	return;

#else
	/* For DMA write first */
	spin_lock_irqsave(&(s_pcit_dma.write.mlock), flags);
	if (CHANNEL_BUSY == s_pcit_dma.write.state) {
		if (__CMESSAGE == s_pcit_dma.write.owner) {
			new_task = DMA_CUR_TASK(s_pcit_dma.write.msg.busy_list_head); 
		} else {
			new_task = DMA_CUR_TASK(s_pcit_dma.write.data.busy_list_head);
		}
		PCIT_PRINT(PCIT_ERR, "DMA task info[src_addr:0x%08x;"
				"dest_addr:0x%08x;len:0x%x ]",
				new_task->src,
				new_task->dest,
				new_task->len);

		g_local_handler->stop_dma_transfer(PCI_DMA_WRITE);
	}
	spin_unlock_irqrestore(&(s_pcit_dma.write.mlock), flags);

	/* For DMA read */
	spin_lock_irqsave(&(s_pcit_dma.read.mlock), flags);
	if (CHANNEL_BUSY == s_pcit_dma.read.state) {
		if (__CMESSAGE == s_pcit_dma.read.owner) {
			new_task = DMA_CUR_TASK(s_pcit_dma.read.msg.busy_list_head); 
		} else {
			new_task = DMA_CUR_TASK(s_pcit_dma.read.data.busy_list_head);
		}
		PCIT_PRINT(PCIT_ERR, "DMA task info[src_addr:0x%08x;"
				"dest_addr:0x%08x;len:0x%x ]",
				new_task->src,
				new_task->dest,
				new_task->len);

		g_local_handler->stop_dma_transfer(PCI_DMA_READ);
	}
	spin_unlock_irqrestore(&(s_pcit_dma.read.mlock), flags);

#endif
}
#endif /* PCI_TRANSFER_TIMER */

static void start_dma_task(struct pcit_dma_ptask *new_task)
{
#ifdef PCI_TRANSFER_TIMER
	s_dma_timeout_timer.function = pcit_timer_proc;
	mod_timer(&s_dma_timeout_timer, jiffies + DMA_TIMEOUT_JIFFIES);
#endif

	g_local_handler->start_dma_task(new_task);

#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_II <= g_proc.proc_enable)
	{
		if (PCI_DMA_READ == new_task->task.dir) {
			((struct dma_info*)g_proc._dma_info.data)->read.src_address = new_task->task.src;
			((struct dma_info*)g_proc._dma_info.data)->read.dest_address = new_task->task.dest;
			((struct dma_info*)g_proc._dma_info.data)->read.len = new_task->task.len;
		} else {
			((struct dma_info*)g_proc._dma_info.data)->write.src_address = new_task->task.src;
			((struct dma_info*)g_proc._dma_info.data)->write.dest_address = new_task->task.dest;
			((struct dma_info*)g_proc._dma_info.data)->write.len = new_task->task.len;
		}
	}
#endif
#ifdef MCC_PROC_ENABLE
	if (PROC_LEVEL_II <= g_proc.proc_enable)
	{
		if (PCI_DMA_WRITE == new_task->task.dir) {
			if (__VDATA == new_task->type)
				((struct dma_count*)g_proc._dma_count.data)->data_write++;
			else
				((struct dma_count*)g_proc._dma_count.data)->msg_write++;
		} else {
			if (__VDATA == new_task->type)
				((struct dma_count*)g_proc._dma_count.data)->data_read++;
			else
				((struct dma_count*)g_proc._dma_count.data)->msg_read++;
		}
	}
#endif
}

struct pcit_dma_ptask * __do_create_task(struct dma_channel *channel,
		struct pcit_dma_task *task,
		int type)
{
	struct list_head *list;
	struct list_head *busy_head = NULL;
	struct list_head *free_head = NULL;
	struct pcit_dma_ptask *new_task = NULL;
	unsigned long flags;

	spin_lock_irqsave(&channel->mlock, flags);

	if (__CMESSAGE == type) {
		free_head = &channel->msg.free_list_head;
		busy_head = &channel->msg.busy_list_head;
	} else if (__VDATA == type) {
		free_head = &channel->data.free_list_head;
		busy_head = &channel->data.busy_list_head;
	} else {
		PCIT_PRINT(PCIT_ERR, "Wrong DMA task type!\n");
		spin_unlock_irqrestore(&channel->mlock, flags);
		return NULL;
	}

	if (unlikely(list_empty(free_head))) {
		PCIT_PRINT(PCIT_ERR, "Too many DMA %s %s tasks!",
				(__CMESSAGE == type) ? "msg" : "data",
				(PCI_DMA_READ == task->dir) ? "read" : "write");
		spin_unlock_irqrestore(&channel->mlock, flags);
		return NULL;
	}

	list = free_head->next;
	list_del(list);
	new_task = list_entry(list, struct pcit_dma_ptask, list);
	if ((__CMESSAGE == type) && (DMA_TASK_EMPTY != new_task->task.state)) {
		if (DMA_TASK_EMPTY != new_task->task.state) {
			printk(KERN_ERR "## %s state 0x%x,old_src 0x%x,old_dest 0x%x.\n",
					(PCI_DMA_READ == task->dir) ? "read" : "write",
					new_task->task.state,
					new_task->task.src,
					new_task->task.dest);
			if ((DMA_TASK_FINISHED == new_task->task.state)
				       && (NULL != new_task->task.private_data))
			{
				/*
				 * DMA task has been transferred, but the thread started that transfer
				 * was not wake up in time. And the list node is temporarily not available,
				 * until this node is completely release, even though it's currently locates
				 * in free list. We need to wait here(return dma task create unsuccessfully)
				 * until the thread started this task was woken up, and this node was released.
				 */
				//wake_up_interruptible((wait_queue_head_t *)new_task->task.private_data);
				printk(KERN_ERR "Fatal: Thread sleep for too long!!!!!!!\n");
			} else {
				/*
				 * DMA task.state neither equates DMA_TASK_EMPTY nor equates DMA_TASK_FINISHED.
				 * We should not get here in whatever situation. And in case we have been here
				 * some how, we are helpless.
				 */
				printk(KERN_ERR "unknow dma_task state[0x%x] in free list!\n",
						new_task->task.state);
				if (NULL != new_task->task.private_data)
					printk(KERN_ERR "DMA task wait is not NULL!\n");

			}
			//BUG_ON(1);
			//new_task->task.state = DMA_TASK_EMPTY;
			list_add_tail(list, free_head);
			spin_unlock_irqrestore(&channel->mlock, flags);
			printk(KERN_ERR "Ignore this DMA task !\n");
			return NULL;
		}
	}

	new_task->type = type;
	memcpy(&new_task->task, task, sizeof(*task));
	if (list_empty(busy_head) && (CHANNEL_IDLE == channel->state)) {
		/*
		 * Nothing else to be transferred, transfer current task immediately.
		 */
		list_add_tail(list, busy_head);
		channel->state = CHANNEL_BUSY;
		channel->owner = new_task->type;
		start_dma_task(new_task);
	} else {
		/*
		 * There are some other dma tasks to be transferred, queue first,
		 * and wait to be transferred.
		 */
		list_add_tail(list, busy_head);
	}

	spin_unlock_irqrestore(&channel->mlock, flags);

	return new_task;
}

struct pcit_dma_task * __pcit_create_task(struct pcit_dma_task *task, int type)
{
	struct pcit_dma_ptask *new_task = NULL;

	if (PCI_DMA_READ == task->dir) {
		new_task = __do_create_task(&s_pcit_dma.read, task, type);
	} else if (PCI_DMA_WRITE == task->dir) {
		new_task = __do_create_task(&s_pcit_dma.write, task, type);
	} else {
		printk(KERN_ERR "Unknow DMA direction![read/write?]\n");
	}

	/*
	 * create dma task failed!
	 */
	if (NULL == new_task)
		return NULL;

	/*
	 * create dma task success
	 */
	return &new_task->task;
}
EXPORT_SYMBOL(__pcit_create_task);

/*
 * Create a DMA task an transfer it.
 * Return value:
 * NULL: create a DMA task failed.
 * Not NULL: success.
 * This function is for DATA transfer.
 */
int pcit_create_task(struct pcit_dma_task *task)
{
	unsigned int src = 0;
	unsigned int dest = 0;

	src = task->src;
	dest = task->dest;

	if (!g_local_handler->has_dma()) {
		printk(KERN_ERR "Host DMA is temporarily not supported!\n");
		printk(KERN_ERR "Please use slave DMA instead.\n");
		return -1;
	}

	/* DMA data length should never be zero */
	if (task->len == 0) {
		PCIT_PRINT(PCIT_FATAL, "FATAL: DMA data len is zero!!");
		return -1;
	}

	/*
	 * DMA address validity check
	 * Host:
	 *	|---------------|	|---------------|
	 *	|		|  DMA	|		|
	 *	| [HOST.DDR]	|/-----\|  [DEV.BAR0]	|
	 *	|		|\-----/|		|
	 *	|---------------|	|---------------|
	 *
	 *Slave:
	 *	|---------------|	|---------------|
	 *	|		|  DMA	|		|
	 *	| [SLV.DDR]	|/-----\|  [HOST.DDR]	|
	 *	|		|\-----/|		|
	 *	|---------------|	|---------------|
	 *
	 * -NOTE-- An illegal DMA address will cause unexpected result.
	 */
#if 0 /* This is not available for host dma */
	if (!g_local_handler->is_ram_address(src)
			|| !g_local_handler->is_ram_address(dest)) {
		PCIT_PRINT(PCIT_FATAL, "DMA addr[src:%08x,dest:%08x] error!",
				src, dest);
		return -1;
	}
#endif
	/*
	 * Both DMA src and dest address should compliant with 
	 * the rule that 4 Bytes align. 
	 */
	src &= DMA_ADDR_ALIGN_MASK;
	dest &= DMA_ADDR_ALIGN_MASK;
	if (unlikely(src || dest)) {
		PCIT_PRINT(PCIT_FATAL, "FATAL: DMA addr not 4Bytes align!");
		return -1;
	}

	if (NULL == __pcit_create_task(task, __VDATA)) {
		PCIT_PRINT(PCIT_ERR, "Create dma task failed!");
		return -1;
	}

	return 0;
}
EXPORT_SYMBOL(pcit_create_task);

/*
 * Get window0's base address.
 * The first 1MB of PCI window-mapping address is for
 * Message communication, and the next 7MB is for video
 * data.
 * If the shared buffer locates in slave side, the returning
 * address is PCI BAR0.
 * If the shared buffer locates in host side, the returning
 * address is the base address of win0[on slave side].
 */
unsigned int get_pf_window_base(int slot)
{
	unsigned int base = 0;

#ifdef __IS_PCI_HOST__
	int i;
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		if (g_hi35xx_dev_map[i]->slot_index == slot) {
			base = g_hi35xx_dev_map[i]->bar0;
			return base;
		}
	}

	PCIT_PRINT(PCIT_ERR, "Target device[%d] does not"
			" exist! func[%s]",
			slot, __func__);

#else
	base = g_local_handler->local_controller[0]->win_base[0];
#endif

	return base;
}
EXPORT_SYMBOL(get_pf_window_base);

static int dma_trans_open(struct inode *inode, struct file *fp)
{
	fp->private_data = 0;

	sema_init(&pcie_dma_ioctl_sem, 1);

	return 0;
}

static int dma_trans_release(struct inode *inode, struct file *fp)
{
	return 0;
}

static ssize_t dma_trans_read(struct file *fp,
		char *buf, 
		size_t count,
		loff_t *ppos)
{
	return -ENOSYS;
}

static ssize_t dma_trans_write(struct file *fp,
		const char __user *data,
		size_t len,
		loff_t *ppos)
{	
	return -ENOSYS;
}

unsigned int dma_trans_poll(struct file *file, poll_table *wait)
{
	return 0;
}

static long dma_trans_ioctl(struct file *fp,
		unsigned int cmd,
		unsigned long arg)
{
	struct pcit_dma_task task;

	PCIT_PRINT(PCIT_INFO, "PCIe DMA ioctl cmd[0x%d] received.", cmd);

	down(&pcie_dma_ioctl_sem);

	if (copy_from_user((void *)&task, (void *)arg,
				sizeof(struct pcit_dma_task))) {
		PCIT_PRINT(PCIT_ERR, "Copy DMA task from usr space error.");
		up(&pcie_dma_ioctl_sem);
		return -1;
	}

	if (_IOC_TYPE(cmd) == 'H') {
		switch (_IOC_NR(cmd)) {
		case _IOC_NR(HI_IOC_PCIT_INQUIRE):

		case _IOC_NR(HI_IOC_PCIT_DMARD):
			if (PCI_DMA_READ == task.dir) {
				if (pcit_create_task(&task)) {
					PCIT_PRINT(PCIT_ERR,
						"Create DMA read task failed!");
					up(&pcie_dma_ioctl_sem);
					return -1;
				}
			} else {
				PCIT_PRINT(PCIT_ERR,
					"Incorrect DMA direction![read]");
				up(&pcie_dma_ioctl_sem);
				return -1;
			}
			break;

		case _IOC_NR(HI_IOC_PCIT_DMAWR):
			if (PCI_DMA_WRITE == task.dir) {
				if (pcit_create_task(&task)) {
					PCIT_PRINT(PCIT_ERR,
						"Creat DMA write task failed!");
					up(&pcie_dma_ioctl_sem);
					return -1;
				}
			} else {
				PCIT_PRINT(PCIT_ERR,
					"Incorrect DMA direction![write]");
				up(&pcie_dma_ioctl_sem);
				return -1;
			}
			break;

		case _IOC_NR(HI_IOC_PCIT_BINDDEV):

		case _IOC_NR(HI_IOC_PCIT_DOORBELL):

		case _IOC_NR(HI_IOC_PCIT_SUBSCRIBE):

		case _IOC_NR(HI_IOC_PCIT_UNSUBSCRIBE):

		case _IOC_NR(HI_IOC_PCIT_LISTEN):

		default:
			up(&pcie_dma_ioctl_sem);
			return -ENOIOCTLCMD;
		}
	}

	up(&pcie_dma_ioctl_sem);

	return 0;
}

static struct file_operations pci_dma_fops = {
	.owner          = THIS_MODULE,
	.llseek         = no_llseek,
	.read           = dma_trans_read,
	.write          = dma_trans_write,
	.unlocked_ioctl = dma_trans_ioctl,
	.open           = dma_trans_open,
	.release        = dma_trans_release,
	.poll           = dma_trans_poll,
};

static struct miscdevice pci_dma_miscdev = {
	.minor          = MISC_DYNAMIC_MINOR,
	.name           = "pci_dma_transfer",
	.fops           = &pci_dma_fops,
};

#if 0
irqreturn_t host_dma_irq_handler(int irq, void *dev_id)
{
#ifdef DOUBLE_DMA_TRANS_LIST
	/*
	 * Host will do nothing but return an irq_handled in this mode.
	 * The driver does not support DMA write in Host side.
	 * Please use DMA read in slave side instead.
	 */
	printk(KERN_ERR "Sorry, host does not support DMA write.\n");
#else
	unsigned long flags;
	struct pcit_dma_ptask *cur_task;
	unsigned int dma_irq_status;

	spin_lock_irqsave(&g_dma_list_lock, flags);

	if (irq == s_dma_irq[0]) {
		dma_irq_status = readl(g_local_pcie0_conf_virt
				+ DMA_WRITE_INTERRUPT_STATUS);
		writel(dma_irq_status, g_local_pcie0_conf_virt
				+ DMA_WRITE_INTERRUPT_CLEAR);
	} else if (irq == s_dma_irq[1]) {
		dma_irq_status = readl(g_local_pcie1_conf_virt
				+ DMA_WRITE_INTERRUPT_STATUS);
		writel(dma_irq_status, g_local_pcie1_conf_virt
				+ DMA_WRITE_INTERRUPT_CLEAR);			
	} else {
		PCIT_PRINT(PCIT_ERR, "unknow dma irq trigger!");
		goto host_irq_err;
	}

	if (list_empty(&busy_task_head)) {
		PCIT_PRINT(PCIT_ERR, "DMA busy task should never be empty here.");
		goto host_irq_err;
	}

	/* abort irq: restart previous task */
	if (dma_irq_status & 0x10000) {
		PCIT_PRINT(PCIT_ERR, "DMA abort irq triggered !");
		start_dma_task(PCIT_DMA_TASK_CUR);
		goto host_irq_err;
	}

	g_dma_irq_count++;

	cur_task = PCIT_DMA_TASK_CUR;
	cur_task->state = PCIT_DMA_TASK_FINISH;
	cur_task->finish(cur_task);

	list_del(&cur_task->list);
	list_add_tail(&cur_task->list, &free_task_head);

	s_taskNum--;

	if (!list_empty(&busy_task_head)) {
		start_dma_task(PCIT_DMA_TASK_CUR);
	}

host_irq_err:
	spin_unlock_irqrestore(&g_dma_list_lock, flags);
#endif

	return IRQ_HANDLED;
}
#endif

static void __do_pcit_dma_trans(struct dma_channel *channel, int direction)
{
	unsigned long flags;
	struct pcit_dma_ptask *cur_task;
	int irq_status = 0;
	unsigned int *repeated;

	spin_lock_irqsave(&channel->mlock, flags);

	if (PCI_DMA_WRITE == direction) {
		repeated = &s_w_repeated;
		irq_status = g_local_handler->clear_dma_write_local_irq(0);
	} else {
		repeated = &s_r_repeated;
		irq_status = g_local_handler->clear_dma_read_local_irq(0);
	}

	if (unlikely(TRANSFER_ABORT == irq_status)) {
		/*
		 * DMA abort occurs:
		 * Try three times at most. If still failed, something
		 * beyond retrieved had happened. Nothing to do but BUG_ON.
		 */
		if (*repeated >= 3) {
			PCIT_PRINT(PCIT_ERR, "DMA %s abort timeout! quit!",
					((__CMESSAGE == channel->owner)
					 ? "msg":"data"));
			channel->state = CHANNEL_ERROR;

			spin_unlock_irqrestore(&channel->mlock, flags);

			*repeated = 0;

			BUG_ON(1);
		}
		(*repeated)++;
		channel->state = CHANNEL_BUSY;
		if (__CMESSAGE == channel->owner)
			start_dma_task(DMA_CUR_TASK(channel->msg.busy_list_head));
		else
			start_dma_task(DMA_CUR_TASK(channel->data.busy_list_head));

		PCIT_PRINT(PCIT_ERR, "DMA write abort! Need retry...");
	} else if (NORMAL_DONE == irq_status) {
		/*
		 * DMA transfer done:
		 */
		PCIT_PRINT(PCIT_INFO, "DMA done %s",
				(__CMESSAGE == s_pcit_dma.write.owner) ? "msg":"data");

		*repeated = 0;
		if (__CMESSAGE == channel->owner) {
			cur_task = DMA_CUR_TASK(channel->msg.busy_list_head);
			cur_task->task.state = DMA_TASK_FINISHED;
			if (cur_task->task.finish)
				cur_task->task.finish(&cur_task->task);
			list_del(&cur_task->list);
			list_add_tail(&cur_task->list, &channel->msg.free_list_head);
		} else if (__VDATA == channel->owner) {
			cur_task = DMA_CUR_TASK(channel->data.busy_list_head);
			cur_task->task.state = DMA_TASK_FINISHED;
			if (cur_task->task.finish)
				cur_task->task.finish(&cur_task->task);
			list_del(&cur_task->list);
			list_add_tail(&cur_task->list, &channel->data.free_list_head);
		} else {
			PCIT_PRINT(PCIT_ERR, "Wrong dma type[0x%x]!", channel->owner);
		}

		channel->state = CHANNEL_IDLE;
		channel->owner = __NOBODY;

		/*
		 * Start new transfer if there are tasks in queue.
		 * MCC message has higher prioriy
		 */
		if (!list_empty(&channel->msg.busy_list_head)) {
			channel->owner = __CMESSAGE;
			channel->state = CHANNEL_BUSY;
			start_dma_task(DMA_CUR_TASK(channel->msg.busy_list_head));
		}

		/*
		 * If there no message task to be transferred,
		 * check data task.
		 */ 
		if (!list_empty(&channel->data.busy_list_head)
				&& (channel->state == CHANNEL_IDLE))
		{
			channel->owner = __VDATA;
			channel->state = CHANNEL_BUSY;
			start_dma_task(DMA_CUR_TASK(channel->data.busy_list_head));
		}
	} else {
		PCIT_PRINT(PCIT_INFO, "Unknow dma irq status, just pass!");
	}

	spin_unlock_irqrestore(&channel->mlock, flags);
}

irqreturn_t slave_dma_irq_handler(int irq, void *dev_id)
{
	/* For DMA write channel */
	__do_pcit_dma_trans(&s_pcit_dma.write, PCI_DMA_WRITE);

	/* For DMA read channel */
	__do_pcit_dma_trans(&s_pcit_dma.read, PCI_DMA_READ);

	return IRQ_HANDLED;
}

irqreturn_t host_dma_irq_handler(int irq, void *dev_id)
{
	__do_pcit_dma_trans(&s_pcit_dma.write, PCI_DMA_WRITE);
	__do_pcit_dma_trans(&s_pcit_dma.read, PCI_DMA_READ);
	return IRQ_HANDLED;
}

static void trans_list_init(struct dma_object *obj)
{
	int i = 0;

	obj->busy_list_head.next = &(obj->busy_list_head);
	obj->busy_list_head.prev = &(obj->busy_list_head);
	obj->free_list_head.next = &(obj->free_list_head);
	obj->free_list_head.prev = &(obj->free_list_head);

	for (i = (DMA_TASK_NR - 1); i >= 0; i--) {
		/*
		 * all node for DMA tasks are marked as available.
		 */
		obj->task_array[i].task.state = DMA_TASK_EMPTY;

		obj->task_array[i].task.private_data = NULL;

		list_add_tail(&(obj->task_array[i].list),
				&(obj->free_list_head));
	}
}

static int dma_sys_init(void)
{
	/* For write channel */
	s_pcit_dma.write.type = PCI_DMA_WRITE;
	s_pcit_dma.write.error = 0x0;
	s_pcit_dma.write.state = CHANNEL_IDLE;
#ifdef MSG_DMA_ENABLE
	trans_list_init(&(s_pcit_dma.write.msg));
#endif
	trans_list_init(&(s_pcit_dma.write.data));

	spin_lock_init(&s_pcit_dma.write.mlock);
	sema_init(&s_pcit_dma.write.sem, 1);

	/* For read channel */
	s_pcit_dma.read.type = PCI_DMA_READ;
	s_pcit_dma.read.error = 0x0;
	s_pcit_dma.read.state = CHANNEL_IDLE;
#ifdef MSG_DMA_ENABLE
	trans_list_init(&(s_pcit_dma.read.msg));
#endif
	trans_list_init(&(s_pcit_dma.read.data));

	spin_lock_init(&s_pcit_dma.read.mlock);
	sema_init(&s_pcit_dma.read.sem, 1);

	return 0;
}

static void dma_sys_exit(void)
{
}

static int dma_arch_init(void)
{
	/* architecture related initalization */
	irqreturn_t (*handler)(int, void*);

	if (NULL == g_local_handler) {
		PCIT_PRINT(PCIT_ERR, "g_local_handler is NULL!");
		return -1;
	}

	if (g_local_handler->dma_controller_init()) {
		PCIT_PRINT(PCIT_ERR, "PCI DMA controller init failed!");
		return -1;
	}

#ifdef __IS_PCI_HOST__
	handler = host_dma_irq_handler;
#else
	handler = slave_dma_irq_handler;
#endif

	if (g_local_handler->request_dma_resource(handler)) {
		PCIT_PRINT(PCIT_ERR, "PCI DMA request resources failed!");
		g_local_handler->dma_controller_exit();
		return -1;
	}

	return 0;
}

static void dma_arch_exit(void)
{
	g_local_handler->dma_controller_exit();
	g_local_handler->release_dma_resource();
}

static int __init pci_dma_transfer_init(void)
{
	int ret;

	printk(version);

	if (dma_sys_init()) {
		PCIT_PRINT(PCIT_ERR, "PCI DMA sys init failed!");
		return -1;
	}

	ret = dma_arch_init();
	if (ret) {
		PCIT_PRINT(PCIT_ERR, "PCI DMA arch init failed!");
		goto arch_init_err;
	}

	ret = misc_register(&pci_dma_miscdev);
	if (ret) {
		PCIT_PRINT(PCIT_ERR, "Register DMA transfer Misc failed!");
		goto register_miscdev_failed;
	}	

#ifdef PCI_TRANSFER_TIMER
	init_timer(&s_dma_timeout_timer);
#endif
#if TIMING_DEBUG
	s_timing_start.tv_sec = 0;
	s_timing_start.tv_usec = 0;
	s_timing_end.tv_sec = 0;
	s_timing_end.tv_usec = 0;
#endif

#ifdef MCC_PROC_ENABLE
	//g_proc._get_dma_trans_list_info = get_trans_list_info;
#endif

	return 0;

register_miscdev_failed:
	dma_arch_exit();
arch_init_err:
	dma_sys_exit();
	return ret;
}

static void __exit pci_dma_transfer_exit(void)
{
	misc_deregister(&pci_dma_miscdev);
	dma_arch_exit();
	dma_sys_exit();
}

module_init(pci_dma_transfer_init);
module_exit(pci_dma_transfer_exit);

MODULE_AUTHOR("Hisilicon");
MODULE_DESCRIPTION("Hisilicon Hi35XX");
MODULE_LICENSE("GPL");
MODULE_VERSION("HI_VERSION=" __DATE__);

