#ifndef __HI_MCC_PROC_HEADER__
#define __HI_MCC_PROC_HEADER__

#define PROC_DIR_NAME		"pci_mcc"

#define  MAX_PROC_ENTRIES	32

#define PROC_DEBUG	4
#define PROC_INFO	3
#define PROC_ERR	2
#define PROC_FATAL	1
#define CURRENT_LEVEL	2
#define proc_trace(level, s, params...) do{if(level <= CURRENT_LEVEL)\
	printk(KERN_INFO "[%s, %d]:" s "\n",__func__, __LINE__, ##params);\
}while(0)

typedef int (*_mcc_proc_read)(struct seq_file *, void *);
typedef int (*_mcc_proc_write)(struct file *file,
		const char __user *buf, size_t count, loff_t *ppos);

enum mcc_proc_message_enable_level {
	PROC_LEVEL_I	= 1,
	PROC_LEVEL_II	= 2,
};

struct mcc_proc_entry {
	char entry_name[32];
	struct proc_dir_entry *entry;

	_mcc_proc_read read;
	_mcc_proc_write write;

	int b_default;
	void *pData;

	void *proc_info;
};

struct int_param {
	char name[64];
	unsigned int count;
	unsigned int handler_cost;
};

struct irq_record {
	struct int_param int_x;
	struct int_param door_bell;
};

struct msg_recv_timer {
	unsigned int cur_interval;
	unsigned int max_interval;
};

struct msg_recv_queue {
	unsigned int cur_interval;
	unsigned int max_interval;
};

struct msg_send_cost {
	unsigned int cur_pciv_cost;
	unsigned int max_pciv_cost;
	unsigned int cur_ctl_cost;
	unsigned int max_ctl_cost;
};

struct msg_len {
	unsigned int cur_thd_send;
	unsigned int max_thd_send;
	unsigned int cur_irq_send;
	unsigned int max_irq_send;
};

struct dev_msg_number {
	unsigned int dev_slot;
	unsigned int irq_send;
	unsigned int irq_recv;
	unsigned int thread_send;
	unsigned int thread_recv;
};

struct msg_number {
	struct dev_msg_number target[MAX_PCIE_DEVICES];
	unsigned int dev_number;
};

struct dev_rp_wp {
	unsigned int dev_slot;
	unsigned int irq_send_rp;
	unsigned int irq_send_wp;
	unsigned int irq_recv_rp;
	unsigned int irq_recv_wp;
	unsigned int thrd_send_rp;
	unsigned int thrd_send_wp;
	unsigned int thrd_recv_rp;
	unsigned int thrd_recv_wp;
};

struct rp_wp {
	struct dev_rp_wp target[MAX_PCIE_DEVICES];
	unsigned int dev_number;
};

struct dev_pmsg_send {
	unsigned int dev_slot;
	unsigned int cur_interval;
	unsigned int max_interval;
	unsigned int last_jf;
};

struct pre_v_msg_send {
	struct dev_pmsg_send target[MAX_PCIE_DEVICES];
	unsigned int dev_number;
};

struct dma_count {
	unsigned int msg_write;
	unsigned int msg_read;
	unsigned int data_write;
	unsigned int data_read;
};

struct dma_list_count {
	unsigned int msg_read_all;
	unsigned int msg_read_busy;
	unsigned int msg_write_all;
	unsigned int msg_write_busy;
	unsigned int data_read_all;
	unsigned int data_read_busy;
	unsigned int data_write_all;
	unsigned int data_write_busy;
};

struct dma_task_info {
	unsigned int src_address;
	unsigned int dest_address;
	unsigned int len;
};

struct dma_info {
	struct dma_task_info read;
	struct dma_task_info write;
};

struct proc_item {
	char name[128];
	struct list_head head;
	void *data;
};

struct proc_message {
	unsigned int proc_enable;
	struct proc_item _irq_record;
	struct proc_item _msg_recv_timer;
	struct proc_item _msg_recv_queue;
	struct proc_item _msg_send_cost;
	struct proc_item _msg_send_len;
	struct proc_item _msg_number;
	struct proc_item _rp_wp;
	struct proc_item _pmsg_interval;
	struct proc_item _dma_count;
	struct proc_item _dma_list;
	struct proc_item _dma_info;
	void (*_get_rp_wp)(void);
	void (*_get_dma_trans_list_info)(void);
};

extern struct proc_message g_proc;

//struct mcc_proc_item *mcc_create_proc(char *entry_name, _mcc_proc_read read, void *data);
struct mcc_proc_entry *mcc_create_proc(char *entry_name, _mcc_proc_read read, void *data);

void mcc_remove_proc(char *entry_name);

int mcc_proc_init(void);

void mcc_proc_exit(void);

int __init mcc_init_sysctl(void);

void mcc_exit_sysctl(void);

#endif /* __HI_MCC_PROC_HEADER__ */
