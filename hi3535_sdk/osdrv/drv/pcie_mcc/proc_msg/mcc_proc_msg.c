
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <asm/uaccess.h>
#include <linux/types.h>

#include <linux/sysctl.h>
#include <linux/kernel.h>

#include "../hi35xx_dev/include/hi35xx_dev.h"
#include "mcc_proc_msg.h"

static struct proc_dir_entry *s_mcc_map_proc = NULL;
static struct mcc_proc_entry s_proc_items[MAX_PROC_ENTRIES];

struct proc_message g_proc;
EXPORT_SYMBOL(g_proc);

static struct irq_record	s_irq_record;
static struct msg_recv_timer	s_recv_timer;
static struct msg_recv_queue	s_recv_queue;
static struct msg_send_cost	s_send_cost;
static struct msg_len		s_msg_len;
static struct msg_number	s_msg_num;
static struct rp_wp		s_rp_wp;
static struct pre_v_msg_send	s_pmsg_send;
static struct dma_count		s_dma_count;
static struct dma_list_count	s_dma_list_count;
static struct dma_info		s_dma_info;

#if 1
static void show_mcc_info(struct seq_file *s)
{
	unsigned int i = 0;

	seq_printf(s, "------------ Number of Doorbell/PCI INT-X IRQ -----------\n"
			"Door Bell       "
			"Handler Cost    "
			"PCI INT-X       "
			"Handler Cost  \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct irq_record *)g_proc._irq_record.data)->door_bell.count,
			((struct irq_record *)g_proc._irq_record.data)->door_bell.handler_cost,
			((struct irq_record *)g_proc._irq_record.data)->int_x.count,
			((struct irq_record *)g_proc._irq_record.data)->int_x.handler_cost);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Time Gap Between Two Message Recv Timer Interrupts -----------\n"
			"last interval   "
			"max interval  \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t",
			((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->cur_interval,
			((struct msg_recv_timer *)g_proc._msg_recv_timer.data)->max_interval);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Time Gap Between Two Message Receiving thread Become Active -----------\n"
			"last interval   "
			"max interval  \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t",
			((struct msg_recv_queue *)g_proc._msg_recv_queue.data)->cur_interval,
			((struct msg_recv_queue *)g_proc._msg_recv_queue.data)->max_interval);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Time Consuming for A Message -----------\n"
			"pciv last cost  "
			"pciv max cost   "
			"ctl last cost   "
			"ctl max cost  \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct msg_send_cost *)g_proc._msg_send_cost.data)->cur_pciv_cost,
			((struct msg_send_cost *)g_proc._msg_send_cost.data)->max_pciv_cost,
			((struct msg_send_cost *)g_proc._msg_send_cost.data)->cur_ctl_cost,
			((struct msg_send_cost *)g_proc._msg_send_cost.data)->max_ctl_cost);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Message Size Record -----------\n"
			"last irq msg    "
			"max irq msg	 "
			"last thrd msg   "
			"max thrd msg  \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct msg_len *)g_proc._msg_send_len.data)->cur_irq_send,
			((struct msg_len *)g_proc._msg_send_len.data)->max_irq_send,
			((struct msg_len *)g_proc._msg_send_len.data)->cur_thd_send,
			((struct msg_len *)g_proc._msg_send_len.data)->max_thd_send);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Message Number Record -----------\n"
			"      dev       "
			"irq-msg sent    "
			"irq-msg recv    "
			"thrd-msg sent   "
			"thrd-msg recv\n");
	//for (i = 0; i < ((struct msg_number *)g_proc._msg_number.data)->dev_number; i++) {
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		((struct msg_number *)g_proc._msg_number.data)->target[i].dev_slot
			= g_hi35xx_dev_map[i]->slot_index;
		seq_printf(s,
				"%8d\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t",
				((struct msg_number *)g_proc._msg_number.data)->target[i].dev_slot,
				((struct msg_number *)g_proc._msg_number.data)->target[i].irq_send,
				((struct msg_number *)g_proc._msg_number.data)->target[i].irq_recv,
				((struct msg_number *)g_proc._msg_number.data)->target[i].thread_send,
				((struct msg_number *)g_proc._msg_number.data)->target[i].thread_recv);
	}
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Shared memory Read/Write pointer -----------\n"
			"      dev       "
			"irq send rp     "
			"irq send wp     "
			"irq recv rp     "
			"irq recv wp     "
			"thrd send rp    "
			"thrd send wp    "
			"thrd recv rp    "
			"thrd recv wp  \n");
	//for (i = 0; i < ((struct rp_wp *)g_proc._rp_wp.data)->dev_number; i++) {
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		((struct rp_wp *)g_proc._rp_wp.data)->target[i].dev_slot
			= g_hi35xx_dev_map[i]->slot_index;
		seq_printf(s,
				"%8d\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t"
				"0x%08x\t",
				((struct rp_wp *)g_proc._rp_wp.data)->target[i].dev_slot,
				((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_rp,
				((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_send_wp,
				((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_rp,
				((struct rp_wp *)g_proc._rp_wp.data)->target[i].irq_recv_wp,
				((struct rp_wp*)g_proc._rp_wp.data)->target[i].thrd_send_rp,
				((struct rp_wp*)g_proc._rp_wp.data)->target[i].thrd_send_wp,
				((struct rp_wp*)g_proc._rp_wp.data)->target[i].thrd_recv_rp,
				((struct rp_wp*)g_proc._rp_wp.data)->target[i].thrd_recv_wp);
	}
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Time Record Between Two Pre-view Messages -----------\n"
			"      dev       "
			"last interval   "
			"max interval  \n");
	//for (i = 0; i < ((struct pre_v_msg_send *)g_proc._pmsg_interval.data)->dev_number; i++) {
	for (i = 0; i < g_local_handler->remote_device_number; i++) {
		((struct pre_v_msg_send *)g_proc._pmsg_interval.data)->target[i].dev_slot
			= g_hi35xx_dev_map[i]->slot_index;
		seq_printf(s,
				"%8d\t"
				"0x%08x\t"
				"0x%08x\t",
				((struct pre_v_msg_send *)g_proc._pmsg_interval.data)->target[i].dev_slot,
				((struct pre_v_msg_send *)g_proc._pmsg_interval.data)->target[i].cur_interval,
				((struct pre_v_msg_send *)g_proc._pmsg_interval.data)->target[i].max_interval);
	}
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ DMA Transfer Timers Record -----------\n"
			"msg DMA read    "
			"msg DMA write   "
			"data DMA read   "
			"data DMA write\n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct dma_count *)g_proc._dma_count.data)->msg_read,
			((struct dma_count *)g_proc._dma_count.data)->msg_write,
			((struct dma_count *)g_proc._dma_count.data)->data_read,
			((struct dma_count *)g_proc._dma_count.data)->data_write);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ DMA Transfer Lists Record -----------\n"
			"msg-r all       "
			"msg-r busy      "
			"msg-w all       "
			"msg-w busy      "
			"data-r all      "
			"data-r busy     "
			"data-w all      "
			"data-w busy   \n");
	seq_printf(s,
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct dma_list_count *)g_proc._dma_list.data)->msg_read_all,
			((struct dma_list_count *)g_proc._dma_list.data)->msg_read_busy,
			((struct dma_list_count *)g_proc._dma_list.data)->msg_write_all,
			((struct dma_list_count *)g_proc._dma_list.data)->msg_write_busy,
			((struct dma_list_count *)g_proc._dma_list.data)->data_read_all,
			((struct dma_list_count *)g_proc._dma_list.data)->data_read_busy,
			((struct dma_list_count *)g_proc._dma_list.data)->data_write_all,
			((struct dma_list_count *)g_proc._dma_list.data)->data_write_busy);
	seq_printf(s, "\n\n");

	seq_printf(s, "------------ Last DMA Transfer Information -----------\n"
			"direction       "
			"src address     "
			"dest address    "
			"size          \n");
	seq_printf(s,
			"   read         "
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct dma_info *)g_proc._dma_info.data)->read.src_address,
			((struct dma_info *)g_proc._dma_info.data)->read.dest_address,
			((struct dma_info *)g_proc._dma_info.data)->read.len);
	seq_printf(s, "\n");
	seq_printf(s,
			"   write        "
			"0x%08x\t"
			"0x%08x\t"
			"0x%08x\t",
			((struct dma_info *)g_proc._dma_info.data)->write.src_address,
			((struct dma_info *)g_proc._dma_info.data)->write.dest_address,
			((struct dma_info *)g_proc._dma_info.data)->write.len);
	seq_printf(s, "\n");
	seq_printf(s, "\n");
}
#endif

static int mcc_proc_read(struct seq_file *s, void *v)
{
	seq_printf(s, "\nBuild Time["__DATE__", "__TIME__"]\n\n");

	//get_dma_info();
	if (g_proc._get_rp_wp)
		g_proc._get_rp_wp();
	if (g_proc._get_dma_trans_list_info)
		g_proc._get_dma_trans_list_info();

	show_mcc_info(s);

	return 0;
}

static int mcc_proc_open(struct inode *inode, struct file *file)
{
	struct mcc_proc_entry *item = PDE(inode)->data;
	return single_open(file, item->read, item);
}

static ssize_t mcc_proc_write(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos)
{
	struct seq_file *s = file->private_data;
	struct mcc_proc_entry *item = s->private;

	if (item->write) {
		return item->write(file, buf, count, ppos);
	}

	return -ENOSYS;
}

static int proc_msg_init(void)
{
	int i = 0;

	g_proc.proc_enable		= 0;
	g_proc._get_rp_wp		= NULL;
	g_proc._get_dma_trans_list_info = NULL;

	//s_irq_record.int_x.name		= "PCI Int-X";
	s_irq_record.int_x.count	= 0;
	s_irq_record.int_x.handler_cost = 0;
	//s_irq_record.door_bell.name	= "Door Bell";
	s_irq_record.door_bell.count	= 0;
	s_irq_record.door_bell.handler_cost = 0;
	//g_proc._irq_record.name		= "Interrupt Number Recording";
	g_proc._irq_record.data		= &s_irq_record;

	s_recv_timer.cur_interval	= 0;
	s_recv_timer.max_interval	= 0;
	//g_proc._msg_recv_timer.name	= "Message Receiving Timer";
	g_proc._msg_recv_timer.data	= &s_recv_timer;

	s_recv_queue.cur_interval	= 0;
	s_recv_queue.max_interval	= 0;
	//g_proc._msg_recv_queue.name	= "Message Receiving Queue";
	g_proc._msg_recv_queue.data	= &s_recv_queue;

	s_send_cost.cur_pciv_cost	= 0;
	s_send_cost.max_ctl_cost	= 0;
	s_send_cost.cur_pciv_cost	= 0;
	s_send_cost.max_ctl_cost	= 0;
	//g_proc._msg_send_cost.name	= "Message Sending Time Cost";
	g_proc._msg_send_cost.data	= &s_send_cost;

	s_msg_len.cur_thd_send		= 0;
	s_msg_len.max_thd_send		= 0;
	s_msg_len.cur_irq_send		= 0;
	s_msg_len.max_irq_send		= 0;
	//g_proc._msg_send_len.name	= "Message Length";
	g_proc._msg_send_len.data	= &s_msg_len;

	//s_msg_num.dev_number		= g_local_handler->remote_device_number;
	//for (i = 0; i < s_msg_num.dev_number; i++) {
	for (i = 0; i < MAX_PCIE_DEVICES; i++) {
		//s_msg_num.target[i].dev_slot = g_hi35xx_dev_map[i]->slot_index;
		s_msg_num.target[i].dev_slot = 0;
		s_msg_num.target[i].irq_send = 0;
		s_msg_num.target[i].irq_recv = 0;
		s_msg_num.target[i].thread_send = 0;
		s_msg_num.target[i].thread_recv = 0;
	}
	//g_proc._msg_number.name		= "Message Send/Recv Count";
	g_proc._msg_number.data		= &s_msg_num;

	//s_rp_wp.dev_number            = g_local_handler->remote_device_number;
	//for (i = 0; i < s_rp_wp.dev_number; i++) {
	for (i = 0; i < MAX_PCIE_DEVICES; i++) {
		//s_rp_wp.target[i].dev_slot = g_hi35xx_dev_map[i]->slot_index;
		s_rp_wp.target[i].dev_slot = 0;
		s_rp_wp.target[i].irq_send_rp = 0;
		s_rp_wp.target[i].irq_send_wp = 0;
		s_rp_wp.target[i].irq_recv_rp = 0;
		s_rp_wp.target[i].irq_recv_wp = 0;
		s_rp_wp.target[i].thrd_send_rp = 0;
		s_rp_wp.target[i].thrd_send_wp = 0;
		s_rp_wp.target[i].thrd_recv_rp = 0;
		s_rp_wp.target[i].thrd_recv_wp = 0;
	}
	//g_proc._rp_wp.name		= "Message Read/Write Pointer";
	g_proc._rp_wp.data		= &s_rp_wp;

	//s_pmsg_send.dev_number            = g_local_handler->remote_device_number;
	//for (i = 0; i < s_pmsg_send.dev_number; i++) {
	for (i = 0; i < MAX_PCIE_DEVICES; i++) {
		//s_pmsg_send.target[i].dev_slot = g_hi35xx_dev_map[i]->slot_index;
		s_pmsg_send.target[i].dev_slot = 0;
		s_pmsg_send.target[i].cur_interval = 0;
		s_pmsg_send.target[i].max_interval = 0;
		s_pmsg_send.target[i].last_jf = 0;
	}
	//g_proc._pmsg_interval.name	= "Pre-view Message Interval";
	g_proc._pmsg_interval.data	= &s_pmsg_send;

	s_dma_count.msg_write		= 0;
	s_dma_count.msg_read		= 0;
	s_dma_count.data_write		= 0;
	s_dma_count.data_read		= 0;
	//g_proc._dma_times.name		= "DMA Transfer Counts";
	g_proc._dma_count.data		= &s_dma_count;

	s_dma_list_count.msg_read_all	= 12;
	s_dma_list_count.msg_read_busy	= 0;
	s_dma_list_count.msg_write_all	= 12;
	s_dma_list_count.msg_write_busy	= 0;
	s_dma_list_count.data_read_all	= 12;
	s_dma_list_count.data_read_busy	= 0;
	s_dma_list_count.data_write_all = 12;
	s_dma_list_count.data_write_busy = 0;
	//g_proc._dma_list.name		= "DMA Transfer List Information";
	g_proc._dma_list.data		= &s_dma_list_count;

	s_dma_info.read.src_address		= 0x0;
	s_dma_info.read.dest_address		= 0x0;
	s_dma_info.read.len			= 0x0;
	s_dma_info.write.src_address		= 0x0;
	s_dma_info.write.dest_address		= 0x0;
	s_dma_info.write.len			= 0x0;
	//g_proc._dma_info.name		= "DMA Task Information";
	g_proc._dma_info.data		= &s_dma_info;
	return 0;
}

static struct file_operations mcc_proc_ops = {
	.owner	= THIS_MODULE,
	.open	= mcc_proc_open,
	.read	= seq_read,
	.write	= mcc_proc_write,
	.llseek	= seq_lseek,
	.release= single_release
};

struct mcc_proc_entry *mcc_create_proc(char *entry_name, _mcc_proc_read read, void *data)
{
	struct proc_dir_entry *entry;
	int i;

	if (proc_msg_init()) {
		printk(KERN_ERR "Proc message init failed\n");
		return NULL;
	}

	for (i = 0; i < MAX_PROC_ENTRIES; i++) {
		if (!s_proc_items[i].entry)
			break;
	}

	if (MAX_PROC_ENTRIES == i) {
		return NULL;
	}

	entry = create_proc_entry(entry_name, 0, s_mcc_map_proc);
	if (!entry)
		return NULL;


	entry->proc_fops	= &mcc_proc_ops;
	strncpy(s_proc_items[i].entry_name, entry_name, strlen(entry_name));
	s_proc_items[i].entry	= entry;
	s_proc_items[i].read	= mcc_proc_read;
	s_proc_items[i].pData	= data;
	entry->data		= &s_proc_items[i];

	return &s_proc_items[i];
}

void mcc_remove_proc(char *entry_name)
{
	int i;
	for (i = 0; i < MAX_PROC_ENTRIES; i++) {
		if (!strcmp(s_proc_items[i].entry_name, entry_name))
			break;
	}

	if (MAX_PROC_ENTRIES == i) {
		proc_trace(PROC_ERR, "proc entry[%s] to be removed doesnot exist!", entry_name);
		return;
	}

	remove_proc_entry(s_proc_items[i].entry_name, s_mcc_map_proc);
	s_proc_items[i].entry = NULL;
}

int mcc_proc_init(void)
{
	s_mcc_map_proc = proc_mkdir(PROC_DIR_NAME, NULL);
	memset(s_proc_items, 0x00, sizeof(s_proc_items));

	if (NULL == s_mcc_map_proc) {
		proc_trace(PROC_ERR, "make proc dir failed!");
		return -1;
	}

	return 0;
}

void mcc_proc_exit(void)
{
	remove_proc_entry(PROC_DIR_NAME, NULL);
}

static ctl_table mcc_eproc_table[] = {
	{
		.procname	= "proc_message_enable",
		.data		= &g_proc.proc_enable,
		.maxlen		= sizeof(g_proc.proc_enable),
		.mode		= 0644,
		.proc_handler	= proc_dointvec
	},
	{}
};

static ctl_table pci_dir_table[] = {
	{
		.procname	= "pci",
		.mode		= 0555,
		.child		= mcc_eproc_table
	},
	{}
};

static ctl_table pci_parent_tbl[] = {
	{
		.procname	= "dev",
		.mode		= 0555,
		.child		= pci_dir_table
	},
	{}
};

static struct ctl_table_header *mcc_eproc_tbl_head;

int __init mcc_init_sysctl(void)
{
	mcc_eproc_tbl_head = register_sysctl_table(pci_parent_tbl);
	if (!mcc_eproc_tbl_head)
		return -ENOMEM;
	return 0;
}

void mcc_exit_sysctl(void)
{
	unregister_sysctl_table(mcc_eproc_tbl_head);
}

